# ae/ — After Effects

Two effects from one core, shaped like the old fnord OpenColorIO plugin: each
declares its own input on its popups, transforms whatever pixels reach it, and
never asks AE what the layer or the project is.

| Effect | Role | Put it on |
| --- | --- | --- |
| **minColor Input** | Interpret media: Input Gamut + Input Transfer to a linear working gamut | Footage layers |
| **minColor Grade** | Colour correction in the working gamut, zones defined through the DRT | Between Input and Output: on a layer, or an adjustment layer below Output |
| **minColor Output** | Picture formation: working-gamut linear in, display-encoded out | An adjustment layer on top, a precomp, or any layer you want rendered |

## The comp around them

This is the whole colour pipeline; AE's own colour management is neither used
nor fought. Set it up once per project:

1. Project Settings > Color: colour management **off**, depth **32 bpc**.
2. **minColor Input** on each footage layer, Input Gamut / Transfer set to what the
   file is: LogC4 in ARRI Wide Gamut 4, S-Log3 in S-Gamut3.Cine, EXR as ACEScg
   Linear, a graphic or a master as 2.2 power / Rec.1886 / PQ in its primaries.
   Whether the Output then *renders* the comp (View: OpenDRT, for scene-referred
   work) or *shows* it (View: Un-tone-mapped, pictures leave as they came in) is
   the Output's View row, the same choice as an OCIO viewer's. The **OpenDRT
   inverse** transfers are back on the Input as a test (2026-10-03): a finished
   delivery through them with the same Display and Look as the Output on top
   round-trips unchanged through the OpenDRT view. Working Gamut left at ACEScg. Put it on the footage
   layer itself rather than an adjustment layer above several clips, so each clip
   is linearised before it composites. A prerender out of any comp that carried a
   display transform is a delivery, not linear.
3. Composite in linear ACEScg.
4. **minColor Output** on an adjustment layer at the top: Input Gamut ACEScg, Input
   Transfer Linear, pick a Display Encoding and a Look.
5. View the comp with **no** display transform (View > Display Color Management
   off, or Un-tone-mapped), and render with the output module passing the working
   space through. The effect's output is already display-encoded; anything AE
   adds after it is applied twice.

It is exactly a Nuke script: Read-node colourspaces on the media, OpenDRT before
the viewer.

**macOS viewport fix (a hack, on purpose).** `ocio/mincolor-viewport-shim.ocio` is a
config whose only job is AE's viewport on macOS. Pin it in Project Settings > Color
(OCIO) and pick the display for your machine: *macOS (AE viewport fix)* re-encodes
the Output's codes for AE's Display P3 viewport (what a studio's P3 2.2 house display does),
*Windows (passthrough)* changes nothing. It assumes the Output's View encoding is
**sRGB Display - 2.2 Power / Rec.709**, and it is a viewer transform, so renders never
see it. It describes no scene: its working space is a label, and AE is assumed to pass
pixels through.

**minColor macOS Fix** is the same correction as an effect with no settings, for
projects that do not pin the shim: decode 2.2, Rec.709 -> P3-D65, encode 2.2,
identical to the shim's macOS view (the probe checks). Put it on an adjustment
layer at the very top and make that layer a **Guide Layer** (Layer > Guide Layer):
guide layers show in the viewer and are skipped by the Render Queue and AME. An
effect cannot tell a preview from a render, so on an ordinary layer it would be
baked into the output. Same contract: the Output's View encoding is sRGB Display
(2.2 power, Rec.709).

## minColor Output controls

**Mode: View | Render.** The look rows (Look, Tonescale, Creative White, the
StickShift groups) are shared: what was approved on the monitor is what
renders. Only the encoding differs per destination: in View mode the rows
below and the Display group apply; in Render mode the **Render** block (its
own Encoding preset, Gamut, EOTF, Peak, Surround and Rendering; default
ACES 2065-1 linear, Un-tone-mapped) replaces them,
and the viewer shows the delivery encoding so it can be checked before it is
written. AE gives an effect no "this is the final render" signal, so the
switch is yours: flip to Render before the queue, back to View after (or keep
two Outputs, the preview on a Guide Layer, which AE skips on render). A
scene-referred delivery (ACES 2065-1, ACEScg) is Render Encoding "None -
Linear / ACES 2065-1" with Rendering Un-tone-mapped: no look goes into those
pixels by definition, the recipient applies their own.

**Rendering**, as an OCIO display has views: *OpenDRT* renders the scene (everything
below); *Un-tone-mapped* is the Input's conversion run backwards, the gamut
matrix and the display's curve and nothing else: no tonescale, no purity, no
clip, scene 1.0 at display white on a power curve and at 100 nits on PQ and
HLG. It matches the "Un-tone-mapped" view of AE's own ACES config to four
decimals (the probe checks), so a picture that came in through the Input
(graphics, a master, decoded as 2.2 power or Rec.1886) leaves as itself, and
CG above 1.0 passes through for whatever is downstream. The look rows grey
out in that view. This is the pass-through the Input's "inverse" transfers
were trying to be, put where it belongs.


Top block, as in the Nuke node and the DCTL's preset mode:

- **Input Gamut**, **Input Transfer**: the DCTL's 15 gamuts and 10 transfer functions,
  plus six display-referred decodes this port adds (Rec.1886, sRGB, 2.2 power,
  BT.709 camera, PQ, HLG) because an AE comp is mostly deliveries, not camera files.
- **Display Encoding**: the nine DCTL presets, default **sRGB Display - 2.2 Power /
  Rec.709** (what the macOS Fix and the viewport shim expect). Picking one writes
  Display Gamut, Display EOTF and Peak Luminance (100 for SDR, 1000 for PQ/HLG),
  not Surround.
- **Peak Luminance**, **HDR Grey Boost**, **HDR Purity**, **Grey Luminance**.
- **Look**: Standard, Arriba, Sylvan, Colorful, Aery, Dystopic, Umbra, Base.
  Picking one writes every look parameter below, including Creative White and the
  tonescale, and sets Tonescale back to "Use Look".
- **Tonescale**: the 13 DCTL tonescale presets, writing only the Tonescale group;
  **Use Look** puts the current Look's tonescale rows back (under a Custom look
  there is nothing to go back to, and it leaves the rows alone).
- **Surround (viewing room)**, directly under Display Encoding, default **Dark**:
  the look's contrast as authored. Dim divides the tonescale contrast by 1.05,
  Bright by 1.10 (flatter, lifting the shadows and grey). Upstream's display presets
  write the display type's standard room (Rec.1886 dim, sRGB and Display P3 bright,
  PQ and DCI dark); here no preset touches it, so a preset change never flattens
  the picture behind your back. Set your room once if Dark is not it. The Render
  block's Render Surround works the same way (default Dark), so a delivery
  renders in the room the view was judged in.
- **Display Encoding** also offers **None - Linear / Working Gamut**, **/ ACES
  2065-1**, **/ ACEScg**, **/ Rec.2020** and **/ Rec.709** (not in upstream): no
  encoding, the light stays linear in that gamut, Clamp off. With View on Un-tone-mapped the Output is then an exact identity;
  with View on OpenDRT it hands over the rendered picture as linear ACEScg (a
  baked-look EXR, or a linear hand-off). "Working Gamut" is also in the Display
  Gamut list for hand-set encodings.
- **Creative White**, **Creative White Limit**.

Below, collapsed, the StickShift groups in Nuke-tab order: Tonescale, Purity,
Brilliance, Hue Shift, Display. Editing any slider after a preset leaves the
preset popup showing the preset name; the popup is a command, not a state.

Presets write parameters through AE's own mechanism: an animated target gets a
keyframe at the current time, an unanimated one just changes.

## minColor Input controls

Input Gamut, Input Transfer, Working Gamut (any of the input gamuts; ACEScg by
default, the camera gamuts at the end of the list), Input Range (Full, or Limited
when the host hands over video-range codes unexpanded, 64..940 of 1023; AE is not
reliable about this), then the same look rows as Output: HDR sliders, Look, Tonescale, Creative White and the StickShift groups.
There is no Display Encoding row and no Display group on Input. The look rows are
greyed out and ignored for every transfer except the six inverse entries:

### Input Transfer = OpenDRT inverse: Rec.1886 / sRGB Display / Display P3 / Rec.2100 PQ / Rec.2100 HLG / Dolby PQ / P3-D65

For finished deliveries: a graded plate, a client-approved render, anything that
already went through a picture formation. A plain EOTF decode puts such a pixel
into the comp as flat scene data, and the Output effect then compresses its
highlights a second time (a plate's white lands at code 0.75). The inverse
instead takes the pixel back to the scene value that this look would have
rendered it from through the named encoding, the way an ACES config's
"Output - Rec.709" inverse ODT does. The entry says what the file is; set Look,
Tonescale and Peak Luminance to match the Output effect on top; Input Gamut is
ignored. **Peak Luminance must be at least the master's peak** (its MaxCLL or
mastering display: 1000, 2000, 4000 nits; the range goes to 10000). Choosing an
inverse entry sets it to that encoding's default (1000 nits for PQ and HLG, 100
for the SDR ones), as the Output's Display preset does; it stays editable, so
raise it for a brighter master. The inverse always assumes the master was
rendered at a Dark surround, the Output's default; there is no Surround row on
the Input (a master rendered at Dim or Bright comes back 5-10 % too contrasty). A pixel
brighter than the peak has no source at all: the inverse can only return "as
bright as it goes", every codec block lands somewhere different, and the region
shows as jagged, dancing white or colour. SDR masters cannot exceed their peak;
PQ and HLG masters do all the time. A 2.4-gamma Rec.709 master is "OpenDRT inverse: Rec.1886". The two PQ
entries differ only in the container: "Rec.2100 PQ" reads Rec.2020 primaries (the
P3-limited Rec.2100 display), "Dolby PQ / P3-D65" reads P3-D65 primaries (the Dolby
display preset); pick the one the master was encoded in, or saturation comes back wrong.

What it guarantees, from the probe: forward(inverse(pixel)) returns the pixel
within one 8-bit step everywhere the display holds unclipped, and neutral and
low-saturation colours get their exact scene value back. Saturated colours are
gamut-compressed by the forward and can be many-to-one; they round-trip to the
screen, but their recovered scene value is one valid choice among several.
Clipped pixels have no scene value and get a best effort: a colour the forward
cannot make is pulled toward its own neutral until it can, so nothing comes back
as a stray.

**Highlight Cap (of peak)**, 0.5 to 1 (off). The forward is many-to-one for
bright saturated colour (everything bright is pulled toward white), so a display
value with a channel in the top fifth of the range has a whole family of
sources, members hundreds apart with channels of either sign, and an exact
inverse picks one by accident: neighbouring pixels land in different members,
and any grade between Input and Output draws a contour along the edge of every
clipped highlight. SDR plates rarely show it (white sits a little under the
ceiling); a PQ master's white sits at its peak, right against it, and does. With
the cap set, display values are held under that fraction of Peak Luminance, and
in the band from 80 % of the cap up to it the inverse is pulled continuously
onto a single, bounded source, so the rim of a clipped highlight comes back
smooth and a little less saturated than the master had it. Colour whose
brightest channel is below the band is still inverted exactly. At 0.9, white
renders about three code steps darker; use it on PQ and HLG masters.

It is iterative (a neutral exact inverse of the tonescale, then damped Newton on
the whole transform, up to a few dozen forward evaluations per pixel): fast on
Metal, noticeably slower than the other transfers on the CPU path. Not part of
upstream OpenDRT.

## minColor Grade

A colour corrector that sits between Input and Output, in the linear working
gamut, whose luminance zones are defined through the OpenDRT tonescale. Not part
of upstream OpenDRT.

- **Wheels**, at the top, no group: one control, "Zone Correction", with two
  lines of three wheels (Black, Dark, Shadow over Light, Highlight, Specular),
  Lumetri-shaped; they follow the width of the Effect Controls panel, from
  0.75x to 1.5x their natural size, and the control grows and shrinks with them. A single seventh wheel,
  "Secondary Correction", sits in the HSL Secondary group over its own
  exposure and tint sliders. Each wheel is pure UI over that zone's
  sliders: the puck's angle is Tint Hue (red at the top, clockwise), its radius
  is Tint Amount, the bar on the left is Exposure (+-4 stops). Drag is relative
  and geared down (the puck moves 0.4 of the mouse, the bar's thumb 0.5), and it is
  measured against the wheel's size, so a wider panel is a finer control;
  Shift drags four times finer still, double-click resets the tint or the exposure, one undo step
  per drag. The ring is coloured by the DRT's own hue direction, so the hue you
  grab is the hue the grade pushes toward. The readout under the label shows the
  zone's exposure and tint when they are not zero.
- **Working Gamut**: what the comp is in (ACEScg by default).
- **Global**: Exposure (stops), Contrast with a Pivot in stops over mid grey
  (default 0), Saturation, Temperature, Tint.
- **Six zones**, Black, Dark, Shadow, Light, Highlight, Specular, each with
  Exposure, Saturation, Tint Hue and Tint Amount. The numeric side of the wheels.
- **Zone Ranges**: the five boundaries in scene stops over mid grey (defaults
  -3, -1.5, 0, +1.5, +3) and a Falloff in stops (default 2). A pixel's zone
  comes from its tonescale norm measured against 0.18 grey, so the zones are
  evenly spaced on both sides of grey and the top three split real highlight
  range. A zone has full weight across its band and feathers out over the
  Falloff beyond each boundary, so neighbours overlap like Lumetri's bands: a
  push on Light reaches two stops into Shadow and Highlight, and two adjacent
  pushes add up where they meet. Tighten the Falloff for a scalpel.
- **HSL Secondary**: a per-pixel qualifier in the DRT's own hue and purity terms
  plus a level window in stops over grey, with Softness and Show Mask, and the same four
  correctors applied through the mask (and their own wheel). Three eyedroppers,
  Lumetri-style: **Set Colour** centres the three windows on the pick and
  enables the secondary, **Add to Range** widens whichever window the pick
  falls outside of, **Remove from Range** narrows the window that excludes the
  pick with the least change. A pick is what the viewer shows, so it goes back
  through the DRT reference's display decode and the inverse DRT to the scene,
  and then global Exposure and Temperature are taken back off, before it is
  measured against the source like the mask is; that assumes nothing else sits
  between the Grade and the Output above it, and a pick is exact with the zones
  at rest (key first, then grade). The swatches display 8-bit, the pick itself is float.
  No mask blur or denoise: those are spatial, and everything here is per-pixel.
- **DRT Reference**: Display Encoding, Peak, Grey Boost, Grey Luminance, Look,
  Tonescale and the tonescale sliders. Match them to the Output on top; since
  the pivot and the level window moved to stops they serve only the eyedroppers
  (the pick has to be undone through the rendering that made it). Purity,
  brilliance and hue-shift settings do not affect the zones and are not here.

Order of operations: everything that *selects* pixels (the zone weights, the
secondary's key) is measured on the source pixel; then temperature and tint,
exposure, contrast, zones, saturation, secondary. So Exposure never moves a
pixel into another zone, a zone's tint never pushes a colour out of the
secondary's window, and Contrast never re-shapes a zone's push. Contrast works
on the tonescale norm so hues hold.

## Build and install

```
cmake -S . -B build -DMINCOLOR_AE_SDK=/path/to/AfterEffectsSDK   # or symlink it at private/sdk/AfterEffectsSDK
cmake --build build --target install_ae
```

Bundles land in `/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor/`.
Restart AE; the effects are under Effect > minColor. Remove the folder to uninstall.

CPU path: the C++ core through AE's iterate suites, 8, 16 and 32 bpc (8 and 16 are
converted to float and clamped back). GPU path: Metal, the same kernel source
as the CPU path, compiled once per device with fast-math off. Alpha is handled
straight: unpremultiply, transform, premultiply.

## What is not there yet

- User presets (Add / Remove Preset against the shared JSON). Built-in presets only.
- Windows (CUDA / DirectX kernels from the same source).
- Greying out a group's sliders when its Enable is off.
