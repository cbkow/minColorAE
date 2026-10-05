# minColor AgX

AgX picture formation, after Troy Sobotka's AgX, from linear scene light in the
working space to linear display light in the same space, 1.0 = 100 nits.

**Blender-compatible at its defaults.** At Peak 100 it reproduces Blender's "AgX"
view (median difference 0.05 % against Blender 5.2); at Peak 1000 with Target
P3-D65, Blender's AgX HDR 1000 view (greys within 0.3 %, colours median 0.6 %).
Unlike a LUT, every part of it can be adjusted. It is not Blender's AgX and
ships none of Blender's files.

<img src="../images/ec_agx.png" width="430" alt="minColor AgX controls">

<p>
<img src="../images/view_raw.jpg" width="49%" alt="Un-tone-mapped">
<img src="../images/view_agx.jpg" width="49%" alt="minColor AgX, defaults">
</p>

*Left un-tone-mapped, right minColor AgX at its defaults.*

Put it on an adjustment layer over the comp's content, under minColor Output and
any macOS Fix layer. In an OCIO project the viewer and the Output Module encode
its result as they would any linear light; in an Unmanaged or Adobe-engine
project, minColor Output with Rendering *Un-tone-mapped* encodes it. AgX is a
complete picture formation: don't stack it with the Output's OpenDRT rendering.
Work in 32 bpc.

- **Working Gamut**: the comp's working space. AgX works in Rec.2020 inside, so
  this tells it what the pixels are; set wrong, saturated colours render wrong.
  Greys do not depend on it.
- **Target Gamut**: the gamut the result has to fit. *Rec.709 (sRGB)* by default,
  *P3-D65* for P3 deliveries and HDR, *Rec.2020* for none. Colours outside it are
  brought in AgX's way (keeping luminance) instead of clipping later.
- **Peak Luminance (nits)**, default 100 (SDR). Above 100, Blender's HDR method:
  grey stays at 18 nits and the shoulder stretches to the peak. Drags 100–4000,
  type up to 10000.
- **White / Black Relative Exposure (EV)**, defaults +6.5 / -10: the scene range,
  in stops from grey, that reaches the top and the floor. Raise White to hold more
  highlight before the shoulder.
- **Contrast**, default 2.4: the curve's slope at grey.
- **Toe Power / Shoulder Power**, defaults 1.5: how hard the curve bends into
  black and into the peak.
- **Advanced**: **Hue Restore** (0.6), **HDR Purity** (0.5; greyed at Peak 100),
  **Purity Restore (Outset)** (1).

Presets: **mCagx_sRGB**, **mCagx_P3**, **mCagx_HDR1000** ([presets](../presets.md)).

AgX is by Troy Sobotka. The Blender version, whose primaries and HDR method this
follows, is by Eary Chow, Mark Faderbauer and Sakari Kapanen. minColor AgX ports
parts of [darktable](https://www.darktable.org/)'s AgX module (GPL-3.0-or-later);
see [CHANGES-AGX.md](../../CHANGES-AGX.md). It is not affiliated with or endorsed
by Blender or darktable.
