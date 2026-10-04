/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* dump_presets — writes presets/drt_presets.json from the C++ tables.
 *
 * The C++ tables in core/opendrt_presets.cpp are canonical (they are what the probe
 * checks against the DCTL). The JSON is the interchange copy for hosts that load
 * presets at run time (the AE effect's user presets, a host's preset column) and
 * for people reading them. Regenerate after any table change:
 *   build/dump_presets presets/drt_presets.json
 */
#include <cstdio>

#include "opendrt.h"

namespace {

void f(std::FILE *o, const char *k, float v, bool last = false)
{
    /* %.8g: enough digits to round-trip the float for these decimal-typed presets, without %.9g's 1.65999997 noise */
    std::fprintf(o, "      \"%s\": %.8g%s\n", k, v, last ? "" : ",");
}
void i(std::FILE *o, const char *k, int v, bool last = false)
{
    std::fprintf(o, "      \"%s\": %d%s\n", k, v, last ? "" : ",");
}

} // namespace

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "drt_presets.json";
    std::FILE *o = std::fopen(path, "w");
    if (!o) { std::perror(path); return 1; }

    std::fprintf(o, "{\n  \"opendrt_version\": \"1.1.0\",\n  \"params_scalars\": %d,\n", DRT_PARAMS_SCALARS);

    std::fprintf(o, "  \"input_gamuts\": [");
    for (int n = 0; n < DRT_IN_GAMUT_COUNT; ++n) std::fprintf(o, "%s\"%s\"", n ? ", " : "", drt::kInGamutNames[n]);
    std::fprintf(o, "],\n  \"input_transfer_functions\": [");
    for (int n = 0; n < DRT_OETF_COUNT; ++n) std::fprintf(o, "%s\"%s\"", n ? ", " : "", drt::kInOetfNames[n]);
    std::fprintf(o, "],\n  \"creative_whites\": [");
    for (int n = 0; n < drt::kCwpCount; ++n) std::fprintf(o, "%s\"%s\"", n ? ", " : "", drt::kCwpNames[n]);
    std::fprintf(o, "],\n");

    std::fprintf(o, "  \"looks\": [\n");
    for (int n = 0; n < drt::kLookCount; ++n) {
        const drt::DrtLook &l = drt::kLooks[n];
        std::fprintf(o, "    {\n      \"name\": \"%s\",\n", l.name);
#define DRT_X_F(k) f(o, #k, l.k);
#define DRT_X_I(k) i(o, #k, l.k);
        DRT_LOOK_FIELDS(DRT_X_F, DRT_X_I)
#undef DRT_X_F
#undef DRT_X_I
        std::fprintf(o, "      \"_\": 0\n    }%s\n", n + 1 < drt::kLookCount ? "," : "");
    }
    std::fprintf(o, "  ],\n  \"tonescales\": [\n");
    for (int n = 0; n < drt::kTonescaleCount; ++n) {
        const drt::DrtTonescale &t = drt::kTonescales[n];
        std::fprintf(o, "    {\n      \"name\": \"%s\",\n", t.name);
#define DRT_X_F(k) f(o, #k, t.k);
#define DRT_X_I(k) i(o, #k, t.k);
        DRT_TONESCALE_FIELDS(DRT_X_F, DRT_X_I)
#undef DRT_X_F
#undef DRT_X_I
        std::fprintf(o, "      \"_\": 0\n    }%s\n", n + 1 < drt::kTonescaleCount ? "," : "");
    }
    std::fprintf(o, "  ],\n  \"displays\": [\n");
    for (int n = 0; n < drt::kDisplayCount; ++n) {
        const drt::DrtDisplay &d = drt::kDisplays[n];
        std::fprintf(o, "    { \"name\": \"%s\", \"eotf\": %d, \"display_gamut\": %d, \"tn_su\": %d, \"default_Lp\": %g }%s\n",
                     d.name, d.eotf, d.display_gamut, d.tn_su, d.default_Lp, n + 1 < drt::kDisplayCount ? "," : "");
    }
    std::fprintf(o, "  ]\n}\n");
    std::fclose(o);
    std::printf("wrote %s\n", path);
    return 0;
}
