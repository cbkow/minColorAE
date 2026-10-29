# minColor Grade

A colour corrector for linear light, whose luminance zones are measured with
the norm OpenDRT's tonescale works on. Put it between Input and Output: on a
layer, or on an adjustment layer below the Output.

<img src="../images/ec_grade.png" width="430" alt="minColor Grade controls">

- **Zone Correction** wheels: Black, Dark, Shadow over Light, Highlight,
  Specular. The puck's angle is the tint's hue, its distance from the centre the
  tint amount (the wheel's edge is full); the bar on the left is the zone's
  exposure (±4 stops). Click anywhere on a wheel or bar to put the puck or thumb
  there, drag to move it with the mouse; Shift drags finer, double-click resets.
  A tint changes colour only: the zone keeps its brightness. The wheels follow
  the width of the Effect Controls panel.
  Under each dial is the zone's boundary, in stops over mid grey: "to −3.00 st"
  is where Black hands over to Dark, and so on; the Specular dial shows where it
  starts. Drag a boundary sideways to move it (it stays between its
  neighbours), double-click to reset it. The **Falloff** bar along the bottom is
  how far each zone blends into its neighbours.
- **Working Gamut**: what the comp is in.
- **Exposure**, **Contrast** with a **Contrast Pivot** in stops over mid grey,
  **Saturation**, **Temperature**, **Tint**. Temperature and Tint are a
  white-balance shift: Temperature moves along the warm–cool (Kelvin) line, +1
  about 6500 K to 3900 K, Tint across it toward magenta (+) or green (−), and
  grey keeps its brightness.
- **Zone Values**: the numeric side of the wheels, for typing, keyframing and
  expressions: each zone's Exposure, Saturation, Tint Hue and Tint Amount, then
  the five boundaries in stops over mid grey (defaults -3, -1.5, 0, +1.5, +3)
  and the **Falloff** (default 1 stop). Saturation has no wheel, so a zone's
  saturation is set here. The zones always share a pixel out in full: the same
  push on all six is the same as that push globally, and a zone owns its own
  band (about three quarters at its centre, half at a boundary). Widen the
  Falloff for softer hand-overs.
- **HSL Secondary**: a hue / saturation / level key with Show Mask, its own
  wheel and correctors, and three eyedroppers: **Set Colour**, **Add to Range**,
  **Remove from Range**. The **key bars** under the eyedroppers show and set the
  key, Lumetri-style: one bar each for **H**, **S** and **L**, dimmed where the key
  passes nothing. Drag the handles above a bar to set its range (on the hue bar,
  drag inside the range to slide it), the ones below to set its softness; a click
  moves the nearest handle there, Shift drags finer, double-click resets the row,
  and the check box turns the row off. The numbers are in **Key Values**.
  **Key Blur (px)** softens the key spatially (a Gaussian, in pixels at full
  resolution) so a noisy or edgy selection blends into its surroundings; it
  blurs the selection, never the picture.
- **DRT Reference > Display Encoding**: how a pick is decoded. After Effects hands
  an eyedropper the comp's final value, before the viewer's display: in an OCIO
  project that is the working space itself, so leave the default *None - Linear /
  Working Gamut*. With a minColor Output above the Grade (Unmanaged or
  Adobe-engine projects), set it to the Output's Display Encoding.

Order of operations: the zone weights and the secondary's key are measured on the
incoming pixel; then temperature and tint, exposure, contrast, zones, saturation,
secondary. Exposure never moves a pixel into another zone, and contrast keeps hues.
