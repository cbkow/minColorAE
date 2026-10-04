/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — no GPU backend: the effect claims no framework, so pre-render never
 * offers the GPU and AE renders every frame through the CPU path. */
#include "drt_ae_common.h"

namespace drtae {

PF_GPU_Framework gpuFramework() { return PF_GPU_Framework_NONE; }
PF_Err gpuDeviceSetup(PF_InData *, PF_OutData *, PF_GPUDeviceSetupExtra *) { return PF_Err_NONE; }
PF_Err gpuDeviceSetdown(PF_InData *, PF_OutData *, PF_GPUDeviceSetdownExtra *) { return PF_Err_NONE; }
PF_Err renderGPU(PF_InData *, PF_OutData *, PF_PixelFormat, PF_EffectWorld *, PF_EffectWorld *, PF_SmartRenderExtra *, const DrtRender *)
{
    return PF_Err_UNRECOGNIZED_PARAM_TYPE;
}

} // namespace drtae
