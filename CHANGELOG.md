# Changelog

## 0.1.4 (2026-10-25)

- **Effect Controls show only what describes the media**: gamuts, transfers,
  display encodings, peak. The OpenDRT looks, tonescale presets and parameters,
  and the Output's Rendering, Surround, Clamp and Peak are hidden. Nothing
  renders differently: hidden controls keep their values, so projects saved
  with them render as before, and the Input's OpenDRT inverse transfers (the
  Netflicker preset) are unchanged.
- **minColor Grade eyedroppers** pick the colour that is actually in the comp: a
  pick is decoded by the Grade's Display Encoding (the Output in reverse) instead
  of through the OpenDRT inverse. Display Encoding now defaults to *None - Linear
  / Working Gamut*, right for OCIO projects; with a minColor Output above the
  Grade, set it to the Output's encoding.
- Retired controls that were meant to be hidden (the Grade's Contrast Pivot
  (nits)) no longer show.
- **minColor Grade, reworked** (grades made with 0.1.3 look different):
  - Wheels: the puck ranges over the whole wheel, follows the mouse 1:1, and a
    click puts it (or the exposure thumb) where you click.
  - Tint is luminance-neutral (it used to darken blues and brighten greens by up
    to 10 %) and twice as strong at full, and lands exactly on the wheel's hue,
    the same hue scale as the HSL Secondary (a hue-direction error of up to a few
    degrees near magenta is fixed).
  - Zones share each pixel out in full (their weights sum to one): the same push
    on all six zones equals that push globally, and a zone no longer drags its
    neighbours at 68 %. Falloff defaults to 1 stop (was 2).
  - Temperature and Tint are a white-balance shift (Bradford, along the Kelvin
    line) that keeps grey's brightness, instead of plain channel gains.
  - HSL Secondary key bars: H, S and L gradient bars with range and softness
    handles and an on/off per row, drawing the key they select; one softness per
    row replaces the single Softness, and the softness now feathers outward from
    the range instead of straddling its ends.
  - Key Blur (px) for the HSL Secondary: a Gaussian blur of the key on the CPU,
    Metal and CUDA. The Grade is no longer flagged pixel-independent.

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
