/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — Metal kernels. Embedded AFTER opendrt_shim_msl.h,
 * opendrt_params.h and opendrt_kernel.h (see ae/CMakeLists.txt), so drt_transform
 * and drt_input_transform are in scope. The `#include <metal_stdlib>` line lives
 * in the wrapper prefix the CMake embed puts first.
 *
 * AE's GPU worlds are MTLBuffers of float4 in BGRA order (x = B, y = G, z = R,
 * w = A), premultiplied. Pitch is in pixels (rowbytes / 16). Colour is transformed
 * on straight RGB: unpremultiply, transform, premultiply, alpha untouched.
 */

struct DrtAeHeader {
    int srcPitch;
    int dstPitch;
    int width;
    int height;
};

kernel void drt_output_kernel(const device float4     *src [[buffer(0)]],
                              device float4           *dst [[buffer(1)]],
                              constant DrtParams      &p   [[buffer(2)]],
                              constant DrtAeHeader    &h   [[buffer(3)]],
                              uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    if (a > 0.0f && a < 1.0f) rgb = rgb / a;
    rgb = drt_transform(p, rgb);
    if (a > 0.0f && a < 1.0f) rgb = rgb * a;
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}

kernel void drt_grade_kernel(const device float4     *src [[buffer(0)]],
                             device float4           *dst [[buffer(1)]],
                             constant DrtParams      &p   [[buffer(2)]],
                             constant DrtAeHeader    &h   [[buffer(3)]],
                             constant DrtGradeParams &g   [[buffer(4)]],
                             uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    if (a > 0.0f && a < 1.0f) rgb = rgb / a;
    rgb = drt_grade(g, p, rgb);
    if (a > 0.0f && a < 1.0f) rgb = rgb * a;
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}

kernel void drt_macfix_kernel(const device float4     *src [[buffer(0)]],
                              device float4           *dst [[buffer(1)]],
                              constant DrtParams      &p   [[buffer(2)]],
                              constant DrtAeHeader    &h   [[buffer(3)]],
                              uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    if (a > 0.0f && a < 1.0f) rgb = rgb / a;
    rgb = drt_macos_fix(rgb);
    if (a > 0.0f && a < 1.0f) rgb = rgb * a;
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}

kernel void drt_knee_kernel(const device float4     *src [[buffer(0)]],
                            device float4           *dst [[buffer(1)]],
                            constant DrtParams      &p   [[buffer(2)]],
                            constant DrtAeHeader    &h   [[buffer(3)]],
                            uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    if (a > 0.0f && a < 1.0f) rgb = rgb / a;
    rgb = drt_knee(p, rgb);
    if (a > 0.0f && a < 1.0f) rgb = rgb * a;
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}

kernel void drt_agx_kernel(const device float4     *src [[buffer(0)]],
                           device float4           *dst [[buffer(1)]],
                           constant DrtParams      &p   [[buffer(2)]],
                           constant DrtAeHeader    &h   [[buffer(3)]],
                           constant DrtAgxParams   &g   [[buffer(4)]],
                           uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    if (a > 0.0f && a < 1.0f) rgb = rgb / a;
    rgb = drt_agx(g, rgb);
    if (a > 0.0f && a < 1.0f) rgb = rgb * a;
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}

kernel void drt_input_kernel(const device float4     *src [[buffer(0)]],
                             device float4           *dst [[buffer(1)]],
                             constant DrtParams      &p   [[buffer(2)]],
                             constant DrtAeHeader    &h   [[buffer(3)]],
                             uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    if (a > 0.0f && a < 1.0f) rgb = rgb / a;
    rgb = drt_input_transform(p, rgb);
    if (a > 0.0f && a < 1.0f) rgb = rgb * a;
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}
