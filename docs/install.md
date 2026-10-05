# Install

minColor has three parts. The effects are required; the panel and the presets are
conveniences.

| Part | macOS | Windows |
| --- | --- | --- |
| Effects | `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/` (`.plugin` bundles) | `C:\Program Files\Adobe\Common\Plug-ins\7.0\MediaCore\minColor\` (`.aex` files) |
| Panel | `~/Library/Preferences/Adobe/After Effects/26.5/Scripts/ScriptUI Panels/minColor.jsx` | `%APPDATA%\Adobe\After Effects\26.5\Scripts\ScriptUI Panels\minColor.jsx` |
| Presets | `~/Documents/Adobe/After Effects 2026/User Presets/minColor/` | `Documents\Adobe\After Effects 2026\User Presets\minColor\` |

Tested with After Effects 2026 (26.5) on macOS (Apple silicon, Metal) and
Windows (NVIDIA, CUDA). The effects run on the CPU everywhere else.

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
