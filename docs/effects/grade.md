# minColor Grade

A colour corrector for linear light, whose luminance zones are measured the way
the OpenDRT tonescale sees brightness. Put it between Input and Output: on a
layer, or on an adjustment layer below the Output.

<img src="../images/ec_grade.png" width="430" alt="minColor Grade controls">

- **Zone Correction** wheels: Black, Dark, Shadow over Light, Highlight,
  Specular. The puck's angle is the tint's hue, its distance from the centre the
  tint amount; the bar on the left is the zone's exposure (±4 stops). Shift drags
  finer, double-click resets. They follow the width of the Effect Controls panel.
- **Working Gamut**: what the comp is in.
- **Exposure**, **Contrast** with a **Contrast Pivot** in stops over mid grey,
  **Saturation**, **Temperature**, **Tint**.
- **Black … Specular**: each zone's Exposure, Saturation, Tint Hue and Tint
  Amount (the numeric side of the wheels).
- **Zone Ranges**: the five boundaries in stops over mid grey (defaults -3, -1.5,
  0, +1.5, +3) and a **Falloff** (default 2 stops). Zones overlap like Lumetri's
  bands; tighten the Falloff for a narrower push.
- **HSL Secondary**: a hue / purity / level qualifier with Softness and Show Mask,
  its own wheel and correctors, and three eyedroppers: **Set Colour**, **Add to
  Range**, **Remove from Range**. A pick is taken from what the viewer shows and
  traced back through the rendering, so keep the **DRT Reference** group
  matched to the Output on top.

Order of operations: the zone weights and the secondary's key are measured on the
incoming pixel; then temperature and tint, exposure, contrast, zones, saturation,
secondary. Exposure never moves a pixel into another zone, and contrast keeps hues.
