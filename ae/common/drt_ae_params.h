/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — the parameter tables of both effects.
 *
 * One row per AE parameter, in Effect Controls order. Each row that carries a
 * value is bound to a DrtParams field by pointer-to-member, so the same table
 * builds the UI (ParamsSetup), reads the UI into a DrtParams (PreRender) and
 * writes a DrtParams back into the UI (preset popups, UserChangedParam). Nothing
 * else knows the parameter list.
 *
 * Ranges are the StickShift DEFINE_UI_PARAMS ranges of the upstream DCTL.
 * Defaults are NOT here: they come from drt_stickshift_defaults() (the Standard
 * look), so a preset table edit changes the effect's defaults with no second copy.
 *
 * Parameter ids. AE stores each parameter stream in a project under the id the
 * effect gave it (PF_ParamDef.uu.id) and, when the effect's version has changed,
 * matches streams to parameters by that id, so rows can be added or moved without
 * breaking saved projects as long as every id stays with its row. Row::id of 0
 * means "auto": the auto rows are numbered 1, 2, 3 ... in table order, counting
 * auto rows only. Rules:
 *   - never remove or reorder an auto row (hide it with PF_PUI_INVISIBLE instead); a row
 *     with an explicit id may be removed (its id is then retired);
 *   - every new row takes an explicit id (DRT_*X macros), unique within its table,
 *     never reused, from 1000 up, and may sit anywhere in the table;
 *   - bump DRT_MINOR_VERSION (and the PiPLs' AE_Effect_Version) with every change.
 * params[] indices are positional (row + 1) and unaffected by ids.
 *
 * Layout, mirroring the Nuke node:
 *   Output: Input Gamut, Input Transfer, Display Encoding, HDR sliders, Look,
 *           Tonescale, Creative White [+ limit], then the StickShift groups
 *           (Tonescale, Purity, Brilliance, Hue Shift, Display) collapsed.
 *   Input:  Input Gamut, Input Transfer, Working Gamut, then the same look rows
 *           and StickShift groups minus Display. Those rows exist so an
 *           "OpenDRT inverse: <encoding>" transfer can undo the Output's look; the
 *           transfer entry itself names the encoding, so Input has no Display
 *           Encoding row and no Display group. For every other transfer the look
 *           rows are ignored and the effect greys them out.
 */
#pragma once

#include "opendrt.h"

namespace drtae {

enum Kind {
    kFloat,          /* float slider bound to a float field */
    kFloatHidden,    /* a retired float row: keeps its id and stream, PF_PUI_INVISIBLE */
    kPopupHidden,    /* a moved popup's old slot: keeps its id, PF_PUI_INVISIBLE, never read or written */
    kCheck,          /* checkbox bound to an int field (0/1) */
    kPopup,          /* popup bound to an int field; field = choice - 1, or map[choice - 1] when map is set */
    kTopic,          /* group start (collapsed) */
    kEndTopic,
    kPresetLook,     /* supervised popups, not bound: on change they write other rows */
    kPresetTonescale,
    kPresetDisplay,
    kPresetRender,   /* Output: writes the Render block's rows */
    kTopicOpen,      /* group start that opens expanded */
    kWheels,         /* Grade: a custom-UI row of colour wheels, three to a line; choices = first wheel (ae/common/drt_ae_wheels.h) */
    kColorPick,      /* Grade: a colour row whose eyedropper drives the HSL secondary; choices = 0 set, 1 add, 2 remove */
};

struct Row {
    Kind        kind;
    const char *name;        /* <= 31 chars: AE truncates longer names */
    float drt::DrtParams::*f;
    int   drt::DrtParams::*i;
    float drt::DrtGradeParams::*gf;   /* Grade effect: rows bound to the grade block instead */
    int   drt::DrtGradeParams::*gi;
    float       lo, hi;      /* slider range (valid range too) */
    short       prec;        /* decimals shown */
    const char *popup;       /* '|' separated, popups only; nullptr = built at runtime from a name table */
    short       choices;     /* for runtime-built popups: how many of the table's names to offer */
    const int  *map;         /* popup choice -> field value, or nullptr for identity */
    int         id;          /* AE parameter id; 0 = auto (see the header comment) */
};

#define DRT_F(field, label, lo, hi, prec) { kFloat, label, &drt::DrtParams::field, nullptr, nullptr, nullptr, lo, hi, prec, nullptr, 0, nullptr, 0 }
#define DRT_FX(field, label, lo, hi, prec, id) { kFloat, label, &drt::DrtParams::field, nullptr, nullptr, nullptr, lo, hi, prec, nullptr, 0, nullptr, id }
#define DRT_C(field, label)               { kCheck, label, nullptr, &drt::DrtParams::field, nullptr, nullptr, 0, 1, 0, nullptr, 0, nullptr, 0 }
#define DRT_P(field, label, str, n)       { kPopup, label, nullptr, &drt::DrtParams::field, nullptr, nullptr, 0, 0, 0, str, n, nullptr, 0 }
#define DRT_PX(field, label, str, n, id)  { kPopup, label, nullptr, &drt::DrtParams::field, nullptr, nullptr, 0, 0, 0, str, n, nullptr, id }
#define DRT_PH(field, label, str, n)      { kPopupHidden, label, nullptr, &drt::DrtParams::field, nullptr, nullptr, 0, 0, 0, str, n, nullptr, 0 }
#define DRT_PM(field, label, str, n, map) { kPopup, label, nullptr, &drt::DrtParams::field, nullptr, nullptr, 0, 0, 0, str, n, map, 0 }
#define DRT_T(label)                      { kTopic, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, 0 }
#define DRT_E()                           { kEndTopic, "", nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, 0 }
#define DRT_PRESET(kind, label)           { kind, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, 0 }
#define DRT_PRESETX(kind, label, id)      { kind, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, id }
/* explicit-id variants for rows added after a table shipped */
#define DRT_TX(label, id)                 { kTopic, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, id }
#define DRT_TXO(label, id)                { kTopicOpen, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, id }
#define DRT_EX(id)                        { kEndTopic, "", nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, 0, nullptr, id }
#define DRT_WHEELS(label, firstZone, id)  { kWheels, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, firstZone, nullptr, id }
#define DRT_PICK(label, mode, id)         { kColorPick, label, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, nullptr, mode, nullptr, id }
/* grade-block bindings */
#define DRT_GF(field, label, lo, hi, prec) { kFloat, label, nullptr, nullptr, &drt::DrtGradeParams::field, nullptr, lo, hi, prec, nullptr, 0, nullptr, 0 }
#define DRT_GC(field, label)               { kCheck, label, nullptr, nullptr, nullptr, &drt::DrtGradeParams::field, 0, 1, 0, nullptr, 0, nullptr, 0 }
#define DRT_GFX(field, label, lo, hi, prec, id) { kFloat, label, nullptr, nullptr, &drt::DrtGradeParams::field, nullptr, lo, hi, prec, nullptr, 0, nullptr, id }
#define DRT_GH(field, label, lo, hi, prec)  { kFloatHidden, label, nullptr, nullptr, &drt::DrtGradeParams::field, nullptr, lo, hi, prec, nullptr, 0, nullptr, 0 }

/* Popup strings. Built once from the core's name tables so the lists can never
   disagree with the kernel's indices. */
const char *inGamutPopup();
const char *inOetfPopup(int count);    /* first `count` transfer names */
const char *workingGamutPopup();
extern const int kWorkingGamutMap[];   /* curated subset of DRT_IN_* for the Working Gamut popup */
extern const int kWorkingGamutCount;
const char *lookPopup();               /* "Custom|Standard|..." */
const char *tonescalePopup();          /* "Use Look|Low Contrast|..." */
const char *displayPopup();            /* "Custom|Rec.1886 ...|..." */

static const char *const kSurroundPopup     = "Dark|Dim|Bright";
static const char *const kCwpPopup          = "D93|D75|D65|D60|D55|D50";
static const char *const kRangePopup        = "Full|Limited (video)";
static const char *const kViewPopup         = "OpenDRT|Un-tone-mapped";
static const char *const kDisplayGamutPopup = "Rec.709|P3-D65|Rec.2020 (P3 Limited)|P3-D60|P3-DCI|XYZ|Working Gamut|ACES 2065-1|ACEScg|Rec.2020";
static const char *const kModePopup         = "View|Render";
static const char *const kEotfPopup         = "Linear|2.2 Power (sRGB Display)|2.4 Power (Rec.1886)|2.6 Power (DCI)|ST 2084 PQ|HLG";

/* The look: HDR sliders, preset popups, creative white, then the StickShift
   groups that a look preset writes. Shared by both effects. */
#define DRT_LOOK_ROWS \
    DRT_F(tn_Lp,  "Peak Luminance (nits)",   100.0f, 10000.0f, 0), \
    DRT_F(tn_gb,  "HDR Grey Boost",           0.0f,    1.0f, 3), \
    DRT_F(pt_hdr, "HDR Purity",               0.0f,    1.0f, 2), \
    DRT_F(tn_Lg,  "Grey Luminance (nits)",    3.0f,   25.0f, 1), \
    DRT_PRESET(kPresetLook,      "Look"), \
    DRT_PRESET(kPresetTonescale, "Tonescale"), \
    DRT_P(cwp,    "Creative White", kCwpPopup, 6), \
    DRT_F(cwp_lm, "Creative White Limit",     0.0f,    1.0f, 2), \
    \
    DRT_T("Tonescale"), \
    DRT_F(tn_con,     "Contrast",               1.0f,  2.0f, 2), \
    DRT_F(tn_sh,      "Shoulder Clip",          0.0f,  1.0f, 2), \
    DRT_F(tn_toe,     "Toe",                    0.0f,  0.1f, 3), \
    DRT_F(tn_off,     "Offset",                 0.0f,  0.02f, 4), \
    DRT_C(tn_hcon_enable, "Enable Contrast High"), \
    DRT_F(tn_hcon,    "Contrast High",         -1.0f,  1.0f, 2), \
    DRT_F(tn_hcon_pv, "Contrast High Pivot",    0.0f,  4.0f, 2), \
    DRT_F(tn_hcon_st, "Contrast High Strength", 0.0f,  4.0f, 2), \
    DRT_C(tn_lcon_enable, "Enable Contrast Low"), \
    DRT_F(tn_lcon,    "Contrast Low",           0.0f,  3.0f, 2), \
    DRT_F(tn_lcon_w,  "Contrast Low Width",     0.0f,  2.0f, 2), \
    DRT_PH(tn_su,     "Surround", kSurroundPopup, 3), /* moved up next to Display Encoding (2026-10-03); slot kept for its id */ \
    DRT_E(), \
    \
    DRT_T("Purity"), \
    DRT_F(rs_sa,    "Render Space Strength",  0.0f, 0.6f, 3), \
    DRT_F(rs_rw,    "Render Space Weight R",  0.0f, 0.8f, 3), \
    DRT_F(rs_bw,    "Render Space Weight B",  0.0f, 0.8f, 3), \
    DRT_C(pt_enable, "Enable Purity Compress High"), \
    DRT_F(pt_lml,   "Purity Limit Low",       0.0f, 1.0f, 2), \
    DRT_F(pt_lml_r, "Purity Limit Low R",     0.0f, 1.0f, 2), \
    DRT_F(pt_lml_g, "Purity Limit Low G",     0.0f, 1.0f, 2), \
    DRT_F(pt_lml_b, "Purity Limit Low B",     0.0f, 1.0f, 2), \
    DRT_F(pt_lmh,   "Purity Limit High",      0.0f, 1.0f, 2), \
    DRT_F(pt_lmh_r, "Purity Limit High R",    0.0f, 1.0f, 2), \
    DRT_F(pt_lmh_b, "Purity Limit High B",    0.0f, 1.0f, 2), \
    DRT_C(ptl_enable, "Enable Purity Softclip"), \
    DRT_F(ptl_c,    "Purity Softclip C",      0.0f, 0.25f, 4), \
    DRT_F(ptl_m,    "Purity Softclip M",      0.0f, 0.25f, 4), \
    DRT_F(ptl_y,    "Purity Softclip Y",      0.0f, 0.25f, 4), \
    DRT_C(ptm_enable, "Enable Mid Purity"), \
    DRT_F(ptm_low,      "Mid Purity Low",          0.0f, 2.0f, 2), \
    DRT_F(ptm_low_rng,  "Mid Purity Low Range",    0.0f, 1.0f, 2), \
    DRT_F(ptm_low_st,   "Mid Purity Low Strength", 0.1f, 1.0f, 2), \
    DRT_F(ptm_high,     "Mid Purity High",        -0.9f, 0.0f, 2), \
    DRT_F(ptm_high_rng, "Mid Purity High Range",   0.0f, 1.0f, 2), \
    DRT_F(ptm_high_st,  "Mid Purity High Strength",0.1f, 1.0f, 2), \
    DRT_E(), \
    \
    DRT_T("Brilliance"), \
    DRT_C(brl_enable, "Enable Brilliance"), \
    DRT_F(brl,     "Brilliance",         -6.0f, 2.0f, 2), \
    DRT_F(brl_r,   "Brilliance R",       -6.0f, 2.0f, 2), \
    DRT_F(brl_g,   "Brilliance G",       -6.0f, 2.0f, 2), \
    DRT_F(brl_b,   "Brilliance B",       -6.0f, 2.0f, 2), \
    DRT_F(brl_rng, "Brilliance Range",    0.0f, 1.0f, 2), \
    DRT_F(brl_st,  "Brilliance Strength", 0.0f, 1.0f, 2), \
    DRT_C(brlp_enable, "Enable Post Brilliance"), \
    DRT_F(brlp,    "Brilliance Post",    -1.0f, 0.0f, 2), \
    DRT_F(brlp_r,  "Post Brilliance R",  -3.0f, 0.0f, 2), \
    DRT_F(brlp_g,  "Post Brilliance G",  -3.0f, 0.0f, 2), \
    DRT_F(brlp_b,  "Post Brilliance B",  -3.0f, 0.0f, 2), \
    DRT_E(), \
    \
    DRT_T("Hue Shift"), \
    DRT_C(hc_enable, "Enable Hue Contrast"), \
    DRT_F(hc_r,     "Hue Contrast R",        0.0f, 2.0f, 2), \
    DRT_F(hc_r_rng, "Hue Contrast R Range",  0.0f, 1.0f, 2), \
    DRT_C(hs_rgb_enable, "Enable Hueshift RGB"), \
    DRT_F(hs_r,     "Hueshift R",            0.0f, 1.0f, 2), \
    DRT_F(hs_r_rng, "Hueshift R Range",      0.0f, 2.0f, 2), \
    DRT_F(hs_g,     "Hueshift G",            0.0f, 1.0f, 2), \
    DRT_F(hs_g_rng, "Hueshift G Range",      0.0f, 2.0f, 2), \
    DRT_F(hs_b,     "Hueshift B",            0.0f, 1.0f, 2), \
    DRT_F(hs_b_rng, "Hueshift B Range",      0.0f, 4.0f, 2), \
    DRT_C(hs_cmy_enable, "Enable Hueshift CMY"), \
    DRT_F(hs_c,     "Hueshift C",            0.0f, 1.0f, 2), \
    DRT_F(hs_c_rng, "Hueshift C Range",      0.0f, 1.0f, 2), \
    DRT_F(hs_m,     "Hueshift M",            0.0f, 1.0f, 2), \
    DRT_F(hs_m_rng, "Hueshift M Range",      0.0f, 1.0f, 2), \
    DRT_F(hs_y,     "Hueshift Y",            0.0f, 1.0f, 2), \
    DRT_F(hs_y_rng, "Hueshift Y Range",      0.0f, 1.0f, 2), \
    DRT_E()

/* ---------------------------------------------------------------- Output */
static const Row kOutputRows[] = {
    DRT_P(in_gamut,  "Input Gamut",    nullptr, DRT_IN_GAMUT_COUNT),
    DRT_P(in_oetf,   "Input Transfer", nullptr, DRT_OETF_INVERSE_FIRST),   /* no inverse entries on the render side */
    DRT_PX(out_mode, "Mode",           kModePopup, 2, 1002),   /* View: the rows below; Render: the Render block (look rows shared) */
    DRT_PX(out_view, "Rendering",      kViewPopup, 2, 1000),   /* as an OCIO display's views: the rendering, or none */
    DRT_PRESET(kPresetDisplay,   "Display Encoding"),
    DRT_PX(tn_su,    "Surround (viewing room)", kSurroundPopup, 3, 1001),   /* the preset writes the display's standard room; set yours once */
    /* the Render block: the delivery's encoding, swapped in for the rows above in Render mode
       (drt_output_mode); all explicit ids so no auto row moves */
    DRT_TX("Render", 1003),
    DRT_PRESETX(kPresetRender, "Render Encoding", 1004),
    DRT_PX(rnd_gamut, "Render Gamut",     kDisplayGamutPopup, 10, 1005),
    DRT_PX(rnd_eotf,  "Render EOTF",      kEotfPopup,         6, 1006),
    DRT_FX(rnd_Lp,    "Render Peak (nits)", 100.0f, 10000.0f, 0, 1007),
    DRT_PX(rnd_su,    "Render Surround",  kSurroundPopup,     3, 1008),
    DRT_PX(rnd_view,  "Render Rendering", kViewPopup,         2, 1009),
    DRT_EX(1010),
    DRT_LOOK_ROWS,

    DRT_T("Display"),
    DRT_P(display_gamut, "Display Gamut", kDisplayGamutPopup, 10),
    DRT_P(eotf,          "Display EOTF",  kEotfPopup, 6),
    DRT_C(clamp_out,     "Clamp"),
    DRT_E(),
};
static const int kOutputRowCount = int(sizeof(kOutputRows) / sizeof(kOutputRows[0]));

/* ----------------------------------------------------------------- Input */
static const Row kInputRows[] = {
    DRT_P(in_gamut,       "Input Gamut",    nullptr, DRT_IN_GAMUT_COUNT),
    DRT_P(in_oetf,        "Input Transfer", nullptr, DRT_OETF_COUNT),
    DRT_PM(working_gamut, "Working Gamut",  nullptr, 0, kWorkingGamutMap),
    DRT_PX(in_range, "Input Range", kRangePopup, 2, 1001),   /* hosts are not reliable about expanding video range */
    /* everything below applies only to the "OpenDRT inverse: ..." transfers */
    DRT_FX(inv_cap, "Highlight Cap (of peak)", 0.5f, 1.0f, 2, 1000),
    DRT_PX(tn_su,   "Surround (viewing room)", kSurroundPopup, 3, 1002),   /* how the master was viewed; the inverse entry writes it */
    DRT_LOOK_ROWS,
};
static const int kInputRowCount = int(sizeof(kInputRows) / sizeof(kInputRows[0]));
static const int kInputLookRowsFrom = 4;   /* first row that is inverse-only */

/* ----------------------------------------------------------------- Grade */
#define DRT_ZONE_ROWS(z, name) \
    DRT_T(name), \
    DRT_GF(z##_exposure,   name " Exposure",     -4.0f, 4.0f, 2), \
    DRT_GF(z##_saturation, name " Saturation",    0.0f, 2.0f, 2), \
    DRT_GF(z##_hue,        name " Tint Hue",      0.0f, 360.0f, 0), \
    DRT_GF(z##_tint,       name " Tint Amount",   0.0f, 1.0f, 2), \
    DRT_E()

/* The DRT reference the zones are measured through: Display Encoding, HDR
   sliders, Look, Tonescale and the Tonescale group only. Purity, brilliance and
   hue shift do not move the tonescale norm, so they are not needed here. */
#define DRT_REFERENCE_ROWS \
    DRT_T("DRT Reference"), \
    DRT_PRESET(kPresetDisplay,   "Display Encoding"), \
    DRT_F(tn_Lp,  "Peak Luminance (nits)",   100.0f, 10000.0f, 0), \
    DRT_F(tn_gb,  "HDR Grey Boost",           0.0f,    1.0f, 3), \
    DRT_F(tn_Lg,  "Grey Luminance (nits)",    3.0f,   25.0f, 1), \
    DRT_PRESET(kPresetLook,      "Look"), \
    DRT_PRESET(kPresetTonescale, "Tonescale"), \
    DRT_F(tn_con,     "Contrast (DRT)",         1.0f,  2.0f, 2), \
    DRT_F(tn_sh,      "Shoulder Clip",          0.0f,  1.0f, 2), \
    DRT_F(tn_toe,     "Toe",                    0.0f,  0.1f, 3), \
    DRT_F(tn_off,     "Offset",                 0.0f,  0.02f, 4), \
    DRT_C(tn_hcon_enable, "Enable Contrast High"), \
    DRT_F(tn_hcon,    "Contrast High",         -1.0f,  1.0f, 2), \
    DRT_F(tn_hcon_pv, "Contrast High Pivot",    0.0f,  4.0f, 2), \
    DRT_F(tn_hcon_st, "Contrast High Strength", 0.0f,  4.0f, 2), \
    DRT_C(tn_lcon_enable, "Enable Contrast Low"), \
    DRT_F(tn_lcon,    "Contrast Low",           0.0f,  3.0f, 2), \
    DRT_F(tn_lcon_w,  "Contrast Low Width",     0.0f,  2.0f, 2), \
    DRT_P(tn_su,      "Surround", kSurroundPopup, 3), \
    DRT_F(rs_sa,    "Render Space Strength",  0.0f, 0.6f, 3), \
    DRT_F(rs_rw,    "Render Space Weight R",  0.0f, 0.8f, 3), \
    DRT_F(rs_bw,    "Render Space Weight B",  0.0f, 0.8f, 3), \
    DRT_E()

static const Row kGradeRows[] = {
    /* the wheels first, no group: pure UI over the zone sliders below (hue = angle, tint = radius,
       exposure = bar). Added after 0.1 shipped, hence the explicit id. One control for all six zones
       (Black, Dark, Shadow over Light, Highlight, Specular), so they share one twirl; until 0.8 this
       was "Dark Correction" and a second row, "Light Correction", id 1002: retired, do not reuse. */
    DRT_WHEELS("Zone Correction", 0, 1001),

    DRT_PM(working_gamut, "Working Gamut",  nullptr, 0, kWorkingGamutMap),
    DRT_GF(exposure,    "Exposure (stops)",   -6.0f,  6.0f, 2),
    DRT_GF(contrast,    "Contrast",            0.25f, 3.0f, 2),
    DRT_GH(pivot_nits,  "Contrast Pivot (nits)", 0.5f, 200.0f, 1),   /* retired 2026-10-03, hidden, id kept */
    DRT_GFX(pivot_stops, "Contrast Pivot (stops)", -6.0f, 6.0f, 2, 1009),
    DRT_GF(saturation,  "Saturation",          0.0f,  2.0f, 2),
    DRT_GF(temperature, "Temperature",        -1.0f,  1.0f, 2),
    DRT_GF(tint,        "Tint",               -1.0f,  1.0f, 2),

    DRT_ZONE_ROWS(z0, "Black"),
    DRT_ZONE_ROWS(z1, "Dark"),
    DRT_ZONE_ROWS(z2, "Shadow"),
    DRT_ZONE_ROWS(z3, "Light"),
    DRT_ZONE_ROWS(z4, "Highlight"),
    DRT_ZONE_ROWS(z5, "Specular"),

    DRT_T("Zone Ranges"),
    DRT_GF(zb0, "Black / Dark (stops)",         -8.0f,  0.0f, 2),
    DRT_GF(zb1, "Dark / Shadow (stops)",        -6.0f,  0.0f, 2),
    DRT_GF(zb2, "Shadow / Light (stops)",       -4.0f,  2.0f, 2),
    DRT_GF(zb3, "Light / Highlight (stops)",    -2.0f,  4.0f, 2),
    DRT_GF(zb4, "Highlight / Specular (stops)",  0.0f,  8.0f, 2),
    DRT_GF(zfalloff, "Falloff (stops)",          0.25f, 6.0f, 2),
    DRT_E(),

    DRT_T("HSL Secondary"),
    DRT_GC(sec_enable,     "Enable Secondary"),
    /* eyedroppers, Lumetri-style: a pick goes through the inverse DRT back to the scene and sets / widens /
       narrows the three windows below (drt_ae_effect.mm, pickColour) */
    DRT_PICK("Set Colour",        0, 1006),
    DRT_PICK("Add to Range",      1, 1007),
    DRT_PICK("Remove from Range", 2, 1008),
    DRT_GF(sec_hue,        "Hue Centre",        0.0f, 360.0f, 0),
    DRT_GF(sec_hue_width,  "Hue Width",         0.0f, 360.0f, 0),
    DRT_GF(sec_sat_min,    "Saturation Min",    0.0f,   2.0f, 2),
    DRT_GF(sec_sat_max,    "Saturation Max",    0.0f,   2.0f, 2),
    DRT_GH(sec_lum_min,    "Luminance Min (nits)", 0.0f, 10000.0f, 1),   /* retired 2026-10-03, hidden, ids kept */
    DRT_GH(sec_lum_max,    "Luminance Max (nits)", 0.0f, 10000.0f, 1),
    DRT_GFX(sec_lev_min,   "Level Min (stops)",   -10.0f, 10.0f, 2, 1010),
    DRT_GFX(sec_lev_max,   "Level Max (stops)",   -10.0f, 10.0f, 2, 1011),
    DRT_GF(sec_softness,   "Softness",          0.0f,   1.0f, 2),
    DRT_GC(sec_show_mask,  "Show Mask"),
    DRT_WHEELS("Secondary Correction", 6, 1005),   /* one wheel: Secondary Exposure / Tint Hue / Tint Amount */
    DRT_GF(sec_exposure,   "Secondary Exposure",   -4.0f, 4.0f, 2),
    DRT_GF(sec_saturation, "Secondary Saturation",  0.0f, 2.0f, 2),
    DRT_GF(sec_hue_shift,  "Secondary Tint Hue",    0.0f, 360.0f, 0),
    DRT_GF(sec_tint,       "Secondary Tint Amount", 0.0f, 1.0f, 2),
    DRT_E(),

    DRT_REFERENCE_ROWS,
};
static const int kGradeRowCount = int(sizeof(kGradeRows) / sizeof(kGradeRows[0]));

/* ------------------------------------------------------------- macOS Fix */
/* No settings. One invisible placeholder, because a row table cannot be empty;
   explicit id so a real row can follow it one day. */
static const Row kMacFixRows[] = {
    { kPopupHidden, "unused", nullptr, &drt::DrtParams::reserved_i3, nullptr, nullptr, 0, 0, 0, "-", 1, nullptr, 1000 },
};
static const int kMacFixRowCount = int(sizeof(kMacFixRows) / sizeof(kMacFixRows[0]));

} // namespace drtae
