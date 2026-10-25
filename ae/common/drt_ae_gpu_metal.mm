/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
/* minColor AE — the Metal backend (macOS). The kernels are the embedded Metal source
 * (prefix + core shim + params + kernel + drt_ae_kernels.metal), compiled once per
 * device. AE's GPU worlds are MTLBuffers; the DrtParams block goes at buffer(2), the
 * header at buffer(3), Grade's or AgX's own block at buffer(4).
 */
#include "drt_ae_common.h"

#include "AEFX_SuiteHelper.h"
#include "AE_Macros.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>

#include "drt_ae_msl.h" /* generated: kDrtAeMsl = prefix + shim + params + kernel + wrapper */

namespace drtae {

PF_GPU_Framework gpuFramework() { return PF_GPU_Framework_METAL; }

struct MetalGPUData {
    id<MTLComputePipelineState> pipeline;
    id<MTLComputePipelineState> key, blur, keyed;   /* Grade's key blur passes; nil elsewhere */
};

/* the key blur's headers, as drt_ae_kernels.metal declares them */
struct DrtBlurHeader {
    int width;
    int height;
    int radius;
    int dir;
};
struct DrtKeyedHeader {
    int keyWidth;
    int offX;
    int offY;
    int pad;
};

PF_Err gpuDeviceSetup(PF_InData *in_data, PF_OutData *out_data, PF_GPUDeviceSetupExtra *extra)
{
    if (extra->input->what_gpu != PF_GPU_Framework_METAL) return PF_Err_NONE; /* CPU fallback */
    @autoreleasepool {
        AEFX_SuiteScoper<PF_HandleSuite1> handles(in_data, kPFHandleSuite, kPFHandleSuiteVersion1, out_data);
        AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);
        PF_GPUDeviceInfo dev;
        AEFX_CLR_STRUCT(dev);
        gpu->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &dev);
        id<MTLDevice> device = (id<MTLDevice>)dev.devicePV;

        NSError *error = nil;
        MTLCompileOptions *opts = [[[MTLCompileOptions alloc] init] autorelease];
        opts.fastMathEnabled = NO; /* the core is verified against the DCTL; keep the floats honest */
        id<MTLLibrary> lib = [[device newLibraryWithSource:[NSString stringWithUTF8String:kDrtAeMsl]
                                                   options:opts error:&error] autorelease];
        if (!lib) {
            dlog("Metal compile failed: %s", error ? [[error localizedDescription] UTF8String] : "?");
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }
        id<MTLFunction> fn = [[lib newFunctionWithName:@DRT_KERNEL_NAME] autorelease];
        if (!fn) { dlog("kernel %s not found", DRT_KERNEL_NAME); return PF_Err_INTERNAL_STRUCT_DAMAGED; }
        id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:fn error:&error];
        if (!pso) {
            dlog("pipeline failed: %s", error ? [[error localizedDescription] UTF8String] : "?");
            return PF_Err_INTERNAL_STRUCT_DAMAGED;
        }
        id<MTLComputePipelineState> keyPso = nil, blurPso = nil, keyedPso = nil;
#if DRT_ROLE_GRADE
        for (int k = 0; k < 3; ++k) {
            NSString *name = k == 0 ? @"drt_grade_key_kernel" : (k == 1 ? @"drt_key_blur_kernel" : @"drt_grade_keyed_kernel");
            id<MTLFunction> f = [[lib newFunctionWithName:name] autorelease];
            id<MTLComputePipelineState> s = f ? [device newComputePipelineStateWithFunction:f error:&error] : nil;
            if (!s) { dlog("pipeline %s failed", [name UTF8String]); [pso release]; [keyPso release]; [blurPso release]; return PF_Err_INTERNAL_STRUCT_DAMAGED; }
            if (k == 0) keyPso = s; else if (k == 1) blurPso = s; else keyedPso = s;
        }
#endif
        dlog("Metal pipeline ready on %s", [[device name] UTF8String]);

        PF_Handle h = handles->host_new_handle(sizeof(MetalGPUData));
        reinterpret_cast<MetalGPUData *>(*h)->pipeline = pso;
        reinterpret_cast<MetalGPUData *>(*h)->key = keyPso;
        reinterpret_cast<MetalGPUData *>(*h)->blur = blurPso;
        reinterpret_cast<MetalGPUData *>(*h)->keyed = keyedPso;
        extra->output->gpu_data = h;
        out_data->out_flags2 = PF_OutFlag2_SUPPORTS_GPU_RENDER_F32;
    }
    return PF_Err_NONE;
}

PF_Err gpuDeviceSetdown(PF_InData *in_data, PF_OutData *out_data, PF_GPUDeviceSetdownExtra *extra)
{
    if (extra->input->what_gpu == PF_GPU_Framework_METAL && extra->input->gpu_data) {
        PF_Handle h = (PF_Handle)extra->input->gpu_data;
        MetalGPUData *md = reinterpret_cast<MetalGPUData *>(*h);
        [md->pipeline release]; [md->key release]; [md->blur release]; [md->keyed release];
        AEFX_SuiteScoper<PF_HandleSuite1> handles(in_data, kPFHandleSuite, kPFHandleSuiteVersion1, out_data);
        handles->host_dispose_handle(h);
    }
    return PF_Err_NONE;
}

PF_Err renderGPU(PF_InData *in_data, PF_OutData *out_data, PF_PixelFormat fmt,
                 PF_EffectWorld *in, PF_EffectWorld *out, PF_SmartRenderExtra *extra, const DrtRender *p)
{
    PF_Err err = PF_Err_NONE;
    if (fmt != PF_PixelFormat_GPU_BGRA128 || extra->input->what_gpu != PF_GPU_Framework_METAL)
        return PF_Err_UNRECOGNIZED_PARAM_TYPE;
    /* No pipeline (device setup failed or was skipped): refuse rather than dereference. */
    if (!extra->input->gpu_data || !*(PF_Handle)extra->input->gpu_data) return PF_Err_UNRECOGNIZED_PARAM_TYPE;

    @autoreleasepool {
        AEFX_SuiteScoper<PF_GPUDeviceSuite1> gpu(in_data, kPFGPUDeviceSuite, kPFGPUDeviceSuiteVersion1, out_data);
        PF_GPUDeviceInfo dev;
        AEFX_CLR_STRUCT(dev);
        ERR(gpu->GetDeviceInfo(in_data->effect_ref, extra->input->device_index, &dev));
        void *srcMem = nullptr, *dstMem = nullptr;
        ERR(gpu->GetGPUWorldData(in_data->effect_ref, in, &srcMem));
        ERR(gpu->GetGPUWorldData(in_data->effect_ref, out, &dstMem));
        if (err) return err;

        MetalGPUData *md = reinterpret_cast<MetalGPUData *>(*(PF_Handle)extra->input->gpu_data);
        id<MTLDevice> device = (id<MTLDevice>)dev.devicePV;
        id<MTLCommandQueue> queue = (id<MTLCommandQueue>)dev.command_queuePV;

        DrtAeHeader h;
        h.srcPitch = in->rowbytes / 16;
        h.dstPitch = out->rowbytes / 16;
        h.width = in->width;
        h.height = in->height;
        id<MTLBuffer> pbuf = [[device newBufferWithBytes:&p->d length:sizeof(drt::DrtParams) options:MTLResourceStorageModeShared] autorelease];
        id<MTLBuffer> hbuf = [[device newBufferWithBytes:&h length:sizeof h options:MTLResourceStorageModeShared] autorelease];
#if DRT_ROLE_AGX
        id<MTLBuffer> gbuf = [[device newBufferWithBytes:&p->a length:sizeof(drt::DrtAgxParams) options:MTLResourceStorageModeShared] autorelease];   /* the AgX block at buffer(4) */
#else
        id<MTLBuffer> gbuf = [[device newBufferWithBytes:&p->g length:sizeof(drt::DrtGradeParams) options:MTLResourceStorageModeShared] autorelease];
#endif

        id<MTLCommandBuffer> cb = [queue commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
#if DRT_ROLE_GRADE
        if ((p->blurRx > 0 || p->blurRy > 0) && md->key && md->blur && md->keyed) {
            /* key -> across -> down -> graded, all on this command buffer */
            const NSUInteger n = NSUInteger(in->width) * NSUInteger(in->height) * sizeof(float);
            id<MTLBuffer> k0 = [[device newBufferWithLength:n options:MTLResourceStorageModePrivate] autorelease];
            id<MTLBuffer> k1 = [[device newBufferWithLength:n options:MTLResourceStorageModePrivate] autorelease];
            const MTLSize tg = MTLSizeMake(16, 16, 1), grid = MTLSizeMake((in->width + 15) / 16, (in->height + 15) / 16, 1);
            [enc setComputePipelineState:md->key];
            [enc setBuffer:(id<MTLBuffer>)srcMem offset:0 atIndex:0];
            [enc setBuffer:k0 offset:0 atIndex:1];
            [enc setBuffer:pbuf offset:0 atIndex:2];
            [enc setBuffer:hbuf offset:0 atIndex:3];
            [enc setBuffer:gbuf offset:0 atIndex:4];
            [enc dispatchThreadgroups:grid threadsPerThreadgroup:tg];
            for (int dir = 0; dir < 2; ++dir) {
                DrtBlurHeader bh = { in->width, in->height, dir == 0 ? p->blurRx : p->blurRy, dir };
                [enc setComputePipelineState:md->blur];
                [enc setBuffer:(dir == 0 ? k0 : k1) offset:0 atIndex:0];
                [enc setBuffer:(dir == 0 ? k1 : k0) offset:0 atIndex:1];
                [enc setBytes:&bh length:sizeof bh atIndex:2];
                [enc dispatchThreadgroups:grid threadsPerThreadgroup:tg];
            }
            DrtAeHeader oh = h;   /* the output's size; it sits at offX, offY in the input */
            oh.width = std::min(out->width, in->width - p->offX);
            oh.height = std::min(out->height, in->height - p->offY);
            const DrtKeyedHeader kh = { in->width, p->offX, p->offY, 0 };
            [enc setComputePipelineState:md->keyed];
            [enc setBuffer:(id<MTLBuffer>)srcMem offset:0 atIndex:0];
            [enc setBuffer:(id<MTLBuffer>)dstMem offset:0 atIndex:1];
            [enc setBuffer:pbuf offset:0 atIndex:2];
            [enc setBytes:&oh length:sizeof oh atIndex:3];
            [enc setBuffer:gbuf offset:0 atIndex:4];
            [enc setBuffer:k0 offset:0 atIndex:5];
            [enc setBytes:&kh length:sizeof kh atIndex:6];
            [enc dispatchThreadgroups:MTLSizeMake((oh.width + 15) / 16, (oh.height + 15) / 16, 1) threadsPerThreadgroup:tg];
            [enc endEncoding];
            [cb commit];
            if ([cb error]) err = PF_Err_INTERNAL_STRUCT_DAMAGED;
            return err;
        }
#endif
        [enc setComputePipelineState:md->pipeline];
        [enc setBuffer:(id<MTLBuffer>)srcMem offset:0 atIndex:0];
        [enc setBuffer:(id<MTLBuffer>)dstMem offset:0 atIndex:1];
        [enc setBuffer:pbuf offset:0 atIndex:2];
        [enc setBuffer:hbuf offset:0 atIndex:3];
        [enc setBuffer:gbuf offset:0 atIndex:4];
        const NSUInteger tw = [md->pipeline threadExecutionWidth];
        [enc dispatchThreadgroups:MTLSizeMake((in->width + tw - 1) / tw, (in->height + 15) / 16, 1)
            threadsPerThreadgroup:MTLSizeMake(tw, 16, 1)];
        [enc endEncoding];
        [cb commit];
        if ([cb error]) err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return err;
}

} // namespace drtae
