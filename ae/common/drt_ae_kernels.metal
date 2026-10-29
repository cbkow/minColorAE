/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — Metal kernels. Embedded AFTER opendrt_shim_msl.h,
 * opendrt_params.h and opendrt_kernel.h (see ae/CMakeLists.txt), so drt_transform
 * and drt_input_transform are in scope. The `#include <metal_stdlib>` line lives
 * in the wrapper prefix the CMake embed puts first.
 *
 * AE's GPU worlds are MTLBuffers of float4 in BGRA order (x = B, y = G, z = R,
 * w = A), STRAIGHT alpha (AE hands effects unmatted pixels; see applyPixel in
 * drt_ae_effect.cpp). Pitch is in pixels (rowbytes / 16). RGB is transformed as it
 * is; alpha passes through.
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
    rgb = drt_transform(p, rgb);
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
    rgb = drt_grade(g, p, rgb);
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}

/* The Grade's key blur (0.1.4): the key of every pixel into a float buffer, a separable
   Gaussian over it (twice, across then down), then the grade with the blurred key. */
struct DrtBlurHeader {
    int width;
    int height;
    int radius;   /* taps either side */
    int dir;      /* 0 across, 1 down */
};

kernel void drt_grade_key_kernel(const device float4     *src [[buffer(0)]],
                                 device float            *key [[buffer(1)]],
                                 constant DrtParams      &p   [[buffer(2)]],
                                 constant DrtAeHeader    &h   [[buffer(3)]],
                                 constant DrtGradeParams &g   [[buffer(4)]],
                                 uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    float4 px = src[xy.y * h.srcPitch + xy.x];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    key[xy.y * uint(h.width) + xy.x] = drt_grade_key(g, p, rgb);
}

kernel void drt_key_blur_kernel(const device float      *src [[buffer(0)]],
                                device float            *dst [[buffer(1)]],
                                constant DrtBlurHeader  &b   [[buffer(2)]],
                                uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(b.width) || xy.y >= uint(b.height)) return;
    const int x = int(xy.x), y = int(xy.y);
    float s = 0.0f, ws = 0.0f;
    for (int k = -b.radius; k <= b.radius; ++k) {
        const float w = drt_key_blur_weight(float(k), float(b.radius));
        const int sx = b.dir == 0 ? clamp(x + k, 0, b.width - 1) : x;
        const int sy = b.dir == 0 ? y : clamp(y + k, 0, b.height - 1);
        s += w * src[sy * b.width + sx];
        ws += w;
    }
    dst[y * b.width + x] = s / ws;
}

/* h.width / height are the OUTPUT's here; it sits at (offX, offY) inside the input and its key */
struct DrtKeyedHeader {
    int keyWidth;
    int offX;
    int offY;
    int pad;
};

kernel void drt_grade_keyed_kernel(const device float4     *src [[buffer(0)]],
                                   device float4           *dst [[buffer(1)]],
                                   constant DrtParams      &p   [[buffer(2)]],
                                   constant DrtAeHeader    &h   [[buffer(3)]],
                                   constant DrtGradeParams &g   [[buffer(4)]],
                                   const device float      *key [[buffer(5)]],
                                   constant DrtKeyedHeader &k   [[buffer(6)]],
                                   uint2 xy                     [[thread_position_in_grid]])
{
    if (xy.x >= uint(h.width) || xy.y >= uint(h.height)) return;
    const uint ix = xy.x + uint(k.offX), iy = xy.y + uint(k.offY);
    float4 px = src[iy * h.srcPitch + ix];
    const float a = px.w;
    float3 rgb = float3(px.z, px.y, px.x);
    rgb = drt_grade_keyed(g, p, rgb, key[iy * uint(k.keyWidth) + ix]);
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
    rgb = drt_macos_fix(rgb);
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
    rgb = drt_knee(p, rgb);
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
    rgb = drt_agx(g, rgb);
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
    rgb = drt_input_transform(p, rgb);
    dst[xy.y * h.dstPitch + xy.x] = float4(rgb.z, rgb.y, rgb.x, a);
}
