# Installing minColorAE @VERSION@ (@PLATFORM@)

Quit After Effects. If version 0.1.5 or earlier is installed, remove its
`minColor` effects folder first (a file was renamed in 0.1.6); from 0.1.6 on,
the new files copy over the old ones. Then copy:

| From this folder | To |
| --- | --- |
@ROWS@

The panel and preset folders above are After Effects 2027's; for another version
use its own (`26.5` and `After Effects 2026`, and so on).

Then start After Effects: the effects are under **Effect > minColor**, the panel is
**Window > minColor.jsx**, and the presets are under **Effects & Presets >
Animation Presets > minColor**. Turn on **Preferences > Scripting & Expressions >
Allow Scripts to Write Files and Access Network** for the panel.

Next: [how to use it](https://github.com/cbkow/minColorAE#use) and the
[reference](https://github.com/cbkow/minColorAE/blob/main/docs/reference.md).

minColorAE is free software under the GPL-3.0 (LICENSE); credits in NOTICE. Source:
https://github.com/cbkow/minColorAE (tag v@VERSION@).
