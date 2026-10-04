/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* The CUDA kernels' host entry (drt_ae_kernels.cu), callable from plain C++: no CUDA
 * or core types cross it. Used by drt_ae_gpu_cuda.cpp and tools/check_cuda.cpp. */
#pragma once

enum { DRT_CUDA_OUTPUT, DRT_CUDA_INPUT, DRT_CUDA_GRADE, DRT_CUDA_MACFIX, DRT_CUDA_KNEE, DRT_CUDA_AGX };

/* float4 BGRA worlds in device memory, pitch in pixels; params / grade / agx point at a
   derived drt::DrtParams / DrtGradeParams / DrtAgxParams. Runs on `stream` (a CUstream /
   cudaStream_t, null = default) and waits for it. Returns a cudaError_t, 0 = success. */
int drtCudaRender(const void *src, void *dst, int srcPitch, int dstPitch, int width, int height,
                  int role, const void *params, const void *grade, const void *agx, void *stream);
const char *drtCudaErrorString(int e);
