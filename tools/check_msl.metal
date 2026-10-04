/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* Compile check for the MSL dialect: the shared core inside a Metal compute
 * kernel, the same way another host and the AE Metal path will wrap it. Built by the
 * check_msl target (xcrun metal -c). Never run; compiling is the test.
 */
#include <metal_stdlib>
using namespace metal;

#include "opendrt_shim_msl.h"
#include "opendrt_params.h"
#include "opendrt_kernel.h"
#include "opendrt_grade.h"
#include "mincolor_knee.h"

kernel void drt_check_grade(texture2d<float, access::read>  src [[texture(0)]],
                            texture2d<float, access::write> dst [[texture(1)]],
                            constant DrtParams &p               [[buffer(0)]],
                            constant DrtGradeParams &g          [[buffer(1)]],
                            uint2 gid                           [[thread_position_in_grid]])
{
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    const float4 c = src.read(gid);
    dst.write(float4(drt_grade(g, p, c.rgb), c.a), gid);
}

kernel void drt_check_apply(texture2d<float, access::read>  src [[texture(0)]],
                            texture2d<float, access::write> dst [[texture(1)]],
                            constant DrtParams &p               [[buffer(0)]],
                            uint2 gid                           [[thread_position_in_grid]])
{
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    const float4 c = src.read(gid);
    const float3 r = drt_transform(p, c.rgb);
    dst.write(float4(r, c.a), gid);
}

kernel void drt_check_knee(texture2d<float, access::read>  src [[texture(0)]],
                           texture2d<float, access::write> dst [[texture(1)]],
                           constant DrtParams &p               [[buffer(0)]],
                           uint2 gid                           [[thread_position_in_grid]])
{
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    const float4 c = src.read(gid);
    dst.write(float4(drt_knee(p, c.rgb), c.a), gid);
}

kernel void drt_check_input(texture2d<float, access::read>  src [[texture(0)]],
                            texture2d<float, access::write> dst [[texture(1)]],
                            constant DrtParams &p               [[buffer(0)]],
                            uint2 gid                           [[thread_position_in_grid]])
{
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    const float4 c = src.read(gid);
    dst.write(float4(drt_input_transform(p, c.rgb), c.a), gid);
}

kernel void drt_check_inverse(texture2d<float, access::read>  src [[texture(0)]],
                              texture2d<float, access::write> dst [[texture(1)]],
                              constant DrtParams &p               [[buffer(0)]],
                              uint2 gid                           [[thread_position_in_grid]])
{
    if (gid.x >= dst.get_width() || gid.y >= dst.get_height()) return;
    const float4 c = src.read(gid);
    dst.write(float4(drt_inverse_transform(p, c.rgb), c.a), gid);
}

/* drt_derive() is host-side in practice; compiling it here proves the derive
   code is in the neutral dialect too. */
kernel void drt_check_derive(device DrtParams *io [[buffer(0)]],
                             uint gid              [[thread_position_in_grid]])
{
    if (gid != 0) return;
    *io = drt_derive(*io);
}
