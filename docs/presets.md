# Animation presets

**Effects & Presets > Animation Presets > minColor.** Each preset is the whole
effect with its settings: drop one on a layer, with or without the effect already
on it. All of them assume the working space is **ACEScg**; in a project on another
working space, set the effect's working gamut after applying one, or use the
panel's Apply In and Add Output, which follow the project.

| Preset | Effect | Settings |
| --- | --- | --- |
| mCin_sRGB | Input | Rec.709, sRGB |
| mCin_rec709video | Input | Rec.709, Rec.1886 (video levels already expanded by After Effects) |
| mCin_ACEScg | Input | ACEScg, linear |
| mCin_ACES2065-1 | Input | ACES 2065-1, linear |
| mCin_linear_rec709 | Input | Rec.709, linear |
| mCin_linear_rec2020 | Input | Rec.2020, linear |
| mCin_Netflicker | Input | OpenDRT inverse Dolby PQ / P3-D65, Limited range, peak 1000 (HEVC review files) |
| mCout_sRGB | Output | Un-tone-mapped, sRGB Display 2.2 / Rec.709 |
| mCout_rec709video | Output | Un-tone-mapped, Rec.1886 2.4 / Rec.709 |
| mCout_ACEScg | Output | Un-tone-mapped, linear ACEScg hand-off |
| mCout_ACES2065-1 | Output | Un-tone-mapped, linear ACES 2065-1 hand-off |
| mCagx_sRGB | AgX | Blender's AgX for sRGB / Rec.709 (the defaults: Peak 100, Target Rec.709) |
| mCagx_P3 | AgX | Peak 100, Target P3-D65 |
| mCagx_HDR1000 | AgX | Peak 1000, Target P3-D65 (Blender's AgX HDR 1000) |
