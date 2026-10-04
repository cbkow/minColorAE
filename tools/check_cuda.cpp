// SPDX-License-Identifier: GPL-3.0-only
// minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
//
// check_cuda: the CUDA kernels (ae/common/drt_ae_kernels.cu, the effects' GPU path on
// Windows) against the C++ core, every role, on a grid of scene values (negatives,
// SDR, HDR to 50) and colours, through float4 BGRA worlds laid out as AE's. Fails when
// any channel differs from the CPU by more than 2e-4 relative (1e-7 typical; the GPU's
// powf / exp2f / log2f are within 2 ulp of the CPU's). Two cases differ by design:
//   - the OpenDRT inverse is fed what it inverts, unclipped codes of the forward
//     rendering (Rec.1886), and judged as probe_core judges it: re-rendered, in display
//     codes, within half an 8-bit step. Saturated colours are many-to-one through the
//     rendering, so CPU and GPU may find different scene values for one code;
//   - the macOS Fix ends in a 2.2 encode, which turns float rounding near zero (~1e-8
//     linear) into codes near 7e-4: 1e-3 there, a quarter of an 8-bit step.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <cuda_runtime.h>

#include "opendrt.h"
#include "drt_ae_cuda.h"

namespace {

struct Case { const char *name; int role; drt::DrtParams d; drt::DrtGradeParams g; drt::DrtAgxParams a; double tol = 2e-4; bool forward = false; drt::DrtParams fwd{}; };

drt::float3 cpu(const Case &c, drt::float3 v)
{
    switch (c.role) {
    case DRT_CUDA_OUTPUT: return drt::drt_transform(c.d, v);
    case DRT_CUDA_INPUT:  return drt::drt_input_transform(c.d, v);
    case DRT_CUDA_GRADE:  return drt::drt_grade(c.g, c.d, v);
    case DRT_CUDA_MACFIX: return drt::drt_macos_fix(v);
    case DRT_CUDA_KNEE:   return drt::drt_knee(c.d, v);
    default:              return drt::drt_agx(c.a, v);
    }
}

Case base(const char *name, int role)
{
    Case c{name, role, drt::drt_stickshift_defaults(), drt::drt_grade_defaults(), drt::drt_agx_defaults(), 2e-4, false, {}};
    c.d.in_gamut = DRT_IN_AP1; c.d.in_oetf = DRT_OETF_LINEAR; c.d.working_gamut = DRT_IN_AP1;
    c.d.kn_src = 1000.0f; c.d.kn_tgt = 100.0f; c.d.kn_auto = 1; c.d.kn_start = 0.5f;
    return c;
}

Case finish(Case c)
{
    if (c.role == DRT_CUDA_INPUT) drt::drt_inverse_display(c.d);
    if (c.role == DRT_CUDA_GRADE) { c.d.in_gamut = c.d.working_gamut; c.d.in_oetf = DRT_OETF_LINEAR; }
    if (c.role == DRT_CUDA_OUTPUT) c.d.working_gamut = c.d.in_gamut;
    c.d = drt::drt_derive(c.d);
    if (c.role == DRT_CUDA_GRADE) c.g = drt::drt_grade_derive(c.g, c.d);
    if (c.role == DRT_CUDA_KNEE) c.d = drt::drt_knee_derive(c.d);
    c.a = drt::drt_agx_derive(c.a);
    return c;
}

} // namespace

int main()
{
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev == 0) { std::printf("check_cuda: no CUDA device; skipped\n"); return 0; }
    cudaDeviceProp prop{};
    cudaGetDeviceProperties(&prop, 0);
    std::printf("check_cuda on %s (sm_%d%d)\n", prop.name, prop.major, prop.minor);

    std::vector<Case> cases;
    { Case c = base("output, OpenDRT rendering", DRT_CUDA_OUTPUT); c.d.out_view = 0; cases.push_back(finish(c)); }
    { Case c = base("output, Un-tone-mapped", DRT_CUDA_OUTPUT); c.d.out_view = 1; cases.push_back(finish(c)); }
    { Case c = base("input, Rec.709 / sRGB", DRT_CUDA_INPUT); c.d.in_gamut = DRT_IN_REC709; c.d.in_oetf = DRT_OETF_SRGB; cases.push_back(finish(c)); }
    {   /* as probe_core's round trip: the forward Rec.1886 rendering and the inverse entry that names it */
        Case c = base("input, OpenDRT inverse (re-rendered)", DRT_CUDA_INPUT);
        drt::DrtSettings st; st.in_gamut = DRT_IN_AP1; st.in_oetf = DRT_OETF_LINEAR; st.display = 0; st.tn_Lp = drt::kDisplays[0].default_Lp;
        c.fwd = drt::drt_resolve(st);
        c.d = c.fwd; c.d.in_oetf = DRT_OETF_INVERSE_REC1886; c.d.working_gamut = DRT_IN_AP1;
        drt::drt_inverse_display(c.d);
        c.d = drt::drt_derive(c.d);
        c.tol = 0.5 / 255.0; c.forward = true;
        cases.push_back(c);
    }
    { Case c = base("grade", DRT_CUDA_GRADE); c.g.exposure = 0.4f; c.g.z1_exposure = -0.5f; c.g.z4_saturation = 1.3f; c.g.z3_tint = 0.3f; cases.push_back(finish(c)); }
    { Case c = base("macOS fix", DRT_CUDA_MACFIX); c.tol = 1e-3; cases.push_back(finish(c)); }
    { Case c = base("knee, 1000 -> 100 auto", DRT_CUDA_KNEE); cases.push_back(finish(c)); }
    { Case c = base("agx, SDR", DRT_CUDA_AGX); cases.push_back(finish(c)); }
    { Case c = base("agx, Peak 1000, Rec.2020 / P3", DRT_CUDA_AGX); c.a.peak = 1000.0f; c.a.working_gamut = DRT_IN_REC2020; c.a.target = 2.0f; cases.push_back(finish(c)); }

    /* the pixels: scene values x colours, as straight (alpha 1) BGRA */
    const float levels[] = { -0.05f, 0.0f, 0.001f, 0.01f, 0.05f, 0.18f, 0.4f, 0.75f, 1.0f, 2.0f, 5.0f, 12.0f, 50.0f };
    const float cols[][3] = { {1, 1, 1}, {1, 0.4f, 0.1f}, {0.2f, 0.5f, 1}, {0.05f, 1, 0.2f}, {1, 0.05f, 0.6f}, {0.9f, 0.8f, 0.7f}, {1, 0, 0}, {0, 0, 1} };
    const int W = int(sizeof levels / sizeof levels[0]), H = int(sizeof cols / sizeof cols[0]);
    std::vector<float> host(size_t(W) * H * 4), back(host.size());
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
        float *p = &host[(size_t(y) * W + x) * 4];
        p[0] = levels[x] * cols[y][2]; p[1] = levels[x] * cols[y][1]; p[2] = levels[x] * cols[y][0]; p[3] = 1.0f;
    }
    void *dsrc = nullptr, *ddst = nullptr;
    cudaMalloc(&dsrc, host.size() * 4); cudaMalloc(&ddst, host.size() * 4);
    const std::vector<float> scene = host;

    bool good = true;
    for (const Case &c : cases) {
        host = scene;
        if (c.forward)   /* the codes this inverse undoes: the forward rendering of the scene grid */
            for (size_t i = 0; i < host.size(); i += 4) {
                const drt::float3 r = drt::drt_transform(c.fwd, drt::make_float3(std::max(0.0f, host[i + 2]), std::max(0.0f, host[i + 1]), std::max(0.0f, host[i])));
                host[i] = r.z; host[i + 1] = r.y; host[i + 2] = r.x;
            }
        cudaMemcpy(dsrc, host.data(), host.size() * 4, cudaMemcpyHostToDevice);
        const int e = drtCudaRender(dsrc, ddst, W, W, W, H, c.role, &c.d, &c.g, &c.a, nullptr);
        if (e) { std::printf("%-34s CUDA error: %s\n", c.name, drtCudaErrorString(e)); good = false; continue; }
        cudaMemcpy(back.data(), ddst, back.size() * 4, cudaMemcpyDeviceToHost);
        double worst = 0.0; int wx = 0, wy = 0; float wg[3] = {0, 0, 0}, wr[3] = {0, 0, 0};
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            const float *s = &host[(size_t(y) * W + x) * 4], *g = &back[(size_t(y) * W + x) * 4];
            drt::float3 r = cpu(c, drt::make_float3(s[2], s[1], s[0]));
            float gv[3] = { g[0], g[1], g[2] };
            if (c.forward) {   /* re-render both answers; skip codes the display clipped */
                if (std::max(s[0], std::max(s[1], s[2])) > 0.98f || std::min(s[0], std::min(s[1], s[2])) <= 0.0f) continue;
                r = drt::drt_transform(c.fwd, r);
                const drt::float3 rg = drt::drt_transform(c.fwd, drt::make_float3(g[2], g[1], g[0]));
                gv[0] = rg.z; gv[1] = rg.y; gv[2] = rg.x;
            }
            const float ref[3] = { r.z, r.y, r.x };
            for (int k = 0; k < 3; ++k) {
                const double d = std::fabs(double(gv[k]) - double(ref[k])) / std::max(1.0, std::fabs(double(ref[k])));
                if (!(d <= worst)) { worst = d; wx = x; wy = y; for (int q = 0; q < 3; ++q) { wg[q] = gv[2 - q]; wr[q] = ref[2 - q]; } }   /* NaN counts as worst */
            }
        }
        const bool ok = worst <= c.tol;
        good = good && ok;
        std::printf("%-34s worst %.2e (scene %g, colour %d)  %s\n", c.name, worst, levels[wx], wy, ok ? "" : "FAILED");
        if (!ok) std::printf("   scene %g %g %g: cpu %.7g %.7g %.7g, gpu %.7g %.7g %.7g\n", levels[wx] * cols[wy][0], levels[wx] * cols[wy][1], levels[wx] * cols[wy][2],
                             wr[0], wr[1], wr[2], wg[0], wg[1], wg[2]);
    }
    cudaFree(dsrc); cudaFree(ddst);
    std::printf("%s\n", good ? "PASS" : "FAIL");
    return good ? 0 : 1;
}
