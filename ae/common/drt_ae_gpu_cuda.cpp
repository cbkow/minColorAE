/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — the CUDA backend (Windows). The kernels are compiled ahead by nvcc
 * (drt_ae_kernels.cu) and linked with the static CUDA runtime, as Adobe's samples do,
 * so device setup has nothing to build. Render enqueues on AE's CUDA stream
 * (PF_GPUDeviceInfo::command_queuePV) and waits for it. */
#include "drt_ae_common.h"
#include "drt_ae_cuda.h"

#include "AEFX_SuiteHelper.h"
#include "AE_Macros.h"

namespace drtae {

namespace {
#if DRT_ROLE_OUTPUT
const int kRole = DRT_CUDA_OUTPUT;
#elif DRT_ROLE_INPUT
const int kRole = DRT_CUDA_INPUT;
#elif DRT_ROLE_GRADE
const int kRole = DRT_CUDA_GRADE;
#elif DRT_ROLE_MACFIX
const int kRole = DRT_CUDA_MACFIX;
#elif DRT_ROLE_KNEE
const int kRole = DRT_CUDA_KNEE;
#else
const int kRole = DRT_CUDA_AGX;
#endif
} // namespace

PF_GPU_Framework gpuFramework() { return PF_GPU_Framework_CUDA; }

PF_Err gpuDeviceSetup(PF_InData *, PF_OutData *out_data, PF_GPUDeviceSetupExtra *extra)
{
    if (extra->input->what_gpu != PF_GPU_Framework_CUDA) return PF_Err_NONE;   /* CPU fallback */
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
    dlog("CUDA ready on device %u", unsigned(extra->input->device_index));
    return PF_Err_NONE;
}

PF_Err gpuDeviceSetdown(PF_InData *, PF_OutData *, PF_GPUDeviceSetdownExtra *) { return PF_Err_NONE; }

PF_Err renderGPU(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                 PF_EffectWorld *in, PF_EffectWorld *out, PF_SmartRenderExtra *extra, const DrtRender *p)
{
    PF_Err err = PF_Err_NONE;
    if (fmt != PF_PixelFormat_GPU_BGRA128 || extra->input->what_gpu != PF_GPU_Framework_CUDA)
        return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);
    PF_GPUDeviceInfo dev;
    AEFX_CLR_STRUCT(dev);
    ERR(gpu->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &dev));
    void *srcMem = nullptr, *dstMem = nullptr;
    ERR(gpu->GetGPUWorldData(in_data->effect_ref, in, &srcMem));
    ERR(gpu->GetGPUWorldData(in_data->effect_ref, out, &dstMem));
    if (err) return err;
    const int e = drtCudaRender(srcMem, dstMem, int(in->rowbytes / 16), int(out->rowbytes / 16), int(in->width), int(in->height),
                                kRole, &p->d, &p->g, &p->a, dev.command_queuePV);
    if (e) {
        dlog("CUDA render failed: %s", drtCudaErrorString(e));
        return PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return PF_Err_NONE;
}

} // namespace drtae
