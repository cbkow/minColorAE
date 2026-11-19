/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — what the effect (drt_ae_effect.cpp) shares with its GPU backend:
 * the role, the block pre-render hands to render, the dev log, and the backend's
 * three entry points. One backend is compiled into each plug-in:
 *   drt_ae_gpu_metal.mm  macOS, Metal
 *   drt_ae_gpu_cuda.cpp  Windows, CUDA (kernels in drt_ae_kernels.cu)
 *   drt_ae_gpu_none.cpp  no GPU: AE renders on the CPU path
 */
#pragma once

#include "drt_ae_params.h"

#include "AEConfig.h"
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"   /* PF_PixelFormat, before the GPU suites */
#include "AE_EffectGPUSuites.h"

#if DRT_ROLE_OUTPUT
#define DRT_EFFECT_NAME  "minColor Output"
#define DRT_MATCH_NAME   "ski.bialkow minColor Output"
#define DRT_KERNEL_NAME  "drt_output_kernel"
#define DRT_ROWS         drtae::kOutputRows
#define DRT_ROW_COUNT    drtae::kOutputRowCount
#define DRT_APPLY(p, v)  drt::drt_transform((p).d, (v))
#elif DRT_ROLE_INPUT
#define DRT_EFFECT_NAME  "minColor Input"
#define DRT_MATCH_NAME   "ski.bialkow minColor Input"
#define DRT_KERNEL_NAME  "drt_input_kernel"
#define DRT_ROWS         drtae::kInputRows
#define DRT_ROW_COUNT    drtae::kInputRowCount
#define DRT_APPLY(p, v)  drt::drt_input_transform((p).d, (v))
#elif DRT_ROLE_GRADE
/* the first Grade, kept for the projects made with it: the match name is what a project and a
   preset find it by and never changes; the name shown is free (0.1.6: "(Legacy)", and the plain
   name went to the new Grade below, whose match name is its own) */
#define DRT_EFFECT_NAME  "minColor Grade (Legacy)"
#define DRT_MATCH_NAME   "ski.bialkow minColor Grade"
#define DRT_KERNEL_NAME  "drt_grade_kernel"
#define DRT_ROWS         drtae::kGradeRows
#define DRT_ROW_COUNT    drtae::kGradeRowCount
#define DRT_APPLY(p, v)  drt::drt_grade((p).g, (p).d, (v))
#define DRT_HAS_KEY      1   /* an HSL secondary: key bars, eyedroppers, key blur */
#define DRT_KEYED(p, v, m) drt::drt_grade_keyed((p).g, (p).d, (v), (m))
#elif DRT_ROLE_MACFIX
#define DRT_EFFECT_NAME  "minColor macOS Fix"
#define DRT_MATCH_NAME   "ski.bialkow minColor macOS Fix"
#define DRT_KERNEL_NAME  "drt_macfix_kernel"
#define DRT_ROWS         drtae::kMacFixRows
#define DRT_ROW_COUNT    drtae::kMacFixRowCount
#define DRT_APPLY(p, v)  drt::drt_macos_fix(v)
#elif DRT_ROLE_KNEE
#define DRT_EFFECT_NAME  "minColor Knee"
#define DRT_MATCH_NAME   "ski.bialkow minColor Knee"
#define DRT_KERNEL_NAME  "drt_knee_kernel"
#define DRT_ROWS         drtae::kKneeRows
#define DRT_ROW_COUNT    drtae::kKneeRowCount
#define DRT_APPLY(p, v)  drt::drt_knee((p).d, (v))
#elif DRT_ROLE_GRADE2
#define DRT_EFFECT_NAME  "minColor Grade"
#define DRT_MATCH_NAME   "ski.bialkow minColor Grade 2"
#define DRT_KERNEL_NAME  "drt_grade2_kernel"
#define DRT_ROWS         drtae::kGrade2Rows
#define DRT_ROW_COUNT    drtae::kGrade2RowCount
/* its secondary's key is the first Grade's, computed from the Grade block it also carries */
#define DRT_APPLY(p, v)  drt::drt_grade2_keyed((p).q, (v), drt::drt_grade_key((p).g, (p).d, (v)))
#define DRT_HAS_KEY      1
#define DRT_KEYED(p, v, m) drt::drt_grade2_keyed((p).q, (v), (m))
#elif DRT_ROLE_AGX
#define DRT_EFFECT_NAME  "minColor AgX"
#define DRT_MATCH_NAME   "ski.bialkow minColor AgX"
#define DRT_KERNEL_NAME  "drt_agx_kernel"
#define DRT_ROWS         drtae::kAgxRows
#define DRT_ROW_COUNT    drtae::kAgxRowCount
#define DRT_APPLY(p, v)  drt::drt_agx((p).a, (v))
#else
#error define DRT_ROLE_OUTPUT, DRT_ROLE_INPUT, DRT_ROLE_GRADE, DRT_ROLE_GRADE2, DRT_ROLE_MACFIX, DRT_ROLE_KNEE or DRT_ROLE_AGX
#endif

#ifndef DRT_HAS_KEY
#define DRT_HAS_KEY 0
#endif

/* What pre-render hands to render: the DRT block, and for Grade, Grade 2 and AgX their own blocks too. */
struct DrtRender {
    drt::DrtParams      d;
    drt::DrtGradeParams g;
    drt::DrtAgxParams   a;
    drt::DrtGrade2Params q;
    int blurRx, blurRy;   /* Grade and Grade 2: the key blur's radius in this render's pixels per axis (0 = none), host only */
    int offX, offY;       /* with a key blur, the output world's origin inside the (bigger) input world */
};

/* The parameter block AE's GPU kernels see beside the pixels. */
struct DrtAeHeader {
    int srcPitch;   /* pixels */
    int dstPitch;
    int width;
    int height;
};

namespace drtae {

/* Development switches (drt_ae_effect.cpp), file-existence checks read once per process,
   in /tmp on macOS and %TEMP% on Windows:
     mincolor_ae.log    -> every command and its result is appended there
     mincolor_ae_nogpu  -> the effect never offers the GPU path (CPU only) */
void dlog(const char *fmt, ...);
bool gpuDisabled();

/* The backend. gpuFramework() is the one framework it renders with (PF_GPU_Framework_NONE
   for none): pre-render offers the GPU only when AE's framework for the frame is that one. */
PF_GPU_Framework gpuFramework();
PF_Err gpuDeviceSetup(PF_InData *in_data, PF_OutData *out_data, PF_GPUDeviceSetupExtra *extra);
PF_Err gpuDeviceSetdown(PF_InData *in_data, PF_OutData *out_data, PF_GPUDeviceSetdownExtra *extra);
PF_Err renderGPU(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                 PF_EffectWorld *in, PF_EffectWorld *out, PF_SmartRenderExtra *extra, const DrtRender *p);

} // namespace drtae
