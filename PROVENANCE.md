# Provenance

minColorAE began on 2026-10-04 as a fresh repository. Its engine (the core,
the After Effects effects, the tests and the viewport shim, now
`ocio/mincolor-unmanaged.ocio`, Fix OCIO's Unmanaged choice beside the generated `ocio/mincolor.ocio`) was carried over
from the author's earlier private exploration repositories, which are kept
only as historical reference and are not dependencies of this one. The
workflow layer (panel, presets, sidecar folder) is new here.

At the carry-over the effects were renamed (OpenDRT Input / Output / Grade /
macOS Fix became minColor Input / Output / Grade / macOS Fix), their match names
and bundle identifiers changed with them, the version restarted at 0.1.0, and
the development-only probe effect and After Effects driving scripts were left
behind. Projects saved with the earlier effect names do not open with these.

Third-party content: the unmodified OpenDRT v1.1.0 DCTL (`upstream/`, GPL-3.0,
Jed Smith), and code ported from darktable's AgX module (GPL-3.0-or-later) in
`core/mincolor_agx.*` (2026-10-04, see CHANGES-AGX.md). See NOTICE. Nothing else from third parties is included; the
Adobe After Effects SDK is a separate, external build dependency.
