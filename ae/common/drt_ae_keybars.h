/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor Grade — the HSL secondary's key bars, an Effect Controls custom UI (0.1.4).
 *
 * Pure UI, like the wheels (drt_ae_wheels.h, whose painter and helpers this uses): three
 * rows, Hue / Saturation / Level, each a gradient over the window's axis with a check box
 * (the window on or ignored), two range handles above the bar and two softness handles
 * below it. They draw and drag the rows that already exist:
 *   Hue:        Hue Centre, Hue Width, Hue Softness        (degrees, the secondary's hue scale)
 *   Saturation: Saturation Min / Max, Saturation Softness  (purity 0..2)
 *   Level:      Level Min / Max, Level Softness            (stops over grey, shown -10..+10)
 * and Use Hue / Use Saturation / Use Level (hidden rows, drawn here as the check boxes).
 * The gradient is dimmed where the window passes nothing, so the bar is a preview of the key.
 * A click on a bar takes the nearest handle of its half (range above the centre line,
 * softness below) to the mouse; a drag moves it 1:1, Shift a quarter as fast. On the hue bar
 * a drag that starts inside the range away from both handles slides the whole range.
 * A softness handle sets the row's one softness, so the two sides move together.
 * Double-click a row to reset it.
 */
#pragma once

#include "drt_ae_wheels.h"

#include <algorithm>

namespace drtkeys {

using drtwheels::Painter;
using drtwheels::AppSuite;
using drtwheels::grey;
using drtwheels::readFloat;
using drtwheels::writeFloat;
using drtwheels::paramOf;

typedef float drt::DrtGradeParams::*GF;
typedef int drt::DrtGradeParams::*GI;

/* ----------------------------------------------------------------- layout */

const int   kRows   = 3;
const float kRowH   = 34.0f;
const float kTopPad = 8.0f;
const float kBarH   = 10.0f;
const float kBarL   = 52.0f;    /* bar's left, from the frame's left: check box + label before it */
const float kBarR   = 16.0f;    /* margin right of the bar */
const float kHandle = 6.0f;     /* triangle half-width */
const int   kWidth  = 360;      /* ui_width (the panel's own width wins) */
const int   kHeight = int(kTopPad * 2.0f + kRowH * kRows);

struct Axis { float lo, hi; };
inline Axis axisOf(int row) { return row == 0 ? Axis{0.0f, 360.0f} : (row == 1 ? Axis{0.0f, 2.0f} : Axis{-10.0f, 10.0f}); }

struct RowGeom { float x0, x1, barTop, barMid, top, bottom, boxX, boxY; };
inline RowGeom rowGeom(const PF_Rect &f, int row)
{
    RowGeom g;
    g.top = float(f.top) + kTopPad + float(row) * kRowH;
    g.bottom = g.top + kRowH;
    g.barTop = g.top + (kRowH - kBarH) * 0.5f;
    g.barMid = g.barTop + kBarH * 0.5f;
    g.x0 = float(f.left) + kBarL;
    g.x1 = float(f.right) - kBarR;
    if (g.x1 < g.x0 + 60.0f) g.x1 = g.x0 + 60.0f;
    g.boxX = float(f.left) + 10.0f;
    g.boxY = g.barMid - 5.5f;
    return g;
}
inline float xOf(const RowGeom &g, int row, float v) { const Axis a = axisOf(row); return g.x0 + (v - a.lo) / (a.hi - a.lo) * (g.x1 - g.x0); }
inline float vOf(const RowGeom &g, int row, float x) { const Axis a = axisOf(row); return a.lo + (x - g.x0) / (g.x1 - g.x0) * (a.hi - a.lo); }

/* ------------------------------------------------------------- the rows' params */

inline int paramOfCheck(GI m)
{
    for (int k = 0; k < drtae::kGradeRowCount; ++k)
        if (drtae::kGradeRows[k].kind == drtae::kCheck && drtae::kGradeRows[k].gi == m) return k + 1;
    return -1;
}
inline bool readOn(PF_ParamDef *params[], int idx) { return idx > 0 && params[idx]->u.bd.value != 0; }

static const GI kOn[kRows]   = { &drt::DrtGradeParams::sec_hue_on, &drt::DrtGradeParams::sec_sat_on, &drt::DrtGradeParams::sec_lev_on };
static const GF kSoft[kRows] = { &drt::DrtGradeParams::sec_hue_soft, &drt::DrtGradeParams::sec_sat_soft, &drt::DrtGradeParams::sec_lev_soft };
static const char *const kLabels[kRows] = { "H", "S", "L" };

/* a row's window as lo / hi on its axis (the hue's lo may be below 0 or hi above 360) and its softness */
struct Window { float lo, hi, soft; };
inline Window readWindow(PF_ParamDef *params[], int row)
{
    Window w;
    w.soft = readFloat(params, paramOf(kSoft[row]));
    if (row == 0) {
        const float c = readFloat(params, paramOf(&drt::DrtGradeParams::sec_hue));
        const float h = 0.5f * readFloat(params, paramOf(&drt::DrtGradeParams::sec_hue_width));
        w.lo = c - h; w.hi = c + h;
    } else if (row == 1) {
        w.lo = readFloat(params, paramOf(&drt::DrtGradeParams::sec_sat_min));
        w.hi = readFloat(params, paramOf(&drt::DrtGradeParams::sec_sat_max));
    } else {
        w.lo = readFloat(params, paramOf(&drt::DrtGradeParams::sec_lev_min));
        w.hi = readFloat(params, paramOf(&drt::DrtGradeParams::sec_lev_max));
    }
    return w;
}
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline void writeWindow(PF_ParamDef *params[], int row, Window w)
{
    w.soft = clampf(w.soft, 0.0f, row == 0 ? 180.0f : (row == 1 ? 1.0f : 6.0f));
    writeFloat(params, paramOf(kSoft[row]), w.soft);
    if (row == 0) {
        float width = clampf(w.hi - w.lo, 0.0f, 360.0f);
        float c = std::fmod(0.5f * (w.lo + w.hi) + 720.0f, 360.0f);
        writeFloat(params, paramOf(&drt::DrtGradeParams::sec_hue), c);
        writeFloat(params, paramOf(&drt::DrtGradeParams::sec_hue_width), width);
    } else {
        const Axis a = axisOf(row);
        const float lo = clampf(w.lo, a.lo, a.hi), hi = clampf(w.hi, lo, a.hi);
        writeFloat(params, paramOf(row == 1 ? &drt::DrtGradeParams::sec_sat_min : &drt::DrtGradeParams::sec_lev_min), lo);
        writeFloat(params, paramOf(row == 1 ? &drt::DrtGradeParams::sec_sat_max : &drt::DrtGradeParams::sec_lev_max), hi);
    }
}

/* the window's weight at v on its axis, as the mask computes it (drt_zone_edge feather) */
inline float edge(float x) { const float u = clampf(x, 0.0f, 1.0f); return 1.0f - u * u * (3.0f - 2.0f * u); }
inline float weightAt(int row, const Window &w, float v)
{
    const float s = std::max(w.soft, 1e-3f);
    if (row == 0) {
        const float c = 0.5f * (w.lo + w.hi), half = 0.5f * (w.hi - w.lo);
        float d = std::fabs(std::fmod(v - c + 540.0f, 360.0f) - 180.0f);
        return edge((d - half) / s);
    }
    if (row == 1 && w.lo <= 0.0f) return edge((v - w.hi) / s);   /* Saturation Min 0: no lower bound */
    return edge((w.lo - v) / s) * edge((v - w.hi) / s);
}

/* --------------------------------------------------------------- drawing */

inline DRAWBOT_ColorRGBA gradientAt(int row, float v, float hueCentre)
{
    if (row == 0) return drtwheels::ringColor(v);
    if (row == 1) {   /* grey to the selected hue's colour as purity rises */
        const DRAWBOT_ColorRGBA c = drtwheels::ringColor(hueCentre);
        const float t = clampf(v / 1.0f, 0.0f, 1.0f);
        DRAWBOT_ColorRGBA o; o.red = 0.5f + (c.red - 0.5f) * t; o.green = 0.5f + (c.green - 0.5f) * t; o.blue = 0.5f + (c.blue - 0.5f) * t; o.alpha = 1.0f;
        return o;
    }
    /* level: grey 0.18 * 2^stops through a 2.2 curve, so the bar reads dark to bright */
    return grey(std::pow(clampf(0.18f * std::exp2(v), 0.0f, 1.0f), 1.0f / 2.2f));
}

inline PF_Err draw(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra)
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
    p.s = &suites; p.sup = nullptr; p.surf = nullptr;
    suites.drawbot_suiteP->GetSupplier(dref, &p.sup);
    suites.drawbot_suiteP->GetSurface(dref, &p.surf);
    if (p.sup && p.surf) {
        AppSuite app(in_data, out_data);
        const DRAWBOT_ColorRGBA bg = app.background(0.19f);
        const DRAWBOT_ColorRGBA textC = app.color(PF_App_Color_TEXT, 0.80f);
        const DRAWBOT_ColorRGBA handle = grey(0.85f), softC = grey(0.55f);
        suites.surface_suiteP->SetAntiAliasPolicy(p.surf, kDRAWBOT_AntiAliasPolicy_High);
        p.rect(float(frame.left), float(frame.top), float(frame.right - frame.left), float(frame.bottom - frame.top + 1), bg);
        float fontSize = 11.0f;
        suites.supplier_suiteP->GetDefaultFontSize(p.sup, &fontSize);
        const float hueCentre = readFloat(params, paramOf(&drt::DrtGradeParams::sec_hue));

        for (int row = 0; row < kRows; ++row) {
            const RowGeom g = rowGeom(frame, row);
            const bool on = readOn(params, paramOfCheck(kOn[row]));
            const Window w = readWindow(params, row);

            /* check box + label */
            p.rect(g.boxX, g.boxY, 11.0f, 11.0f, grey(0.08f));
            if (on) p.rect(g.boxX + 2.5f, g.boxY + 2.5f, 6.0f, 6.0f, textC);
            p.text(kLabels[row], g.boxX + 24.0f, g.barMid + 0.35f * fontSize, textC, fontSize);

            /* gradient, dimmed where the window passes nothing (and all of it when the row is off) */
            const int n = 120;
            const float segW = (g.x1 - g.x0) / float(n);
            for (int k = 0; k < n; ++k) {
                const float v = vOf(g, row, g.x0 + (float(k) + 0.5f) * segW);
                DRAWBOT_ColorRGBA c = gradientAt(row, v, hueCentre);
                const float wt = on ? weightAt(row, w, v) : 0.0f;
                const float k2 = 0.22f + 0.78f * wt;
                c.red = bg.red + (c.red - bg.red) * k2; c.green = bg.green + (c.green - bg.green) * k2; c.blue = bg.blue + (c.blue - bg.blue) * k2;
                p.rect(g.x0 + float(k) * segW, g.barTop, segW + 0.6f, kBarH, c);
            }

            /* handles: range above (pointing down), softness below (pointing up); the hue's wrap past the ends */
            const float vals[4] = { w.lo, w.hi, w.lo - w.soft, w.hi + w.soft };
            for (int h = 0; h < 4; ++h) {
                float v = vals[h];
                if (row == 0) v = std::fmod(v + 720.0f, 360.0f);
                else if (row == 1 && h == 0 && w.lo <= 0.0f) v = 0.0f;
                const Axis a = axisOf(row);
                const float x = xOf(g, row, clampf(v, a.lo, a.hi));
                if (h < 2) p.triangle(x - kHandle, g.barTop - kHandle * 1.3f, x + kHandle, g.barTop - kHandle * 1.3f, x, g.barTop - 0.5f, on ? handle : softC);
                else       p.triangle(x - kHandle * 0.8f, g.barTop + kBarH + kHandle * 1.1f, x + kHandle * 0.8f, g.barTop + kBarH + kHandle * 1.1f, x, g.barTop + kBarH + 0.5f, softC);
            }
        }
        suites.surface_suiteP->Flush(p.surf);
    }
    AEFX_ReleaseDrawbotSuites(in_data, out_data);
    extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
    return PF_Err_NONE;
}

/* ----------------------------------------------------------------- events */

enum Part { kNone = 0, kLo = 1, kHi = 2, kSlide = 3, kSoftLo = 4, kSoftHi = 5, kBox = 6 };

inline int rowAt(const PF_Rect &f, const PF_Point &m)
{
    for (int row = 0; row < kRows; ++row) {
        const RowGeom g = rowGeom(f, row);
        if (float(m.v) >= g.top && float(m.v) < g.bottom) return row;
    }
    return -1;
}

/* which part of a row the mouse is on */
inline Part partAt(PF_ParamDef *params[], const PF_Rect &f, int row, const PF_Point &m)
{
    const RowGeom g = rowGeom(f, row);
    const float mx = float(m.h), my = float(m.v);
    if (mx >= g.boxX - 4.0f && mx <= g.boxX + 30.0f) return kBox;
    if (mx < g.x0 - kHandle * 2.0f || mx > g.x1 + kHandle * 2.0f) return kNone;
    const Window w = readWindow(params, row);
    const Axis a = axisOf(row);
    auto px = [&](float v) { if (row == 0) v = std::fmod(v + 720.0f, 360.0f); return xOf(g, row, clampf(v, a.lo, a.hi)); };
    if (my <= g.barMid) {
        const float dl = std::fabs(mx - px(w.lo)), dh = std::fabs(mx - px(w.hi));
        if (row == 0 && dl > kHandle * 1.5f && dh > kHandle * 1.5f) {
            /* inside the range (on the hue's circle) and off both handles: slide it */
            const float c = 0.5f * (w.lo + w.hi), half = 0.5f * (w.hi - w.lo);
            const float d = std::fabs(std::fmod(vOf(g, row, mx) - c + 540.0f, 360.0f) - 180.0f);
            if (d < half) return kSlide;
        }
        return dl <= dh ? kLo : kHi;
    }
    const float dl = std::fabs(mx - px(w.lo - w.soft)), dh = std::fabs(mx - px(w.hi + w.soft));
    return dl <= dh ? kSoftLo : kSoftHi;
}

/* move a part to axis value v (a click), or by dv (a drag) */
inline void movePart(PF_ParamDef *params[], int row, Part part, float v, bool absolute)
{
    Window w = readWindow(params, row);
    if (row == 0 && absolute) {
        /* take v onto the side of the circle nearest the handle it moves */
        const float ref = part == kLo ? w.lo : (part == kHi ? w.hi : (part == kSoftLo ? w.lo - w.soft : w.hi + w.soft));
        v += 360.0f * std::round((ref - v) / 360.0f);
    }
    switch (part) {
    case kLo:     w.lo = absolute ? v : w.lo + v; if (w.lo > w.hi) w.lo = w.hi; break;
    case kHi:     w.hi = absolute ? v : w.hi + v; if (w.hi < w.lo) w.hi = w.lo; break;
    case kSlide:  w.lo += v; w.hi += v; break;
    case kSoftLo: w.soft = absolute ? w.lo - v : w.soft - v; break;
    case kSoftHi: w.soft = absolute ? v - w.hi : w.soft + v; break;
    default: return;
    }
    if (w.soft < 0.0f) w.soft = 0.0f;
    writeWindow(params, row, w);
}

inline void resetRow(PF_ParamDef *params[], int row)
{
    const drt::DrtGradeParams d = drt::drt_grade_defaults();
    Window w;
    w.soft = d.*(kSoft[row]);
    if (row == 0) { w.lo = d.sec_hue - 0.5f * d.sec_hue_width; w.hi = d.sec_hue + 0.5f * d.sec_hue_width; }
    else if (row == 1) { w.lo = d.sec_sat_min; w.hi = d.sec_sat_max; }
    else { w.lo = d.sec_lev_min; w.hi = d.sec_lev_max; }
    writeWindow(params, row, w);
}

inline PF_Err handleEvent(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra)
{
    const PF_Rect frame = extra->effect_win.current_frame;
    switch (extra->e_type) {
    case PF_Event_DRAW:
        return draw(in_data, out_data, params, extra);

    case PF_Event_ADJUST_CURSOR: {
        const int row = rowAt(frame, extra->u.adjust_cursor.screen_point);
        if (row >= 0) {
            const Part part = partAt(params, frame, row, extra->u.adjust_cursor.screen_point);
            if (part == kSlide) extra->u.adjust_cursor.set_cursor = PF_Cursor_DRAG_DOT;
            else if (part != kNone && part != kBox) extra->u.adjust_cursor.set_cursor = PF_Cursor_FINGER_POINTER_SCRUB;
            if (part != kNone) extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
        }
        return PF_Err_NONE;
    }

    case PF_Event_DO_CLICK: {
        if (extra->effect_win.area != PF_EA_CONTROL) return PF_Err_NONE;
        PF_DoClickEventInfo &ck = extra->u.do_click;
        const int row = rowAt(frame, ck.screen_point);
        if (row < 0) return PF_Err_NONE;
        const Part part = partAt(params, frame, row, ck.screen_point);
        if (part == kNone) return PF_Err_NONE;
        drtwheels::undoGroup(in_data, true);
        ck.send_drag = FALSE;
        if (part == kBox) {
            const int idx = paramOfCheck(kOn[row]);
            if (idx > 0) { params[idx]->u.bd.value = params[idx]->u.bd.value ? 0 : 1; params[idx]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE; }
            drtwheels::undoGroup(in_data, false);
        } else if (ck.num_clicks >= 2) {
            resetRow(params, row);
            drtwheels::undoGroup(in_data, false);
        } else {
            if (part != kSlide) movePart(params, row, part, vOf(rowGeom(frame, row), row, float(ck.screen_point.h)), true);
            ck.send_drag = TRUE;
            ck.continue_refcon[0] = A_intptr_t((row << 4) | int(part));
            ck.continue_refcon[1] = A_intptr_t(ck.screen_point.h);
        }
        AppSuite(in_data, out_data).invalidate(extra->contextH, frame);
        extra->evt_out_flags |= PF_EO_HANDLED_EVENT | PF_EO_UPDATE_NOW;
        return PF_Err_NONE;
    }

    case PF_Event_DRAG: {
        PF_DoClickEventInfo &ck = extra->u.do_click;
        const Part part = Part(ck.continue_refcon[0] & 15);
        const int row = int(ck.continue_refcon[0] >> 4);
        if (part == kNone || row < 0 || row >= kRows) return PF_Err_NONE;
        const float dx = float(ck.screen_point.h) - float(ck.continue_refcon[1]);
        ck.continue_refcon[1] = A_intptr_t(ck.screen_point.h);
        if (dx != 0.0f) {
            const RowGeom g = rowGeom(frame, row);
            const Axis a = axisOf(row);
            const float gain = (ck.modifiers & PF_Mod_SHIFT_KEY) ? 0.25f : 1.0f;
            movePart(params, row, part, dx / (g.x1 - g.x0) * (a.hi - a.lo) * gain, false);
        }
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

} // namespace drtkeys
