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
| **minColor Output** | The rendering, on an adjustment layer at the top: look, tonescale, display encoding. Two renderings (Un-tone-mapped by default, or the OpenDRT-derived one) and two modes (View, Render) sharing one look. |
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

Manual and passive: it never reads or reports the project's colour settings
(AE's scripting answers for those can be stale), and nothing a frame renders
depends on it. **Add Output** puts an adjustment layer with minColor Output at the
top of the active comp (under a macOS Fix layer if there is one), once per comp,
as one undo step. **Fix OCIO** saves the project, writes the shim into
`minColor/` next to the `.aep`, makes it the project's OCIO config (OCIO on; on
an Adobe-engine project that changes how footage is interpreted), sets the
working space to minColor Output and reopens the project (undo history is
cleared; the saved file before the change is kept in `minColor/`). AE has no API
that sets an OCIO working space, so the panel writes it into the saved `.aep`.
Press it again after moving the project. In an Adobe-engine project, add minColor macOS Fix by
hand on a guide layer instead.

**Apply In** sets minColor Input on the selected footage layers of the active comp
from `minColor/in.json` (written from the panel's starter the first time); the
first rule matching the file extension wins: EXR as ACEScg linear, stills and
graphics as sRGB, video as Rec.709 / Rec.1886 at Full range (AE already expands
video levels when it decodes; measured on 26.5), R3D and ARRIRAW as the camera log
the importer is assumed to decode to. It adds the effect first in the stack where
a layer has none, shows the changes before applying, applies them as one undo
step, and reports every selected layer, including the ones it skipped and why.
**In Rules…** edits those rules for the project in a table (extensions, and
Gamut / Transfer / Range from the Input's own menus; add, remove, reorder, reset to
the defaults) and saves `minColor/in.json`.

## License

GPL-3.0-only (see [LICENSE](LICENSE)). Copyright (C) 2026 cbkow. The rendering
is derived from OpenDRT v1.1.0 by Jed Smith, GPLv3.
