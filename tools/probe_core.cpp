/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* probe_core — the core against the untouched upstream DCTL.
 *
 * Every preset combination the DCTL's UI can express, over a grid of scene-linear
 * values that covers negatives, deep shadows, midgrey and far-out highlights, plus
 * an HDR sweep and an input gamut/transfer sweep. Both sides run the same float
 * ops in the same order, so the tolerance is tight; a real port error shows up as
 * a difference many orders of magnitude larger than float noise.
 *
 * Exit code 0 = every case within tolerance. Prints the worst case per section.
 */
#include <cstdio>
#include <cmath>
#include <vector>

#include "opendrt.h"
#include "dctl_ref.h"

/* the kernel's matrix macros expand to unqualified make_float3 / drt_make_mat3 */
using drt::make_float3;
using drt::drt_make_mat3;

namespace {

struct Worst {
    double err = 0.0;
    float in[3] = {0, 0, 0};
    float core[3] = {0, 0, 0};
    float ref[3] = {0, 0, 0};
    drt::DrtSettings s;
    long count = 0;
    long bad = 0;
};

const double kTolerance = 1e-5;

dctl_ref::Settings toRef(const drt::DrtSettings &s)
{
    dctl_ref::Settings r;
    r.in_gamut = s.in_gamut;
    r.in_oetf = s.in_oetf;
    r.tn_Lp = s.tn_Lp;
    r.tn_gb = s.tn_gb;
    r.pt_hdr = s.pt_hdr;
    r.tn_Lg = s.tn_Lg;
    r.look_preset = s.look;
    r.tonescale_preset = s.tonescale;
    r.cwp = s.cwp;
    r.cwp_lm = s.cwp_lm;
    r.display_encoding_preset = s.display;
    return r;
}

bool finiteOrBothNan(float a, float b)
{
    return (std::isnan(a) && std::isnan(b)) || (std::isfinite(a) && std::isfinite(b));
}

void run(const drt::DrtSettings &s, const std::vector<float> &grid, Worst &w)
{
    const drt::DrtParams p = drt::drt_resolve(s);
    const dctl_ref::Settings rs = toRef(s);
    for (float r : grid)
        for (float g : grid)
            for (float b : grid) {
                const float in[3] = {r, g, b};
                float a[3], c[3];
                drt::apply(p, in, a);
                dctl_ref::transform(rs, in, c);
                double e = 0.0;
                for (int i = 0; i < 3; ++i) {
                    if (!finiteOrBothNan(a[i], c[i])) { e = 1e9; break; }
                    if (std::isnan(a[i])) continue;
                    e = std::fmax(e, std::fabs(double(a[i]) - double(c[i])));
                }
                ++w.count;
                if (e > kTolerance) ++w.bad;
                if (e > w.err) {
                    w.err = e;
                    for (int i = 0; i < 3; ++i) { w.in[i] = in[i]; w.core[i] = a[i]; w.ref[i] = c[i]; }
                    w.s = s;
                }
            }
}

void report(const char *title, const Worst &w)
{
    std::printf("%-34s cases %9ld  over-tolerance %6ld  max |diff| %.3e\n", title, w.count, w.bad, w.err);
    if (w.bad > 0) {
        std::printf("   worst: look %d ts %d cwp %d disp %d Lp %.0f gb %.2f pthdr %.2f Lg %.1f gamut %d oetf %d\n",
                    w.s.look, w.s.tonescale, w.s.cwp, w.s.display, w.s.tn_Lp, w.s.tn_gb, w.s.pt_hdr, w.s.tn_Lg,
                    w.s.in_gamut, w.s.in_oetf);
        std::printf("   in   %.6g %.6g %.6g\n   core %.8g %.8g %.8g\n   ref  %.8g %.8g %.8g\n",
                    w.in[0], w.in[1], w.in[2], w.core[0], w.core[1], w.core[2], w.ref[0], w.ref[1], w.ref[2]);
    }
}

} // namespace

int main()
{
    /* Scene-linear grid: negatives, sub-black, deep shadow, midgrey, highlights out to 256. */
    const std::vector<float> lin = {-0.02f, -0.001f, 0.0f, 1e-4f, 1e-3f, 0.005f, 0.01f, 0.02f, 0.05f, 0.1f,
                                    0.18f, 0.3f, 0.5f, 0.8f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 64.0f, 256.0f};
    /* Log-encoded grid for the transfer-function sweep (code values, slight over/under range). */
    const std::vector<float> logv = {-0.05f, 0.0f, 0.05f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f};
    const std::vector<float> small = {-0.01f, 0.0f, 0.01f, 0.18f, 1.0f, 12.0f};

    bool ok = true;

    /* 1. Every look x every display, look's own tonescale and white. */
    {
        Worst w;
        for (int d = 0; d < drt::kDisplayUpstreamCount; ++d)
            for (int l = 0; l < drt::kLookCount; ++l) {
                drt::DrtSettings s;
                s.in_gamut = DRT_IN_AP0; s.in_oetf = DRT_OETF_LINEAR;
                s.look = l; s.display = d;
                s.tn_Lp = drt::kDisplays[d].default_Lp;
                run(s, lin, w);
            }
        report("looks x displays", w);
        ok = ok && w.bad == 0;
    }

    /* 2. Every tonescale preset over every look (SDR Rec.1886 and PQ). */
    {
        Worst w;
        for (int d : {0, 6})
            for (int l = 0; l < drt::kLookCount; ++l)
                for (int t = 1; t <= drt::kTonescaleCount; ++t) {
                    drt::DrtSettings s;
                    s.in_gamut = DRT_IN_AP1; s.in_oetf = DRT_OETF_LINEAR;
                    s.look = l; s.tonescale = t; s.display = d;
                    s.tn_Lp = drt::kDisplays[d].default_Lp;
                    run(s, lin, w);
                }
        report("tonescale presets", w);
        ok = ok && w.bad == 0;
    }

    /* 3. Every creative white override x every display gamut family, two limits. */
    {
        Worst w;
        for (int d = 0; d < drt::kDisplayUpstreamCount; ++d)
            for (int c = 1; c <= drt::kCwpCount; ++c)
                for (float lm : {0.0f, 0.25f, 1.0f}) {
                    drt::DrtSettings s;
                    s.in_gamut = DRT_IN_REC709; s.in_oetf = DRT_OETF_LINEAR;
                    s.look = 0; s.cwp = c; s.cwp_lm = lm; s.display = d;
                    s.tn_Lp = drt::kDisplays[d].default_Lp;
                    run(s, lin, w);
                }
        report("creative white", w);
        ok = ok && w.bad == 0;
    }

    /* 4. HDR sliders on PQ, HLG and one SDR display. */
    {
        Worst w;
        for (int d : {0, 6, 7, 8})
            for (float Lp : {100.0f, 400.0f, 1000.0f})
                for (float gb : {0.0f, 0.13f, 0.6f})
                    for (float ph : {0.0f, 0.5f, 1.0f})
                        for (float Lg : {3.0f, 10.0f, 25.0f}) {
                            drt::DrtSettings s;
                            s.in_gamut = DRT_IN_AP0; s.in_oetf = DRT_OETF_LINEAR;
                            s.look = 2; s.display = d;
                            s.tn_Lp = Lp; s.tn_gb = gb; s.pt_hdr = ph; s.tn_Lg = Lg;
                            run(s, small, w);
                        }
        report("HDR sliders", w);
        ok = ok && w.bad == 0;
    }

    /* 5. Every input gamut, linear. */
    {
        Worst w;
        for (int g = 0; g < DRT_IN_GAMUT_COUNT; ++g) {
            drt::DrtSettings s;
            s.in_gamut = g; s.in_oetf = DRT_OETF_LINEAR; s.look = 0; s.display = 0;
            run(s, lin, w);
        }
        report("input gamuts", w);
        ok = ok && w.bad == 0;
    }

    /* 6. Every upstream input transfer function, on its usual gamut and on AP0.
          (The display-referred decodes appended after them have no DCTL twin; section 9.) */
    {
        Worst w;
        for (int t = 1; t < DRT_OETF_UPSTREAM_COUNT; ++t)
            for (int g : {DRT_IN_AP0, DRT_IN_DAVINCI_WG, DRT_IN_ARRI_WG4, DRT_IN_SONY_SGAMUT3CINE}) {
                drt::DrtSettings s;
                s.in_gamut = g; s.in_oetf = t; s.look = 0; s.display = 0;
                run(s, logv, w);
            }
        report("input transfer functions", w);
        ok = ok && w.bad == 0;
    }

    /* 7. Sanity: a zero diff is only meaningful if the outputs are real numbers.
          Grey ramp through Standard / Rec.1886 must be finite, monotonic, and land
          midgrey where the tonescale puts it (10 nits of 100 -> 0.1 display linear
          -> 0.1^(1/2.4) = 0.383). */
    {
        drt::DrtSettings s;
        s.in_gamut = DRT_IN_REC709; s.in_oetf = DRT_OETF_LINEAR; s.look = 0; s.display = 0;
        const drt::DrtParams p = drt::drt_resolve(s);
        const float ramp[] = {0.0f, 0.001f, 0.01f, 0.05f, 0.18f, 0.5f, 1.0f, 4.0f, 16.0f, 100.0f};
        float prev = -1.0f;
        bool sane = true;
        std::printf("grey ramp, Standard / Rec.1886:");
        for (float v : ramp) {
            const float in[3] = {v, v, v};
            float out[3];
            drt::apply(p, in, out);
            std::printf(" %.3g->%.4f", v, out[1]);
            if (!std::isfinite(out[1]) || out[1] < prev) sane = false;
            if (v == 0.18f && std::fabs(out[1] - 0.383f) > 0.02f) sane = false;
            if (v == 100.0f && out[1] < 0.99f) sane = false;
            prev = out[1];
        }
        std::printf("\n");
        /* And a saturated primary must come out saturated, not grey. */
        const float red[3] = {1.0f, 0.0f, 0.0f};
        float out[3];
        drt::apply(p, red, out);
        std::printf("Rec.709 red 1,0,0 -> %.4f %.4f %.4f\n", out[0], out[1], out[2]);
        if (!(out[0] > 0.6f && out[1] < 0.35f && out[2] < 0.35f)) sane = false;
        report("sanity", Worst{});
        if (!sane) std::printf("   sanity FAILED: ramp not finite/monotonic, midgrey off, or red desaturated\n");
        ok = ok && sane;
    }

    /* 8. drt_input_transform (the Input effect's half) has no upstream twin, so it is
          checked by composition: Input(gamut g -> working w) followed by Output with
          in_gamut = w / Linear must equal Output with in_gamut = g on the raw value.
          Two matrix products instead of one, so allow float noise. */
    {
        Worst w;
        const std::vector<float> grid = {-0.01f, 0.0f, 0.005f, 0.18f, 0.9f, 4.0f, 64.0f};
        for (int g = 0; g < DRT_IN_GAMUT_COUNT; ++g)
            for (int wk : {DRT_IN_AP1, DRT_IN_AP0, DRT_IN_REC709, DRT_IN_P3D65, DRT_IN_REC2020, DRT_IN_FILMLIGHT_EGAMUT2, DRT_IN_DAVINCI_WG})
                for (int t : {DRT_OETF_LINEAR, DRT_OETF_ARRI_LOGC4}) {
                    drt::DrtSettings sIn;
                    sIn.in_gamut = g; sIn.in_oetf = t;
                    drt::DrtParams pIn = drt::drt_resolve(sIn);
                    pIn.working_gamut = wk;
                    pIn = drt::drt_derive(pIn);

                    drt::DrtSettings sOutW; sOutW.in_gamut = wk; sOutW.in_oetf = DRT_OETF_LINEAR; sOutW.look = 1; sOutW.display = 0;
                    drt::DrtSettings sOutG; sOutG.in_gamut = g;  sOutG.in_oetf = t;               sOutG.look = 1; sOutG.display = 0;
                    const drt::DrtParams pOutW = drt::drt_resolve(sOutW);
                    const drt::DrtParams pOutG = drt::drt_resolve(sOutG);

                    for (float r : grid) for (float gg : grid) for (float b : grid) {
                        float in[3] = {r, gg, b};
                        if (t != DRT_OETF_LINEAR) { in[0] = 0.1f + r * 0.01f; in[1] = 0.1f + gg * 0.01f; in[2] = 0.1f + b * 0.01f; }
                        float lin[3], a[3], c[3];
                        const drt::float3 l = drt::drt_input_transform(pIn, drt::make_float3(in[0], in[1], in[2]));
                        lin[0] = l.x; lin[1] = l.y; lin[2] = l.z;
                        drt::apply(pOutW, lin, a);
                        drt::apply(pOutG, in, c);
                        double e = 0.0;
                        for (int i = 0; i < 3; ++i) {
                            if (!finiteOrBothNan(a[i], c[i])) { e = 1e9; break; }
                            if (std::isnan(a[i])) continue;
                            e = std::fmax(e, std::fabs(double(a[i]) - double(c[i])));
                        }
                        ++w.count;
                        if (e > 2e-5) ++w.bad;
                        if (e > w.err) { w.err = e; for (int i = 0; i < 3; ++i) { w.in[i] = in[i]; w.core[i] = a[i]; w.ref[i] = c[i]; } w.s = sIn; }
                    }
                }
        report("input transform (composition)", w);
        ok = ok && w.bad == 0;
    }

    /* 9. Display-referred decodes (not in upstream): closed-form checks. */
    {
        bool good = true;
        auto dec = [](int tf, float v) {
            drt::DrtParams p = drt::drt_stickshift_defaults();
            p.in_oetf = tf; p.in_gamut = DRT_IN_XYZ; p.working_gamut = DRT_IN_XYZ;
            p = drt::drt_derive(p);
            return drt::drt_input_transform(p, drt::make_float3(v, v, v)).y;
        };
        auto near = [&](const char *name, float got, double want, double tol) {
            const bool okv = std::fabs(double(got) - want) <= tol;
            if (!okv) { std::printf("   %s: got %.6f want %.6f\n", name, got, want); good = false; }
        };
        near("Rec.1886 0.5", dec(DRT_OETF_REC1886, 0.5f), std::pow(0.5, 2.4), 1e-6);
        near("Rec.1886 -0.1 (sign kept)", dec(DRT_OETF_REC1886, -0.1f), -std::pow(0.1, 2.4), 1e-6);
        near("sRGB 0.5", dec(DRT_OETF_SRGB, 0.5f), std::pow((0.5 + 0.055) / 1.055, 2.4), 1e-6);
        near("sRGB 0.02", dec(DRT_OETF_SRGB, 0.02f), 0.02 / 12.92, 1e-7);
        near("2.2 power 0.5", dec(DRT_OETF_POWER_2_2, 0.5f), std::pow(0.5, 2.2), 1e-6);
        near("BT.709 camera 0.5", dec(DRT_OETF_BT709_CAMERA, 0.5f), std::pow((0.5 + 0.099) / 1.099, 1.0 / 0.45), 1e-6);
        near("BT.709 camera 0.05", dec(DRT_OETF_BT709_CAMERA, 0.05f), 0.05 / 4.5, 1e-7);
        near("PQ 0.5 (92.24 nits -> 0.9224)", dec(DRT_OETF_PQ_100, 0.5f), 0.92244, 1e-3);
        near("PQ of 100 nits -> 1.0", dec(DRT_OETF_PQ_100, 0.508078f), 1.0, 2e-3);
        near("HLG 0.75 (~203 nits -> 2.03)", dec(DRT_OETF_HLG_1000, 0.75f), 2.03, 0.1);
        report("display-referred inputs", Worst{});
        if (!good) std::printf("   display-referred decode FAILED\n");
        ok = ok && good;
    }

    /* 10. OpenDRT inverse (not in upstream). Two guarantees, tested separately:
           (a) display round trip: forward(inverse(pixel)) returns the pixel within
               half an 8-bit step for everything the display holds unclipped — this
               is what a plate needs;
           (b) scene round trip: for neutral and low-saturation colours (max/min
               ratio <= 2) the scene value itself comes back within 0.5 %. Saturated
               colours are gamut-compressed by the forward and can be many-to-one,
               so (b) is not asked of them. Clipped results are skipped. */
    {
        double worstDisp = 0.0, worstScene = 0.0; long n = 0, badDisp = 0, nScene = 0, badScene = 0, skipped = 0;
        float wIn[3] = {0, 0, 0}; int wLook = 0, wDisp = 0;
        const std::vector<float> g = {0.005f, 0.02f, 0.05f, 0.1f, 0.18f, 0.3f, 0.5f, 0.8f, 1.2f, 2.0f, 4.0f};
        for (int look : {0, 1, 4, 6})
            for (int d : {0, 2, 6, 8}) {
                drt::DrtSettings s; s.in_gamut = DRT_IN_AP1; s.in_oetf = DRT_OETF_LINEAR; s.look = look; s.display = d;
                s.tn_Lp = drt::kDisplays[d].default_Lp;
                const drt::DrtParams fwd = drt::drt_resolve(s);
                /* the inverse entry that names this display; drt_inverse_display sets the same display fields the forward has */
                drt::DrtParams inv = fwd;
                inv.in_oetf = (d == 0) ? DRT_OETF_INVERSE_REC1886 : (d == 2) ? DRT_OETF_INVERSE_P3
                            : (d == 6) ? DRT_OETF_INVERSE_PQ : DRT_OETF_INVERSE_PQ_P3;
                inv.working_gamut = DRT_IN_AP1;
                drt::drt_inverse_display(inv);
                inv = drt::drt_derive(inv);
                for (float r : g) for (float gg : g) for (float b : g) {
                    const float mx = std::max(r, std::max(gg, b)), mn = std::min(r, std::min(gg, b));
                    const float ratio = mx / mn;
                    if (ratio > 8.0f) continue;   /* keep to plausible colours */
                    const float in[3] = {r, gg, b};
                    drt::float3 p3 = drt::drt_vdot(drt::drt_matrix_xyz_to_p3d65, drt::drt_vdot(drt::drt_matrix_ap1_to_xyz, drt::make_float3(r, gg, b)));
                    drt::float3 lin = drt::drt_render_linear(fwd, p3);
                    const float top = (d == 6 || d == 8) ? 0.1f : 1.0f;   /* PQ: 1.0 = 10000 nits, peak 1000 = 0.1 */
                    if (lin.x < 0.0f || lin.y < 0.0f || lin.z < 0.0f || lin.x > 0.98f * top || lin.y > 0.98f * top || lin.z > 0.98f * top) { ++skipped; continue; }
                    float enc[3]; drt::apply(fwd, in, enc);
                    const drt::float3 back = drt::drt_input_transform(inv, drt::make_float3(enc[0], enc[1], enc[2]));
                    const float got[3] = {back.x, back.y, back.z};
                    float enc2[3]; drt::apply(fwd, got, enc2);
                    double eD = 0.0, eS = 0.0;
                    for (int i = 0; i < 3; ++i) {
                        eD = std::max(eD, double(std::fabs(enc2[i] - enc[i])));
                        eS = std::max(eS, std::fabs(double(got[i]) - in[i]) / std::max(0.02, double(std::fabs(in[i]))));
                    }
                    ++n;
                    if (eD > 1.0 / 255.0) { ++badDisp; if (eD > worstDisp) { wLook = look; wDisp = d; for (int i = 0; i < 3; ++i) wIn[i] = in[i]; } }
                    worstDisp = std::max(worstDisp, eD);
                    /* scene exactness asked only above the toe (below 0.01 the forward is flat to float precision) */
                    if (ratio <= 2.0f && mn >= 0.01f) { ++nScene; if (eS > 5e-3) ++badScene; worstScene = std::max(worstScene, eS); }
                }
            }
        /* Known state (2026-10-02, weighted residual): 0 of 5915 over one step,
           worst 0.14 steps, over Rec.1886, Display P3, Rec.2100 PQ and Dolby PQ P3-D65.
           Allowed: at most 0.2 % over one step and nothing over three; anything worse
           is a regression of the solver. */
        std::printf("%-34s cases %9ld  over one 8-bit step %4ld  worst %.2f steps  (skipped %ld clipped)\n",
                    "inverse: display round trip", n, badDisp, worstDisp * 255.0, skipped);
        if (badDisp) std::printf("   worst: look %d disp %d in %.4g %.4g %.4g\n", wLook, wDisp, wIn[0], wIn[1], wIn[2]);
        std::printf("%-34s cases %9ld  over 0.5%%          %4ld  worst rel %.2e\n", "inverse: scene, ratio <= 2", nScene, badScene, worstScene);
        const bool dispOk = badDisp * 500 <= n && worstDisp * 255.0 <= 3.0;
        if (!dispOk) std::printf("   inverse display round trip FAILED\n");
        ok = ok && dispOk && badScene == 0;

        /* (c) black: code 0 (full-range letterbox) and sub-black through every inverse
               entry must come back as black. PQ code 0 once decoded to -443 nits. */
        long badBlack = 0;
        for (int e = DRT_OETF_INVERSE_FIRST; e < DRT_OETF_COUNT; ++e)
            for (float code : {-0.01f, 0.0f}) {
                drt::DrtSettings s; s.in_gamut = DRT_IN_AP1; s.in_oetf = DRT_OETF_LINEAR; s.look = 0;
                s.display = drt::kInverseDisplayMap[e - DRT_OETF_INVERSE_FIRST];
                s.tn_Lp = drt::kDisplays[s.display].default_Lp;
                const drt::DrtParams fwd = drt::drt_resolve(s);
                drt::DrtParams inv = fwd; inv.in_oetf = e; inv.working_gamut = DRT_IN_AP1;
                drt::drt_inverse_display(inv); inv = drt::drt_derive(inv);
                const drt::float3 back = drt::drt_input_transform(inv, drt::make_float3(code, code, code));
                const float got[3] = {back.x, back.y, back.z};
                float enc2[3]; drt::apply(fwd, got, enc2);
                if (std::max(enc2[0], std::max(enc2[1], enc2[2])) > 1.0f / 255.0f) { ++badBlack; std::printf("   black via entry %d code %.2f -> %.4f %.4f %.4f\n", e, code, enc2[0], enc2[1], enc2[2]); }
            }
        std::printf("%-34s entries %7d  not black %4ld\n", "inverse: black", DRT_OETF_COUNT - DRT_OETF_INVERSE_FIRST, badBlack);
        ok = ok && badBlack == 0;
    }

    /* 10d. Un-tone-mapped view (not in upstream): the Input's conversion reversed,
            matching OCIO's view of that name in AE's ACES 1.3 config to 4 decimals
            (Rec.1886 and Rec.2100 PQ), a graphic round-tripping Input -> Output within
            a code value, and the look rows doing nothing. */
    {
        bool good = true; double worst = 0.0;
        const float sc[4] = {0.18f, 1.0f, 2.03f, 10.0f};
        const float ocioR[4] = {0.4894f, 1.0f, 1.3431f, 2.6102f}, ocioQ[4] = {0.3480f, 0.5081f, 0.5807f, 0.7518f};
        for (int d : {0, 6}) {
            drt::DrtSettings s; s.in_gamut = DRT_IN_AP1; s.in_oetf = DRT_OETF_LINEAR; s.look = 7; s.display = d; s.tn_Lp = drt::kDisplays[d].default_Lp;
            drt::DrtParams p = drt::drt_resolve(s); p.out_view = 1; p = drt::drt_derive(p);
            for (int i = 0; i < 4; ++i) {
                const float in[3] = {sc[i], sc[i], sc[i]}; float e[3]; drt::apply(p, in, e);
                worst = std::max(worst, double(std::fabs(e[0] - (d == 0 ? ocioR[i] : ocioQ[i]))));
            }
        }
        good = worst < 1e-3;
        drt::DrtSettings is; is.in_gamut = DRT_IN_REC709; is.in_oetf = DRT_OETF_POWER_2_2; is.look = 0; is.display = 0; is.tn_Lp = 100;
        drt::DrtParams inp = drt::drt_resolve(is); inp.working_gamut = DRT_IN_AP1; inp = drt::drt_derive(inp);
        drt::DrtSettings os = is; os.in_gamut = DRT_IN_AP1; os.in_oetf = DRT_OETF_LINEAR; os.display = 1;
        drt::DrtParams o = drt::drt_resolve(os); o.out_view = 1; o = drt::drt_derive(o);
        double rt = 0.0;
        for (float c : {0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f}) for (int ch = 0; ch < 4; ++ch) {
            const drt::float3 code = ch == 3 ? drt::make_float3(c, c, c) : drt::make_float3(ch == 0 ? c : 0, ch == 1 ? c : 0, ch == 2 ? c : 0);
            const drt::float3 cg = drt::drt_input_transform(inp, code);
            const float in[3] = {cg.x, cg.y, cg.z}; float e[3]; drt::apply(o, in, e);
            rt = std::max(rt, double(std::max(std::fabs(e[0] - code.x), std::max(std::fabs(e[1] - code.y), std::fabs(e[2] - code.z)))));
        }
        good = good && rt < 1.0 / 255.0;
        drt::DrtParams o2 = o; o2.tn_con = 2.0f; o2.tn_Lg = 20.0f; o2 = drt::drt_derive(o2);
        const float in[3] = {0.5f, 0.3f, 0.2f}; float a[3], b[3]; drt::apply(o, in, a); drt::apply(o2, in, b);
        const bool ignored = std::fabs(a[0] - b[0]) + std::fabs(a[1] - b[1]) + std::fabs(a[2] - b[2]) < 1e-6f;
        good = good && ignored;
        std::printf("%-34s vs OCIO worst %.2e  graphic round trip %.2e  look rows ignored %s  %s\n", "un-tone-mapped view", worst, rt, ignored ? "yes" : "NO", good ? "" : "FAILED");
        ok = ok && good;
    }

    /* 10e. Display "None - Linear / Working Gamut" (not in upstream): an identity in the
            Un-tone-mapped view; in the OpenDRT view the rendered light in the working gamut,
            which encoded as Display P3 2.2 equals the Display P3 preset's own output. */
    {
        drt::DrtSettings s; s.in_gamut = DRT_IN_AP1; s.in_oetf = DRT_OETF_LINEAR; s.look = 0; s.display = drt::drt_display_preset_index(DRT_DG_WORKING, DRT_EOTF_LINEAR); s.tn_Lp = 100;
        drt::DrtParams n = drt::drt_resolve(s); n.working_gamut = DRT_IN_AP1; n = drt::drt_derive(n);
        drt::DrtParams u = n; u.out_view = 1;
        drt::DrtSettings ds = s; ds.display = 2;
        drt::DrtParams p3 = drt::drt_resolve(ds); p3.tn_su = n.tn_su; p3 = drt::drt_derive(p3);   /* same surround as None (dark) */
        double idw = 0.0, eqw = 0.0;
        for (float r : {0.0f, 0.18f, 1.0f, 4.0f}) for (float g : {0.05f, 0.5f, 2.0f}) for (float b : {0.2f, 0.7f, 8.0f}) {
            const float in[3] = {r, g, b}; float e[3]; drt::apply(u, in, e);
            idw = std::max(idw, double(std::max(std::fabs(e[0] - r), std::max(std::fabs(e[1] - g), std::fabs(e[2] - b)))));
            float lin[3]; drt::apply(n, in, lin);   /* rendered, linear AP1 */
            /* encode that as Display P3 2.2 by hand: AP1 -> XYZ -> P3, 1/2.2 */
            const drt::float3 xyz = drt::drt_vdot(drt::drt_matrix_ap1_to_xyz, drt::make_float3(lin[0], lin[1], lin[2]));
            const drt::float3 pp = drt::drt_encode_eotf(drt::drt_vdot(drt::drt_matrix_xyz_to_p3d65, xyz), DRT_EOTF_POWER_2_2, 1);
            float ref[3]; drt::apply(p3, in, ref);
            eqw = std::max(eqw, double(std::max(std::fabs(pp.x - ref[0]), std::max(std::fabs(pp.y - ref[1]), std::fabs(pp.z - ref[2])))));
        }
        const bool good = idw < 1e-5 && eqw < 2e-3;
        std::printf("%-34s un-tone-mapped identity %.2e  rendered == Display P3 preset %.2e  %s\n", "display None", idw, eqw, good ? "" : "FAILED");
        ok = ok && good;
    }

    /* 10f. Output Mode (not in upstream): Render mode with a Rec.2100 PQ block equals the view
            set to Rec.2100 PQ directly, look rows shared; the ACES 2065-1 hand-off in the
            Un-tone-mapped rendering equals the Input's AP1 -> AP0 conversion. */
    {
        drt::DrtSettings vs; vs.in_gamut = DRT_IN_AP1; vs.in_oetf = DRT_OETF_LINEAR; vs.look = 1; vs.display = 2; vs.tn_Lp = 100;
        drt::DrtParams a = drt::drt_resolve(vs);                 /* view: Display P3 */
        a.out_mode = 1; a.rnd_gamut = DRT_DG_REC2020_P3LIM; a.rnd_eotf = DRT_EOTF_PQ; a.rnd_su = DRT_SURROUND_DARK; a.rnd_Lp = 1000; a.rnd_view = 0;
        drt::drt_output_mode(a); a = drt::drt_derive(a);
        drt::DrtSettings rs = vs; rs.display = 6; rs.tn_Lp = 1000;
        const drt::DrtParams b = drt::drt_resolve(rs);           /* the same, set as the view */
        double worst = 0.0;
        for (float r : {0.01f, 0.18f, 1.0f, 8.0f}) for (float g : {0.05f, 0.5f, 2.0f}) for (float bl : {0.2f, 0.7f, 4.0f}) {
            const float in[3] = {r, g, bl}; float ea[3], eb[3]; drt::apply(a, in, ea); drt::apply(b, in, eb);
            worst = std::max(worst, double(std::max(std::fabs(ea[0] - eb[0]), std::max(std::fabs(ea[1] - eb[1]), std::fabs(ea[2] - eb[2])))));
        }
        drt::DrtParams h = drt::drt_resolve(vs); h.display_gamut = DRT_DG_AP0; h.eotf = DRT_EOTF_LINEAR; h.out_view = 1; h.clamp_out = 0; h = drt::drt_derive(h);
        drt::DrtSettings is; is.in_gamut = DRT_IN_AP1; is.in_oetf = DRT_OETF_LINEAR; is.look = 0; is.display = 0; is.tn_Lp = 100;
        drt::DrtParams inp = drt::drt_resolve(is); inp.working_gamut = DRT_IN_AP0; inp = drt::drt_derive(inp);
        double hw = 0.0;
        for (float r : {0.01f, 0.18f, 1.0f, 8.0f}) for (float g : {0.05f, 0.5f, 2.0f}) for (float bl : {0.2f, 0.7f, 4.0f}) {
            const float in[3] = {r, g, bl}; float e[3]; drt::apply(h, in, e);
            const drt::float3 ref = drt::drt_input_transform(inp, drt::make_float3(r, g, bl));
            hw = std::max(hw, double(std::max(std::fabs(e[0] - ref.x), std::max(std::fabs(e[1] - ref.y), std::fabs(e[2] - ref.z)))));
        }
        double hw2 = 0.0;
        for (int dg : {DRT_DG_REC2020, DRT_DG_REC709}) {
            drt::DrtParams h2 = drt::drt_resolve(vs); h2.display_gamut = dg; h2.eotf = DRT_EOTF_LINEAR; h2.out_view = 1; h2.clamp_out = 0; h2 = drt::drt_derive(h2);
            drt::DrtParams in2 = drt::drt_resolve(is); in2.working_gamut = dg == DRT_DG_REC2020 ? DRT_IN_REC2020 : DRT_IN_REC709; in2 = drt::drt_derive(in2);
            for (float r : {0.01f, 0.18f, 1.0f, 8.0f}) for (float g : {0.05f, 0.5f, 2.0f}) for (float bl : {0.2f, 0.7f, 4.0f}) {
                const float in[3] = {r, g, bl}; float e[3]; drt::apply(h2, in, e);
                const drt::float3 ref = drt::drt_input_transform(in2, drt::make_float3(r, g, bl));
                hw2 = std::max(hw2, double(std::max(std::fabs(e[0] - ref.x), std::max(std::fabs(e[1] - ref.y), std::fabs(e[2] - ref.z)))));
            }
        }
        const bool good = worst < 1e-6 && hw < 1e-5 && hw2 < 1e-5;
        std::printf("%-34s render == view %.2e  ACES 2065-1 hand-off == Input %.2e  Rec.2020/709 %.2e  %s\n", "output mode", worst, hw, hw2, good ? "" : "FAILED");
        ok = ok && good;
    }

    /* 10h. macOS Fix (not in upstream): equals the macOS view of ocio/mincolor-viewport-shim.ocio
            (PyOpenColorIO 2.6, 2026-10-03). */
    {
        const float in[6][3]  = {{0.5f,0.5f,0.5f},{1,1,1},{1,0,0},{0,1,0},{0,0,1},{0.8f,0.4f,0.2f}};
        const float ref[6][3] = {{0.5f,0.5f,0.5f},{1,1,1},{0.9150f,0.2127f,0.1573f},{0.4558f,0.9848f,0.3032f},{0.0f,0.0f,0.9583f},{0.7474f,0.4210f,0.2479f}};
        double worst = 0.0;
        for (int i = 0; i < 6; ++i) {
            const drt::float3 o = drt::drt_macos_fix(drt::make_float3(in[i][0], in[i][1], in[i][2]));
            worst = std::max(worst, double(std::max(std::fabs(o.x - ref[i][0]), std::max(std::fabs(o.y - ref[i][1]), std::fabs(o.z - ref[i][2])))));
        }
        const bool good = worst < 2e-4;
        std::printf("%-34s vs viewport shim worst %.2e  %s\n", "macOS fix", worst, good ? "" : "FAILED");
        ok = ok && good;
    }

    /* 11. Grade (not in upstream). Identity at defaults; the neutral tonescale forward
           and inverse are twins; zone weights sum to one and move with exposure as
           nits say; the secondary mask is 1 inside its window and 0 well outside. */
    {
        bool good = true;
        drt::DrtSettings s; s.in_gamut = DRT_IN_AP1; s.in_oetf = DRT_OETF_LINEAR; s.look = 0; s.display = 0;
        const drt::DrtParams p = drt::drt_resolve(s);      /* in_gamut AP1 = the working gamut */
        drt::DrtGradeParams g = drt::drt_grade_defaults();
        g = drt::drt_grade_derive(g, p);

        /* identity */
        double worst = 0.0;
        for (float v : {0.001f, 0.05f, 0.18f, 1.0f, 7.0f}) {
            const drt::float3 in = drt::make_float3(v, v * 0.6f, v * 0.3f);
            const drt::float3 o = drt::drt_grade(g, p, in);
            worst = std::max({worst, std::fabs(double(o.x - in.x)) / v, std::fabs(double(o.y - in.y)) / v, std::fabs(double(o.z - in.z)) / v});
        }
        if (worst > 1e-6) { std::printf("   grade identity drifts: %.2e\n", worst); good = false; }

        /* neutral tonescale twins */
        double twin = 0.0;
        for (float t : {0.001f, 0.01f, 0.18f, 1.0f, 4.0f, 30.0f}) {
            const float d = drt::drt_tonescale_neutral_fwd(p, t);
            const float back = drt::drt_tonescale_neutral_inv(p, d);
            twin = std::max(twin, std::fabs(double(back) - t) / t);
        }
        if (twin > 1e-4) { std::printf("   neutral tonescale fwd/inv mismatch: %.2e\n", twin); good = false; }

        /* mid grey lands at Grey Luminance nits (a reference fact; the Grade itself no longer uses nits) */
        const float greyNits = drt::drt_display_nits(p, drt::drt_tonescale_neutral_fwd(p, drt::drt_grade_norm(p, drt::make_float3(0.18f, 0.18f, 0.18f))));
        /* the Standard look's toe and offset put 0.18 at ~11.2 nits, not exactly Grey Luminance */
        if (std::fabs(greyNits - 10.0f) > 2.0f) { std::printf("   mid grey at %.2f nits, expected ~10\n", greyNits); good = false; }

        /* zone exposure: +1 stop on Light only; grey 0.18 sits on the Shadow/Light boundary, inside Light's
           band, so it moves a full stop; 0.001 (-7.5 st) and 8.0 (+5.5 st) are past Light's 2-stop feather */
        drt::DrtGradeParams gz = g; gz.z3_exposure = 1.0f; gz = drt::drt_grade_derive(gz, p);
        const float dark = drt::drt_grade(gz, p, drt::make_float3(0.001f, 0.001f, 0.001f)).y / 0.001f;
        const float mid  = drt::drt_grade(gz, p, drt::make_float3(0.18f, 0.18f, 0.18f)).y / 0.18f;
        const float hi   = drt::drt_grade(gz, p, drt::make_float3(8.0f, 8.0f, 8.0f)).y / 8.0f;
        if (std::fabs(dark - 1.0f) > 1e-3f || std::fabs(hi - 1.0f) > 1e-3f || mid < 1.9f || mid > 2.1f) {
            std::printf("   zone Light +1 stop: dark x%.3f mid x%.3f high x%.3f (want 1, ~2, 1)\n", dark, mid, hi); good = false;
        }

        /* secondary: a red window keeps red (mask 1) and drops a cyan (mask 0) */
        drt::DrtGradeParams gs = g; gs.sec_enable = 1; gs.sec_show_mask = 1; gs.sec_hue = 0.0f; gs.sec_hue_width = 60.0f;
        gs.sec_sat_min = 0.2f; gs.sec_sat_max = 2.0f; gs.sec_lev_min = -10.0f; gs.sec_lev_max = 10.0f; gs = drt::drt_grade_derive(gs, p);
        const float mRed  = drt::drt_grade(gs, p, drt::make_float3(0.5f, 0.05f, 0.05f)).x;
        const float mCyan = drt::drt_grade(gs, p, drt::make_float3(0.05f, 0.5f, 0.5f)).x;
        const float mGrey = drt::drt_grade(gs, p, drt::make_float3(0.18f, 0.18f, 0.18f)).x;
        if (mRed < 0.95f || mCyan > 0.05f || mGrey > 0.05f) { std::printf("   secondary mask red %.3f cyan %.3f grey %.3f (want 1, 0, 0)\n", mRed, mCyan, mGrey); good = false; }

        /* order of operations (2026-10-03): selection is measured on the source.
           (a) Light +1 stop with global Exposure +2: 0.18 is still Light (x2 on top of x4 = x8),
               0.001 is still outside it (x4 only) -- before, +2 pushed 0.18 into Highlight;
           (b) Contrast 2 with Light +1: contrast pivots on grey so 0.18 stays 0.18, then the zone
               doubles it (0.36) -- contrast after the zones would have given 0.72;
           (c) a red the secondary keys with a +-2 stop level window stays keyed under Exposure -5. */
        {
            drt::DrtGradeParams ga = g; ga.z3_exposure = 1.0f; ga.exposure = 2.0f; ga = drt::drt_grade_derive(ga, p);
            const float a1 = drt::drt_grade(ga, p, drt::make_float3(0.18f, 0.18f, 0.18f)).y / 0.18f;
            const float a2 = drt::drt_grade(ga, p, drt::make_float3(0.001f, 0.001f, 0.001f)).y / 0.001f;
            drt::DrtGradeParams gb = g; gb.z3_exposure = 1.0f; gb.contrast = 2.0f; gb = drt::drt_grade_derive(gb, p);
            const float b1 = drt::drt_grade(gb, p, drt::make_float3(0.18f, 0.18f, 0.18f)).y / 0.18f;
            drt::DrtGradeParams gc = gs; gc.sec_lev_min = -2.0f; gc.sec_lev_max = 2.0f; gc.exposure = -5.0f; gc = drt::drt_grade_derive(gc, p);
            const float c1 = drt::drt_grade(gc, p, drt::make_float3(0.5f, 0.05f, 0.05f)).x;
            if (std::fabs(a1 - 8.0f) > 0.1f || std::fabs(a2 - 4.0f) > 0.01f || std::fabs(b1 - 2.0f) > 0.05f || c1 < 0.95f) {
                std::printf("   order: Light+1 & Exp+2 -> grey x%.2f (8) dark x%.2f (4); Contrast 2 & Light+1 -> grey x%.2f (2); key under Exp-5 mask %.2f (1)\n", a1, a2, b1, c1);
                good = false;
            }
        }

        report("grade", Worst{});
        if (!good) std::printf("   grade checks FAILED\n");
        ok = ok && good;
    }

    std::printf("%s (tolerance %.1e)\n", ok ? "PASS" : "FAIL", kTolerance);
    return ok ? 0 : 1;
}
