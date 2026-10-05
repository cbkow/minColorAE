# minColor Input

Says what a file is and brings it into the comp's linear working space. Put it on
the footage layer itself, first in the stack, or let the panel's **Apply In** do
it from the file extension.

<img src="../images/ec_input.png" width="430" alt="minColor Input controls">

- **Input Gamut** and **Input Transfer**: what the file is. Camera spaces (ARRI,
  RED, Sony, Panasonic, DaVinci, FilmLight and more, with their log curves), the
  linear spaces, and the display encodings deliveries come in: Rec.1886 (2.4
  power), sRGB, 2.2 power, BT.709 camera, PQ and HLG.
- **Working Gamut**: the comp's working space, ACEScg by default. In an OCIO
  project Apply In sets it to the project's working space.
- **Input Range**: *Full*, or *Limited (video)* when a file arrives with video
  levels unexpanded (64–940 of 1023). After Effects expands ProRes and most video
  itself, so leave Full unless the picture looks lifted and flat.

Everything below Input Range applies only to the **OpenDRT inverse** transfers,
and is greyed out otherwise.

## OpenDRT inverse: bringing a finished render back

For a delivery that already went through a picture formation: a graded master,
a client-approved render. Decoding it with its plain curve puts its rendered
highlights into the comp as if they were scene light, and any rendering on top
compresses them a second time. The inverse transfers instead return each pixel
to the scene value the OpenDRT rendering would have made it from, like an ACES
config's inverse output transform.

Pick the entry that names the file's encoding: **Rec.1886**, **sRGB Display**,
**Display P3**, **Rec.2100 PQ**, **Rec.2100 HLG**, or **Dolby PQ / P3-D65** (PQ in a
P3-D65 container; *Rec.2100 PQ* is the Rec.2020 container). Then set the look
rows (Look, Tonescale) to match the rendering that made the file.

- **Peak Luminance must be at least the master's peak** (its MaxCLL or mastering
  display: 1000, 2000, 4000 nits). Choosing an inverse entry sets the encoding's
  default (1000 for PQ and HLG, 100 for SDR); raise it for brighter masters.
- **Highlight Cap (of peak)**, 0.5–1 (1 = off): bright saturated colours are
  many-to-one through a rendering, so near the peak the inverse has many valid
  answers and neighbouring pixels can pick different ones, which shows as
  contours once you grade. The cap pulls the top of the range onto one smooth
  answer. Use about 0.9 on PQ and HLG masters.
- The inverse assumes the master was rendered for a Dark viewing room (the
  Output's default).

What it guarantees: re-rendering the inverted pixel returns the original within
one 8-bit step wherever the display was not clipped, and greys and low-saturation
colours get their exact scene value back. It is iterative: fast on the GPU,
noticeably slower on the CPU.
