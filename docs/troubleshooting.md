# Troubleshooting

**The viewer is dark and flat after Set OCIO.** The viewer's display is *None*.
Pick your display at the bottom of the Composition panel
([Quick start, step 4](quick-start.md#4-pick-the-viewers-display)).

**A multilayer EXR shows black.** After Effects shows nothing until a layer is
pulled out: put EXtractoR (or After Effects' channel extraction) first on the
layer, then minColor Input.

**Colours look oversaturated on a Mac.** You are viewing without the macOS
correction: pick **macOS (AE viewport fix)** in an OCIO project, or add **minColor
macOS Fix** on a guide layer in an Adobe-engine project.

**Highlights clip.** Un-tone-mapped is a straight conversion: anything above
display white clips on screen and in display-referred renders. Add
[minColor Knee](effects/knee.md) or [minColor AgX](effects/agx.md), or use the
Output's OpenDRT rendering. Check the project is 32 bpc; 8 and 16 bpc clip at 1.0.

**The project lost its OCIO config** (opened from another location, or the
`minColor` folder did not come along). After Effects silently falls back to its
Adobe colour engine; the effects still render, only the conversions in the
viewer and the Output Module are gone. If the folder comes back before the
project is saved, the project opens on OCIO again by itself. If it was saved in
the fallback, press **Set OCIO** again.

**The minColor effects are not installed on this machine.** The project opens and
keeps every instance with all of its settings; they render as if off. Installing
the effects brings them back unchanged, even after saving without them. The
panel's Apply In and Add Output refuse to run until the effects are installed.

**The panel is missing.** Nothing changes in the project: the panel only adds
effects and writes files. Reinstall it and restart After Effects, or open it from
**Window > minColor.jsx**.

**Windows: the effects are slow.** They render on the GPU only when the project's
renderer is *Mercury GPU Acceleration (CUDA)* on an NVIDIA card; otherwise they
use the CPU.
