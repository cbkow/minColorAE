# Colour setups

minColor works in three project setups. The effects behave the same in all of
them; what changes is who encodes the picture for the screen and for the render.

| | OCIO (recommended) | Unmanaged | Adobe engine |
| --- | --- | --- | --- |
| Set with | Panel: **Set OCIO**, a working space | Panel: **Set OCIO**, *Unmanaged* | Project Settings by hand |
| Comp contains | Linear light in the working space | Display-encoded pixels | Display-encoded pixels |
| Viewer encodes with | The config's display | The config's display (macOS fix only) | Nothing (add **macOS Fix** on a Mac) |
| Render encodes with | Output Module *Output Color Space* | **minColor Output** | **minColor Output** |

## OCIO: linear conversions

The project's OCIO config is `minColor/mincolor.ocio`, next to the `.aep`. It has
five linear working spaces (ACEScg, ACES2065-1, Linear Rec.709, Linear P3-D65,
Linear Rec.2020), three encoded spaces for delivery and viewing (sRGB, Rec.709
BT.1886, Display P3), and one view, **Un-tone-mapped**, on five displays.

- **minColor Input** on each footage layer interprets it into the working space.
  The config has no file rules, so After Effects converts nothing on import:
  the Input is the only interpreter.
- Composite in linear light.
- The viewer's display encodes for your screen (see [Quick start, step
  4](quick-start.md#4-pick-the-viewers-display)).
- The Output Module's **Output Color Space** encodes the render.
- **minColor Output** is optional: on top, with Display Encoding *None - Linear /
  Working Gamut*, it adds the OpenDRT rendering and hands back linear light, which
  the viewer and the Output Module then encode as usual.

The config and minColor Input share their colour matrices, so a conversion made
by either one agrees with the other exactly. The ACES spaces reach D65 through
CAT02, as OpenDRT does; After Effects' own ACES configs use Bradford, which
differs by at most about 0.004 on saturated colours and not at all on greys.

## Unmanaged: OCIO as a pass-through

The config is `minColor/mincolor-unmanaged.ocio`, whose working space
"minColor Output" is only a label. Pixels pass through; **minColor Output** at the
top encodes for the display (default *sRGB Display - 2.2 Power / Rec.709*), and
what it writes is what renders. The config's **macOS (AE viewport fix)** display
corrects the viewer on a Mac; **Windows (passthrough)** leaves pixels alone.

## Adobe engine

**Project Settings > Color**: Adobe engine, working space **None**, footage
interpreted as **Preserve RGB**, 32 bpc. After Effects passes pixels through and
the effects do everything:

1. **minColor Input** on each footage layer (Working Gamut ACEScg).
2. **minColor Output** at the top, Display Encoding set to the delivery.
3. On a Mac, **minColor macOS Fix** on a guide layer at the very top, to correct
   the viewer.

## The Mac viewer, and why there is a fix

After Effects' viewer on macOS is a Display P3 surface decoded with a 2.2 power
curve, so a Rec.709 picture sent to it unchanged looks oversaturated. The
**macOS (AE viewport fix)** display sends linear light to it as P3-D65 encoded
2.2, and **minColor macOS Fix** does the same to an Output's sRGB codes. Both are
for the viewer only: a render never carries them. **macOS Video (AE viewport fix)**
shows a Rec.709 BT.1886 file as a Mac's video players show it (decoded with the
sRGB curve: lighter mid-tones, lifted shadows).

## HDR

The effects carry values above 1.0 (100 nits) through everything when the
project is 32 bpc. After Effects' viewer shows only up to 1.0 through OCIO, so
judge HDR with an external monitor or a HDR-capable player. minColor Knee and
minColor AgX both take a peak in nits for HDR deliveries.
