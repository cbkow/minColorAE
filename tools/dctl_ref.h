/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow.
 * Derived from OpenDRT v1.1.0 by Jed Smith (https://github.com/jedypod/open-display-transform), GPLv3;
 * modified 2026-09-30 to 2026-10-04, see CHANGES-FROM-OPENDRT.md. Not affiliated with or endorsed by OpenDRT.
 */
/* The reference: upstream OpenDRT_v1.1.0.dctl compiled as C++, unmodified, driven
 * through its own preset-mode UI variables. tools/probe_core compares the core
 * against this. If upstream changes, drop the new DCTL into upstream/ and the
 * probe tells you what the port has to catch up on.
 */
#pragma once

namespace dctl_ref {

/* The DCTL's preset-mode controls, by their DCTL names and indices. */
struct Settings {
    int   in_gamut = 14;                /* DCTL default: DaVinci Wide Gamut */
    int   in_oetf = 1;                  /* DCTL default: DaVinci Intermediate */
    float tn_Lp = 100.0f;
    float tn_gb = 0.13f;
    float pt_hdr = 0.5f;
    float tn_Lg = 10.0f;
    int   look_preset = 0;              /* 0..7 (7 = Base; the DCTL code handles it even though the combo hides it) */
    int   tonescale_preset = 0;         /* 0 = use look, 1..13 */
    int   cwp = 0;                      /* DCTL `_cwp`: 0 = use look, 1..6 = D93..D50 */
    float cwp_lm = 0.25f;               /* DCTL `_cwp_lm` */
    int   display_encoding_preset = 0;  /* 0..8 */
};

/* One pixel through the DCTL, overlay off. */
void transform(const Settings &s, const float in[3], float out[3]);

} // namespace dctl_ref
