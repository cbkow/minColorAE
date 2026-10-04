/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — one source, four effects (Input, Output, Grade, macOS Fix).
 *
 * Compiled once per role: -DDRT_ROLE_OUTPUT=1 gives "minColor Output" (picture formation,
 * drt_transform), -DDRT_ROLE_INPUT=1 gives "minColor Input" (media interpretation,
 * drt_input_transform). Everything else is shared: the parameter table drives the
 * UI, the read into DrtParams and the preset write-back; the CPU path is the C++
 * twin through AE's iterate suites; the Metal path is the embedded shim + params +
 * kernel + wrapper compiled once per device.
 *
 * fnord-shaped on purpose: the effect declares its own input on its popups,
 * transforms whatever pixels reach it, and never asks AE what the layer or the
 * project is. See ae/README.md for the comp discipline around it.
 */
#include "drt_ae_params.h"
#if DRT_ROLE_GRADE
#include "drt_ae_wheels.h"
#endif

#include "AEConfig.h"
#include "entry.h"
#include "AEFX_SuiteHelper.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectGPUSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "drt_ae_msl.h" /* generated: kDrtAeMsl = prefix + shim + params + kernel + wrapper */

#include <cstdio>
#include <mutex>
#include <sys/stat.h>

/* Development switches, read once per process:
 *   touch /tmp/mincolor_ae.log    -> every command and its result is appended there
 *   touch /tmp/mincolor_ae_nogpu  -> the effect never offers the GPU path (CPU only)
 * Both are file-existence checks so they work inside AE without env vars. */
namespace {
bool fileExists(const char *p) { struct stat st; return ::stat(p, &st) == 0; }
bool debugLogOn()  { static const bool on = fileExists("/tmp/mincolor_ae.log"); return on; }
bool gpuDisabled() { static const bool off = fileExists("/tmp/mincolor_ae_nogpu"); return off; }
void dlog(const char *fmt, ...)
{
    if (!debugLogOn()) return;
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    FILE *f = std::fopen("/tmp/mincolor_ae.log", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fputc('\n', f);
    std::fclose(f);
}
} // namespace

#if DRT_ROLE_OUTPUT
#define DRT_EFFECT_NAME  "minColor Output"
#define DRT_MATCH_NAME   "ski.bialkow minColor Output"
#define DRT_KERNEL_NAME  "drt_output_kernel"
#define DRT_ROWS         drtae::kOutputRows
#define DRT_ROW_COUNT    drtae::kOutputRowCount
#define DRT_APPLY(p, v)  drt::drt_transform((p).d, (v))
#elif DRT_ROLE_INPUT
#define DRT_EFFECT_NAME  "minColor Input"
#define DRT_MATCH_NAME   "ski.bialkow minColor Input"
#define DRT_KERNEL_NAME  "drt_input_kernel"
#define DRT_ROWS         drtae::kInputRows
#define DRT_ROW_COUNT    drtae::kInputRowCount
#define DRT_APPLY(p, v)  drt::drt_input_transform((p).d, (v))
#elif DRT_ROLE_GRADE
#define DRT_EFFECT_NAME  "minColor Grade"
#define DRT_MATCH_NAME   "ski.bialkow minColor Grade"
#define DRT_KERNEL_NAME  "drt_grade_kernel"
#define DRT_ROWS         drtae::kGradeRows
#define DRT_ROW_COUNT    drtae::kGradeRowCount
#define DRT_APPLY(p, v)  drt::drt_grade((p).g, (p).d, (v))
#elif DRT_ROLE_MACFIX
#define DRT_EFFECT_NAME  "minColor macOS Fix"
#define DRT_MATCH_NAME   "ski.bialkow minColor macOS Fix"
#define DRT_KERNEL_NAME  "drt_macfix_kernel"
#define DRT_ROWS         drtae::kMacFixRows
#define DRT_ROW_COUNT    drtae::kMacFixRowCount
#define DRT_APPLY(p, v)  drt::drt_macos_fix(v)
#elif DRT_ROLE_KNEE
#define DRT_EFFECT_NAME  "minColor Knee"
#define DRT_MATCH_NAME   "ski.bialkow minColor Knee"
#define DRT_KERNEL_NAME  "drt_knee_kernel"
#define DRT_ROWS         drtae::kKneeRows
#define DRT_ROW_COUNT    drtae::kKneeRowCount
#define DRT_APPLY(p, v)  drt::drt_knee((p).d, (v))
#else
#error define DRT_ROLE_OUTPUT, DRT_ROLE_INPUT, DRT_ROLE_GRADE, DRT_ROLE_MACFIX or DRT_ROLE_KNEE
#endif

/* What pre-render hands to render: the DRT block, and for Grade its own block too. */
struct DrtRender {
    drt::DrtParams      d;
    drt::DrtGradeParams g;
};

/* Bump the minor version with every parameter-table change (see drt_ae_params.h)
   and mirror it in the PiPLs' AE_Effect_Version: PF_VERSION(major, minor, 0, DEVELOP, 1). */
#define DRT_MAJOR_VERSION 0
#define DRT_MINOR_VERSION 1
#define DRT_BUG_VERSION   2   /* 1: the Output's View/Render pair removed; 2: the Knee and slider drag ranges (2026-10-04). PF_VERSION's minor field is 4 bits (15 max); further table changes count here */

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
    if (r.i == &drt::DrtParams::working_gamut) return drtae::workingGamutPopup();
    return "";
}
int popupChoices(const Row &r) { return r.map ? drtae::kWorkingGamutCount : r.choices; }

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
    return x;
}

/* Read a bound row's value out of a checked-out or live PF_ParamDef. */
void rowFromDef(const Row &r, const PF_ParamDef &d, DrtRender &x)
{
    switch (r.kind) {
    case drtae::kFloat: case drtae::kFloatHidden: if (r.f) x.d.*(r.f) = float(d.u.fs_d.value); else x.g.*(r.gf) = float(d.u.fs_d.value); break;
    case drtae::kCheck: if (r.i) x.d.*(r.i) = d.u.bd.value ? 1 : 0; else x.g.*(r.gi) = d.u.bd.value ? 1 : 0; break;
    case drtae::kPopup: x.d.*(r.i) = choiceToField(r, int(d.u.pd.value)); break;
    default: break;
    }
}

/* Write a bound row's value into a live PF_ParamDef; true if it changed. */
bool rowToDef(const Row &r, const DrtRender &x, PF_ParamDef &d)
{
    const drt::DrtParams &p = x.d;
    switch (r.kind) {
    case drtae::kFloat: case drtae::kFloatHidden: {
        const double v = r.f ? double(p.*(r.f)) : double(x.g.*(r.gf));
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
        const int v = fieldToChoice(r, p.*(r.i));
        if (d.u.pd.value == v) return false;
        d.u.pd.value = v;
        return true;
    }
    default: return false;
    }
}

/* ----------------------------------------------------------------- commands */

/* Must equal the PiPL words. Grade adds CUSTOM_UI for its wheels. */
const A_long kOutFlags  = PF_OutFlag_PIX_INDEPENDENT | PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_SEND_UPDATE_PARAMS_UI
#if DRT_ROLE_GRADE
                        | PF_OutFlag_CUSTOM_UI
#endif
                        ;
const A_long kOutFlags2 = PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_SMART_RENDER |
                          PF_OutFlag2_SUPPORTS_THREADED_RENDERING | PF_OutFlag2_SUPPORTS_GPU_RENDER_F32 |
                          PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG;
#if DRT_ROLE_GRADE
static_assert(kOutFlags == 0x6008400, "update AE_Effect_Global_OutFlags in the Grade PiPL");
#else
static_assert(kOutFlags == 0x6000400, "update AE_Effect_Global_OutFlags in the PiPL");
#endif
static_assert(kOutFlags2 == 0xA001408, "update AE_Effect_Global_OutFlags_2 in the PiPL");

PF_Err About(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *[], PF_LayerDef *)
{
    (void)in_data; /* PF_SPRINTF reaches for it */
    PF_SPRINTF(out_data->return_msg, "%s v%d.%d\rRendering derived from OpenDRT v1.1.0 by Jed Smith (GPL-3.0), modified.\rNot affiliated with or endorsed by OpenDRT. minColorAE, GPL-3.0.",
               DRT_EFFECT_NAME, DRT_MAJOR_VERSION, DRT_MINOR_VERSION);
    return PF_Err_NONE;
}

PF_Err GlobalSetup(PF_InData *, PF_OutData *out_data, PF_ParamDef *[], PF_LayerDef *)
{
    out_data->my_version = PF_VERSION(DRT_MAJOR_VERSION, DRT_MINOR_VERSION, DRT_BUG_VERSION, PF_Stage_DEVELOP, 1);
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
    const drt::DrtParams &dflt = dx.d;

    int autoId = 0;
    for (int row = 0; row < DRT_ROW_COUNT; ++row) {
        const Row &r = DRT_ROWS[row];
        const int id = r.id ? r.id : ++autoId;   /* the id AE files the stream under; params[] stays positional */
        AEFX_CLR_STRUCT(def);
        switch (r.kind) {
        case drtae::kFloatHidden:
            def.ui_flags = PF_PUI_INVISIBLE;
            /* fall through */
        case drtae::kFloat:
            PF_ADD_FLOAT_SLIDERX(r.name, r.lo, r.hi, r.shi > r.slo ? r.slo : r.lo, r.shi > r.slo ? r.shi : r.hi, double(r.f ? dflt.*(r.f) : dx.g.*(r.gf)), r.prec,
                                 PF_ValueDisplayFlag_NONE, 0, id);
            break;
        case drtae::kCheck:
            PF_ADD_CHECKBOXX(r.name, (r.i ? dflt.*(r.i) : dx.g.*(r.gi)) ? 1 : 0, 0, id);
            break;
        case drtae::kPopupHidden:
            def.ui_flags = PF_PUI_INVISIBLE;
            PF_ADD_POPUP(r.name, popupChoices(r), fieldToChoice(r, dflt.*(r.i)), popupString(r), id);
            break;
        case drtae::kPopup:
#if DRT_ROLE_INPUT
            if (r.i == &drt::DrtParams::in_oetf) def.flags = PF_ParamFlag_SUPERVISE;   /* an inverse entry sets Peak Luminance */
#endif
            PF_ADD_POPUP(r.name, popupChoices(r), fieldToChoice(r, dflt.*(r.i)), popupString(r), id);
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
            PF_ADD_POPUP(r.name, drt::kDisplayCount + 1, drt::drt_display_preset_index(DRT_DG_REC709, DRT_EOTF_POWER_2_2) + 2 /* sRGB Display */, drtae::displayPopup(), id);
            break;
        case drtae::kColorPick:
            def.flags = PF_ParamFlag_SUPERVISE;
            PF_ADD_COLOR(r.name, 128, 128, 128, id);
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

/* Read every bound row into a DrtRender and derive it the way PreRender does. */
DrtRender liveParams(PF_ParamDef *params[])
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
    return x;
}

#if DRT_ROLE_GRADE
/* An eyedropper row changed (Set Colour / Add to Range / Remove from Range). The pick is a
   display code value as the viewer shows it, i.e. after the Output on top: decode it with
   the DRT reference's display settings, invert the DRT back to the scene, measure it the way
   the secondary does, then move the hue / purity / luminance windows. */
inline float wrap180(float d) { d = std::fmod(d + 540.0f, 360.0f); if (d < 0.0f) d += 360.0f; return d - 180.0f; }

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
    drt::DrtParams q = x.d;
    q.in_oetf = DRT_OETF_INVERSE_FIRST;   /* any inverse entry: decode with q's own display settings, invert, to working */
    drt::float3 scene = drt::drt_input_transform(q, drt::make_float3(c.red, c.green, c.blue));
    drt::DrtGradeParams &g = x.g;
    /* the key is measured on the source, the pick is the graded pixel: take the global
       exposure and temperature back off (exact); the zones are not undone, so a pick is
       exact with the zones at rest and close otherwise */
    const float ex = std::exp2(g.exposure);
    scene = drt::make_float3(scene.x / (g.d_gain_r * ex), scene.y / (g.d_gain_g * ex), scene.z / (g.d_gain_b * ex));
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
#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_KNEE
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
#else
    return k >= drtae::kInputLookRowsFrom && grey;
#endif
}
#endif

PF_Err UpdateParamsUI(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[])
{
#if DRT_ROLE_OUTPUT || DRT_ROLE_INPUT || DRT_ROLE_KNEE
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
#else
    grey = choiceToField(DRT_ROWS[1], int(params[paramIndex(1)]->u.pd.value)) < DRT_OETF_INVERSE_FIRST;
#endif
    AEFX_SuiteScoper<PF_ParamUtilsSuite3> pu(in_data, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, out_data);
    for (int k = 0; k < DRT_ROW_COUNT; ++k) {
        const Row &r = DRT_ROWS[k];
        if (r.kind == drtae::kEndTopic || r.kind == drtae::kTopic || r.kind == drtae::kTopicOpen) continue;
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

void DisposePreRenderData(void *p) { std::free(p); }

PF_Err PreRender(PF_InData *in_data, PF_OutData *, PF_PreRenderExtra *extra)
{
    PF_Err err = PF_Err_NONE;
    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult in_result;

    if (!gpuDisabled()) extra->output->flags |= PF_RenderOutputFlag_GPU_RENDER_POSSIBLE;

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
#endif
#if DRT_ROLE_KNEE
    p->d = drt::drt_knee_derive(p->d);
#endif

    extra->output->pre_render_data = p;
    extra->output->delete_pre_render_data_func = DisposePreRenderData;

    ERR(extra->cb->checkout_layer(in_data->effect_ref, 0, 0, &req, in_data->current_time, in_data->time_step,
                                  in_data->time_scale, &in_result));
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

PF_Err RenderCPU(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                 PF_EffectWorld *in, PF_EffectWorld *out, DrtRender *p)
{
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

/* ------------------------------------------------------------ Metal render */

struct DrtAeHeader {
    int srcPitch;
    int dstPitch;
    int width;
    int height;
};

struct MetalGPUData {
    id<MTLComputePipelineState> pipeline;
};

PF_Err GPUDeviceSetup(PF_InData *in_data, PF_OutData *out_data, PF_GPUDeviceSetupExtra *extra)
{
    if (extra->input->what_gpu != PF_GPU_Framework_METAL) return PF_Err_NONE; /* CPU fallback */
    @autoreleasepool {
        AEFX_SuiteScoper<PF_HandleSuite1> handles(in_data, kPFHandleSuite, kPFHandleSuiteVersion1, out_data);
        AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);
        PF_GPUDeviceInfo dev;
        AEFX_CLR_STRUCT(dev);
        gpu->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &dev);
        id<MTLDevice> device = (id<MTLDevice>)dev.devicePV;

        NSError *error = nil;
        MTLCompileOptions *opts = [[[MTLCompileOptions alloc] init] autorelease];
        opts.fastMathEnabled = NO; /* the core is verified against the DCTL; keep the floats honest */
        id<MTLLibrary> lib = [[device newLibraryWithSource:[NSString stringWithUTF8String:kDrtAeMsl]
                                                   options:opts error:&error] autorelease];
        if (!lib) {
            dlog("Metal compile failed: %s", error ? [[error localizedDescription] UTF8String] : "?");
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }
        id<MTLFunction> fn = [[lib newFunctionWithName:@DRT_KERNEL_NAME] autorelease];
        if (!fn) { dlog("kernel %s not found", DRT_KERNEL_NAME); return PF_Err_INTERNAL_STRUCT_DAMAGED; }
        id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:fn error:&error];
        if (!pso) {
            dlog("pipeline failed: %s", error ? [[error localizedDescription] UTF8String] : "?");
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }
        dlog("Metal pipeline ready on %s", [[device name] UTF8String]);

        PF_Handle h = handles->host_new_handle(sizeof(MetalGPUData));
        reinterpret_cast<MetalGPUData *>(*h)->pipeline = pso;
        extra->output->gpu_data = h;
        out_data->out_flags2 = PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
    }
    return PF_Err_NONE;
}

PF_Err GPUDeviceSetdown(PF_InData *in_data, PF_OutData *out_data, PF_GPUDeviceSetdownExtra *extra)
{
    if (extra->input->what_gpu == PF_GPU_Framework_METAL && extra->input->gpu_data) {
        PF_Handle h = (PF_Handle)extra->input->gpu_data;
        [reinterpret_cast<MetalGPUData *>(*h)->pipeline release];
        AEFX_SuiteScoper<PF_HandleSuite1> handles(in_data, kPFHandleSuite, kPFHandleSuiteVersion1, out_data);
        handles->host_dispose_handle(h);
    }
    return PF_Err_NONE;
}

PF_Err RenderGPU(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                 PF_EffectWorld *in, PF_EffectWorld *out, PF_SmartRenderExtra *extra, const DrtRender *p)
{
    PF_Err err = PF_Err_NONE;
    if (fmt != PF_PixelFormat_GPU_BGRA128 || extra->input->what_gpu != PF_GPU_Framework_METAL)
        return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    /* No pipeline (device setup failed or was skipped): refuse rather than dereference. */
    if (!extra->input->gpu_data || !*(PF_Handle)extra->input->gpu_data) return PF_Err_UNRECOGNIZED_PARAM_TYPE;

    @autoreleasepool {
        AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);
        PF_GPUDeviceInfo dev;
        AEFX_CLR_STRUCT(dev);
        ERR(gpu->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &dev));
        void *srcMem = nullptr, *dstMem = nullptr;
        ERR(gpu->GetGPUWorldData(in_data->effect_ref, in, &srcMem));
        ERR(gpu->GetGPUWorldData(in_data->effect_ref, out, &dstMem));
        if (err) return err;

        MetalGPUData *md = reinterpret_cast<MetalGPUData *>(*(PF_Handle)extra->input->gpu_data);
        id<MTLDevice> device = (id<MTLDevice>)dev.devicePV;
        id<MTLCommandQueue> queue = (id<MTLCommandQueue>)dev.command_queuePV;

        DrtAeHeader h;
        h.srcPitch = in->rowbytes / 16;
        h.dstPitch = out->rowbytes / 16;
        h.width = in->width;
        h.height = in->height;
        id<MTLBuffer> pbuf = [[device newBufferWithBytes:&p->d length:sizeof(drt::DrtParams) options:MTLResourceStorageModeShared] autorelease];
        id<MTLBuffer> hbuf = [[device newBufferWithBytes:&h length:sizeof h options:MTLResourceStorageModeShared] autorelease];
        id<MTLBuffer> gbuf = [[device newBufferWithBytes:&p->g length:sizeof(drt::DrtGradeParams) options:MTLResourceStorageModeShared] autorelease];

        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
        [enc setComputePipelineState:md->pipeline];
        [enc setBuffer:(id<MTLBuffer>)srcMem offset:0 atIndex:0];
        [enc setBuffer:(id<MTLBuffer>)dstMem offset:0 atIndex:1];
        [enc setBuffer:pbuf offset:0 atIndex:2];
        [enc setBuffer:hbuf offset:0 atIndex:3];
        [enc setBuffer:gbuf offset:0 atIndex:4];
        const NSUInteger tw = [md->pipeline threadExecutionWidth];
        [enc dispatchThreadgroups:MTLSizeMake((in->width + tw - 1) / tw, (in->height + 15) / 16, 1)
            threadsPerThreadgroup:MTLSizeMake(tw, 16, 1)];
        [enc endEncoding];
        [cb commit];
        if ([cb error]) err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return err;
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
        if (isGPU) ERR(RenderGPU(in_data, out_data, fmt, in, out, extra, p));
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
        case PF_Cmd_EVENT:              err = drtwheels::handleEvent(in_data, out_data, params, static_cast<PF_EventExtra *>(extra)); break;
#endif
        case PF_Cmd_GPU_DEVICE_SETUP:   err = GPUDeviceSetup(in_data, out_data, static_cast<PF_GPUDeviceSetupExtra *>(extra)); break;
        case PF_Cmd_GPU_DEVICE_SETDOWN: err = GPUDeviceSetdown(in_data, out_data, static_cast<PF_GPUDeviceSetdownExtra *>(extra)); break;
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
