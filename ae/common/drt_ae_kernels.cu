/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — CUDA kernels (Windows), the twin of drt_ae_kernels.metal.
 *
 * One kernel for every role (the role is an argument), so the effects and the
 * standalone GPU-vs-CPU check (tools/check_cuda.cpp) run the same code. The core
 * compiles here at file scope through opendrt_shim_cuda.h; the host passes its
 * blocks (namespace drt, same layout) as raw bytes, copied into the kernel's
 * parameters by value (DrtParams 496 + DrtGradeParams 272 + DrtAgxParams 336 bytes).
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

const char *drtCudaErrorString(int e) { return cudaGetErrorString(cudaError_t(e)); }
