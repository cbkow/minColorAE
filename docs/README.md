# minColor documentation

minColor is a set of After Effects effects and a small panel for working in
linear light: interpret footage into one working space, composite, and deliver
through OpenColorIO, with optional picture formations (OpenDRT, AgX) and a
highlight knee when you want them.

<p>
<img src="images/view_raw.jpg" width="49%" alt="Linear ACEScg EXR viewed un-tone-mapped: highlights clip">
<img src="images/view_agx.jpg" width="49%" alt="The same frame through minColor AgX">
</p>

*The proof footage (a Blender render, linear ACEScg, highlights to 55× diffuse
white, spectral lasers outside ACEScg), left un-tone-mapped, right through
minColor AgX at its defaults.*

## Start here

1. [Install](install.md): the effects, the panel and the presets, macOS and Windows.
2. [Quick start](quick-start.md): an OCIO project from an empty comp to a render, in six steps.

## Guides

- [Color setups](color-setups.md): the three ways to set up a project (OCIO,
  Unmanaged, Adobe engine), the viewer displays, and how deliveries are encoded.
- [The minColor panel](panel.md): Set OCIO, In Rules, Apply In, Add Output.
- [Animation presets](presets.md): one-click Input, Output and AgX settings.
- [Troubleshooting](troubleshooting.md): missing configs, missing effects, black or
  clipped pictures.

## Effects (Effect > minColor)

| Effect | What it does |
| --- | --- |
| [minColor Input](effects/input.md) | Says what a file is (gamut, transfer, range) and brings it into the comp's linear working space. |
| [minColor Output](effects/output.md) | Encodes for a display or a delivery, un-tone-mapped or through an OpenDRT-derived rendering. |
| [minColor Grade](effects/grade.md) | Exposure, contrast, six luminance zones with colour wheels, and an HSL secondary. |
| [minColor Knee](effects/knee.md) | Rolls off highlights brighter than where they are going (HDR into SDR). |
| [minColor AgX](effects/agx.md) | AgX picture formation, parametric and HDR-capable; Blender-compatible at its defaults. |
| [minColor macOS Fix](effects/macos-fix.md) | Corrects After Effects' viewer on a Mac when you are not using OCIO. |
