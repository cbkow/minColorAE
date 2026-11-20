# minColorAE

Colour tools for Adobe After Effects that work in linear light: seven effects and
a panel. minColor Input says what each file is and converts it to the comp's
working space. You composite and grade there. minColor Output encodes the result
for a display or a delivery. A highlight knee and AgX are separate effects, used
only when you add them.

**[Read the documentation](docs/README.md)** ·
[Install](docs/install.md) · [Quick start](docs/quick-start.md)

<p>
<img src="docs/images/view_raw.jpg" width="49%" alt="Un-tone-mapped">
<img src="docs/images/view_agx.jpg" width="49%" alt="minColor AgX">
</p>

*One frame of the proof footage (a Blender render in linear ACEScg, highlights to
55× diffuse white, spectral lasers outside ACEScg): un-tone-mapped, and through
[minColor AgX](docs/effects/agx.md).*

<p>
<img src="docs/images/panel.png" height="300" alt="The minColor panel">
<img src="docs/images/ec_input.png" height="300" alt="minColor Input">
<img src="docs/images/ec_agx.png" height="300" alt="minColor AgX">
<img src="docs/images/ec_grade.png" height="300" alt="minColor Grade">
</p>

*The panel, and minColor Input, AgX and Grade in Effect Controls.*

## What's in it

| | |
| --- | --- |
| **minColor Input** | What a file is (camera logs, display encodings, linear), into the comp's working space. |
| **minColor Output** | Encodes for a display or a delivery: a gamut matrix and a curve, no tonescale. |
| **minColor Grade** | Colour correction for linear light: Primaries wheels, HDR zone wheels and an HSL secondary. |
| **minColor Knee** | A BT.2390 highlight knee for HDR content heading somewhere dimmer. |
| **minColor AgX** | Parametric, HDR-capable AgX; Blender-compatible at its defaults. |
| **minColor macOS Native Fix** | Corrects After Effects' viewer on a Mac whose monitor is left in its native mode, when not using OCIO. |
| **minColor Grade (Legacy)** | The first Grade, kept so projects made before 0.1.6 open and render as they did. |
| **minColor panel** | Sets the project up (Set OCIO), puts Input on footage (Apply In, In Rules) and adds the Output. |
| **OCIO config** | Written next to the project by Set OCIO, for After Effects' OCIO mode. Generated from the same matrices as the effects. |

After Effects 2027 (27.0) on macOS (Metal) and Windows (CUDA), with CPU rendering
everywhere; made on After Effects 2026 (26.5) until 0.1.5. Version 0.1.8.

## Credits and licensing

minColorAE is free software under the **GPL-3.0** ([LICENSE](LICENSE)).
Copyright (C) 2026 cbkow. It builds on others' work, credited in [NOTICE](NOTICE)
and [PROVENANCE.md](PROVENANCE.md):

- The colour engine is derived from
  [OpenDRT](https://github.com/jedypod/open-display-transform) v1.1.0 by Jed Smith
  (GPL-3.0), modified; see [CHANGES-FROM-OPENDRT.md](CHANGES-FROM-OPENDRT.md). The
  effects use its gamut matrices, transfer functions and, for the Input's inverse
  transfers, the Legacy Grade's zones and both Grades' HSL key, its rendering; its looks and tonescale presets
  are not offered. The unmodified DCTL is kept in `upstream/` as the test reference.
- minColor AgX ports parts of darktable's AgX module by István Kovács and the
  darktable developers (GPL-3.0-or-later); see [CHANGES-AGX.md](CHANGES-AGX.md).
  AgX is by Troy Sobotka; the Blender version, whose primaries and HDR method it
  follows, is by Eary Chow, Mark Faderbauer and Sakari Kapanen. No Blender files
  are included.
- The Knee follows ITU-R BT.2390.

minColorAE is not affiliated with or endorsed by OpenDRT, darktable, Blender,
Adobe or the Academy (ACES). After Effects is a trademark of Adobe. The Adobe
After Effects SDK is proprietary and is not part of this repository.

## Build

Needs Adobe's After Effects SDK (proprietary, obtained separately, never in this
repository): pass its path, or link it at `private/sdk/AfterEffectsSDK` (a symlink on
macOS, a directory junction on Windows: `mklink /J`).

```
cmake -S . -B build -DMINCOLOR_AE_SDK=/path/to/AfterEffectsSDK
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target install_ae
cmake --build build --target install_panel
cmake --build build --target install_presets
```

The plug-ins install to
`/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/`, the
panel to `~/Library/Preferences/Adobe/After Effects/27.0/Scripts/ScriptUI Panels/`
(Window > minColor.jsx, dockable; AE reads that folder at launch), and the
animation presets to `~/Documents/Adobe/After Effects 2027/User Presets/minColor/`
(the folders of After Effects 2027; set `MINCOLOR_PANEL_DIR` / `MINCOLOR_PRESETS_DIR`
for another version).

**Windows.** Visual Studio (2026 tested; its CMake and Ninja are used) and, for
the GPU path, the CUDA toolkit (13.5 tested; only the compiler and runtime
libraries are needed, not Nsight or the Visual Studio integration). Without the
toolkit the effects build CPU-only. From an *x64 Native Tools* prompt (or after
`vcvars64.bat`), elevated for `install_ae`:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target install_ae install_panel install_presets
```

The effects (`.aex`) install to `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\`,
the panel to `%APPDATA%\Adobe\After Effects\27.0\Scripts\ScriptUI Panels\`, and the
presets to `Documents\Adobe\After Effects 2027\User Presets\minColor\`. The CUDA kernels
are built for Turing and newer (PTX 7.5, SASS for Ampere, Ada and Blackwell). AE uses
them when the project's renderer is *Mercury GPU Acceleration (CUDA)*; under any other
renderer the effects render on the CPU, with the same results.

## Repository

```
core/       the colour maths, dialect-neutral (C++, Metal, CUDA)
ae/         the After Effects effects (one source, compiled per effect)
panel/      the ScriptUI panel
ocio/       mincolor.ocio (generated by tools/make_ocio) and mincolor-unmanaged.ocio
presets/    animation presets and the presets JSON
proof/      the Blender proof scene (scripted)
tools/      tests: the core against the upstream DCTL, the GPU kernels against the CPU
docs/       the user documentation
upstream/   OpenDRT v1.1.0, unmodified
```
