/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — CUDA kernels (Windows), the twin of drt_ae_kernels.metal.
 *
 * One kernel for every role (the role is an argument), so the effects and the
 * standalone GPU-vs-CPU check (tools/check_cuda.cpp) run the same code. The core
 * compiles here at file scope through opendrt_shim_cuda.h; the host passes its
 * blocks (namespace drt, same layout) as raw bytes, copied into the kernel's
 * parameters by value (DrtParams 496 + DrtGradeParams 328 + DrtAgxParams 336 bytes).
 *
 * AE's CUDA worlds are float4 BGRA (x = B, y = G, z = R, w = A), premultiplied,
 * pitch in pixels. Colour is transformed on straight RGB, alpha untouched.
 */
#include <cuda_runtime.h>
#include <cstring>

#include "opendrt_shim_cuda.h"
#include "opendrt_params.h"
#include "opendrt_kernel.h"
#include "opendrt_grade.h"
#include "mincolor_knee.h"
#include "mincolor_agx.h"

#include "drt_ae_cuda.h"

static_assert(sizeof(DrtParams) == DRT_PARAMS_SCALARS * 4, "DrtParams layout");
static_assert(sizeof(DrtGradeParams) == DRT_GRADE_SCALARS * 4, "DrtGradeParams layout");
static_assert(sizeof(DrtAgxParams) == DRT_AGX_SCALARS * 4, "DrtAgxParams layout");

__device__ __forceinline__ float3 drt_cuda_apply(int role, const DrtParams &p, const DrtGradeParams &g, const DrtAgxParams &a, float3 rgb)
{
    switch (role) {
    case DRT_CUDA_OUTPUT: return drt_transform(p, rgb);
    case DRT_CUDA_INPUT:  return drt_input_transform(p, rgb);
    case DRT_CUDA_GRADE:  return drt_grade(g, p, rgb);
    case DRT_CUDA_MACFIX: return drt_macos_fix(rgb);
    case DRT_CUDA_KNEE:   return drt_knee(p, rgb);
    case DRT_CUDA_AGX:    return drt_agx(a, rgb);
    default:              return rgb;
    }
}

__global__ void drt_cuda_kernel(const float4 *src, float4 *dst, int srcPitch, int dstPitch, int width, int height,
                                int role, DrtParams p, DrtGradeParams g, DrtAgxParams a)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x, y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    const float4 px = src[y * srcPitch + x];
    const float al = px.w;
    float3 rgb = make_float3(px.z, px.y, px.x);
    if (al > 0.0f && al < 1.0f) rgb = rgb / al;
    rgb = drt_cuda_apply(role, p, g, a, rgb);
    if (al > 0.0f && al < 1.0f) rgb = rgb * al;
    dst[y * dstPitch + x] = make_float4(rgb.z, rgb.y, rgb.x, al);
}

int drtCudaRender(const void *src, void *dst, int srcPitch, int dstPitch, int width, int height,
                  int role, const void *params, const void *grade, const void *agx, void *stream)
{
    DrtParams p; DrtGradeParams g; DrtAgxParams a;
    std::memcpy(&p, params, sizeof p);
    std::memcpy(&g, grade, sizeof g);
    std::memcpy(&a, agx, sizeof a);
    const dim3 block(16, 16, 1);
    const dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y, 1);
    cudaStream_t s = static_cast<cudaStream_t>(stream);
    drt_cuda_kernel<<<grid, block, 0, s>>>(static_cast<const float4 *>(src), static_cast<float4 *>(dst),
                                           srcPitch, dstPitch, width, height, role, p, g, a);
    cudaError_t e = cudaGetLastError();
    if (e == cudaSuccess) e = cudaStreamSynchronize(s);
    return int(e);
}

/* The Grade's key blur (0.1.4), the twin of the Metal passes: key -> across -> down -> graded. */
__global__ void drt_cuda_key_kernel(const float4 *src, float *key, int srcPitch, int width, int height, DrtParams p, DrtGradeParams g)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x, y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    const float4 px = src[y * srcPitch + x];
    const float al = px.w;
    float3 rgb = make_float3(px.z, px.y, px.x);
    if (al > 0.0f && al < 1.0f) rgb = rgb / al;
    key[y * width + x] = drt_grade_key(g, p, rgb);
}

__global__ void drt_cuda_key_blur_kernel(const float *src, float *dst, int width, int height, int radius, int dir)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x, y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    float s = 0.0f, ws = 0.0f;
    for (int k = -radius; k <= radius; ++k) {
        const float w = drt_key_blur_weight(float(k), float(radius));
        const int sx = dir == 0 ? min(max(x + k, 0), width - 1) : x;
        const int sy = dir == 0 ? y : min(max(y + k, 0), height - 1);
        s += w * src[sy * width + sx];
        ws += w;
    }
    dst[y * width + x] = s / ws;
}

/* width / height are the output's; it sits at (offX, offY) inside the input and its key */
__global__ void drt_cuda_keyed_kernel(const float4 *src, float4 *dst, const float *key, int srcPitch, int dstPitch, int width, int height,
                                      int keyWidth, int offX, int offY, DrtParams p, DrtGradeParams g)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x, y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    const int ix = x + offX, iy = y + offY;
    const float4 px = src[iy * srcPitch + ix];
    const float al = px.w;
    float3 rgb = make_float3(px.z, px.y, px.x);
    if (al > 0.0f && al < 1.0f) rgb = rgb / al;
    rgb = drt_grade_keyed(g, p, rgb, key[iy * keyWidth + ix]);
    if (al > 0.0f && al < 1.0f) rgb = rgb * al;
    dst[y * dstPitch + x] = make_float4(rgb.z, rgb.y, rgb.x, al);
}

int drtCudaRenderKeyBlur(const void *src, void *dst, int srcPitch, int dstPitch, int width, int height,
                         int outWidth, int outHeight, int offX, int offY,
                         const void *params, const void *grade, int radiusX, int radiusY, void *stream)
{
    DrtParams p; DrtGradeParams g;
    std::memcpy(&p, params, sizeof p);
    std::memcpy(&g, grade, sizeof g);
    cudaStream_t s = static_cast<cudaStream_t>(stream);
    const size_t n = size_t(width) * size_t(height) * sizeof(float);
    float *k0 = nullptr, *k1 = nullptr;
    cudaError_t e = cudaMallocAsync(reinterpret_cast<void **>(&k0), n, s);
    if (e == cudaSuccess) e = cudaMallocAsync(reinterpret_cast<void **>(&k1), n, s);
    if (e == cudaSuccess) {
        const dim3 block(16, 16, 1);
        const dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y, 1);
        drt_cuda_key_kernel<<<grid, block, 0, s>>>(static_cast<const float4 *>(src), k0, srcPitch, width, height, p, g);
        drt_cuda_key_blur_kernel<<<grid, block, 0, s>>>(k0, k1, width, height, radiusX, 0);
        drt_cuda_key_blur_kernel<<<grid, block, 0, s>>>(k1, k0, width, height, radiusY, 1);
        const dim3 ogrid((outWidth + block.x - 1) / block.x, (outHeight + block.y - 1) / block.y, 1);
        drt_cuda_keyed_kernel<<<ogrid, block, 0, s>>>(static_cast<const float4 *>(src), static_cast<float4 *>(dst), k0,
                                                      srcPitch, dstPitch, outWidth, outHeight, width, offX, offY, p, g);
        e = cudaGetLastError();
    }
    if (k1) cudaFreeAsync(k1, s);
    if (k0) cudaFreeAsync(k0, s);
    const cudaError_t e2 = cudaStreamSynchronize(s);
    return int(e != cudaSuccess ? e : e2);
}

const char *drtCudaErrorString(int e) { return cudaGetErrorString(cudaError_t(e)); }
