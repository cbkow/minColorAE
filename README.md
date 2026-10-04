# minColorAE

Colour tools for After Effects: four effects that interpret media, grade it, and
render it for a display, built on a rendering derived from
[OpenDRT](https://github.com/jedypod/open-display-transform) v1.1.0 by Jed Smith.

minColorAE is not OpenDRT and is not affiliated with or endorsed by the OpenDRT
project or its author. The rendering is modified from upstream; see
[CHANGES-FROM-OPENDRT.md](CHANGES-FROM-OPENDRT.md) and [NOTICE](NOTICE).

Version 0.1.0. macOS (Metal + CPU), After Effects 26.5.

## The effects (Effect > minColor)

| Effect | Job |
| --- | --- |
| **minColor Input** | What a file is: input gamut and transfer (camera logs, Rec.1886, sRGB, PQ, HLG, linear), video range, into the comp's linear working gamut. Also the "OpenDRT inverse" transfers for material rendered through this same rendering. |
| **minColor Output** | The rendering, on an adjustment layer at the top: look, tonescale, display encoding. Two renderings (the OpenDRT-derived one, or Un-tone-mapped) and two modes (View, Render) sharing one look. |
| **minColor Grade** | Exposure, contrast, six luminance zones defined through the tonescale with colour wheels, and an HSL secondary with eyedroppers. |
| **minColor macOS Fix** | No settings. Corrects how After Effects shows colour in its viewport on macOS; viewer only, on a Guide Layer. |

`ocio/mincolor-viewport-shim.ocio` is the same macOS correction as an OCIO
config to pin in Project Settings instead.

Full controls and the comp setup: [ae/README.md](ae/README.md).

## Layout

```
core/       the rendering, dialect-neutral (C++ / Metal; HLSL and GLSL shims untested),
            presets, the Grade maths
ae/         the four After Effects effects (one source, compiled per role)
panel/      the minColor ScriptUI panel (workflow only; the shim is embedded at build)
ocio/       the macOS viewport shim
presets/    drt_presets.json, generated from the C++ tables
tools/      probe_core (the core against the unmodified upstream DCTL), check_msl,
            dump_presets
upstream/   OpenDRT_v1.1.0.dctl, unmodified, the test reference
```

## Build, test, install

Needs Adobe's After Effects SDK (proprietary, obtained separately, never in this
repository): pass its path, or symlink it at `private/sdk/AfterEffectsSDK`.

```
cmake -S . -B build -DMINCOLOR_AE_SDK=/path/to/AfterEffectsSDK
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target install_ae
cmake --build build --target install_panel
```

The plug-ins install to
`/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/`, the
panel to `~/Library/Preferences/Adobe/After Effects/26.5/Scripts/ScriptUI Panels/`
(Window > minColor.jsx, dockable; AE reads that folder at launch).

## The panel

Nothing a frame renders depends on it. **Add Output** puts an adjustment layer
with minColor Output at the top of the active comp (under a macOS Fix layer if
there is one), once per comp, as one undo step. In an OCIO project pinned to
the viewport shim, it keeps a copy of the shim in `minColor/` next to the `.aep`
and points the pin there (AE stores that path absolutely). It never switches an
Adobe-engine project to OCIO; there, add minColor macOS Fix by hand on a guide
layer. The panel reads the project only when it gains focus or after a click.

## License

GPL-3.0-only (see [LICENSE](LICENSE)). Copyright (C) 2026 cbkow. The rendering
is derived from OpenDRT v1.1.0 by Jed Smith, GPLv3.
