# Install

minColor has three parts. The effects are required; the panel and the presets are
conveniences.

| Part | macOS | Windows |
| --- | --- | --- |
| Effects | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` (`.plugin` bundles) | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` (`.aex` files) |
| Panel | `~/Library/Preferences/Adobe/After Effects/27.0/Scripts/ScriptUI Panels/minColor.jsx` | `%APPDATA%\Adobe\After Effects\27.0\Scripts\ScriptUI Panels\minColor.jsx` |
| Presets | `~/Documents/Adobe/After Effects 2027/User Presets/minColor/` | `Documents\Adobe\After Effects 2027\User Presets\minColor\` |

The panel and preset folders carry After Effects' version: `27.0` and
`After Effects 2027` above are those of After Effects 2027; for another version
use its own (`26.5` and `After Effects 2026`, and so on).

**Updating from 0.1.5 or earlier:** remove the old `minColor` effects folder
before copying the new one. The first Grade's file was renamed (it is now
*minColor Grade (Legacy)*), and the file with its old name is the new Grade.
Projects and presets made with the first Grade keep working.

Tested with After Effects 2027 (27.0) on macOS (Apple silicon, Metal). On
Windows (NVIDIA, CUDA) this version is checked against the CPU by its test
suite, and has not yet been run inside After Effects; versions up to 0.1.5 were
tested there in After Effects 2026 (26.5). The effects run on the CPU everywhere
else.

## After installing

1. Restart After Effects.
2. The effects are under **Effect > minColor**.
3. Open the panel from **Window > minColor.jsx** and dock it.
4. The presets are under **Effects & Presets > Animation Presets > minColor**
   (use the panel menu's *Refresh List* if they do not show).

Turn on **Preferences > Scripting & Expressions > Allow Scripts to Write Files
and Access Network**: the panel writes the OCIO config and its rules next to your
project.

## GPU

- **macOS**: Metal, whenever the project renders on the GPU.
- **Windows**: CUDA, when **File > Project Settings > Video Rendering and Effects**
  is *Mercury GPU Acceleration (CUDA)*. Under any other renderer the effects run
  on the CPU with the same results.

Use **32 bpc** projects. The effects work at 8 and 16 bpc, but those depths clip
at 1.0, so there is no HDR and no highlight headroom.

## Building from source

See the [README](../README.md#build). You need Adobe's After Effects SDK, which is
not part of this repository.

## Uninstall

Delete the three locations above and restart After Effects.
