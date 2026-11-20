# minColorAE

Colour tools for Adobe After Effects that work in linear light: seven effects and
a panel. After Effects 2027 on macOS (Metal) and Windows (CUDA), CPU everywhere
else. Version 0.1.8.

<p>
<img src="docs/images/view_raw.jpg" width="49%" alt="A linear render shown as it is">
<img src="docs/images/view_agx.jpg" width="49%" alt="The same frame through minColor AgX">
</p>

*One frame as a plain conversion, and through minColor AgX.*

## Install

Download the zip for your system from
[Releases](https://github.com/cbkow/minColorAE/releases), quit After Effects, and
copy its three parts:

| Part | macOS | Windows |
| --- | --- | --- |
| Effects | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` |
| Panel | `~/Library/Preferences/Adobe/After Effects/27.0/Scripts/ScriptUI Panels/` | `%APPDATA%\Adobe\After Effects\27.0\Scripts\ScriptUI Panels\` |
| Presets | `~/Documents/Adobe/After Effects 2027/User Presets/minColor/` | `Documents\Adobe\After Effects 2027\User Presets\minColor\` |

Start After Effects and turn on **Preferences > Scripting & Expressions > Allow
Scripts to Write Files and Access Network**. The panel is **Window >
minColor.jsx**; the effects are under **Effect > minColor**.

Updating from 0.1.5 or earlier: remove the old `minColor` effects folder first.

## Use

<img src="docs/images/panel.png" width="240" alt="The minColor panel">

1. **Save the project.** The panel keeps its files in a `minColor` folder next
   to it.
2. **Press Set OCIO** and pick a working space. ACEScg is the usual one. The
   panel sets 32 bpc and reopens the project.
3. **Select your footage layers and press Apply In.** Each gets minColor Input,
   set from its file type. Check the Input on anything unusual. On a multilayer
   EXR, extract the beauty layer first.
4. **Pick your display** at the bottom of the Composition panel:

   | You are on | Choose |
   | --- | --- |
   | A Mac, monitor left in its native mode | **macOS native monitor (AE viewport fix)** |
   | A Mac, monitor in sRGB or Display P3 mode | **sRGB** |
   | Windows, an sRGB monitor | **sRGB** |
   | Windows, a P3 monitor in P3 mode | **Display P3** |
   | A Rec.709 reference monitor | **Rec.709 (BT.1886)** |

5. **Grade, and shape the highlights if you want to**, with minColor Grade,
   minColor Knee or minColor AgX on adjustment layers.
6. **Use minColor Output to render.** Add Output puts it at the top of the comp.

Work in 32 bpc. Linear light above 1.0 clips at 8 and 16 bpc.

## The effects

| | |
| --- | --- |
| [minColor Input](docs/reference.md#mincolor-input) | Says what a file is and converts it to the working space. |
| [minColor Output](docs/reference.md#mincolor-output) | Encodes for a display or a delivery. |
| [minColor Grade](docs/reference.md#mincolor-grade) | Primaries wheels, HDR zone wheels and an HSL secondary. |
| [minColor Knee](docs/reference.md#mincolor-knee) | Rolls off highlights that are brighter than the target. |
| [minColor AgX](docs/reference.md#mincolor-agx) | AgX picture formation, adjustable; matches Blender's at its defaults. |
| [minColor macOS Native Fix](docs/reference.md#mincolor-macos-native-fix) | Corrects the viewer on a Mac whose monitor is in its native mode. |
| [minColor Grade (Legacy)](docs/reference.md#mincolor-grade-legacy) | The first Grade, for projects made before 0.1.6. |

Controls, the panel's buttons, presets and troubleshooting are in the
[reference](docs/reference.md). Changes by version are in the
[changelog](CHANGELOG.md). To build from source, see [BUILDING.md](BUILDING.md).

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
