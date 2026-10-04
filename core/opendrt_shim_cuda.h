/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* DRT core — CUDA shim.
 *
 * Gives opendrt_kernel.h the DCTL dialect under nvcc: CUDA's own float2 / float3 and
 * make_float* (vector_types.h, vector_functions.h), the component-wise operators the
 * C++ shim defines, the _xxxf intrinsics on CUDA's float maths, and the qualifier
 * macros. Device code only; the host side of a CUDA build uses opendrt.h (namespace
 * drt) as every C++ host does. Include <cuda_runtime.h> first.
 *
 * Build without --use_fast_math (and with --fmad=false) so the floats stay the CPU's.
 */

#define DRT_CU static __device__ __forceinline__

DRT_CU float3 operator+(float3 a, float3 b) { return make_float3(a.x + b.x, a.y + b.y, a.z + b.z); }
DRT_CU float3 operator+(float3 a, float b)  { return make_float3(a.x + b, a.y + b, a.z + b); }
DRT_CU float3 operator+(float a, float3 b)  { return make_float3(a + b.x, a + b.y, a + b.z); }
DRT_CU float3 operator-(float3 a, float3 b) { return make_float3(a.x - b.x, a.y - b.y, a.z - b.z); }
DRT_CU float3 operator-(float3 a, float b)  { return make_float3(a.x - b, a.y - b, a.z - b); }
DRT_CU float3 operator-(float a, float3 b)  { return make_float3(a - b.x, a - b.y, a - b.z); }
DRT_CU float3 operator-(float3 a)           { return make_float3(-a.x, -a.y, -a.z); }
DRT_CU float3 operator*(float3 a, float3 b) { return make_float3(a.x * b.x, a.y * b.y, a.z * b.z); }
DRT_CU float3 operator*(float3 a, float b)  { return make_float3(a.x * b, a.y * b, a.z * b); }
DRT_CU float3 operator*(float a, float3 b)  { return make_float3(a * b.x, a * b.y, a * b.z); }
DRT_CU float3 operator/(float3 a, float3 b) { return make_float3(a.x / b.x, a.y / b.y, a.z / b.z); }
DRT_CU float3 operator/(float3 a, float b)  { return make_float3(a.x / b, a.y / b, a.z / b); }
DRT_CU float3 operator/(float a, float3 b)  { return make_float3(a / b.x, a / b.y, a / b.z); }
DRT_CU float3 &operator+=(float3 &a, float3 b) { a = a + b; return a; }
DRT_CU float3 &operator+=(float3 &a, float b)  { a = a + b; return a; }
DRT_CU float3 &operator-=(float3 &a, float3 b) { a = a - b; return a; }
DRT_CU float3 &operator-=(float3 &a, float b)  { a = a - b; return a; }
DRT_CU float3 &operator*=(float3 &a, float3 b) { a = a * b; return a; }
DRT_CU float3 &operator*=(float3 &a, float b)  { a = a * b; return a; }
DRT_CU float3 &operator/=(float3 &a, float3 b) { a = a / b; return a; }
DRT_CU float3 &operator/=(float3 &a, float b)  { a = a / b; return a; }

#define __DEVICE__ static __device__ __forceinline__
#define __CONSTANT__ static constexpr   /* scalar constants; constexpr, so device code can read them */

#define _powf(a, b)   powf((a), (b))
#define _expf(a)      expf((a))
#define _exp2f(a)     exp2f((a))
#define _exp10f(a)    exp10f((a))
#define _logf(a)      logf((a))
#define _log2f(a)     log2f((a))
#define _sqrtf(a)     sqrtf((a))
#define _fabs(a)      fabsf((a))
#define _fmod(a, b)   fmodf((a), (b))
#define _floorf(a)    floorf((a))
#define _atan2f(a, b) atan2f((a), (b))
#define _fminf(a, b)  fminf((a), (b))
#define _fmaxf(a, b)  fmaxf((a), (b))

#define DRT_PARAMS_ARG const DrtParams &
#define DRT_AGX_ARG const DrtAgxParams &
