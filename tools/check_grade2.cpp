/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow.
 */
/* check_grade2 — the Grade 2 core against the reference grading application.
 *
 * tools/grade2_ref.txt holds sampled reference renders of a grey / warm ramp and a
 * 16-patch chart, one control (or two) pushed per render. This runs the same pixels through
 * drt_grade2() with the same settings and reports the worst difference per render.
 *
 * Error measure, per sample: max over channels of |ours - the reference|, over the larger of
 * the reference's biggest channel and 0.002 (so a near-black sample does not turn rounding into
 * a percentage). Each family of controls has the limit its model is known to hold:
 * exact formulas are held tight, the fitted colour matrices and the saturation lerp loose
 * (see the notes in core/mincolor_grade2.h). Two things the reference does are left out of the
 * comparison because the core does not model them: its ceiling near 16 stops over grey, and
 * the balance fading out below about -6 stops. A limit that fails means the core moved away
 * from the measurements, not that the reference changed.
 *
 *   check_grade2 <path to grade2_ref.txt> [-v]
 */
#include "opendrt.h"
#include "opendrt_presets.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

const float kChart[16][3] = {
    {.18f, .18f, .18f}, {.4f, .1f, .1f}, {.1f, .4f, .1f}, {.1f, .1f, .4f}, {.1f, .4f, .4f}, {.4f, .1f, .4f}, {.4f, .4f, .1f},
    {.3f, 0, 0}, {0, .3f, 0}, {0, 0, .3f}, {.27f, .18f, .09f}, {.35f, .2f, .13f}, {.02f, .02f, .02f}, {2, 2, 2}, {.5f, .5f, .2f}, {.8f, .2f, .2f}};

typedef float drt::DrtGrade2Params::*Field;
#define F(n) {#n, &drt::DrtGrade2Params::n}
const std::map<std::string, Field> kFields = {
    F(lift_r), F(lift_g), F(lift_b), F(gamma_r), F(gamma_g), F(gamma_b), F(gain_r), F(gain_g), F(gain_b), F(offset_r), F(offset_g), F(offset_b),
    F(z0_exp), F(z0_sat), F(z0_x), F(z0_y), F(z0_range), F(z0_falloff), F(z1_exp), F(z1_sat), F(z1_x), F(z1_y), F(z1_range), F(z1_falloff),
    F(z2_exp), F(z2_sat), F(z2_x), F(z2_y), F(z2_range), F(z2_falloff), F(z3_exp), F(z3_sat), F(z3_x), F(z3_y), F(z3_range), F(z3_falloff),
    F(z4_exp), F(z4_sat), F(z4_x), F(z4_y), F(z4_range), F(z4_falloff), F(z5_exp), F(z5_sat), F(z5_x), F(z5_y), F(z5_range), F(z5_falloff),
    F(g_exp), F(g_sat), F(g_x), F(g_y), F(temp), F(tint), F(hue), F(contrast), F(pivot), F(blk)};
#undef F

/* The limit for a render, from which controls it sets. */
double limitFor(const std::string &settings, const std::string &image)
{
    auto has = [&](const char *k) { return settings.find(k) != std::string::npos; };
    const bool chart = image == "chart";
    if (has("hue=")) return 0.07;
    if (has("_sat=")) return chart ? 0.30 : 0.06;                 /* the lerp vs the reference's perceptual saturation */
    if (has("_x=") || has("_y=")) return chart ? 0.14 : 0.06;     /* fitted 3x3; X -0.1 is the worst */
    if (has("tint=")) return 0.05;
    if (has("temp=")) return 0.06;                                /* fitted at +1000; -1000 is the worse side */
    if (has("contrast=")) return 0.012;
    if (has("blk=")) return 0.01;
    if (has("_exp=")) return 0.012;                               /* weights within 0.007, the reference's own Exp +-0.006 st */
    return 0.004;                                                 /* Primaries alone: exact but for float rounding in the toe */
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: check_grade2 <grade2_ref.txt> [-v]\n"); return 2; }
    const bool verbose = argc > 2 && !std::strcmp(argv[2], "-v");
    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "check_grade2: cannot read %s\n", argv[1]); return 2; }

    /* the reference was rendered linear Rec.709 in and out: that is the working gamut here */
    drt::DrtSettings s; s.in_gamut = DRT_IN_REC709; s.in_oetf = DRT_OETF_LINEAR; s.look = 0; s.display = 0;
    drt::DrtParams p0 = drt::drt_resolve(s);
    p0.working_gamut = DRT_IN_REC709;
    const drt::DrtParams p = drt::drt_derive(p0);

    bool ok = true;
    int count = 0;

    /* at rest the effect is an exact pass-through */
    {
        const drt::DrtGrade2Params g = drt::drt_grade2_derive(drt::drt_grade2_defaults(), p);
        const drt::float3 o = drt::drt_grade2(g, drt::make_float3(0.37f, -0.02f, 5.5f));
        const bool good = o.x == 0.37f && o.y == -0.02f && o.z == 5.5f;
        std::printf("%-16s %-5s %-40s %s\n", "defaults", "", "exact pass-through", good ? "" : "FAILED");
        ok = ok && good;
    }

    /* the secondary: off it changes nothing whatever the key; one wheel at +1 stop through a
       half key is half a stop; three wheels pushed alike are one wheel (their shares sum to one) */
    {
        drt::DrtGrade2Params g = drt::drt_grade2_defaults();
        g.g_exp = 0.3f; g.s1_exp = 1.0f;
        const drt::DrtGrade2Params off = drt::drt_grade2_derive(g, p);
        g.sec_on = 1;
        const drt::DrtGrade2Params one = drt::drt_grade2_derive(g, p);
        g.sec_three = 1.0f; g.s0_exp = g.s2_exp = 1.0f; g.s0_x = g.s1_x = g.s2_x = 0.03f; g.s0_sat = g.s1_sat = g.s2_sat = 1.2f;
        const drt::DrtGrade2Params three = drt::drt_grade2_derive(g, p);
        g.sec_three = 0.0f;
        const drt::DrtGrade2Params same = drt::drt_grade2_derive(g, p);
        double w1 = 0.0, w2 = 0.0, w3 = 0.0;
        for (float v : {0.002f, 0.03f, 0.18f, 0.9f, 6.0f}) {
            const drt::float3 px = drt::make_float3(v * 1.3f, v, v * 0.6f);
            const drt::float3 a = drt::drt_grade2_keyed(off, px, 1.0f), b = drt::drt_grade2(off, px);
            w1 = std::max({w1, std::fabs(double(a.x - b.x)), std::fabs(double(a.y - b.y)), std::fabs(double(a.z - b.z))});
            const drt::float3 h = drt::drt_grade2_keyed(one, px, 0.5f);
            w2 = std::max(w2, std::fabs(double(h.y / b.y) - std::sqrt(2.0)));
            const drt::float3 t = drt::drt_grade2_keyed(three, px, 1.0f), u = drt::drt_grade2_keyed(same, px, 1.0f);
            w3 = std::max({w3, std::fabs(double(t.x - u.x)) / v, std::fabs(double(t.y - u.y)) / v, std::fabs(double(t.z - u.z)) / v});
        }
        const bool good = w1 == 0.0 && w2 < 1e-5 && w3 < 2e-5;
        std::printf("%-16s %-5s %-40s off %.1e  half key %.1e  three = one %.1e  %s\n", "secondary", "", "", w1, w2, w3, good ? "" : "FAILED");
        ok = ok && good;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("exp ", 0) != 0) continue;
        std::istringstream hs(line.substr(4));
        std::string name, image, kv, settings;
        hs >> name >> image;
        drt::DrtGrade2Params g = drt::drt_grade2_defaults();
        while (hs >> kv) {
            const size_t eq = kv.find('=');
            const auto it = kFields.find(kv.substr(0, eq));
            if (eq == std::string::npos || it == kFields.end()) { std::fprintf(stderr, "check_grade2: unknown control '%s' in %s\n", kv.c_str(), name.c_str()); return 2; }
            g.*(it->second) = std::stof(kv.substr(eq + 1));
            settings += (settings.empty() ? "" : " ") + kv;
        }
        g = drt::drt_grade2_derive(g, p);

        std::string data;
        if (!std::getline(in, data)) break;
        std::istringstream ds(data);
        std::vector<float> ref;
        for (float v; ds >> v;) ref.push_back(v);

        std::vector<drt::float3> src;
        if (image == "ramp") {
            for (int row = 0; row < 2; ++row)
                for (int x = 40; x <= 1880; x += 40) {
                    const float v = 0.18f * std::pow(2.0f, -8.0f + 16.0f * float(x) / 1919.0f);
                    src.push_back(row == 0 ? drt::make_float3(v, v, v) : drt::make_float3(v * 1.5f, v, v * 0.5f));
                }
        } else {
            for (const auto &c : kChart) src.push_back(drt::make_float3(c[0], c[1], c[2]));
        }
        if (ref.size() != src.size() * 3) { std::fprintf(stderr, "check_grade2: %s has %zu values, expected %zu\n", name.c_str(), ref.size(), src.size() * 3); return 2; }

        const bool ball = settings.find("_x=") != std::string::npos || settings.find("_y=") != std::string::npos;
        double worst = 0.0;
        size_t at = 0;
        for (size_t i = 0; i < src.size(); ++i) {
            const drt::float3 o = drt::drt_grade2(g, src[i]);
            const float *r = &ref[i * 3];
            const double scale = std::max({double(std::fabs(r[0])), double(std::fabs(r[1])), double(std::fabs(r[2])), 0.002});
            if (scale > 5000.0) continue;                                  /* the reference's ceiling (near 16 stops over grey): not modelled */
            if (ball && src[i].y < 0.003f) continue;                       /* the reference fades the balance out in the deepest blacks: not modelled */
            const double e = std::max({std::fabs(double(o.x) - r[0]), std::fabs(double(o.y) - r[1]), std::fabs(double(o.z) - r[2])}) / scale;
            if (e > worst) { worst = e; at = i; }
        }
        const double limit = limitFor(settings, image);
        const bool good = worst <= limit;
        if (verbose || !good)
            std::printf("%-16s %-5s %-40.40s worst %.4f (limit %.3f, sample %zu)  %s\n", name.c_str(), image.c_str(), settings.c_str(), worst, limit, at, good ? "" : "FAILED");
        ok = ok && good;
        ++count;
    }
    std::printf("check_grade2: %d reference renders compared, %s\n", count, ok ? "all within their limits" : "FAILED");
    return ok ? 0 : 1;
}
