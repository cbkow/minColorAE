/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor Grade — colour wheels, the Effect Controls custom UI.
 *
 * Pure UI: the wheels carry no data of their own. Each wheel draws and drags
 * three of the zone sliders that already exist in the parameter table:
 *   angle  = "<zone> Tint Hue"     (degrees, clockwise from the top; red at the top)
 *   radius = "<zone> Tint Amount"  (0 at the centre, 1 at the inner edge of the ring)
 *   bar    = "<zone> Exposure"     (stops, 0 at the middle, +-4 at the ends)
 * A drag writes those sliders with PF_ChangeFlag_CHANGED_VALUE, the same path the
 * preset popups use, so keyframes, expressions, undo and the numeric readout below
 * keep working. The ring is coloured by the DRT's own hue direction (drt_hue_dir),
 * so the hue you grab on the wheel is the hue the grade pushes toward.
 *
 * A control row (a kWheels row; Row::choices = first wheel) lays its wheels out
 * three to a line: one row at the top holds the six zones in two lines (Black,
 * Dark, Shadow over Light, Highlight, Specular), so they share a single twirl,
 * and one single-wheel row sits inside the HSL
 * Secondary group for its own exposure / tint correctors. The cells follow the width of the
 * Effect Controls panel, from 0.75x to 1.5x the natural 360 points, and the row's height follows
 * them: the draw event asks for the height that width wants (PF_UpdateParamUI, ui_height).
 * Drawing is Drawbot (cross-platform); events are DO_CLICK -> DRAG with the last
 * mouse point kept in continue_refcon, so motion is relative and geared down (kWheelGain,
 * kBarGain; Shift = finer still). A drag is measured against the wheel's own size, so a
 * wider panel is a finer control.
 * Double-click resets the wheel's tint or the bar's exposure.
 */
#pragma once

#include "drt_ae_params.h"

#include "AE_Effect.h"
#include "AE_EffectUI.h"
#include "AE_EffectSuites.h"
#include "AE_GeneralPlug.h"
#include "AEFX_SuiteHelper.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace drtwheels {

/* ----------------------------------------------------------------- layout */

const int   kPerRow   = 3;                /* wheels per line of a control row, Lumetri-style */
const int   kCellW    = 120;              /* natural cell: one wheel + its bar + label */
const int   kCellH    = 150;
const int   kWidth    = kPerRow * kCellW;  /* ui_width of a control row */
const float kMinScale = 0.75f;             /* cell scale limits vs the natural cell: a 57 pt ring ... */
const float kMaxScale = 1.5f;              /* ... to a 114 pt one; past these the cells stop following the panel */
const float kTextH    = 54.0f;             /* label + readout under the wheel; the text does not scale */
const float kWheelGain = 0.4f;             /* puck travel per mouse travel (1 = the puck tracks the mouse) */
const float kBarGain   = 0.5f;             /* same for the exposure bar's thumb */
/* natural geometry at kCellW; everything scales with the cell */
const float kRingOut  = 38.0f;
const float kRingIn   = 23.0f;
const float kWheelCy  = 58.0f;             /* wheel centre, from the row top */
const float kWheelCx  = 70.0f;             /* wheel centre, from the cell left */
const float kBarX     = 14.0f;             /* exposure bar, from the cell left */
const float kBarW     = 6.0f;
const float kBarH     = 88.0f;
const float kPuckR    = 4.5f;
const float kExpRange = 4.0f;              /* stops at the bar ends (the slider range) */
const float kPi       = 3.14159265f;

typedef float drt::DrtGradeParams::*GF;

/* the wheels: the six zones, then the HSL secondary's correctors (index 6) */
const int kWheelCount = DRT_ZONE_COUNT + 1;
static const GF kZoneExposure[kWheelCount] = {
    &drt::DrtGradeParams::z0_exposure, &drt::DrtGradeParams::z1_exposure, &drt::DrtGradeParams::z2_exposure,
    &drt::DrtGradeParams::z3_exposure, &drt::DrtGradeParams::z4_exposure, &drt::DrtGradeParams::z5_exposure,
    &drt::DrtGradeParams::sec_exposure };
static const GF kZoneHue[kWheelCount] = {
    &drt::DrtGradeParams::z0_hue, &drt::DrtGradeParams::z1_hue, &drt::DrtGradeParams::z2_hue,
    &drt::DrtGradeParams::z3_hue, &drt::DrtGradeParams::z4_hue, &drt::DrtGradeParams::z5_hue,
    &drt::DrtGradeParams::sec_hue_shift };
static const GF kZoneTint[kWheelCount] = {
    &drt::DrtGradeParams::z0_tint, &drt::DrtGradeParams::z1_tint, &drt::DrtGradeParams::z2_tint,
    &drt::DrtGradeParams::z3_tint, &drt::DrtGradeParams::z4_tint, &drt::DrtGradeParams::z5_tint,
    &drt::DrtGradeParams::sec_tint };
static const char *const kZoneLabels[kWheelCount] = { "Black", "Dark", "Shadow", "Light", "Highlight", "Specular", "Secondary" };

/* how many wheels a row starting at `first` holds: the zones from there on, or the secondary's one */
inline int wheelsInRow(int first)
{
    const int n = (first < DRT_ZONE_COUNT ? DRT_ZONE_COUNT : kWheelCount) - first;
    return n < 0 ? 0 : n;
}
inline int linesFor(int count) { return count > 0 ? (count + kPerRow - 1) / kPerRow : 1; }

/* Parameter index (params[] slot) of the float row bound to a grade field. */
inline int paramOf(GF m)
{
    for (int k = 0; k < drtae::kGradeRowCount; ++k)
        if (drtae::kGradeRows[k].kind == drtae::kFloat && drtae::kGradeRows[k].gf == m) return k + 1;
    return -1;
}

struct Cell {
    float s;             /* scale vs the natural cell */
    float cx, cy;        /* wheel centre */
    float ringOut, ringIn, puckR;
    float barX, barTop, barW, barH;
    float left;
};

/* the scale a row of this width wants, and the row height that scale needs */
inline float widthScale(float rowW)
{
    const float s = rowW / float(kWidth);
    return s < kMinScale ? kMinScale : (s > kMaxScale ? kMaxScale : s);
}
inline float lineHeight(float s) { return (kWheelCy + kRingOut) * s + kTextH; }
inline int heightFor(float s, int count) { return int(float(linesFor(count)) * lineHeight(s) + 0.5f); }

inline Cell cellGeom(const PF_Rect &frame, int i, int count)
{
    Cell c;
    const float rowW = float(frame.right - frame.left);
    /* the width sets the scale; until the row has been given the matching height, stay inside the one it has */
    c.s = widthScale(rowW);
    const float fit = (float(frame.bottom - frame.top) / float(linesFor(count)) - kTextH) / (kWheelCy + kRingOut);
    if (c.s > fit) c.s = fit;
    if (c.s < kMinScale) c.s = kMinScale;
    const float cellW = float(kCellW) * c.s;
    float off = (rowW - cellW * float(count < kPerRow ? count : kPerRow)) * 0.5f;
    if (off < 0.0f) off = 0.0f;
    c.left = float(frame.left) + off + float(i % kPerRow) * cellW;
    c.cx = c.left + kWheelCx * c.s;
    c.cy = float(frame.top) + float(i / kPerRow) * lineHeight(c.s) + kWheelCy * c.s;
    c.ringOut = kRingOut * c.s;
    c.ringIn = kRingIn * c.s;
    c.puckR = kPuckR * (c.s > 1.0f ? std::sqrt(c.s) : c.s);   /* a big wheel keeps a small puck */
    c.barX = c.left + kBarX * c.s;
    c.barW = kBarW * c.s;
    c.barH = kBarH * c.s;
    c.barTop = c.cy - c.barH * 0.5f;
    return c;
}

/* hue (deg, clockwise from top) + tint (0..1)  <->  puck vector, unit disc */
inline void puckFromHueTint(float hue, float tint, float &px, float &py)
{
    const float a = hue * (kPi / 180.0f);
    px = tint * std::sin(a);
    py = -tint * std::cos(a);
}
inline void hueTintFromPuck(float px, float py, float oldHue, float &hue, float &tint)
{
    tint = std::sqrt(px * px + py * py);
    if (tint > 1.0f) tint = 1.0f;
    if (tint < 1.0e-3f) { tint = 0.0f; hue = oldHue; return; }
    hue = std::atan2(px, -py) * (180.0f / kPi);
    if (hue < 0.0f) hue += 360.0f;
}

enum Hit { kNone = 0, kWheel = 1, kBar = 2 };

inline Hit hitTest(const PF_Rect &frame, int firstZone, const PF_Point &m, int &zoneOut)
{
    const int count = wheelsInRow(firstZone);
    for (int i = 0; i < count; ++i) {
        const Cell c = cellGeom(frame, i, count);
        const float dx = float(m.h) - c.cx, dy = float(m.v) - c.cy;
        if (std::sqrt(dx * dx + dy * dy) <= c.ringOut + 4.0f) { zoneOut = firstZone + i; return kWheel; }
        if (float(m.h) >= c.barX - 8.0f && float(m.h) <= c.barX + c.barW + 10.0f &&
            float(m.v) >= c.barTop - 6.0f && float(m.v) <= c.barTop + c.barH + 6.0f) { zoneOut = firstZone + i; return kBar; }
    }
    zoneOut = -1;
    return kNone;
}

/* ------------------------------------------------------------- param I/O */

inline float readFloat(PF_ParamDef *params[], int idx) { return idx > 0 ? float(params[idx]->u.fs_d.value) : 0.0f; }
inline void writeFloat(PF_ParamDef *params[], int idx, float v)
{
    if (idx <= 0) return;
    params[idx]->u.fs_d.value = double(v);
    params[idx]->uu.change_flags |= PF_ChangeFlag_CHANGED_VALUE;
}

/* ------------------------------------------------------------- host bits */

struct AppSuite {
    PFAppSuite6 *s = nullptr;
    PF_InData *in; PF_OutData *out;
    AppSuite(PF_InData *i, PF_OutData *o) : in(i), out(o)
    {
        if (AEFX_AcquireSuite(in, out, kPFAppSuite, kPFAppSuiteVersion6, nullptr, (void **)&s) != PF_Err_NONE) s = nullptr;
    }
    ~AppSuite() { if (s) AEFX_ReleaseSuite(in, out, kPFAppSuite, kPFAppSuiteVersion6, nullptr); }
    DRAWBOT_ColorRGBA color(PF_App_ColorType t, float fallback) const
    {
        DRAWBOT_ColorRGBA c; c.red = c.green = c.blue = fallback; c.alpha = 1.0f;
        PF_App_Color a; a.red = a.green = a.blue = 0;
        if (s && s->PF_AppGetColor(t, &a) == PF_Err_NONE) {
            c.red = float(a.red) / 65535.0f; c.green = float(a.green) / 65535.0f; c.blue = float(a.blue) / 65535.0f;
        }
        return c;
    }
    DRAWBOT_ColorRGBA background(float fallback) const
    {
        DRAWBOT_ColorRGBA c; c.red = c.green = c.blue = fallback; c.alpha = 1.0f;
        PF_App_Color a; a.red = a.green = a.blue = 0;
        if (s && s->PF_AppGetBgColor(&a) == PF_Err_NONE) {
            c.red = float(a.red) / 65535.0f; c.green = float(a.green) / 65535.0f; c.blue = float(a.blue) / 65535.0f;
        }
        return c;
    }
    void invalidate(PF_ContextH ctx, const PF_Rect &r) const { if (s) s->PF_InvalidateRect(ctx, &r); }
};

/* One undo step per drag: AEGP_UtilitySuite6 is reachable from an effect through PICA. */
inline void undoGroup(PF_InData *in_data, bool start)
{
    static bool open = false;
    if (!in_data->pica_basicP) return;
    AEGP_UtilitySuite6 *u = nullptr;
    if (in_data->pica_basicP->AcquireSuite(kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6, (const void **)&u) != kSPNoError || !u) return;
    if (start) {
        if (open) u->AEGP_EndUndoGroup();
        u->AEGP_StartUndoGroup("minColor Grade wheel");
        open = true;
    } else if (open) {
        u->AEGP_EndUndoGroup();
        open = false;
    }
    in_data->pica_basicP->ReleaseSuite(kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
}

/* optional log sink, set by the effect to its dlog */
inline void (*g_log)(const char *fmt, ...) = nullptr;

/* Ask AE for the row height the row's width wants. ui_height is one of the few fields
   PF_UpdateParamUI may change; AE lays the panel out again and sends a fresh draw. The
   change is cosmetic (not saved, no undo), so a reopened project starts at the natural height and
   the first draw corrects it. */
inline void fitHeight(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], int paramIdx, const PF_Rect &frame, int count)
{
    const int have = int(frame.bottom - frame.top);
    const int want = heightFor(widthScale(float(frame.right - frame.left)), count);
    if (std::abs(want - have) < 3) return;
    /* the same request with nothing moved means AE is not taking it: do not spin */
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
    const PF_Err err = pu->PF_UpdateParamUI(in_data->effect_ref, paramIdx, &def);
    AEFX_ReleaseSuite(in_data, out_data, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3, nullptr);
    if (g_log) g_log("wheels row %d: width %d height %d -> %d (err %d)", paramIdx, int(frame.right - frame.left), have, want, int(err));
}

/* ---------------------------------------------------------------- drawing */

inline DRAWBOT_ColorRGBA grey(float v, float a = 1.0f) { DRAWBOT_ColorRGBA c; c.red = c.green = c.blue = v; c.alpha = a; return c; }

/* Ring colour at a screen angle: the direction the grade pushes toward for that hue. */
inline DRAWBOT_ColorRGBA ringColor(float hueDeg)
{
    const drt::float3 d = drt::drt_hue_dir(hueDeg);
    DRAWBOT_ColorRGBA c;
    c.red   = 0.5f + 0.62f * d.x;
    c.green = 0.5f + 0.62f * d.y;
    c.blue  = 0.5f + 0.62f * d.z;
    if (c.red < 0.0f) c.red = 0.0f;     if (c.red > 1.0f) c.red = 1.0f;
    if (c.green < 0.0f) c.green = 0.0f; if (c.green > 1.0f) c.green = 1.0f;
    if (c.blue < 0.0f) c.blue = 0.0f;   if (c.blue > 1.0f) c.blue = 1.0f;
    c.alpha = 1.0f;
    return c;
}

struct Painter {
    DRAWBOT_Suites *s;
    DRAWBOT_SupplierRef sup;
    DRAWBOT_SurfaceRef surf;

    void fillPath(DRAWBOT_PathRef path, const DRAWBOT_ColorRGBA &c)
    {
        DRAWBOT_BrushRef b = nullptr;
        if (s->supplier_suiteP->NewBrush(sup, &c, &b) == kSPNoError && b) {
            s->surface_suiteP->FillPath(surf, b, path, kDRAWBOT_FillType_Winding);
            s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)b);
        }
    }
    void strokePath(DRAWBOT_PathRef path, const DRAWBOT_ColorRGBA &c, float w)
    {
        DRAWBOT_PenRef p = nullptr;
        if (s->supplier_suiteP->NewPen(sup, &c, w, &p) == kSPNoError && p) {
            s->surface_suiteP->StrokePath(surf, p, path);
            s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)p);
        }
    }
    void disc(float cx, float cy, float r, const DRAWBOT_ColorRGBA &fill, const DRAWBOT_ColorRGBA *stroke = nullptr, float sw = 1.0f)
    {
        DRAWBOT_PathRef path = nullptr;
        if (s->supplier_suiteP->NewPath(sup, &path) != kSPNoError || !path) return;
        DRAWBOT_PointF32 c; c.x = cx; c.y = cy;
        s->path_suiteP->AddArc(path, &c, r, 0.0f, 360.0f);
        s->path_suiteP->Close(path);
        fillPath(path, fill);
        if (stroke) strokePath(path, *stroke, sw);
        s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path);
    }
    void wedge(float cx, float cy, float r, float startDeg, float sweepDeg, const DRAWBOT_ColorRGBA &fill)
    {
        DRAWBOT_PathRef path = nullptr;
        if (s->supplier_suiteP->NewPath(sup, &path) != kSPNoError || !path) return;
        DRAWBOT_PointF32 c; c.x = cx; c.y = cy;
        s->path_suiteP->MoveTo(path, cx, cy);
        s->path_suiteP->AddArc(path, &c, r, startDeg, sweepDeg);
        s->path_suiteP->Close(path);
        fillPath(path, fill);
        s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path);
    }
    void rect(float x, float y, float w, float h, const DRAWBOT_ColorRGBA &c)
    {
        DRAWBOT_RectF32 r; r.left = x; r.top = y; r.width = w; r.height = h;
        s->surface_suiteP->PaintRect(surf, &c, &r);
    }
    void triangle(float x0, float y0, float x1, float y1, float x2, float y2, const DRAWBOT_ColorRGBA &c)
    {
        DRAWBOT_PathRef path = nullptr;
        if (s->supplier_suiteP->NewPath(sup, &path) != kSPNoError || !path) return;
        s->path_suiteP->MoveTo(path, x0, y0);
        s->path_suiteP->LineTo(path, x1, y1);
        s->path_suiteP->LineTo(path, x2, y2);
        s->path_suiteP->Close(path);
        fillPath(path, c);
        s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path);
    }
    void text(const char *ascii, float cx, float y, const DRAWBOT_ColorRGBA &c, float size)
    {
        DRAWBOT_Boolean ok = 0;
        if (s->supplier_suiteP->SupportsText(sup, &ok) != kSPNoError || !ok) return;
        DRAWBOT_FontRef font = nullptr;
        if (s->supplier_suiteP->NewDefaultFont(sup, size, &font) != kSPNoError || !font) return;
        DRAWBOT_BrushRef b = nullptr;
        if (s->supplier_suiteP->NewBrush(sup, &c, &b) == kSPNoError && b) {
            DRAWBOT_UTF16Char u[96];
            int n = 0;
            for (; ascii[n] && n < 95; ++n) u[n] = DRAWBOT_UTF16Char((unsigned char)ascii[n]);
            u[n] = 0;
            DRAWBOT_PointF32 o; o.x = cx; o.y = y;
            s->surface_suiteP->DrawString(surf, b, font, u, &o, kDRAWBOT_TextAlignment_Center, kDRAWBOT_TextTruncation_None, 0.0f);
            s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)b);
        }
        s->supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)font);
    }
};

inline PF_Err draw(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra, int firstZone)
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
        DRAWBOT_ColorRGBA dim = textC; dim.alpha = 0.55f;
        const DRAWBOT_ColorRGBA track = grey(0.08f);
        const DRAWBOT_ColorRGBA thumb = grey(0.78f);
        const DRAWBOT_ColorRGBA puckFill = grey(0.90f);
        const DRAWBOT_ColorRGBA puckEdge = grey(0.10f);

        suites.surface_suiteP->SetAntiAliasPolicy(p.surf, kDRAWBOT_AntiAliasPolicy_High);
        p.rect(float(frame.left), float(frame.top), float(frame.right - frame.left), float(frame.bottom - frame.top + 1), bg);

        float fontSize = 11.0f;
        suites.supplier_suiteP->GetDefaultFontSize(p.sup, &fontSize);

        const int count = wheelsInRow(firstZone);
        for (int i = 0; i < count; ++i) {
            const int z = firstZone + i;
            const Cell c = cellGeom(frame, i, count);
            const float exposure = readFloat(params, paramOf(kZoneExposure[z]));
            const float hue = readFloat(params, paramOf(kZoneHue[z]));
            const float tint = readFloat(params, paramOf(kZoneTint[z]));

            /* ring: wedges of 5 degrees (2 on a big wheel), overlapping a little so no seams show; then the hole */
            const int wedges = c.s > 1.25f ? 180 : 72;
            const float step = 360.0f / float(wedges);
            for (int k = 0; k < wedges; ++k) {
                const float h = float(k) * step;
                p.wedge(c.cx, c.cy, c.ringOut, h - 90.0f - 0.4f, step + 0.8f, ringColor(h + 0.5f * step));
            }
            p.disc(c.cx, c.cy, c.ringIn, bg);

            /* puck */
            float px, py;
            puckFromHueTint(hue, tint, px, py);
            const float reach = c.ringIn - c.puckR - 1.0f;
            p.disc(c.cx + px * reach, c.cy + py * reach, c.puckR, puckFill, &puckEdge, 1.0f);
            if (tint > 0.0f) p.disc(c.cx, c.cy, 1.5f, dim);   /* centre mark once the puck has left it */

            /* exposure bar */
            p.rect(c.barX, c.barTop, c.barW, c.barH, track);
            p.rect(c.barX, c.cy - 0.5f, c.barW, 1.0f, grey(0.35f));
            float e = exposure;
            if (e > kExpRange) e = kExpRange;
            if (e < -kExpRange) e = -kExpRange;
            const float ty = c.cy - (e / kExpRange) * (c.barH * 0.5f);
            const float th = 4.5f * c.s + 0.5f;
            p.triangle(c.barX + c.barW + 1.0f, ty, c.barX + c.barW + 1.0f + 7.0f * c.s, ty - th, c.barX + c.barW + 1.0f + 7.0f * c.s, ty + th, thumb);

            /* label + readout */
            const float labelY = c.cy + c.ringOut + 15.0f;
            p.text(kZoneLabels[z], c.cx, labelY, textC, fontSize);
            char line[64] = "";
            if (exposure != 0.0f || tint != 0.0f) {
                if (exposure != 0.0f && tint != 0.0f)
                    std::snprintf(line, sizeof line, "%+.2f st   %d%% @ %d\xB0", exposure, int(tint * 100.0f + 0.5f), int(hue + 0.5f) % 360);
                else if (exposure != 0.0f)
                    std::snprintf(line, sizeof line, "%+.2f st", exposure);
                else
                    std::snprintf(line, sizeof line, "%d%% @ %d\xB0", int(tint * 100.0f + 0.5f), int(hue + 0.5f) % 360);
                p.text(line, c.cx, labelY + fontSize + 3.0f, dim, fontSize - 1.0f);
            }
        }
        suites.surface_suiteP->Flush(p.surf);
    }
    AEFX_ReleaseDrawbotSuites(in_data, out_data);
    extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
    fitHeight(in_data, out_data, params, int(extra->effect_win.index), frame, wheelsInRow(firstZone));
    return PF_Err_NONE;
}

/* ----------------------------------------------------------------- events */

inline void applyWheelDelta(PF_ParamDef *params[], int z, float dx, float dy, float gain, const Cell &c)
{
    const int ih = paramOf(kZoneHue[z]), it = paramOf(kZoneTint[z]);
    const float oldHue = readFloat(params, ih);
    float px, py;
    puckFromHueTint(oldHue, readFloat(params, it), px, py);
    const float reach = c.ringIn - c.puckR - 1.0f;
    px += dx / reach * kWheelGain * gain;
    py += dy / reach * kWheelGain * gain;
    float hue, tint;
    hueTintFromPuck(px, py, oldHue, hue, tint);
    writeFloat(params, ih, hue);
    writeFloat(params, it, tint);
}

inline void applyBarDelta(PF_ParamDef *params[], int z, float dy, float gain, const Cell &c)
{
    const int ie = paramOf(kZoneExposure[z]);
    float e = readFloat(params, ie) + (-dy) / (c.barH * 0.5f) * kExpRange * kBarGain * gain;
    if (e > kExpRange) e = kExpRange;
    if (e < -kExpRange) e = -kExpRange;
    writeFloat(params, ie, e);
}

inline PF_Err handleEvent(PF_InData *in_data, PF_OutData *out_data, PF_ParamDef *params[], PF_EventExtra *extra)
{
    if (!extra->contextH || (**extra->contextH).w_type != PF_Window_EFFECT) return PF_Err_NONE;
    const int row = int(extra->effect_win.index) - 1;
    if (row < 0 || row >= drtae::kGradeRowCount || drtae::kGradeRows[row].kind != drtae::kWheels) return PF_Err_NONE;
    const int firstZone = drtae::kGradeRows[row].choices;
    const PF_Rect frame = extra->effect_win.current_frame;

    switch (extra->e_type) {
    case PF_Event_DRAW:
        return draw(in_data, out_data, params, extra, firstZone);

    case PF_Event_ADJUST_CURSOR: {
        int z;
        const Hit h = hitTest(frame, firstZone, extra->u.adjust_cursor.screen_point, z);
        if (h == kWheel) extra->u.adjust_cursor.set_cursor = PF_Cursor_DRAG_DOT;
        else if (h == kBar) extra->u.adjust_cursor.set_cursor = PF_Cursor_FINGER_POINTER_SCRUB;
        if (h != kNone) extra->evt_out_flags |= PF_EO_HANDLED_EVENT;
        return PF_Err_NONE;
    }

    case PF_Event_DO_CLICK: {
        if (extra->effect_win.area != PF_EA_CONTROL) return PF_Err_NONE;
        PF_DoClickEventInfo &ck = extra->u.do_click;
        int z;
        const Hit h = hitTest(frame, firstZone, ck.screen_point, z);
        if (h == kNone) return PF_Err_NONE;
        if (ck.num_clicks >= 2) {
            /* double-click: reset */
            undoGroup(in_data, true);
            if (h == kWheel) writeFloat(params, paramOf(kZoneTint[z]), 0.0f);
            else             writeFloat(params, paramOf(kZoneExposure[z]), 0.0f);
            undoGroup(in_data, false);
            ck.send_drag = FALSE;
        } else {
            undoGroup(in_data, true);
            ck.send_drag = TRUE;
            ck.continue_refcon[0] = A_intptr_t((z << 4) | int(h));
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
        const int z = int(ck.continue_refcon[0] >> 4);
        if (what == int(kNone) || z < 0 || z >= kWheelCount) return PF_Err_NONE;
        const float dx = float(ck.screen_point.h) - float(ck.continue_refcon[1]);
        const float dy = float(ck.screen_point.v) - float(ck.continue_refcon[2]);
        ck.continue_refcon[1] = A_intptr_t(ck.screen_point.h);
        ck.continue_refcon[2] = A_intptr_t(ck.screen_point.v);
        const float gain = (ck.modifiers & PF_Mod_SHIFT_KEY) ? 0.25f : 1.0f;
        if (dx != 0.0f || dy != 0.0f) {
            const Cell c = cellGeom(frame, z - firstZone, wheelsInRow(firstZone));
            if (what == kWheel) applyWheelDelta(params, z, dx, dy, gain, c);
            else                applyBarDelta(params, z, dy, gain, c);
        }
        if (ck.last_time) {
            undoGroup(in_data, false);
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

} // namespace drtwheels
