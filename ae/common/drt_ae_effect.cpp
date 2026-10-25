/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — one source, six effects (Input, Output, Grade, macOS Fix, Knee, AgX).
 *
 * Compiled once per role: -DDRT_ROLE_OUTPUT=1 gives "minColor Output" (picture formation,
 * drt_transform), -DDRT_ROLE_INPUT=1 gives "minColor Input" (media interpretation,
 * drt_input_transform). Everything else is shared: the parameter table drives the
 * UI, the read into DrtParams and the preset write-back; the CPU path is the C++
 * twin through AE's iterate suites; the Metal path is the embedded shim + params +
 * kernel + wrapper compiled once per device (drt_ae_gpu_metal.mm; CUDA on Windows,
 * drt_ae_gpu_cuda.cpp; see drt_ae_common.h).
 *
 * fnord-shaped on purpose: the effect declares its own input on its popups,
 * transforms whatever pixels reach it, and never asks AE what the layer or the
 * project is. See ae/README.md for the comp discipline around it.
 */
#include "drt_ae_common.h"
#if DRT_ROLE_GRADE
#include "drt_ae_wheels.h"
#include "drt_ae_keybars.h"
#endif

#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_GeneralPlug.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <sys/stat.h>

namespace drtae {

namespace {
/* /tmp/<name> on macOS, %TEMP%\<name> on Windows */
std::string devPath(const char *name)
{
#ifdef AE_OS_WIN
    const char *t = std::getenv("TEMP");
    return std::string(t ? t : "C:\\Windows\\Temp") + "\\" + name;
#else
    return std::string("/tmp/") + name;
#endif
}
bool fileExists(const std::string &p) { struct stat st; return ::stat(p.c_str(), &st) == 0; }
const std::string &logPath() { static const std::string p = devPath("mincolor_ae.log"); return p; }
bool debugLogOn() { static const bool on = fileExists(logPath()); return on; }
} // namespace

bool gpuDisabled() { static const bool off = fileExists(devPath("mincolor_ae_nogpu")); return off; }

void dlog(const char *fmt, ...)
{
    if (!debugLogOn()) return;
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    FILE *f = std::fopen(logPath().c_str(), "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}

} // namespace drtae

using drtae::dlog;

/* Bump the minor version with every parameter-table change (see drt_ae_params.h)
   and mirror it in the PiPLs' AE_Effect_Version: PF_VERSION(major, minor, 0, DEVELOP, 1). */
#define DRT_MAJOR_VERSION 0
#define DRT_MINOR_VERSION 1
#define DRT_BUG_VERSION   4   /* 1: the Output's View/Render pair removed; 2: the Knee and slider drag ranges; 3: the AgX (2026-10-04); 4: the OpenDRT look controls hidden (2026-10-25). PF_VERSION's minor field is 4 bits (15 max); further table changes count here */

extern "C" {
DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data,
                            PF_ParamDef *params[], PF_LayerDef *output, void *extra);
}

/* ------------------------------------------------------------ popup strings */

namespace drtae {

const int kWorkingGamutMap[] = { DRT_IN_AP1, DRT_IN_AP0, DRT_IN_REC709, DRT_IN_P3D65, DRT_IN_REC2020,
                                 DRT_IN_FILMLIGHT_EGAMUT, DRT_IN_FILMLIGHT_EGAMUT2, DRT_IN_DAVINCI_WG,
                                 /* the rest of the input gamuts, appended 2026-10-03 so the comp can be in any of
                                    them (parity with the Output's Input Gamut); earlier indices unchanged */
                                 DRT_IN_ARRI_WG3, DRT_IN_ARRI_WG4, DRT_IN_RED_WG, DRT_IN_SONY_SGAMUT3,
                                 DRT_IN_SONY_SGAMUT3CINE, DRT_IN_PANASONIC_VGAMUT, DRT_IN_XYZ };
const int kWorkingGamutCount = int(sizeof(kWorkingGamutMap) / sizeof(kWorkingGamutMap[0]));

namespace {
std::string join(const char *first, const char *const *names, int n, const int *map = nullptr)
{
    std::string s;
    if (first) s += first;
    for (int k = 0; k < n; ++k) {
        if (!s.empty()) s += '|';
        s += names[map ? map[k] : k];
    }
    return s;
}
} // namespace

const char *inGamutPopup()      { static std::string s = join(nullptr, drt::kInGamutNames, DRT_IN_GAMUT_COUNT); return s.c_str(); }
const char *inOetfPopup(int count)
{
    static std::string all, part;
    if (count >= DRT_OETF_COUNT) { if (all.empty()) all = join(nullptr, drt::kInOetfNames, DRT_OETF_COUNT); return all.c_str(); }
    if (part.empty()) part = join(nullptr, drt::kInOetfNames, count);
    return part.c_str();
}
const char *workingGamutPopup() { static std::string s = join(nullptr, drt::kInGamutNames, kWorkingGamutCount, kWorkingGamutMap); return s.c_str(); }
const char *lookPopup()
{
    static std::string s;
    if (s.empty()) { s = "Custom"; for (int k = 0; k < drt::kLookCount; ++k) { s += '|'; s += drt::kLooks[k].name; } }
    return s.c_str();
}
const char *tonescalePopup()
{
    static std::string s;
    if (s.empty()) { s = "Use Look"; for (int k = 0; k < drt::kTonescaleCount; ++k) { s += '|'; s += drt::kTonescales[k].name; } }
    return s.c_str();
}
const char *displayPopup()
{
    static std::string s;
    if (s.empty()) { s = "Custom"; for (int k = 0; k < drt::kDisplayCount; ++k) { s += '|'; s += drt::kDisplays[k].name; } }
    return s.c_str();
}

} // namespace drtae

namespace {

using drtae::Row;

/* ----------------------------------------------------------- table helpers */

int paramIndex(int row) { return row + 1; } /* 0 is the input layer */

const char *popupString(const Row &r)
{
    if (r.popup) return r.popup;
    if (r.i == &drt::DrtParams::in_gamut) return drtae::inGamutPopup();
    if (r.i == &drt::DrtParams::in_oetf) return drtae::inOetfPopup(r.choices);
    if (r.i == &drt::DrtParams::working_gamut || r.af == &drt::DrtAgxParams::working_gamut) return drtae::workingGamutPopup();
    return "";
}
int popupChoices(const Row &r) { return r.map && !r.choices ? drtae::kWorkingGamutCount : r.choices; }

int fieldToChoice(const Row &r, int v)
{
    if (!r.map) return v + 1;
    for (int k = 0; k < popupChoices(r); ++k) if (r.map[k] == v) return k + 1;
    return 1;
}
int choiceToField(const Row &r, int choice)
{
    const int k = choice - 1;
    if (k < 0 || k >= popupChoices(r)) return r.map ? r.map[0] : 0;
    return r.map ? r.map[k] : k;
}

/* Rows not shown since 0.1.4: the OpenDRT look and tonescale controls, and the Output's
   Rendering / Surround / Clamp / Peak, which only the OpenDRT rendering reads. minColor
   offers the un-tone-mapped conversion; the processes that use OpenDRT underneath (the
   Input's inverse transfers, the Grade's zones) keep running on these rows' values,
   which are still read, so nothing renders differently. Ids and streams are kept. */
bool rowHidden(int row)
{
    const Row &r = DRT_ROWS[row];
    (void)r;
#if DRT_ROLE_OUTPUT
    if (row >= drtae::kOutputLookRowsFrom && row < drtae::kOutputLookRowsFrom + drtae::kLookRowCount) return true;
    return (r.kind == drtae::kPopup && (r.i == &drt::DrtParams::out_view || r.i == &drt::DrtParams::tn_su)) ||
           (r.kind == drtae::kCheck && r.i == &drt::DrtParams::clamp_out);
#elif DRT_ROLE_INPUT
    /* Peak stays: an inverse transfer reads the master's peak */
    return row >= drtae::kInputLookTableFrom && row < drtae::kInputLookTableFrom + drtae::kLookRowCount &&
           !(r.kind == drtae::kFloat && r.f == &drt::DrtParams::tn_Lp);
#elif DRT_ROLE_GRADE
    /* the secondary's per-window switches are drawn by the key bars */
    if (r.kind == drtae::kCheck && (r.gi == &drt::DrtGradeParams::sec_hue_on || r.gi == &drt::DrtGradeParams::sec_sat_on ||
                                    r.gi == &drt::DrtGradeParams::sec_lev_on)) return true;
    /* the reference group keeps its twirl and Display Encoding (the eyedroppers' decode);
       Peak goes too: the zones' norm is taken before the tonescale, so no peak reaches it */
    return row > drtae::kGradeReferenceRowsFrom && row < DRT_ROW_COUNT - 1 && r.kind != drtae::kPresetDisplay;
#else
    (void)row;
    return false;
#endif
}

#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_GRADE
AEGP_PluginID g_aegpId = 0;   /* GlobalSetup; for the stream calls in hideStreams() */

/* PF_PUI_INVISIBLE from ParamsSetup is enough on a fresh instance, but an instance saved
   before 0.1.4 still shows every hidden group's twirl and any hidden row whose stored
   value is not the default (AE 26.5, checked 2026-10-25). So each instance's hidden
   streams also get the dynamic HIDDEN flag, as the SDK's Supervisor sample hides params. */
void hideStreams(PF_InData *in_data)
{
    if (!g_aegpId) return;
    SPBasicSuite *sp = in_data->pica_basicP;
    AEGP_PFInterfaceSuite1 *pfi = nullptr; AEGP_StreamSuite6 *ss = nullptr;
    AEGP_DynamicStreamSuite4 *ds = nullptr; AEGP_EffectSuite5 *es = nullptr;
    const bool ok = sp->AcquireSuite(kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1, (const void **)&pfi) == kSPNoError &&
                    sp->AcquireSuite(kAEGPStreamSuite, kAEGPStreamSuiteVersion6, (const void **)&ss) == kSPNoError &&
                    sp->AcquireSuite(kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4, (const void **)&ds) == kSPNoError &&
                    sp->AcquireSuite(kAEGPEffectSuite, kAEGPEffectSuiteVersion5, (const void **)&es) == kSPNoError;
    AEGP_EffectRefH meH = nullptr;
    if (ok && pfi->AEGP_GetNewEffectForEffect(g_aegpId, in_data->effect_ref, &meH) == A_Err_NONE && meH) {
        for (int k = 0; k < DRT_ROW_COUNT; ++k) {
            if (!rowHidden(k)) continue;
            AEGP_StreamRefH st = nullptr;
            if (ss->AEGP_GetNewEffectStreamByIndex(g_aegpId, meH, paramIndex(k), &st) != A_Err_NONE || !st) continue;   /* group ends have no stream */
            AEGP_DynStreamFlags f = 0;
            if (ds->AEGP_GetDynamicStreamFlags(st, &f) == A_Err_NONE && !(f & AEGP_DynStreamFlag_HIDDEN))
                ds->AEGP_SetDynamicStreamFlag(st, AEGP_DynStreamFlag_HIDDEN, FALSE, TRUE);
            ss->AEGP_DisposeStream(st);
        }
        es->AEGP_DisposeEffect(meH);
    }
    if (pfi) sp->ReleaseSuite(kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1);
    if (ss)  sp->ReleaseSuite(kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
    if (ds)  sp->ReleaseSuite(kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    if (es)  sp->ReleaseSuite(kAEGPEffectSuite, kAEGPEffectSuiteVersion5);
}
#endif

/* Effect defaults: the DCTL's StickShift defaults with the input set for a
   colour-managed compositor (linear ACEScg) instead of Resolve's DWG / DI. */
DrtRender effectDefaults()
{
    DrtRender x;
    x.d = drt::drt_stickshift_defaults();
    x.d.in_gamut = DRT_IN_AP1;
    x.d.in_oetf = DRT_OETF_LINEAR;
    x.d.working_gamut = DRT_IN_AP1;
#if DRT_ROLE_INPUT
    x.d.tn_sh = 0.0f;   /* Shoulder Clip 0 on a fresh Input (2026-10-02); a Look preset still brings its own */
#endif
    x.g = drt::drt_grade_defaults();
    /* Knee: an HDR source into an SDR delivery, BT.2390's knee start */
    x.d.kn_src = 1000.0f; x.d.kn_tgt = 100.0f; x.d.kn_auto = 1; x.d.kn_start = 0.5f;
    x.a = drt::drt_agx_defaults();
    return x;
}

/* A row's bound value, whichever block it lives in. Popups on the AgX block are floats. */
float rowFloat(const Row &r, const DrtRender &x) { return r.f ? x.d.*(r.f) : r.gf ? x.g.*(r.gf) : x.a.*(r.af); }
void setRowFloat(const Row &r, DrtRender &x, float v) { if (r.f) x.d.*(r.f) = v; else if (r.gf) x.g.*(r.gf) = v; else x.a.*(r.af) = v; }
int rowPopupField(const Row &r, const DrtRender &x) { return r.i ? x.d.*(r.i) : int(std::lround(x.a.*(r.af))); }
void setRowPopupField(const Row &r, DrtRender &x, int v) { if (r.i) x.d.*(r.i) = v; else x.a.*(r.af) = float(v); }

/* Read a bound row's value out of a checked-out or live PF_ParamDef. */
void rowFromDef(const Row &r, const PF_ParamDef &d, DrtRender &x)
{
    switch (r.kind) {
    case drtae::kFloat: case drtae::kFloatHidden: setRowFloat(r, x, float(d.u.fs_d.value)); break;
    case drtae::kCheck: if (r.i) x.d.*(r.i) = d.u.bd.value ? 1 : 0; else x.g.*(r.gi) = d.u.bd.value ? 1 : 0; break;
    case drtae::kPopup: setRowPopupField(r, x, choiceToField(r, int(d.u.pd.value))); break;
    default: break;
    }
}

/* Write a bound row's value into a live PF_ParamDef; true if it changed. */
bool rowToDef(const Row &r, const DrtRender &x, PF_ParamDef &d)
{
    const drt::DrtParams &p = x.d;
    switch (r.kind) {
    case drtae::kFloat: case drtae::kFloatHidden: {
        const double v = double(rowFloat(r, x));
        if (d.u.fs_d.value == v) return false;
        d.u.fs_d.value = v;
        return true;
    }
    case drtae::kCheck: {
        const PF_Boolean v = (r.i ? p.*(r.i) : x.g.*(r.gi)) ? 1 : 0;
        if (d.u.bd.value == v) return false;
        d.u.bd.value = v;
        return true;
    }
    case drtae::kPopup: {
        const int v = fieldToChoice(r, rowPopupField(r, x));
        if (d.u.pd.value == v) return false;
        d.u.pd.value = v;
        return true;
    }
    default: return false;
    }
}

/* ----------------------------------------------------------------- commands */

/* Must equal the PiPL words. Grade adds CUSTOM_UI for its wheels and, since its key blur
   reads neighbouring pixels (0.1.4), is not PIX_INDEPENDENT. */
const A_long kOutFlags  = PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_SEND_UPDATE_PARAMS_UI
#if DRT_ROLE_GRADE
                        | PF_OutFlag_CUSTOM_UI
#else
                        | PF_OutFlag_PIX_INDEPENDENT
#endif
                        ;
const A_long kOutFlags2 = PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER |
                          PF_OutFlag2_SUPPORTS_THREADED_RENDERING | PF_OutFlag2_SUPPORTS_GPU_RENDER_F32 |
                          PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;
#if DRT_ROLE_GRADE
static_assert(kOutFlags == 0x6008000, "update AE_Effect_Global_OutFlags in the Grade PiPL");
#else
static_assert(kOutFlags == 0x6000400, "update AE_Effect_Global_OutFlags in the PiPL");
#endif
static_assert(kOutFlags2 == 0xA001408, "update AE_Effect_Global_OutFlags_2 in the PiPL");

PF_Err About(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *[], PF_LayerDef *)
{
    (void)in_data; /* PF_SPRINTF reaches for it */
#if DRT_ROLE_AGX
    PF_SPRINTF(out_data->return_msg, "%s v%d.%d\rAgX by Troy Sobotka; Blender's AgX by Eary Chow et al. Ported in part from darktable's AgX module (GPL-3.0-or-later), modified.\rNot affiliated with or endorsed by Blender or darktable. minColorAE, GPL-3.0.",
               DRT_EFFECT_NAME, DRT_MAJOR_VERSION, DRT_MINOR_VERSION);
#elif DRT_ROLE_KNEE
    PF_SPRINTF(out_data->return_msg, "%s v%d.%d\rA highlight knee after BT.2390 (QCView's). minColorAE, GPL-3.0.",
               DRT_EFFECT_NAME, DRT_MAJOR_VERSION, DRT_MINOR_VERSION);
#else
    PF_SPRINTF(out_data->return_msg, "%s v%d.%d\rRendering derived from OpenDRT v1.1.0 by Jed Smith (GPL-3.0), modified.\rNot affiliated with or endorsed by OpenDRT. minColorAE, GPL-3.0.",
               DRT_EFFECT_NAME, DRT_MAJOR_VERSION, DRT_MINOR_VERSION);
#endif
    return PF_Err_NONE;
}

PF_Err GlobalSetup(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *[], PF_LayerDef *)
{
    out_data->my_version = PF_VERSION(DRT_MAJOR_VERSION, DRT_MINOR_VERSION, DRT_BUG_VERSION, PF_Stage_DEVELOP, 1);
#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_GRADE
    AEGP_UtilitySuite6 *u = nullptr;   /* an id for hideStreams(); without it old instances only look untidy */
    if (in_data->pica_basicP->AcquireSuite(kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6, (const void **)&u) == kSPNoError && u) {
        u->AEGP_RegisterWithAEGP(nullptr, DRT_EFFECT_NAME, &g_aegpId);
        in_data->pica_basicP->ReleaseSuite(kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    }
#else
    (void)in_data;
#endif
#if DRT_ROLE_GRADE
    drtwheels::g_log = dlog;
#endif
    out_data->out_flags  = kOutFlags;
    out_data->out_flags2 = kOutFlags2;
    return PF_Err_NONE;
}

PF_Err ParamsSetup(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *[], PF_LayerDef *)
{
    PF_ParamDef def;
    const DrtRender dx = effectDefaults();

    int autoId = 0;
    for (int row = 0; row < DRT_ROW_COUNT; ++row) {
        const Row &r = DRT_ROWS[row];
        const int id = r.id ? r.id : ++autoId;   /* the id AE files the stream under; params[] stays positional */
        AEFX_CLR_STRUCT(def);
        /* the SDK's ...X macros clear def, so floats and checkboxes use the plain ones to keep this */
        if (r.kind == drtae::kFloatHidden || r.kind == drtae::kPopupHidden || rowHidden(row)) def.ui_flags = PF_PUI_INVISIBLE;
        switch (r.kind) {
        case drtae::kFloatHidden:
        case drtae::kFloat:
            PF_ADD_FLOAT_SLIDER(r.name, r.lo, r.hi, r.shi > r.slo ? r.slo : r.lo, r.shi > r.slo ? r.shi : r.hi, AEFX_DEFAULT_CURVE_TOLERANCE,
                                double(rowFloat(r, dx)), r.prec, PF_ValueDisplayFlag_NONE, false, id);
            break;
        case drtae::kCheck:
            PF_ADD_CHECKBOX(r.name, "", (r.i ? dx.d.*(r.i) : dx.g.*(r.gi)) ? 1 : 0, 0, id);
            break;
        case drtae::kPopupHidden:
            PF_ADD_POPUP(r.name, popupChoices(r), fieldToChoice(r, rowPopupField(r, dx)), popupString(r), id);
            break;
        case drtae::kPopup:
#if DRT_ROLE_INPUT
            if (r.i == &drt::DrtParams::in_oetf) def.flags = PF_ParamFlag_SUPERVISE;   /* an inverse entry sets Peak Luminance */
#endif
            PF_ADD_POPUP(r.name, popupChoices(r), fieldToChoice(r, rowPopupField(r, dx)), popupString(r), id);
            break;
        case drtae::kTopic:
            def.flags = PF_ParamFlag_START_COLLAPSED;
            PF_ADD_TOPIC(r.name, id);
            break;
        case drtae::kTopicOpen:
            PF_ADD_TOPIC(r.name, id);
            break;
        case drtae::kEndTopic:
            PF_END_TOPIC(id);
            break;
        case drtae::kPresetLook:
            def.flags = PF_ParamFlag_SUPERVISE;
            PF_ADD_POPUP(r.name, drt::kLookCount + 1, 2 /* Standard */, drtae::lookPopup(), id);
            break;
        case drtae::kPresetTonescale:
            def.flags = PF_ParamFlag_SUPERVISE;
            PF_ADD_POPUP(r.name, drt::kTonescaleCount + 1, 1 /* Use Look */, drtae::tonescalePopup(), id);
            break;
        case drtae::kPresetDisplay:
            def.flags = PF_ParamFlag_SUPERVISE;
#if DRT_ROLE_GRADE
            /* the Grade's is the encoding of whatever sits above it, which in an OCIO project
               is nothing: the comp's own working space (0.1.4; sRGB Display before) */
            PF_ADD_POPUP(r.name, drt::kDisplayCount + 1, drt::drt_display_preset_index(DRT_DG_WORKING, DRT_EOTF_LINEAR) + 2 /* None - Linear / Working Gamut */, drtae::displayPopup(), id);
#else
            PF_ADD_POPUP(r.name, drt::kDisplayCount + 1, drt::drt_display_preset_index(DRT_DG_REC709, DRT_EOTF_POWER_2_2) + 2 /* sRGB Display */, drtae::displayPopup(), id);
#endif
            break;
        case drtae::kColorPick:
            def.flags = PF_ParamFlag_SUPERVISE;
            PF_ADD_COLOR(r.name, 128, 128, 128, id);
            break;
        case drtae::kPresetRender:   /* retired, no row */
            break;
        case drtae::kWheels:
#if DRT_ROLE_GRADE
            /* a data-less row that owns a rectangle in the Effect Controls panel; we paint it and take its events */
            def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
            def.ui_width = drtwheels::kWidth;
            def.ui_height = A_short(drtwheels::heightFor(1.0f, drtwheels::wheelsInRow(r.choices)));
            PF_ADD_NULL(r.name, id);
#endif
            break;
        case drtae::kKeyBars:
#if DRT_ROLE_GRADE
            def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
            def.ui_width = drtkeys::kWidth;
            def.ui_height = drtkeys::kHeight;
            PF_ADD_NULL(r.name, id);
#endif
            break;
        }
    }
    out_data->num_params = DRT_ROW_COUNT + 1;
#if DRT_ROLE_GRADE
    {
        PF_CustomUIInfo ci;
        AEFX_CLR_STRUCT(ci);
        ci.events = PF_CustomEFlag_EFFECT;   /* Effect Controls panel events only; nothing in the comp or layer views */
        ci.comp_ui_alignment = ci.layer_ui_alignment = ci.preview_ui_alignment = PF_UIAlignment_NONE;
        const PF_Err uerr = PF_REGISTER_UI(in_data, &ci);
        if (uerr) return uerr;
    }
#endif
    return PF_Err_NONE;
}

/* Read every bound row into a DrtRender and derive it the way PreRender does (the Grade's eyedroppers). */
[[maybe_unused]] DrtRender liveParams(PF_ParamDef *params[])
{
    DrtRender x = effectDefaults();
    for (int k = 0; k < DRT_ROW_COUNT; ++k) rowFromDef(DRT_ROWS[k], *params[paramIndex(k)], x);
#if DRT_ROLE_INPUT
    drt::drt_inverse_display(x.d);
    x.d.tn_su = DRT_SURROUND_DARK;   /* the inverse assumes a Dark-surround render; no row (old projects' stored value is ignored) */
#endif
#if DRT_ROLE_GRADE
    x.d.in_gamut = x.d.working_gamut;
    x.d.in_oetf = DRT_OETF_LINEAR;
#endif
#if DRT_ROLE_OUTPUT
    x.d.working_gamut = x.d.in_gamut;   /* "Working Gamut" as a display gamut means the comp's */
#endif
    x.d = drt::drt_derive(x.d);
#if DRT_ROLE_GRADE
    x.g = drt::drt_grade_derive(x.g, x.d);
#endif
#if DRT_ROLE_AGX
    x.a = drt::drt_agx_derive(x.a);
#endif
    return x;
}

#if DRT_ROLE_GRADE
/* An eyedropper row changed (Set Colour / Add to Range / Remove from Range). AE hands over
   the comp's final value at the picked pixel, before the viewer's OCIO transform (AE 26.5,
   checked 2026-10-25): in an OCIO project the working-space value itself; with a minColor
   Output above the Grade, that Output's encoding. Undo the encoding named by the Grade's
   Display Encoding (pickToWorking), measure it the way the secondary does, then move the
   hue / purity / level windows. Until 0.1.4 the pick went through the OpenDRT inverse and
   sRGB whatever the setting, which no Un-tone-mapped Output had applied. */
inline float wrap180(float d) { d = std::fmod(d + 540.0f, 360.0f); if (d < 0.0f) d += 360.0f; return d - 180.0f; }

/* The Un-tone-mapped Output (drt_transform's out_view 1) backwards, for the Output's Input
   Gamut = the Grade's Working Gamut: decode the EOTF, undo the 100-nit scaling of PQ and
   HLG, display gamut -> XYZ (the inverse of the forward's matrix, CATs included, built from
   the forward itself), XYZ -> working. "None - Linear / Working Gamut" comes back unchanged.
   Custom (choice 1) has nothing to name a display by and is taken as no encoding. */
drt::float3 pickToWorking(const drt::DrtParams &d, int choice, drt::float3 c)
{
    drt::DrtParams q = d;
    q.display_gamut = DRT_DG_WORKING; q.eotf = DRT_EOTF_LINEAR;
    if (choice >= 2 && choice - 2 < drt::kDisplayCount) {
        q.display_gamut = drt::kDisplays[choice - 2].display_gamut;
        q.eotf = drt::kDisplays[choice - 2].eotf;
    }
    drt::float3 lin = drt::drt_decode(q, c);
    const float s = q.eotf == DRT_EOTF_PQ ? 0.01f : (q.eotf == DRT_EOTF_HLG ? 0.1f : 1.0f);
    lin = lin / s;
    const drt::float3 c0 = drt::drt_xyz_to_display_gamut(q, drt::make_float3(1.0f, 0.0f, 0.0f), q.display_gamut);
    const drt::float3 c1 = drt::drt_xyz_to_display_gamut(q, drt::make_float3(0.0f, 1.0f, 0.0f), q.display_gamut);
    const drt::float3 c2 = drt::drt_xyz_to_display_gamut(q, drt::make_float3(0.0f, 0.0f, 1.0f), q.display_gamut);
    const drt::drt_mat3 toDisplay = drt::drt_make_mat3(drt::make_float3(c0.x, c1.x, c2.x),
                                                       drt::make_float3(c0.y, c1.y, c2.y),
                                                       drt::make_float3(c0.z, c1.z, c2.z));
    const drt::float3 xyz = drt::drt_vdot(drt::drt_mat3_inverse(toDisplay), lin);
    const drt::drt_mat3 xyzToWorking = drt::drt_make_mat3(drt::make_float3(q.wk_m00, q.wk_m01, q.wk_m02),
                                                          drt::make_float3(q.wk_m10, q.wk_m11, q.wk_m12),
                                                          drt::make_float3(q.wk_m20, q.wk_m21, q.wk_m22));
    return drt::drt_vdot(xyzToWorking, xyz);
}

PF_Err pickColour(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], int row)
{
    const int mode = DRT_ROWS[row].choices;
    PF_ColorParamSuite1 *cs = nullptr;
    if (AEFX_AcquireSuite(in_data, out_data, kPFColorParamSuite, kPFColorParamSuiteVersion1, nullptr, (void **)&cs) != PF_Err_NONE || !cs)
        return PF_Err_NONE;
    PF_PixelFloat c; c.red = c.green = c.blue = 0.0f; c.alpha = 1.0f;
    const PF_Err cerr = cs->PF_GetFloatingPointColorFromColorDef(in_data->effect_ref, params[paramIndex(row)], &c);
    AEFX_ReleaseSuite(in_data, out_data, kPFColorParamSuite, kPFColorParamSuiteVersion1, nullptr);
    if (cerr) return PF_Err_NONE;

    DrtRender x = liveParams(params);
    int displayChoice = 1;
    for (int k = 0; k < DRT_ROW_COUNT; ++k)
        if (DRT_ROWS[k].kind == drtae::kPresetDisplay) displayChoice = int(params[paramIndex(k)]->u.pd.value);
    drt::float3 scene = pickToWorking(x.d, displayChoice, drt::make_float3(c.red, c.green, c.blue));
    drt::DrtGradeParams &g = x.g;
    /* the key is measured on the source, the pick is the graded pixel: take the global
       exposure and temperature back off (exact); the zones are not undone, so a pick is
       exact with the zones at rest and close otherwise */
    const float ex = std::exp2(g.exposure);
    const drt::drt_mat3 wb = drt::drt_make_mat3(drt::make_float3(g.d_wb_m00, g.d_wb_m01, g.d_wb_m02),
                                                drt::make_float3(g.d_wb_m10, g.d_wb_m11, g.d_wb_m12),
                                                drt::make_float3(g.d_wb_m20, g.d_wb_m21, g.d_wb_m22));
    scene = drt::drt_vdot(drt::drt_mat3_inverse(wb), scene / ex);
    const drt::drt_sec_measure m = drt::drt_secondary_measure(x.d, scene);
    const float lev = m.log2_norm - g.d_grey_l;   /* stops over grey */
    dlog("pick mode %d rgb %.4f %.4f %.4f -> source %.4f %.4f %.4f -> hue %.1f purity %.3f level %.2f st",
         mode, c.red, c.green, c.blue, scene.x, scene.y, scene.z, m.hue_deg, m.purity, lev);

    const float hueMargin = 5.0f, purMargin = 0.05f, lumMargin = 0.25f;   /* degrees, purity, stops */
    if (mode == 0) {
        g.sec_hue = m.hue_deg;
        g.sec_hue_width = m.purity < 0.05f ? 360.0f : 60.0f;   /* a neutral pick has no hue: take them all */
        g.sec_sat_min = std::max(0.0f, m.purity - 0.15f);
        g.sec_sat_max = std::min(2.0f, m.purity + 0.15f);
        g.sec_lev_min = lev - 1.0f;
        g.sec_lev_max = lev + 1.0f;
        g.sec_enable = 1;
        g.sec_show_mask = 0;
    } else if (mode == 1) {
        /* widen every window the pick falls outside of, by just enough plus a margin */
        const float half = 0.5f * g.sec_hue_width, d = wrap180(m.hue_deg - g.sec_hue);
        if (std::fabs(d) > half) {
            const float lo = std::min(-half, d - hueMargin), hi = std::max(half, d + hueMargin);
            g.sec_hue = std::fmod(g.sec_hue + 0.5f * (lo + hi) + 720.0f, 360.0f);
            g.sec_hue_width = std::min(360.0f, hi - lo);
        }
        if (m.purity < g.sec_sat_min) g.sec_sat_min = std::max(0.0f, m.purity - purMargin);
        if (m.purity > g.sec_sat_max) g.sec_sat_max = std::min(2.0f, m.purity + purMargin);
        if (lev < g.sec_lev_min) g.sec_lev_min = lev - lumMargin;
        if (lev > g.sec_lev_max) g.sec_lev_max = lev + lumMargin;
        g.sec_enable = 1;
    } else {
        /* narrow the one window that excludes the pick with the smallest change */
        const float half = 0.5f * g.sec_hue_width, d = wrap180(m.hue_deg - g.sec_hue);
        const float lmin = g.sec_lev_min, lmax = g.sec_lev_max;
        float costH = 1e9f, costP = 1e9f, costL = 1e9f;
        if (std::fabs(d) < half) costH = (half - std::fabs(d)) / 30.0f;
        if (m.purity > g.sec_sat_min && m.purity < g.sec_sat_max) costP = std::min(m.purity - g.sec_sat_min, g.sec_sat_max - m.purity) / 0.2f;
        if (lev > lmin && lev < lmax) costL = std::min(lev - lmin, lmax - lev);
        if (costH <= costP && costH <= costL && costH < 1e8f) {
            float lo = -half, hi = half;
            if (d >= 0.0f) hi = d - hueMargin; else lo = d + hueMargin;
            if (hi <= lo) { hi = lo + 1.0f; }
            g.sec_hue = std::fmod(g.sec_hue + 0.5f * (lo + hi) + 720.0f, 360.0f);
            g.sec_hue_width = hi - lo;
        } else if (costP <= costL && costP < 1e8f) {
            if (m.purity - g.sec_sat_min < g.sec_sat_max - m.purity) g.sec_sat_min = std::min(g.sec_sat_max, m.purity + purMargin);
            else                                                       g.sec_sat_max = std::max(g.sec_sat_min, m.purity - purMargin);
        } else if (costL < 1e8f) {
            if (lev - lmin < lmax - lev) g.sec_lev_min = std::min(g.sec_lev_max, lev + lumMargin);
            else                         g.sec_lev_max = std::max(g.sec_lev_min, lev - lumMargin);
        }
    }

    for (int k = 0; k < DRT_ROW_COUNT; ++k) {
        PF_ParamDef *d = params[paramIndex(k)];
        if (rowToDef(DRT_ROWS[k], x, *d)) d->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
    }
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
    return PF_Err_NONE;
}
#endif

/* A preset popup changed: resolve the preset into a DrtParams built from the live
   values, then write back every bound row that differs. AE adds a keyframe where
   the target is animated and just sets the value otherwise (probe, 2026-09-30). */
PF_Err UserChangedParam(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[],
                        const PF_UserChangedParamExtra *extra)
{
    const int row = extra->param_index - 1;
    if (row < 0 || row >= DRT_ROW_COUNT) return PF_Err_NONE;
    const Row &changed = DRT_ROWS[row];
#if DRT_ROLE_GRADE
    if (changed.kind == drtae::kColorPick) return pickColour(in_data, out_data, params, row);
#endif
#if DRT_ROLE_INPUT
    /* Input Transfer set to an inverse entry: Peak Luminance takes that encoding's
       default (1000 nits for PQ and HLG, 100 for the SDR ones), as the Output's
       Display preset does. The row stays editable and the render honours it; the
       inverse always assumes a Dark surround (the Output's default, no row here). The Input's default Peak
       Luminance is the SDR 100, and a PQ master inverted against a 100-nit
       ceiling has no source above 100 nits. */
    if (row == 1 && changed.kind == drtae::kPopup) {
        const int oetf = choiceToField(changed, int(params[extra->param_index]->u.pd.value));
        if (oetf >= DRT_OETF_INVERSE_FIRST && oetf < DRT_OETF_COUNT) {
            const drt::DrtDisplay &disp = drt::kDisplays[drt::kInverseDisplayMap[oetf - DRT_OETF_INVERSE_FIRST]];
            for (int k = 0; k < DRT_ROW_COUNT; ++k) {
                const Row &r = DRT_ROWS[k];
                PF_ParamDef *d = params[paramIndex(k)];
                if (r.kind == drtae::kFloat && r.f == &drt::DrtParams::tn_Lp) {
                    if (float(d->u.fs_d.value) != disp.default_Lp) {
                        d->u.fs_d.value = disp.default_Lp;
                        d->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
                        out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
                    }
                }
            }
        }
        return PF_Err_NONE;
    }
#endif
    if (changed.kind != drtae::kPresetLook && changed.kind != drtae::kPresetTonescale &&
        changed.kind != drtae::kPresetDisplay)
        return PF_Err_NONE;

    const int choice = int(params[extra->param_index]->u.pd.value); /* 1-based; 1 = Custom / Use Look */
    if (choice <= 1 && changed.kind != drtae::kPresetTonescale) return PF_Err_NONE;   /* Custom: leave the rows */

    DrtRender x = effectDefaults();
    for (int k = 0; k < DRT_ROW_COUNT; ++k) rowFromDef(DRT_ROWS[k], *params[paramIndex(k)], x);
    drt::DrtParams &p = x.d;
    (void)in_data;

    int tonescaleRow = -1;
    for (int k = 0; k < DRT_ROW_COUNT; ++k) if (DRT_ROWS[k].kind == drtae::kPresetTonescale) tonescaleRow = k;

    switch (changed.kind) {
    case drtae::kPresetLook:
        drt::drt_apply_look(p, drt::kLooks[choice - 2]);
        /* the look carries its own tonescale; the Tonescale popup goes back to "Use Look" */
        if (tonescaleRow >= 0 && params[paramIndex(tonescaleRow)]->u.pd.value != 1) {
            params[paramIndex(tonescaleRow)]->u.pd.value = 1;
            params[paramIndex(tonescaleRow)]->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
        }
        break;
    case drtae::kPresetTonescale:
        if (choice <= 1) {
            /* "Use Look": put the current Look's tonescale rows back (nothing to go back to under Custom) */
            int lookRow = -1;
            for (int k = 0; k < DRT_ROW_COUNT; ++k) if (DRT_ROWS[k].kind == drtae::kPresetLook) lookRow = k;
            const int look = lookRow >= 0 ? int(params[paramIndex(lookRow)]->u.pd.value) : 1;
            if (look <= 1) return PF_Err_NONE;
            drt::drt_apply_look_tonescale(p, drt::kLooks[look - 2]);
        } else {
            drt::drt_apply_tonescale(p, drt::kTonescales[choice - 2]);
        }
        break;
    case drtae::kPresetDisplay: {
        /* the encoding only: Surround is the viewing room, set once by hand (upstream's
           preset writes the display type's standard room; minColor keeps the row) */
        const int su = p.tn_su;
        drt::drt_apply_display(p, drt::kDisplays[choice - 2]);
        p.tn_su = su;
        p.tn_Lp = drt::kDisplays[choice - 2].default_Lp; /* as the Nuke node's display presets do */
        p.clamp_out = drt::kDisplays[choice - 2].eotf == DRT_EOTF_LINEAR ? 0 : 1;   /* None carries values above 1 */
        break;
    }
    default: break;
    }

    for (int k = 0; k < DRT_ROW_COUNT; ++k) {
        PF_ParamDef *d = params[paramIndex(k)];
        if (rowToDef(DRT_ROWS[k], x, *d)) d->uu.change_flags = PF_ChangeFlag_CHANGED_VALUE;
    }
    out_data->out_flags |= PF_OutFlag_FORCE_RERENDER;
    return PF_Err_NONE;
}

/* Output: in the Un-tone-mapped view the look rows do nothing, so grey them; what
   stays live is the input rows, Rendering, and the display encoding rows.
   Input: the look rows mean nothing unless an inverse transfer is selected, so
   grey them out otherwise. AE sends PF_Cmd_UPDATE_PARAMS_UI whenever a param
   changes (PF_OutFlag_SEND_UPDATE_PARAMS_UI); only flags that differ are written back. */
#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_KNEE || DRT_ROLE_AGX
static bool rowGreyed(const Row &r, int k, bool grey)
{
    if (r.kind == drtae::kEndTopic || r.kind == drtae::kTopic || r.kind == drtae::kTopicOpen) return false;
#if DRT_ROLE_OUTPUT
    (void)k;
    const bool encoding = r.kind == drtae::kPresetDisplay ||
                           (r.kind == drtae::kPopup && (r.i == &drt::DrtParams::out_view || r.i == &drt::DrtParams::display_gamut ||
                                                        r.i == &drt::DrtParams::eotf || r.i == &drt::DrtParams::tn_su)) ||
                           (r.kind == drtae::kCheck && r.i == &drt::DrtParams::clamp_out) ||
                           (r.kind == drtae::kFloat && r.f == &drt::DrtParams::tn_Lp);
    const bool always = r.kind == drtae::kPopup && (r.i == &drt::DrtParams::in_gamut || r.i == &drt::DrtParams::in_oetf);
    if (always || encoding) return false;        /* input and encoding rows stay live */
    return grey;                                 /* look rows: grey when the active rendering is Un-tone-mapped */
#elif DRT_ROLE_KNEE
    (void)k;
    return grey && r.kind == drtae::kFloat && r.f == &drt::DrtParams::kn_start;   /* Knee Start greys under Auto */
#elif DRT_ROLE_AGX
    (void)k;
    return grey && r.kind == drtae::kFloat && r.af == &drt::DrtAgxParams::hdr_purity;   /* HDR Purity greys at Peak 100 (SDR) */
#else
    return k >= drtae::kInputLookRowsFrom && grey;
#endif
}
#endif

PF_Err UpdateParamsUI(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[])
{
#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_GRADE
    hideStreams(in_data);
#endif
#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_KNEE || DRT_ROLE_AGX
    bool grey = false;
#if DRT_ROLE_OUTPUT
    int viewRow = -1;
    for (int k = 0; k < DRT_ROW_COUNT; ++k)
        if (DRT_ROWS[k].kind == drtae::kPopup && DRT_ROWS[k].i == &drt::DrtParams::out_view) viewRow = k;
    if (viewRow < 0) return PF_Err_NONE;
    grey = choiceToField(DRT_ROWS[viewRow], int(params[paramIndex(viewRow)]->u.pd.value)) == 1;
#elif DRT_ROLE_KNEE
    for (int k = 0; k < DRT_ROW_COUNT; ++k)
        if (DRT_ROWS[k].kind == drtae::kCheck && DRT_ROWS[k].i == &drt::DrtParams::kn_auto) grey = params[paramIndex(k)]->u.bd.value != 0;
#elif DRT_ROLE_AGX
    for (int k = 0; k < DRT_ROW_COUNT; ++k)
        if (DRT_ROWS[k].kind == drtae::kFloat && DRT_ROWS[k].af == &drt::DrtAgxParams::peak) grey = params[paramIndex(k)]->u.fs_d.value <= 100.0;
#else
    grey = choiceToField(DRT_ROWS[1], int(params[paramIndex(1)]->u.pd.value)) < DRT_OETF_INVERSE_FIRST;
#endif
    AEFX_SuiteScoper<PF_ParamUtilsSuite3> pu(in_data, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, out_data);
    for (int k = 0; k < DRT_ROW_COUNT; ++k) {
        const Row &r = DRT_ROWS[k];
        if (r.kind == drtae::kEndTopic || r.kind == drtae::kTopic || r.kind == drtae::kTopicOpen) continue;
        if (rowHidden(k)) continue;   /* writing a hidden row's flags back could show it again */
        const bool want = rowGreyed(r, k, grey);
        PF_ParamDef def = *params[paramIndex(k)];
        const bool isGrey = (def.ui_flags & PF_PUI_DISABLED) != 0;
        if (isGrey == want) continue;
        if (want) def.ui_flags |= PF_PUI_DISABLED; else def.ui_flags &= ~PF_PUI_DISABLED;
        pu->PF_UpdateParamUI(in_data->effect_ref, paramIndex(k), &def);
    }
#else
    (void)in_data; (void)out_data; (void)params;
#endif
    return PF_Err_NONE;
}

#if DRT_ROLE_GRADE
/* Effect Controls events: the key bars row has its own handler, the wheel rows theirs. */
PF_Err gradeEvent(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra)
{
    if (!extra->contextH || (**extra->contextH).w_type != PF_Window_EFFECT) return PF_Err_NONE;
    const int row = int(extra->effect_win.index) - 1;
    if (row >= 0 && row < DRT_ROW_COUNT && DRT_ROWS[row].kind == drtae::kKeyBars)
        return drtkeys::handleEvent(in_data, out_data, params, extra);
    return drtwheels::handleEvent(in_data, out_data, params, extra);
}
#endif

void DisposePreRenderData(void *p) { std::free(p); }

PF_Err PreRender(PF_InData *in_data, PF_OutData *, PF_PreRenderExtra *extra)
{
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;

    /* the GPU only when AE's framework for this frame is the one our backend renders with */
    if (!drtae::gpuDisabled() && drtae::gpuFramework() != PF_GPU_Framework_NONE && extra->input->what_gpu == drtae::gpuFramework())
        extra->output->flags |= PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;

    DrtRender *p = static_cast<DrtRender *>(std::malloc(sizeof(DrtRender)));
    if (!p) return PF_Err_OUT_OF_MEMORY;
    *p = effectDefaults();

    for (int k = 0; k < DRT_ROW_COUNT && !err; ++k) {
        const Row &r = DRT_ROWS[k];
        if (r.kind != drtae::kFloat && r.kind != drtae::kFloatHidden && r.kind != drtae::kCheck && r.kind != drtae::kPopup) continue;
        PF_ParamDef d;
        AEFX_CLR_STRUCT(d);
        ERR(PF_CHECKOUT_PARAM(in_data, paramIndex(k), in_data->current_time, in_data->time_step, in_data->time_scale, &d));
        if (!err) {
            rowFromDef(r, d, *p);
            ERR(PF_CHECKIN_PARAM(in_data, &d));
        }
    }
#if DRT_ROLE_INPUT
    drt::drt_inverse_display(p->d);   /* "OpenDRT inverse: <encoding>" names the file's gamut + EOTF */
    p->d.tn_su = DRT_SURROUND_DARK;   /* the inverse assumes a Dark-surround render; no row */
#endif
#if DRT_ROLE_GRADE
    p->d.in_gamut = p->d.working_gamut;   /* the reference measures working-gamut pixels: in_m** = working -> XYZ */
    p->d.in_oetf = DRT_OETF_LINEAR;
#endif
#if DRT_ROLE_OUTPUT
    p->d.working_gamut = p->d.in_gamut;   /* "Working Gamut" as a display gamut means the comp's */
#endif
    p->d = drt::drt_derive(p->d);
#if DRT_ROLE_GRADE
    p->g = drt::drt_grade_derive(p->g, p->d);
    /* the key blur, in this render's pixels (a half-resolution preview blurs half as far); the
       input is asked for that much more on every side so the key blurs across tile edges; the
       output stays inside the request (AE renders nothing for a result bigger than it asked
       for), so the input world is bigger and the output sits at offX, offY inside it */
    p->blurRx = p->blurRy = 0;
    p->offX = p->offY = 0;
    const PF_LRect asked = req.rect;
    if (p->g.sec_enable != 0 && p->g.sec_blur > 0.0f) {
        const double sx = double(in_data->downsample_x.num) / double(in_data->downsample_x.den ? in_data->downsample_x.den : 1);
        const double sy = double(in_data->downsample_y.num) / double(in_data->downsample_y.den ? in_data->downsample_y.den : 1);
        p->blurRx = int(std::ceil(double(p->g.sec_blur) * sx));
        p->blurRy = int(std::ceil(double(p->g.sec_blur) * sy));
        req.rect.left -= p->blurRx; req.rect.right += p->blurRx;
        req.rect.top -= p->blurRy; req.rect.bottom += p->blurRy;
    }
#else
    p->blurRx = p->blurRy = 0;
    p->offX = p->offY = 0;
#endif
#if DRT_ROLE_KNEE
    p->d = drt::drt_knee_derive(p->d);
#endif
#if DRT_ROLE_AGX
    p->a = drt::drt_agx_derive(p->a);
#endif

    extra->output->pre_render_data = p;
    extra->output->delete_pre_render_data_func = DisposePreRenderData;

    ERR(extra->cb->checkout_layer(in_data->effect_ref, 0, 0, &req, in_data->current_time, in_data->time_step,
                                  in_data->time_scale, &in_result));
#if DRT_ROLE_GRADE
    if (!err && (p->blurRx > 0 || p->blurRy > 0)) {
        PF_LRect r = in_result.result_rect;   /* what came back, cut to what was asked */
        r.left = std::max(r.left, asked.left); r.top = std::max(r.top, asked.top);
        r.right = std::min(r.right, asked.right); r.bottom = std::min(r.bottom, asked.bottom);
        if (r.right < r.left) r.right = r.left;
        if (r.bottom < r.top) r.bottom = r.top;
        extra->output->result_rect = r;
        extra->output->max_result_rect = in_result.max_result_rect;
        p->offX = int(r.left - in_result.result_rect.left);
        p->offY = int(r.top - in_result.result_rect.top);
        return err;
    }
#endif
    UnionLRect(&in_result.result_rect, &extra->output->result_rect);
    UnionLRect(&in_result.max_result_rect, &extra->output->max_result_rect);
    return err;
}

/* -------------------------------------------------------------- CPU render */

/* Straight-alpha colour transform: unpremultiply, apply, premultiply. */
inline void applyStraight(const DrtRender &p, float &r, float &g, float &b, float a)
{
    drt::float3 c = drt::make_float3(r, g, b);
    const bool partial = a > 0.0f && a < 1.0f;
    if (partial) c = c / a;
    c = DRT_APPLY(p, c);
    if (partial) c = c * a;
    r = c.x; g = c.y; b = c.z;
}

PF_Err pixel32(void *refcon, A_long, A_long, PF_PixelFloat *in, PF_PixelFloat *out)
{
    const DrtRender &p = *static_cast<const DrtRender *>(refcon);
    float r = in->red, g = in->green, b = in->blue;
    applyStraight(p, r, g, b, in->alpha);
    out->red = r; out->green = g; out->blue = b; out->alpha = in->alpha;
    return PF_Err_NONE;
}

inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

PF_Err pixel16(void *refcon, A_long, A_long, PF_Pixel16 *in, PF_Pixel16 *out)
{
    const DrtRender &p = *static_cast<const DrtRender *>(refcon);
    const float s = 1.0f / 32768.0f;
    float r = in->red * s, g = in->green * s, b = in->blue * s;
    applyStraight(p, r, g, b, in->alpha * s);
    out->red = A_u_short(clamp01(r) * 32768.0f + 0.5f);
    out->green = A_u_short(clamp01(g) * 32768.0f + 0.5f);
    out->blue = A_u_short(clamp01(b) * 32768.0f + 0.5f);
    out->alpha = in->alpha;
    return PF_Err_NONE;
}

PF_Err pixel8(void *refcon, A_long, A_long, PF_Pixel8 *in, PF_Pixel8 *out)
{
    const DrtRender &p = *static_cast<const DrtRender *>(refcon);
    const float s = 1.0f / 255.0f;
    float r = in->red * s, g = in->green * s, b = in->blue * s;
    applyStraight(p, r, g, b, in->alpha * s);
    out->red = A_u_char(clamp01(r) * 255.0f + 0.5f);
    out->green = A_u_char(clamp01(g) * 255.0f + 0.5f);
    out->blue = A_u_char(clamp01(b) * 255.0f + 0.5f);
    out->alpha = in->alpha;
    return PF_Err_NONE;
}

#if DRT_ROLE_GRADE
/* The Grade with a blurred key, on the CPU: the key of every pixel, blurred across then
   down (a Gaussian, 3 sigma = the radius, edges clamped), then the grade with each pixel's
   blurred key. Rows run on AE's threads (iterate_generic). in and out are the same world. */
struct KeyBlurJob {
    const DrtRender *p;
    PF_PixelFormat fmt;
    PF_EffectWorld *in, *out;
    int w, h;            /* the input world: the key and its blur */
    int ow, oh;          /* the output world, at offX, offY inside it */
    float *key, *tmp;
    std::vector<float> wx, wy;   /* normalised taps, index k + radius */
};

inline void loadStraight(const KeyBlurJob &j, int x, int y, float &r, float &g, float &b, float &a)
{
    const char *row = reinterpret_cast<const char *>(j.in->data) + std::ptrdiff_t(y) * j.in->rowbytes;
    if (j.fmt == PF_PixelFormat_ARGB128) { const PF_PixelFloat &q = reinterpret_cast<const PF_PixelFloat *>(row)[x]; r = q.red; g = q.green; b = q.blue; a = q.alpha; }
    else if (j.fmt == PF_PixelFormat_ARGB64) { const PF_Pixel16 &q = reinterpret_cast<const PF_Pixel16 *>(row)[x]; const float s = 1.0f / 32768.0f; r = q.red * s; g = q.green * s; b = q.blue * s; a = q.alpha * s; }
    else { const PF_Pixel8 &q = reinterpret_cast<const PF_Pixel8 *>(row)[x]; const float s = 1.0f / 255.0f; r = q.red * s; g = q.green * s; b = q.blue * s; a = q.alpha * s; }
    if (a > 0.0f && a < 1.0f) { r /= a; g /= a; b /= a; }
}

PF_Err keyRow(void *refcon, A_long, A_long y, A_long)
{
    KeyBlurJob &j = *static_cast<KeyBlurJob *>(refcon);
    for (int x = 0; x < j.w; ++x) {
        float r, g, b, a;
        loadStraight(j, x, int(y), r, g, b, a);
        j.key[std::size_t(y) * j.w + x] = drt::drt_grade_key(j.p->g, j.p->d, drt::make_float3(r, g, b));
    }
    return PF_Err_NONE;
}

PF_Err blurRowX(void *refcon, A_long, A_long y, A_long)
{
    KeyBlurJob &j = *static_cast<KeyBlurJob *>(refcon);
    const int rad = j.p->blurRx;
    const float *src = j.key + std::size_t(y) * j.w;
    float *dst = j.tmp + std::size_t(y) * j.w;
    for (int x = 0; x < j.w; ++x) {
        float s = 0.0f;
        for (int k = -rad; k <= rad; ++k) s += j.wx[k + rad] * src[std::min(std::max(x + k, 0), j.w - 1)];
        dst[x] = s;
    }
    return PF_Err_NONE;
}

PF_Err blurRowY(void *refcon, A_long, A_long y, A_long)
{
    KeyBlurJob &j = *static_cast<KeyBlurJob *>(refcon);
    const int rad = j.p->blurRy;
    float *dst = j.key + std::size_t(y) * j.w;
    for (int x = 0; x < j.w; ++x) {
        float s = 0.0f;
        for (int k = -rad; k <= rad; ++k) s += j.wy[k + rad] * j.tmp[std::size_t(std::min(std::max(int(y) + k, 0), j.h - 1)) * j.w + x];
        dst[x] = s;
    }
    return PF_Err_NONE;
}

PF_Err gradeRow(void *refcon, A_long, A_long y, A_long)
{
    KeyBlurJob &j = *static_cast<KeyBlurJob *>(refcon);
    char *row = reinterpret_cast<char *>(j.out->data) + std::ptrdiff_t(y) * j.out->rowbytes;
    const int iy = int(y) + j.p->offY;
    for (int x = 0; x < j.ow; ++x) {
        const int ix = x + j.p->offX;
        float r, g, b, a;
        loadStraight(j, ix, iy, r, g, b, a);
        drt::float3 c = drt::drt_grade_keyed(j.p->g, j.p->d, drt::make_float3(r, g, b), j.key[std::size_t(iy) * j.w + ix]);
        if (a > 0.0f && a < 1.0f) c = c * a;
        if (j.fmt == PF_PixelFormat_ARGB128) { PF_PixelFloat &q = reinterpret_cast<PF_PixelFloat *>(row)[x]; q.red = c.x; q.green = c.y; q.blue = c.z; q.alpha = a; }
        else if (j.fmt == PF_PixelFormat_ARGB64) { PF_Pixel16 &q = reinterpret_cast<PF_Pixel16 *>(row)[x];
            q.red = A_u_short(clamp01(c.x) * 32768.0f + 0.5f); q.green = A_u_short(clamp01(c.y) * 32768.0f + 0.5f); q.blue = A_u_short(clamp01(c.z) * 32768.0f + 0.5f); q.alpha = A_u_short(a * 32768.0f + 0.5f); }
        else { PF_Pixel8 &q = reinterpret_cast<PF_Pixel8 *>(row)[x];
            q.red = A_u_char(clamp01(c.x) * 255.0f + 0.5f); q.green = A_u_char(clamp01(c.y) * 255.0f + 0.5f); q.blue = A_u_char(clamp01(c.z) * 255.0f + 0.5f); q.alpha = A_u_char(a * 255.0f + 0.5f); }
    }
    return PF_Err_NONE;
}

std::vector<float> blurTaps(int rad)
{
    std::vector<float> t(std::size_t(2 * rad + 1));
    float sum = 0.0f;
    for (int k = -rad; k <= rad; ++k) sum += (t[std::size_t(k + rad)] = drt::drt_key_blur_weight(float(k), float(rad)));
    for (float &v : t) v /= sum;
    return t;
}

PF_Err RenderCPUKeyBlur(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                        PF_EffectWorld *in, PF_EffectWorld *out, DrtRender *p)
{
    if (fmt != PF_PixelFormat_ARGB128 && fmt != PF_PixelFormat_ARGB64 && fmt != PF_PixelFormat_ARGB32) return PF_Err_BAD_CALLBACK_PARAM;
    KeyBlurJob j;
    j.p = p; j.fmt = fmt; j.in = in; j.out = out;
    j.w = in->width; j.h = in->height;
    j.ow = std::min(out->width, in->width - p->offX); j.oh = std::min(out->height, in->height - p->offY);
    if (j.ow <= 0 || j.oh <= 0 || p->offX < 0 || p->offY < 0) return PF_Err_NONE;
    std::vector<float> key(std::size_t(j.w) * std::size_t(j.h)), tmp(key.size());
    j.key = key.data(); j.tmp = tmp.data();
    j.wx = blurTaps(p->blurRx); j.wy = blurTaps(p->blurRy);
    AEFX_SuiteScoper<PF_Iterate8Suite2> it(in_data, kPFIterate8Suite, kPFIterate8SuiteVersion2, out_data);
    PF_Err err = PF_Err_NONE;
    ERR(it->iterate_generic(j.h, &j, keyRow));
    ERR(it->iterate_generic(j.h, &j, blurRowX));
    ERR(it->iterate_generic(j.h, &j, blurRowY));
    ERR(it->iterate_generic(j.oh, &j, gradeRow));
    return err;
}
#endif

PF_Err RenderCPU(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                 PF_EffectWorld *in, PF_EffectWorld *out, DrtRender *p)
{
#if DRT_ROLE_GRADE
    if (p->blurRx > 0 || p->blurRy > 0) return RenderCPUKeyBlur(in_data, out_data, fmt, in, out, p);
#endif
    switch (fmt) {
    case PF_PixelFormat_ARGB128: {
        AEFX_SuiteScoper<PF_iterateFloatSuite2> it(in_data, kPFIterateFloatSuite, kPFIterateFloatSuiteVersion2, out_data);
        return it->iterate(in_data, 0, out->height, in, nullptr, p, pixel32, out);
    }
    case PF_PixelFormat_ARGB64: {
        AEFX_SuiteScoper<PF_iterate16Suite2> it(in_data, kPFIterate16Suite, kPFIterate16SuiteVersion2, out_data);
        return it->iterate(in_data, 0, out->height, in, nullptr, p, pixel16, out);
    }
    case PF_PixelFormat_ARGB32: {
        AEFX_SuiteScoper<PF_Iterate8Suite2> it(in_data, kPFIterate8Suite, kPFIterate8SuiteVersion2, out_data);
        return it->iterate(in_data, 0, out->height, in, nullptr, p, pixel8, out);
    }
    default:
        return PF_Err_BAD_CALLBACK_PARAM;
    }
}

PF_Err SmartRender(PF_InData *in_data, PF_OutData *out_data, PF_SmartRenderExtra *extra, bool isGPU)
{
    PF_Err err = PF_Err_NONE, err2 = PF_Err_NONE;
    DrtRender *p = static_cast<DrtRender *>(extra->input->pre_render_data);
    if (!p) return PF_Err_INTERNAL_STRUCT_DAMAGED;

    PF_EffectWorld *in = nullptr, *out = nullptr;
    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, 0, &in));
    ERR(extra->cb->checkout_output(in_data->effect_ref, &out));
    if (!err && in && out) {
        AEFX_SuiteScoper<PF_WorldSuite2> world(in_data, kPFWorldSuite, kPFWorldSuiteVersion2, out_data);
        PF_PixelFormat fmt = PF_PixelFormat_INVALID;
        ERR(world->PF_GetPixelFormat(in, &fmt));
        if (isGPU) ERR(drtae::renderGPU(in_data, out_data, fmt, in, out, extra, p));
        else       ERR(RenderCPU(in_data, out_data, fmt, in, out, p));
    }
    ERR2(extra->cb->checkin_layer_pixels(in_data->effect_ref, 0));
    return err;
}

} // namespace

/* ------------------------------------------------------------- entry points */

extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr, PF_PluginDataCB2 inPluginDataCallBackPtr,
                                                     SPBasicSuite *, const char *, const char *)
{
    PF_Err result = PF_Err_INVALID_CALLBACK;
    result = PF_REGISTER_EFFECT_EXT2(inPtr, inPluginDataCallBackPtr, DRT_EFFECT_NAME, DRT_MATCH_NAME, "minColor",
                                     AE_RESERVED_INFO, "EffectMain", "https://github.com/cbkow/minColorAE");
    return result;
}

PF_Err EffectMain(PF_Cmd cmd, PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_LayerDef *output,
                  void *extra)
{
    PF_Err err = PF_Err_NONE;
    dlog("%s cmd %d begin", DRT_EFFECT_NAME, int(cmd));
    try {
        switch (cmd) {
        case PF_Cmd_ABOUT:              err = About(in_data, out_data, params, output); break;
        case PF_Cmd_GLOBAL_SETUP:       err = GlobalSetup(in_data, out_data, params, output); break;
        case PF_Cmd_PARAMS_SETUP:       err = ParamsSetup(in_data, out_data, params, output); break;
        case PF_Cmd_USER_CHANGED_PARAM: err = UserChangedParam(in_data, out_data, params, static_cast<const PF_UserChangedParamExtra *>(extra)); break;
        case PF_Cmd_UPDATE_PARAMS_UI:   err = UpdateParamsUI(in_data, out_data, params); break;
#if DRT_ROLE_GRADE
        case PF_Cmd_EVENT:              err = gradeEvent(in_data, out_data, params, static_cast<PF_EventExtra *>(extra)); break;
#endif
        case PF_Cmd_GPU_DEVICE_SETUP:   err = drtae::gpuDeviceSetup(in_data, out_data, static_cast<PF_GPUDeviceSetupExtra *>(extra)); break;
        case PF_Cmd_GPU_DEVICE_SETDOWN: err = drtae::gpuDeviceSetdown(in_data, out_data, static_cast<PF_GPUDeviceSetdownExtra *>(extra)); break;
        case PF_Cmd_SMART_PRE_RENDER:   err = PreRender(in_data, out_data, static_cast<PF_PreRenderExtra *>(extra)); break;
        case PF_Cmd_SMART_RENDER:       err = SmartRender(in_data, out_data, static_cast<PF_SmartRenderExtra *>(extra), false); break;
        case PF_Cmd_SMART_RENDER_GPU:   err = SmartRender(in_data, out_data, static_cast<PF_SmartRenderExtra *>(extra), true); break;
        default: break;
        }
    } catch (PF_Err &thrown) {
        err = thrown;
    } catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    dlog("%s cmd %d end err %d", DRT_EFFECT_NAME, int(cmd), int(err));
    return err;
}
