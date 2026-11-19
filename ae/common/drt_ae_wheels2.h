/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow.
 */
/* minColor Grade 2 — its colour wheels, the Effect Controls custom UI.
 *
 * Pure UI, like the first Grade's wheels (drt_ae_wheels.h, whose painter and host helpers
 * this reuses): a wheel row carries no data. It draws and drags sliders that exist in the
 * parameter table, writing them with PF_ChangeFlag_CHANGED_VALUE, so keyframes, expressions,
 * undo and presets all work on the numbers.
 *
 * Three rows (a kWheels row; Row::choices = which), four wheels to a line:
 *   0  Primaries   Lift, Gamma, Gain, Offset
 *   1  HDR         Dark, Shadow, Light, Global
 *   2  More Zones  Black, Highlight, Specular
 *   3  Secondary   the HSL secondary's correction: one wheel, or Shadows, Midtones, Highlights
 *                  (the Correction Wheels popup); each is an HDR-style wheel without a range
 *
 * The handling is the reference grading application's, measured there (2026-11-19), not Lumetri's:
 *   - Every control is a RELATIVE drag: a click never jumps a value, a drag moves it from
 *     where it is. Double-click resets the one control under the mouse. Shift drags a
 *     quarter as fast.
 *   - A Primaries ball is a colour-difference push on the wheel's R, G, B with their luma
 *     held (Rec.709 weights): right raises blue, up raises red, green gives way. The strip
 *     under it is the master wheel: it moves all three together. Gearing per point of mouse
 *     (the reference's): Offset 0.0437 of colour difference and 0.03 of master; Gain, Lift and
 *     Gamma scale those as the reference does.
 *   - An HDR ball is the zone's X / Y: 0.000667 per point (60 points = 0.04), stopping at
 *     0.19 where the reference's fields stop. Under it: Exposure and Saturation bars, the X / Y
 *     readout, and for a zone its Range (stops over grey) and Falloff, both scrubbed sideways.
 *   - The puck travels a tenth of the mouse on a wheel the reference's size, so it stays near the
 *     centre for any sane grade; it is an indicator, not a handle you must hit.
 *
 * The ring is coloured by the push itself (the same colour-difference axes), which lands
 * red near 11 o'clock, magenta 1, blue 3 to 4, cyan 5, green 7 to 8, yellow 9 to 10: the reference's
 * wheel, and a vectorscope's.
 */
#pragma once

#include "drt_ae_wheels.h"   /* Painter, AppSuite, undoGroup, readFloat / writeFloat */

namespace drtwheels2 {

using drtwheels::Painter;
using drtwheels::AppSuite;
using drtwheels::grey;
using drtwheels::readFloat;
using drtwheels::writeFloat;

/* ----------------------------------------------------------------- layout */

const int   kPerRow   = 4;
const int   kCellW    = 105;               /* natural cell: one wheel and what sits under it */
const int   kWidth    = kPerRow * kCellW;  /* ui_width of a wheel row */
const float kMinScale = 0.75f;             /* the cells follow the panel between these, as the first Grade's do */
const float kMaxScale = 1.5f;
const float kRingOut  = 38.0f;             /* natural, scales with the cell */
const float kRingW    = 6.0f;
const float kPuckR    = 4.0f;
const float kLabelH   = 18.0f;             /* text does not scale: the label above the wheel ... */
const float kGap      = 8.0f;              /* ... and the space under it */
const float kLineH    = 15.0f;             /* a line of readout text */
const float kBarRowH  = 23.0f;             /* a value line with its bar under it */
const float kJogH     = 16.0f;             /* the master strip */
const float kBoxH     = 17.0f;             /* Range / Falloff value boxes */
const float kPad      = 8.0f;
const float kKr = 0.2126f, kKg = 0.7152f, kKb = 0.0722f;   /* the luma a Primaries ball holds */
const float kCbToB = 1.8556f, kCrToR = 1.5748f;            /* B - Y = 1.8556 cb, R - Y = 1.5748 cr */

/* gearing, per point of mouse */
const float kPuckPerPt   = 0.00125f;       /* puck travel as a fraction of the ring's radius */
const float kHdrBallPerPt = 0.04f / 60.0f; /* X or Y */
const float kHdrBallMax  = 0.19f;
const float kExpPerPt    = 0.01f;          /* stops */
const float kSatPerPt    = 0.005f;
const float kRangePerPt  = 0.02f;          /* stops */
const float kFalloffPerPt = 0.002f;
const float kExpShown    = 4.0f;           /* the Exposure bar spans +-4 stops, the Saturation bar 0 .. 2 */
const float kSatShown    = 2.0f;

typedef float drt::DrtGrade2Params::*QF;
#define QFIELD(n) &drt::DrtGrade2Params::n

struct PrimWheel { const char *label; QF r, g, b; float neutral, ballPerPt, jogPerPt; };
static const PrimWheel kPrim[4] = {
    { "Lift",   QFIELD(lift_r),   QFIELD(lift_g),   QFIELD(lift_b),    0.0f, 0.0437f * 0.0094f, 0.0006f },
    { "Gamma",  QFIELD(gamma_r),  QFIELD(gamma_g),  QFIELD(gamma_b),   0.0f, 0.0437f * 0.0060f, 0.0003f },
    { "Gain",   QFIELD(gain_r),   QFIELD(gain_g),   QFIELD(gain_b),    1.0f, 0.0437f * 0.0320f, 0.0012f },
    { "Offset", QFIELD(offset_r), QFIELD(offset_g), QFIELD(offset_b), 25.0f, 0.0437f,           0.03f   },
};

struct HdrWheel { const char *label; QF exp, sat, x, y, range, falloff; bool dark; };   /* range == nullptr: the Global wheel */
static const HdrWheel kHdr[10] = {
    { "Dark",      QFIELD(z1_exp), QFIELD(z1_sat), QFIELD(z1_x), QFIELD(z1_y), QFIELD(z1_range), QFIELD(z1_falloff), true },
    { "Shadow",    QFIELD(z2_exp), QFIELD(z2_sat), QFIELD(z2_x), QFIELD(z2_y), QFIELD(z2_range), QFIELD(z2_falloff), true },
    { "Light",     QFIELD(z3_exp), QFIELD(z3_sat), QFIELD(z3_x), QFIELD(z3_y), QFIELD(z3_range), QFIELD(z3_falloff), false },
    { "Global",    QFIELD(g_exp),  QFIELD(g_sat),  QFIELD(g_x),  QFIELD(g_y),  nullptr,          nullptr,            false },
    { "Black",     QFIELD(z0_exp), QFIELD(z0_sat), QFIELD(z0_x), QFIELD(z0_y), QFIELD(z0_range), QFIELD(z0_falloff), true },
    { "Highlight", QFIELD(z4_exp), QFIELD(z4_sat), QFIELD(z4_x), QFIELD(z4_y), QFIELD(z4_range), QFIELD(z4_falloff), false },
    { "Specular",  QFIELD(z5_exp), QFIELD(z5_sat), QFIELD(z5_x), QFIELD(z5_y), QFIELD(z5_range), QFIELD(z5_falloff), false },
    /* the secondary's: with one wheel only the middle one (s1) is shown, labelled Secondary */
    { "Shadows",    QFIELD(s0_exp), QFIELD(s0_sat), QFIELD(s0_x), QFIELD(s0_y), nullptr, nullptr, true },
    { "Midtones",   QFIELD(s1_exp), QFIELD(s1_sat), QFIELD(s1_x), QFIELD(s1_y), nullptr, nullptr, false },
    { "Highlights", QFIELD(s2_exp), QFIELD(s2_sat), QFIELD(s2_x), QFIELD(s2_y), nullptr, nullptr, false },
};
static const QF kSecThree = QFIELD(sec_three);
#undef QFIELD

enum Bank { kBankPrim = 0, kBankHdr = 1, kBankMore = 2, kBankSec = 3 };
inline int wheelsIn(int bank) { return (bank == kBankMore || bank == kBankSec) ? 3 : 4; }
inline const HdrWheel &hdrWheel(int bank, int i) { return kHdr[(bank == kBankSec ? 7 : bank == kBankMore ? 4 : 0) + i]; }

/* Is the secondary on three wheels? (The Correction Wheels popup; choice 2.) */
inline bool secThree(PF_ParamDef *params[])
{
    for (int k = 0; k < drtae::kGrade2RowCount; ++k)
        if (drtae::kGrade2Rows[k].kind == drtae::kPopup && drtae::kGrade2Rows[k].qf == kSecThree) return params[k + 1]->u.pd.value == 2;
    return false;
}
/* A wheel that is not drawn takes no clicks: on one wheel the secondary shows only its middle one. */
inline bool shown(PF_ParamDef *params[], int bank, int i) { return bank != kBankSec || i == 1 || secThree(params); }

/* Parameter index (params[] slot) of the float row bound to a Grade 2 field. */
inline int paramOf(QF m)
{
    if (!m) return -1;
    for (int k = 0; k < drtae::kGrade2RowCount; ++k)
        if (drtae::kGrade2Rows[k].kind == drtae::kFloat && drtae::kGrade2Rows[k].qf == m) return k + 1;
    return -1;
}
inline void rowRange(int idx, float &lo, float &hi) { lo = drtae::kGrade2Rows[idx - 1].lo; hi = drtae::kGrade2Rows[idx - 1].hi; }
inline float clampTo(int idx, float v)
{
    float lo, hi;
    rowRange(idx, lo, hi);
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float widthScale(float rowW)
{
    const float s = rowW / float(kWidth);
    return s < kMinScale ? kMinScale : (s > kMaxScale ? kMaxScale : s);
}
/* what sits under the wheel, top to bottom (heights that do not scale) */
inline float underHeight(int bank)
{
    if (bank == kBankPrim) return kLineH + kJogH + kPad;                      /* R G B readout, master strip */
    if (bank == kBankSec) return 2.0f * kBarRowH + kLineH + kPad;            /* Exposure, Saturation, X / Y: no range to show */
    return 2.0f * kBarRowH + kLineH + kBoxH + 4.0f + kPad;                    /* Exposure, Saturation, X / Y, Range + Falloff */
}
inline int heightFor(float s, int bank) { return int(kLabelH + 2.0f * kRingOut * s + kGap + underHeight(bank) + 0.5f); }

struct Cell {
    float s, left, w;        /* scale, cell left edge and width */
    float cx, cy, ringOut;   /* the wheel */
    float under;             /* y where the rows under the wheel start */
    float x0, x1;            /* the bars' and boxes' left and right */
};

inline Cell cellGeom(const PF_Rect &frame, int i, int bank)
{
    Cell c;
    const float rowW = float(frame.right - frame.left);
    c.s = widthScale(rowW);
    /* until AE has given the row the height this width wants, stay inside the height it has */
    const float fit = (float(frame.bottom - frame.top) - kLabelH - kGap - underHeight(bank)) / (2.0f * kRingOut);
    if (c.s > fit) c.s = fit;
    if (c.s < kMinScale) c.s = kMinScale;
    c.w = float(kCellW) * c.s;
    float off = (rowW - c.w * float(kPerRow)) * 0.5f;   /* a three-wheel row keeps the four-wheel columns */
    if (off < 0.0f) off = 0.0f;
    c.left = float(frame.left) + off + float(i) * c.w;
    c.cx = c.left + c.w * 0.5f;
    c.ringOut = kRingOut * c.s;
    c.cy = float(frame.top) + kLabelH + c.ringOut;
    c.under = c.cy + c.ringOut + kGap;
    c.x0 = c.left + 9.0f;
    c.x1 = c.left + c.w - 9.0f;
    return c;
}
/* the rows under an HDR wheel: the text baseline and the bar's centre line of each */
inline float expTextY(const Cell &c) { return c.under + 10.0f; }
inline float expBarY(const Cell &c)  { return c.under + 17.0f; }
inline float satTextY(const Cell &c) { return c.under + kBarRowH + 10.0f; }
inline float satBarY(const Cell &c)  { return c.under + kBarRowH + 17.0f; }
inline float xyTextY(const Cell &c)  { return c.under + 2.0f * kBarRowH + 11.0f; }
inline float boxTop(const Cell &c)   { return c.under + 2.0f * kBarRowH + kLineH + 3.0f; }
inline float boxMid(const Cell &c)   { return 0.5f * (c.x0 + c.x1) + 6.0f; }   /* the Range box is the wider of the two */
/* under a Primaries wheel */
inline float rgbTextY(const Cell &c) { return c.under + 10.0f; }
inline float jogTop(const Cell &c)   { return c.under + kLineH + 2.0f; }

enum Hit { kNone = 0, kBall = 1, kJog = 2, kExp = 3, kSat = 4, kRange = 5, kFalloff = 6 };

inline Hit hitTest(const PF_Rect &frame, int bank, const PF_Point &m, int &wheelOut, PF_ParamDef *params[])
{
    const float mx = float(m.h), my = float(m.v);
    for (int i = 0; i < wheelsIn(bank); ++i) {
        if (!shown(params, bank, i)) continue;
        const Cell c = cellGeom(frame, i, bank);
        wheelOut = i;
        if (std::hypot(mx - c.cx, my - c.cy) <= c.ringOut + 3.0f) return kBall;
        if (mx < c.x0 - 4.0f || mx > c.x1 + 4.0f) continue;
        if (bank == kBankPrim) {
            if (my >= jogTop(c) - 3.0f && my <= jogTop(c) + kJogH - 2.0f) return kJog;
            continue;
        }
        if (my >= c.under && my < c.under + kBarRowH) return kExp;
        if (my >= c.under + kBarRowH && my < c.under + 2.0f * kBarRowH) return kSat;
        if (hdrWheel(bank, i).range && my >= boxTop(c) - 2.0f && my <= boxTop(c) + kBoxH + 2.0f)
            return mx < boxMid(c) ? kRange : kFalloff;
    }
    wheelOut = -1;
    return kNone;
}

/* ------------------------------------------------- Primaries ball <-> R G B */
/* A wheel's three values as their luma and two colour differences, and back. */
inline void toYCC(float r, float g, float b, float &y, float &cb, float &cr)
{
    y = kKr * r + kKg * g + kKb * b;
    cb = (b - y) / kCbToB;
    cr = (r - y) / kCrToR;
}
inline void fromYCC(float y, float cb, float cr, float &r, float &g, float &b)
{
    r = y + kCrToR * cr;
    b = y + kCbToB * cb;
    g = (y - kKr * r - kKb * b) / kKg;
}

/* ---------------------------------------------------------------- drawing */

/* Ring colour at a screen angle (degrees clockwise from the top): where a push that way goes. */
inline DRAWBOT_ColorRGBA ringColor(float deg)
{
    const float a = deg * (3.14159265f / 180.0f);
    const float cb = std::sin(a), cr = std::cos(a);
    float r, g, b;
    fromYCC(0.0f, cb, cr, r, g, b);
    /* as vivid as the direction allows: its largest channel taken to the rail */
    const float m = std::fmax(std::fabs(r), std::fmax(std::fabs(g), std::fabs(b)));
    const float k = m > 0.0f ? 0.46f / m : 0.0f;
    DRAWBOT_ColorRGBA c;
    c.red = 0.5f + k * r; c.green = 0.5f + k * g; c.blue = 0.5f + k * b; c.alpha = 1.0f;
    if (c.red < 0.0f) c.red = 0.0f;     if (c.red > 1.0f) c.red = 1.0f;
    if (c.green < 0.0f) c.green = 0.0f; if (c.green > 1.0f) c.green = 1.0f;
    if (c.blue < 0.0f) c.blue = 0.0f;   if (c.blue > 1.0f) c.blue = 1.0f;
    return c;
}

/* Where the puck sits for a push of (h, v) control units, h right and v up, in ring radii. */
inline void puckPos(float h, float v, float perPt, float &px, float &py)
{
    px = h / perPt * kPuckPerPt;
    py = -v / perPt * kPuckPerPt;
    const float len = std::sqrt(px * px + py * py);
    if (len > 0.92f) { px *= 0.92f / len; py *= 0.92f / len; }
}

inline void drawWheel(Painter &p, const Cell &c, float px, float py, const DRAWBOT_ColorRGBA &bg)
{
    const int wedges = c.s > 1.2f ? 120 : 72;
    const float step = 360.0f / float(wedges);
    for (int k = 0; k < wedges; ++k) {
        const float h = float(k) * step;
        p.wedge(c.cx, c.cy, c.ringOut, h - 90.0f - 0.4f, step + 0.8f, ringColor(h + 0.5f * step));
    }
    const float in = c.ringOut - kRingW * c.s;
    p.disc(c.cx, c.cy, in, grey(0.11f));
    p.rect(c.cx - in, c.cy - 0.5f, 2.0f * in, 1.0f, grey(0.24f));
    p.rect(c.cx - 0.5f, c.cy - in, 1.0f, 2.0f * in, grey(0.24f));
    (void)bg;
    const DRAWBOT_ColorRGBA edge = grey(0.05f);
    p.disc(c.cx + px * in, c.cy + py * in, kPuckR * (c.s > 1.0f ? std::sqrt(c.s) : c.s), grey(0.90f), &edge, 1.0f);
}

/* A value line with its bar: `t` is the thumb's place along the bar, 0 .. 1; `zero` where the neutral mark goes. */
inline void drawBar(Painter &p, const Cell &c, float textY, float barY, const char *text, float t, float zero,
                    const DRAWBOT_ColorRGBA &textC, float fontSize)
{
    p.text(text, c.cx, textY, textC, fontSize - 1.0f);
    const DRAWBOT_ColorRGBA edge = grey(0.05f);
    p.rect(c.x0, barY - 1.5f, c.x1 - c.x0, 3.0f, grey(0.08f));
    p.rect(c.x0 + zero * (c.x1 - c.x0) - 0.5f, barY - 3.5f, 1.0f, 7.0f, grey(0.36f));
    const float tt = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    p.disc(c.x0 + tt * (c.x1 - c.x0), barY, 4.0f, grey(0.78f), &edge, 1.0f);
}

inline void drawBox(Painter &p, float x0, float x1, float top, const char *text, const DRAWBOT_ColorRGBA &textC, float fontSize)
{
    p.rect(x0, top, x1 - x0, kBoxH, grey(0.30f));
    p.rect(x0 + 1.0f, top + 1.0f, x1 - x0 - 2.0f, kBoxH - 2.0f, grey(0.08f));
    p.text(text, 0.5f * (x0 + x1), top + kBoxH - 4.5f, textC, fontSize - 1.0f);
}

/* Ask AE for the height this width wants (see drtwheels::fitHeight: cosmetic, the first draw corrects it). */
inline void fitHeight(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], int paramIdx, const PF_Rect &frame, int bank)
{
    const int have = int(frame.bottom - frame.top);
    const int want = heightFor(widthScale(float(frame.right - frame.left)), bank);
    if (std::abs(want - have) < 3) return;
    static int lastIdx = -1, lastWant = 0, lastHave = 0, repeats = 0;
    if (paramIdx == lastIdx && want == lastWant && have == lastHave) { if (++repeats > 2) return; }
    else { lastIdx = paramIdx; lastWant = want; lastHave = have; repeats = 0; }
    PF_ParamUtilsSuite3 *pu = nullptr;
    if (AEFX_AcquireSuite(in_data, out_data, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, nullptr, (void **)&pu) != PF_Err_NONE || !pu)
        return;
    PF_ParamDef def = *params[paramIdx];
    def.ui_flags = PF_PUI_CONTROL | PF_PUI_DONT_ERASE_CONTROL;
    def.ui_width = A_short(kWidth);
    def.ui_height = A_short(want);
    pu->PF_UpdateParamUI(in_data->effect_ref, paramIdx, &def);
    AEFX_ReleaseSuite(in_data, out_data, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, nullptr);
}

inline PF_Err draw(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra, int bank)
{
    if (extra->effect_win.area != PF_EA_CONTROL) return PF_Err_NONE;
    const PF_Rect frame = extra->effect_win.current_frame;

    PF_EffectCustomUISuite2 *cui = nullptr;
    if (AEFX_AcquireSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion2, nullptr, (void **)&cui) != PF_Err_NONE || !cui)
        return PF_Err_NONE;
    DRAWBOT_DrawRef dref = nullptr;
    cui->PF_GetDrawingReference(extra->contextH, &dref);
    AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion2, nullptr);
    if (!dref) return PF_Err_NONE;

    DRAWBOT_Suites suites;
    if (AEFX_AcquireDrawbotSuites(in_data, out_data, &suites) != PF_Err_NONE) return PF_Err_NONE;
    Painter p;
    p.s = &suites;
    p.sup = nullptr; p.surf = nullptr;
    suites.drawbot_suiteP->GetSupplier(dref, &p.sup);
    suites.drawbot_suiteP->GetSurface(dref, &p.surf);
    if (p.sup && p.surf) {
        AppSuite app(in_data, out_data);
        const DRAWBOT_ColorRGBA bg = app.background(0.19f);
        const DRAWBOT_ColorRGBA textC = app.color(PF_App_Color_TEXT, 0.80f);
        DRAWBOT_ColorRGBA dim = textC; dim.alpha = 0.6f;
        suites.surface_suiteP->SetAntiAliasPolicy(p.surf, kDRAWBOT_AntiAliasPolicy_High);
        p.rect(float(frame.left), float(frame.top), float(frame.right - frame.left), float(frame.bottom - frame.top + 1), bg);
        float fontSize = 11.0f;
        suites.supplier_suiteP->GetDefaultFontSize(p.sup, &fontSize);

        char line[64];
        const bool three = bank == kBankSec && secThree(params);
        for (int i = 0; i < wheelsIn(bank); ++i) {
            if (!shown(params, bank, i)) continue;
            const Cell c = cellGeom(frame, i, bank);
            float px = 0.0f, py = 0.0f;
            if (bank == kBankPrim) {
                const PrimWheel &w = kPrim[i];
                const float r = readFloat(params, paramOf(w.r)), g = readFloat(params, paramOf(w.g)), b = readFloat(params, paramOf(w.b));
                float y, cb, cr;
                toYCC(r, g, b, y, cb, cr);
                puckPos(cb, cr, w.ballPerPt, px, py);
                p.text(w.label, c.cx, float(frame.top) + 12.0f, textC, fontSize);
                drawWheel(p, c, px, py, bg);
                std::snprintf(line, sizeof line, "%.2f  %.2f  %.2f", r, g, b);
                p.text(line, c.cx, rgbTextY(c), (r != w.neutral || g != w.neutral || b != w.neutral) ? textC : dim, fontSize - 1.0f);
                /* the master strip: a ridged track, with a tick that rides the master away from neutral */
                const float jt = jogTop(c), jh = kJogH - 6.0f;
                p.rect(c.x0, jt, c.x1 - c.x0, jh, grey(0.08f));
                const float shift = std::fabs(y - w.neutral) > 0.25f * w.jogPerPt ? (y - w.neutral) / w.jogPerPt : 0.0f;   /* the mouse travel that master stands for */
                const float pitch = 5.0f;
                float ph = std::fmod(shift, pitch);
                if (ph < 0.0f) ph += pitch;
                for (float x = c.x0 + ph; x < c.x1 - 0.5f; x += pitch) p.rect(x, jt + 2.0f, 1.0f, jh - 4.0f, grey(0.27f));
                /* luma of three equal values is that value only to rounding, so compare with a little room */
                if (std::fabs(y - w.neutral) > 0.25f * w.jogPerPt) p.rect(0.5f * (c.x0 + c.x1) - 0.5f, jt, 1.0f, jh, grey(0.75f));
            } else {
                const HdrWheel &w = hdrWheel(bank, i);
                const float x = readFloat(params, paramOf(w.x)), y = readFloat(params, paramOf(w.y));
                const float e = readFloat(params, paramOf(w.exp)), s = readFloat(params, paramOf(w.sat));
                puckPos(x, y, kHdrBallPerPt, px, py);
                p.text((bank == kBankSec && !three) ? "Secondary" : w.label, c.cx, float(frame.top) + 12.0f, textC, fontSize);
                drawWheel(p, c, px, py, bg);
                std::snprintf(line, sizeof line, "Exp  %+.2f", e);
                drawBar(p, c, expTextY(c), expBarY(c), line, 0.5f + e / (2.0f * kExpShown), 0.5f, e != 0.0f ? textC : dim, fontSize);
                std::snprintf(line, sizeof line, "Sat  %.2f", s);
                drawBar(p, c, satTextY(c), satBarY(c), line, s / kSatShown, 1.0f / kSatShown, s != 1.0f ? textC : dim, fontSize);
                std::snprintf(line, sizeof line, "X %+.3f  Y %+.3f", x, y);
                p.text(line, c.cx, xyTextY(c), (x != 0.0f || y != 0.0f) ? textC : dim, fontSize - 1.0f);
                if (w.range) {
                    /* a dark-side zone reaches down from its range, a light-side one up from it */
                    std::snprintf(line, sizeof line, w.dark ? "to %+.2f" : "from %+.2f", readFloat(params, paramOf(w.range)));
                    drawBox(p, c.x0, boxMid(c) - 2.0f, boxTop(c), line, textC, fontSize);
                    std::snprintf(line, sizeof line, "f %.2f", readFloat(params, paramOf(w.falloff)));
                    drawBox(p, boxMid(c) + 2.0f, c.x1, boxTop(c), line, textC, fontSize);
                }
            }
        }
        suites.surface_suiteP->Flush(p.surf);
    }
    AEFX_ReleaseDrawbotSuites(in_data, out_data);
    extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
    fitHeight(in_data, out_data, params, int(extra->effect_win.index), frame, bank);
    return PF_Err_NONE;
}

/* ----------------------------------------------------------------- events */

/* Move one control by a mouse delta (points; `gain` is 1, or 0.25 with Shift). */
inline void applyDelta(PF_ParamDef *params[], int bank, int i, Hit h, float dx, float dy, float gain)
{
    if (bank == kBankPrim) {
        const PrimWheel &w = kPrim[i];
        const int ir = paramOf(w.r), ig = paramOf(w.g), ib = paramOf(w.b);
        float r = readFloat(params, ir), g = readFloat(params, ig), b = readFloat(params, ib);
        if (h == kBall) {
            float y, cb, cr;
            toYCC(r, g, b, y, cb, cr);
            cb += dx * w.ballPerPt * gain;
            cr += -dy * w.ballPerPt * gain;
            fromYCC(y, cb, cr, r, g, b);
        } else if (h == kJog) {
            const float d = dx * w.jogPerPt * gain;
            r += d; g += d; b += d;
        } else return;
        writeFloat(params, ir, clampTo(ir, r));
        writeFloat(params, ig, clampTo(ig, g));
        writeFloat(params, ib, clampTo(ib, b));
        return;
    }
    const HdrWheel &w = hdrWheel(bank, i);
    if (h == kBall) {
        const int ix = paramOf(w.x), iy = paramOf(w.y);
        float x = readFloat(params, ix) + dx * kHdrBallPerPt * gain;
        float y = readFloat(params, iy) - dy * kHdrBallPerPt * gain;
        x = x < -kHdrBallMax ? -kHdrBallMax : (x > kHdrBallMax ? kHdrBallMax : x);
        y = y < -kHdrBallMax ? -kHdrBallMax : (y > kHdrBallMax ? kHdrBallMax : y);
        writeFloat(params, ix, x);
        writeFloat(params, iy, y);
        return;
    }
    const QF f = h == kExp ? w.exp : h == kSat ? w.sat : h == kRange ? w.range : h == kFalloff ? w.falloff : nullptr;
    const float per = h == kExp ? kExpPerPt : h == kSat ? kSatPerPt : h == kRange ? kRangePerPt : kFalloffPerPt;
    const int idx = paramOf(f);
    if (idx > 0) writeFloat(params, idx, clampTo(idx, readFloat(params, idx) + dx * per * gain));
}

/* Double-click: the one control under the mouse back to rest. A Primaries ball keeps its master
   and a master its ball, as the reference's two resets do. */
inline void resetControl(PF_ParamDef *params[], int bank, int i, Hit h)
{
    const drt::DrtGrade2Params d = drt::drt_grade2_defaults();
    if (bank == kBankPrim) {
        const PrimWheel &w = kPrim[i];
        const int ir = paramOf(w.r), ig = paramOf(w.g), ib = paramOf(w.b);
        float r = readFloat(params, ir), g = readFloat(params, ig), b = readFloat(params, ib), y, cb, cr;
        toYCC(r, g, b, y, cb, cr);
        if (h == kBall) fromYCC(y, 0.0f, 0.0f, r, g, b);
        else if (h == kJog) fromYCC(w.neutral, cb, cr, r, g, b);
        else return;
        writeFloat(params, ir, r); writeFloat(params, ig, g); writeFloat(params, ib, b);
        return;
    }
    const HdrWheel &w = hdrWheel(bank, i);
    if (h == kBall) { writeFloat(params, paramOf(w.x), 0.0f); writeFloat(params, paramOf(w.y), 0.0f); return; }
    const QF f = h == kExp ? w.exp : h == kSat ? w.sat : h == kRange ? w.range : h == kFalloff ? w.falloff : nullptr;
    const int idx = paramOf(f);
    if (idx > 0) writeFloat(params, idx, d.*f);
}

inline PF_Err handleEvent(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra)
{
    if (!extra->contextH || (**extra->contextH).w_type != PF_Window_EFFECT) return PF_Err_NONE;
    const int row = int(extra->effect_win.index) - 1;
    if (row < 0 || row >= drtae::kGrade2RowCount || drtae::kGrade2Rows[row].kind != drtae::kWheels) return PF_Err_NONE;
    const int bank = drtae::kGrade2Rows[row].choices;
    const PF_Rect frame = extra->effect_win.current_frame;

    switch (extra->e_type) {
    case PF_Event_DRAW:
        return draw(in_data, out_data, params, extra, bank);

    case PF_Event_ADJUST_CURSOR: {
        int i;
        const Hit h = hitTest(frame, bank, extra->u.adjust_cursor.screen_point, i, params);
        if (h == kBall) extra->u.adjust_cursor.set_cursor = PF_Cursor_DRAG_DOT;
        else if (h != kNone) extra->u.adjust_cursor.set_cursor = PF_Cursor_FINGER_POINTER_SCRUB;
        if (h != kNone) extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }

    case PF_Event_DO_CLICK: {
        if (extra->effect_win.area != PF_EA_CONTROL) return PF_Err_NONE;
        PF_DoClickEventInfo &ck = extra->u.do_click;
        int i;
        const Hit h = hitTest(frame, bank, ck.screen_point, i, params);
        if (h == kNone) return PF_Err_NONE;
        if (ck.num_clicks >= 2) {
            drtwheels::undoGroup(in_data, true);
            resetControl(params, bank, i, h);
            drtwheels::undoGroup(in_data, false);
            ck.send_drag = FALSE;
        } else {
            /* nothing moves on the click itself: the drag carries the value from where it is */
            drtwheels::undoGroup(in_data, true);
            ck.send_drag = TRUE;
            ck.continue_refcon[0] = A_intptr_t((i << 4) | int(h));
            ck.continue_refcon[1] = A_intptr_t(ck.screen_point.h);
            ck.continue_refcon[2] = A_intptr_t(ck.screen_point.v);
            ck.continue_refcon[3] = 0;
        }
        AppSuite(in_data, out_data).invalidate(extra->contextH, frame);
        extra->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
        return PF_Err_NONE;
    }

    case PF_Event_DRAG: {
        PF_DoClickEventInfo &ck = extra->u.do_click;
        const int what = int(ck.continue_refcon[0] & 15);
        const int i = int(ck.continue_refcon[0] >> 4);
        if (what == int(kNone) || i < 0 || i >= wheelsIn(bank)) return PF_Err_NONE;
        const float dx = float(ck.screen_point.h) - float(ck.continue_refcon[1]);
        const float dy = float(ck.screen_point.v) - float(ck.continue_refcon[2]);
        ck.continue_refcon[1] = A_intptr_t(ck.screen_point.h);
        ck.continue_refcon[2] = A_intptr_t(ck.screen_point.v);
        const float gain = (ck.modifiers & PF_Mod_SHIFT_KEY) ? 0.25f : 1.0f;
        if (dx != 0.0f || dy != 0.0f) applyDelta(params, bank, i, Hit(what), dx, dy, gain);
        if (ck.last_time) {
            drtwheels::undoGroup(in_data, false);
            ck.send_drag = FALSE;
            ck.continue_refcon[0] = 0;
        } else {
            ck.send_drag = TRUE;
        }
        AppSuite(in_data, out_data).invalidate(extra->contextH, frame);
        extra->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
        return PF_Err_NONE;
    }

    default:
        return PF_Err_NONE;
    }
}

} // namespace drtwheels2
