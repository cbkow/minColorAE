# Color setups

minColor works in three project setups. The effects behave the same in all of
them; what changes is who encodes the picture for the screen and for the render.

| | OCIO (recommended) | Unmanaged | Adobe engine |
| --- | --- | --- | --- |
| Set with | Panel: **Set OCIO**, a working space | Panel: **Set OCIO**, *Unmanaged* | Project Settings by hand |
| Comp contains | Linear light in the working space | Display-encoded pixels | Display-encoded pixels |
| Viewer encodes with | The config's display | The config's display (the native-monitor fix only) | Nothing (add **macOS Native Fix** on a Mac that needs it) |
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
  Working Gamut*, it hands back the linear light unchanged, which the viewer and
  the Output Module then encode as usual.

The config and minColor Input share their colour matrices, so a conversion made
by either one agrees with the other exactly. The ACES spaces reach D65 through
CAT02, as OpenDRT does; After Effects' own ACES configs use Bradford, which
differs by at most about 0.004 on saturated colours and not at all on greys.

## Unmanaged: OCIO as a pass-through

The config is `minColor/mincolor-unmanaged.ocio`, whose working space
"minColor Output" is only a label. Pixels pass through; **minColor Output** at the
top encodes for the display (default *sRGB Display - 2.2 Power / Rec.709*), and
what it writes is what renders. The config's **macOS native monitor (AE viewport fix)**
display corrects the viewer on a Mac whose monitor is in its native mode;
**Windows (passthrough)** leaves pixels alone, and is the one to pick on any
machine that needs no correction.

## Adobe engine

**Project Settings > Color**: Adobe engine, working space **None**, footage
interpreted as **Preserve RGB**, 32 bpc. After Effects passes pixels through and
the effects do everything:

1. **minColor Input** on each footage layer (Working Gamut ACEScg).
2. **minColor Output** at the top, Display Encoding set to the delivery.
3. On a Mac whose monitor is in its native mode, **minColor macOS Native Fix** on
   a guide layer at the very top, to correct the viewer.

## The Mac viewer, and when it needs a fix

On a Mac, After Effects hands the system a Display P3 picture and the system
converts it for the monitor. That goes wrong in one case: the monitor is left in
its **native** mode and macOS uses the vendor's profile for it. After Effects and
macOS then disagree about the conversion, and a Rec.709 picture looks
oversaturated in the viewer while the rest of the system shows it correctly.

| The monitor is | The viewer |
| --- | --- |
| In its native mode, with the vendor's profile | Needs the fix |
| In a mode (sRGB, Display P3) with the matching profile | Is correct; use no fix |

For the first case, the **macOS native monitor (AE viewport fix)** display sends
linear light as P3-D65 encoded 2.2, and **minColor macOS Native Fix** does the
same to an Output's sRGB codes. Both are for the viewer only: a render never
carries them. **macOS Video native monitor (AE viewport fix)** shows a Rec.709
BT.1886 file as a Mac's video players show it (decoded with the sRGB curve:
lighter mid-tones, lifted shadows), on the same kind of monitor.

Apple's XDR displays are not covered by this page.

Until 0.1.7 these were named *macOS (AE viewport fix)*, *macOS Video (AE viewport
fix)* and *minColor macOS Fix*, and this page said every Mac needed them.

## HDR

The effects carry values above 1.0 (100 nits) through everything when the
project is 32 bpc. After Effects' viewer shows only up to 1.0 through OCIO, so
judge HDR with an external monitor or a HDR-capable player. minColor Knee and
minColor AgX both take a peak in nits.

How HDR leaves a project depends on the setup:

- **OCIO**: the config's encoded spaces (sRGB, Rec.709 BT.1886, Display P3) stop
  at display white. HDR leaves as a linear EXR, through one of the linear Output
  Color Spaces.
- **Unmanaged or Adobe engine**: minColor Output can encode Rec.2100 PQ, Rec.2100
  HLG, or PQ in a P3-D65 container, with 1.0 at 100 nits.

minColor writes pixels only. It puts no HDR tags or mastering metadata in a file;
those come from the encoder or a later step, and a PQ or HLG file without them
will not play as HDR.
