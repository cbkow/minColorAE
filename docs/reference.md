# minColor reference

[The panel](#the-panel) · [Input](#mincolor-input) · [Output](#mincolor-output) ·
[Grade](#mincolor-grade) · [Knee](#mincolor-knee) · [AgX](#mincolor-agx) ·
[macOS Native Fix](#mincolor-macos-native-fix) · [Grade (Legacy)](#mincolor-grade-legacy) ·
[Presets](#presets)

Everything is linear light, with 1.0 at 100 nits.

## The panel

**Window > minColor.jsx**. It only sets things up; nothing renders through it.

- **Set OCIO**: asks for a working space, sets 32 bpc, writes the config into
  `minColor/` next to the project and reopens the project (undo history is
  cleared; a backup of the project goes in `minColor/`). Press it again after
  moving or copying a project. *Unmanaged* makes After Effects pass pixels
  through, so minColor Output's Display Encoding is what you see and render.
- **Apply In**: puts minColor Input on the selected footage layers, set from each
  file's extension. It lists the changes and asks first.
- **In Rules…**: the extension rules Apply In uses, for this project.

  | Files | Gamut | Transfer |
  | --- | --- | --- |
  | exr | ACEScg | Linear |
  | hdr | Rec.709 | Linear |
  | Stills and graphics | Rec.709 | sRGB |
  | Video (mov, mp4, mxf…) | Rec.709 | Rec.1886 |
  | r3d | RED Wide Gamut RGB | RED Log3G10 |
  | ari / arx | ARRI Wide Gamut 3 / 4 | ARRI LogC3 / LogC4 |

- **Add Output**: puts minColor Output on an adjustment layer at the top of the
  active comp. One per comp.

## minColor Input

On the footage layer, first in its effects.

- **Input Gamut**, **Input Transfer**: what the file is.
- **Working Gamut**: the comp's working space. Apply In sets it.
- **Input Range**: *Full*. Use *Limited (video)* only when a file looks lifted
  and flat.
- **OpenDRT inverse** transfers: for a finished, graded master. Pick the entry
  that names its encoding and set **Peak Luminance** to at least the master's
  peak. **Highlight Cap** about 0.9 on PQ and HLG masters.

Cameras whose files do not say what they are:

| Footage | Input Gamut | Input Transfer |
| --- | --- | --- |
| Apple Log (iPhone) | Rec.2020 | Apple Log |
| Apple Log 2 | Apple Wide Gamut | Apple Log |
| Canon Log 2 / 3 | Canon Cinema Gamut | Canon Log 2 / Canon Log 3 |
| Blackmagic, Gen 5 | Blackmagic Wide Gamut Gen 5 | Blackmagic Film Gen 5 |
| DJI D-Log | DJI D-Gamut | DJI D-Log |

## minColor Output

On an adjustment layer at the top of the comp. It has no tonescale: anything
brighter than display white clips. Put a Knee or AgX under it to roll off.

- **Input Gamut**, **Input Transfer**: what the comp holds, normally the working
  space, linear.
- **Display Encoding**: the display or delivery to encode for: sRGB, Rec.1886,
  Display P3, Rec.2100 PQ and HLG, DCI, or a linear hand-off. *None - Linear /
  Working Gamut* passes the picture on unchanged.
- **Display**: the gamut and curve separately, for a pairing no preset has.

minColor writes no HDR metadata into a file.

## minColor Grade

Between Input and Output, on a layer or an adjustment layer. Set **Working
Gamut** to the comp's.

Every wheel, bar and box is a relative drag. **Shift** drags finer.
**Double-click** resets the control under the mouse. The same values are sliders
in the twirl under each row, for typing, keyframes and presets.

- **Primaries**: **Lift**, **Gamma**, **Gain**, **Offset**. The ball pushes
  colour; the strip under it moves all three channels. Offset is an even
  exposure or white-balance change.
- **HDR**: **Dark**, **Shadow**, **Light** and **Global**, with **Black**,
  **Highlight** and **Specular** in **More Zones**. Each has **Exp** in stops,
  **Sat** and a colour ball. A zone also has a **range** in stops over mid grey
  and a **falloff** ("f"); drag either sideways. A larger falloff makes a softer
  edge.
- **Adjustments**: **Temp**, **Tint**, **Hue**, **Contrast** about a **Pivot**,
  **Black Offset**.
- **HSL Secondary**: **Enable Secondary**, then pick with **Set Colour**, **Add
  to Range**, **Remove from Range**. The **H**, **S** and **L** bars set the key:
  handles above for range, below for softness. **Key Blur** softens it; **Show
  Mask** shows it. **Correction Wheels** gives one wheel or three (Shadows,
  Midtones, Highlights).

## minColor Knee

On an adjustment layer at the top, under the Output. Not needed with AgX.

- **Source Peak (nits)**: the brightest content that should still map. Set it to
  the content.
- **Target Peak (nits)**: 100 for SDR, or the HDR delivery's peak.
- **Auto Knee Start (BT.2390)**: on by default.
- **Knee Start** (Auto off): where the roll-off begins, as a fraction of the
  target.

## minColor AgX

On an adjustment layer over the comp's content, under the Output.

- **Working Gamut**: the comp's working space.
- **Target Gamut**: *Rec.709 (sRGB)*, *P3-D65*, or *Rec.2020* for none.
- **Peak Luminance (nits)**: 100 for SDR; higher for HDR.
- **White / Black Relative Exposure (EV)**: the scene range that reaches the top
  and the floor. Raise White to hold more highlight.
- **Contrast**, **Toe Power**, **Shoulder Power**: the curve.
- **Advanced**: **Hue Restore**, **HDR Purity**, **Purity Restore (Outset)**.

## minColor macOS Native Fix

No settings. Only for a Mac whose monitor is left in its native mode, in a
project where minColor Output encodes for the display. Put it on a **guide
layer** at the very top, so it shows in the viewer and never renders. In a
project made with Set OCIO, pick the **macOS native monitor (AE viewport fix)**
display instead. A monitor in a mode that matches its profile needs neither.

**macOS Video native monitor (AE viewport fix)** shows a Rec.709 video file as a
Mac's players show it, on the same kind of monitor.

## minColor Grade (Legacy)

The first Grade, under *minColor Legacy*. Projects made with it open and render
as they did. Its six zone wheels set a tint and an exposure per zone, the
boundaries under them are in stops over mid grey, and the **Falloff** bar sets
how far each zone blends into its neighbours. Use minColor Grade for new work.

## Presets

**Effects & Presets > Animation Presets > minColor.** All assume an ACEScg
working space.

| Effect | Presets |
| --- | --- |
| Input | mCin_sRGB, mCin_rec709video, mCin_ACEScg, mCin_ACES2065-1, mCin_linear_rec709, mCin_linear_rec2020, mCin_Netflicker (PQ / P3-D65 review files) |
| Output | mCout_sRGB, mCout_rec709video, mCout_ACEScg, mCout_ACES2065-1 |
| AgX | mCagx_sRGB, mCagx_P3, mCagx_HDR1000 |
