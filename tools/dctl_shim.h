/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow.
 * Derived from OpenDRT v1.1.0 by Jed Smith (https://github.com/jedypod/open-display-transform), GPLv3;
 * modified 2026-09-30 to 2026-10-04, see CHANGES-FROM-OPENDRT.md. Not affiliated with or endorsed by OpenDRT.
 */
/* Emulates enough of Resolve's DCTL environment to compile the UNTOUCHED upstream
 * OpenDRT_v1.1.0.dctl as C++. Included after opendrt_shim_cpp.h, inside a private
 * namespace, by dctl_ref.cpp. Nothing here is used by the core.
 *
 * DEFINE_UI_PARAMS(name, label, type, default, ...) becomes a mutable namespace-
 * scope variable; the probe writes the preset indices and sliders into those, then
 * calls the DCTL's transform(). The brace lists of a combo box split on their
 * commas in the preprocessor, which is fine: they land in the ignored `...`.
 */

#define DRT_DCTL_DCTLUI_COMBO_BOX(name, dflt)    int name = dflt;
#define DRT_DCTL_DCTLUI_SLIDER_FLOAT(name, dflt) float name = dflt;
#define DRT_DCTL_DCTLUI_CHECK_BOX(name, dflt)    int name = dflt;
#define DEFINE_UI_PARAMS(name, label, type, dflt, ...) DRT_DCTL_##type(name, dflt)
#define DEFINE_UI_TOOLTIP(...)
