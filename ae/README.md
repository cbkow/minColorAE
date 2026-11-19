# ae/ — the After Effects effects (developer notes)

Using the effects: see the [documentation](../docs/README.md). This page is about
how they are built.

## One source, seven effects

`common/drt_ae_effect.cpp` is compiled once per effect with `-DDRT_ROLE_<NAME>=1`
(Input, Output, Grade, Grade 2, macOS Fix, Knee, AgX). Per role it picks a parameter table,
a per-pixel function from `core/` and a kernel name. Each effect declares its own
input on its popups, transforms whatever pixels reach it, and never asks AE what
the layer or the project is.

- `common/drt_ae_params.h`: the parameter tables. One row per AE parameter, bound
  to a `DrtParams`, `DrtGradeParams`, `DrtGrade2Params` or `DrtAgxParams` field by pointer-to-member;
  the same table builds the UI, reads it in pre-render and writes presets back.
  Parameter ids are AE project state: never remove or reorder an auto-id row
  (hide it); new rows take explicit ids from 1000 up; bump the version with every
  table change (`DRT_BUG_VERSION` and the PiPLs' `AE_Effect_Version`).
- `common/drt_ae_common.h`: what the effect shares with its GPU backend (the role,
  the block pre-render hands to render, the dev log, the backend's entry points).
- `common/drt_ae_wheels.h`: the first Grade's colour wheels (custom UI);
  `common/drt_ae_keybars.h`: the HSL key bars, shared by both Grades.
- `common/drt_ae_wheels2.h`: the new Grade's wheels (Primaries, HDR, More Zones,
  the secondary's): relative, geared drags over sliders.

Two Grades, and their names. In the code the new one is "Grade 2"
(`DRT_ROLE_GRADE2`, `core/mincolor_grade2.h`, `ae/grade2/`, match name
`ski.bialkow minColor Grade 2`); the user sees it as "minColor Grade". The first
(`DRT_ROLE_GRADE`, `core/opendrt_grade.h`, match name `ski.bialkow minColor
Grade`) is shown as "minColor Grade (Legacy)". Match names are what projects and
presets find an effect by: they never change. The new Grade's secondary uses the
first Grade's key, so it carries a `DrtGradeParams` for those rows.
`tools/check_grade2` holds the new core to sampled reference renders
(`tools/grade2_ref.txt`).
- `<effect>/`: each effect's PiPL and Info.plist.

## Render paths

- **CPU**: the C++ core through AE's iterate suites, 8, 16 and 32 bpc (8 and 16
  are converted to float and clamped back).
- **Metal** (macOS, `common/drt_ae_gpu_metal.mm`): the embedded Metal source
  (prefix + core shim + params + kernels + `drt_ae_kernels.metal`), compiled once
  per device with fast-math off. The parameter block goes at buffer(2), the
  header at buffer(3), Grade's or AgX's own block at buffer(4); Grade 2 has the
  Grade's block there (its key) and its own at buffer(7).
- **CUDA** (Windows, `common/drt_ae_gpu_cuda.cpp` + `drt_ae_kernels.cu`): compiled
  ahead by nvcc without fast maths or FMA contraction, one kernel for every role,
  run on AE's CUDA stream. `tools/check_cuda` compares it with the CPU for every
  effect.
- **None** (`common/drt_ae_gpu_none.cpp`): Windows without the CUDA toolkit.

Pre-render offers the GPU only when AE's framework for the frame is the
backend's. AE hands effects straight (unmatted) alpha, so RGB is transformed as
it is and alpha passes through (0.1.4 and earlier unpremultiplied first, which
rendered edges and semi-transparent pixels wrong).

Development switches, read once per process (`/tmp` on macOS, `%TEMP%` on
Windows): create `mincolor_ae.log` to log every command, `mincolor_ae_nogpu` to
force the CPU path.

## Build

From the repository root (see the top-level [README](../README.md#build)):

```
cmake -S . -B build -DMINCOLOR_AE_SDK=/path/to/AfterEffectsSDK
cmake --build build --target install_ae
```

macOS: `.plugin` bundles; the PiPL is Rez'd and copied into the bundle before
codesigning (copying after the linker seals the bundle ships an invalid signature,
and AE loads nothing). Windows: `.aex` DLLs; the PiPL goes through Adobe's
PiPLtool into a `.rc` (`cmake/pipl_win.cmake`).

## Not there yet

- User presets (Add / Remove Preset against the shared JSON).
- A DirectX 12 path for non-NVIDIA GPUs on Windows (they render on the CPU).
- Greying out a group's sliders when its Enable is off.
