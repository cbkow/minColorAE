/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow.
 * Derived from OpenDRT v1.1.0 by Jed Smith (https://github.com/jedypod/open-display-transform), GPLv3;
 * modified 2026-09-30 to 2026-10-04, see CHANGES-FROM-OPENDRT.md. Not affiliated with or endorsed by OpenDRT.
 */
/* See dctl_ref.h. The DCTL is textually included inside dctl_ref::dctl so its
 * globals (in_gamut, look_preset, ...) and its transform() cannot collide with the
 * core, which lives in drt::. Both namespaces get their own copy of the C++ shim.
 */
#include <cmath>

#include "dctl_ref.h"

namespace dctl_ref {
namespace dctl {

#include "opendrt_shim_cpp.h"
#include "dctl_shim.h"
#include "OpenDRT_v1.1.0.dctl"

} // namespace dctl

void transform(const Settings &s, const float in[3], float out[3])
{
    dctl::in_gamut = s.in_gamut;
    dctl::in_oetf = s.in_oetf;
    dctl::tn_Lp = s.tn_Lp;
    dctl::tn_gb = s.tn_gb;
    dctl::pt_hdr = s.pt_hdr;
    dctl::tn_Lg = s.tn_Lg;
    dctl::crv_enable = 0;
    dctl::look_preset = s.look_preset;
    dctl::tonescale_preset = s.tonescale_preset;
    dctl::_cwp = s.cwp;
    dctl::_cwp_lm = s.cwp_lm;
    dctl::display_encoding_preset = s.display_encoding_preset;

    const dctl::float3 r = dctl::transform(1, 1, 0, 0, in[0], in[1], in[2]);
    out[0] = r.x;
    out[1] = r.y;
    out[2] = r.z;
}

} // namespace dctl_ref
