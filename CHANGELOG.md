# Changelog

## 0.1.3 (2026-10-05): first release

Six After Effects effects, a panel and an OCIO config for working in linear light.
After Effects 2026 (26.5); macOS (universal, Metal) and Windows (x64, CUDA), CPU
rendering everywhere.

- **minColor Input**: camera logs, display encodings and linear files into the
  comp's working space; video range; OpenDRT inverse transfers for finished
  renders.
- **minColor Output**: display and delivery encodings, un-tone-mapped by default
  (a pure conversion matching After Effects' own ACES "Un-tone-mapped" view), or
  a rendering derived from OpenDRT v1.1.0 with its looks and tonescales.
- **minColor Grade**: exposure, contrast, six luminance zones with colour wheels,
  an HSL secondary with eyedroppers.
- **minColor Knee**: a BT.2390 highlight knee for HDR content heading somewhere
  dimmer.
- **minColor AgX**: parametric, HDR-capable AgX, Blender-compatible at its
  defaults (median 0.05 % from Blender 5.2's AgX view in SDR).
- **minColor macOS Fix**: corrects After Effects' viewer on a Mac without OCIO.
- **Panel**: Set OCIO (OCIO config, working space and 32 bpc in one step), In
  Rules (per-project file-extension rules), Apply In, Add Output.
- **OCIO config**: five linear working spaces, sRGB, Rec.709 BT.1886 and Display
  P3 for delivery, viewer displays including the macOS viewport fix; generated
  from the same matrices as the effects.
- **Animation presets**: 14 one-click Input, Output and AgX settings.
- Tests: the core against the unmodified upstream OpenDRT DCTL, the OCIO config
  against the core, the Metal and CUDA kernels against the CPU, and the effects
  in After Effects on both platforms.
