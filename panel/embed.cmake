# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# cmake -DIN=panel/minColor.jsx -DSHIM=ocio/mincolor-viewport-shim.ocio -DVERSION=x.y.z -DOUT=... -P embed.cmake
# Fills the panel's @MINCOLOR_VERSION@ and @MINCOLOR_SHIM_JS@ tokens; the shim
# becomes one JavaScript string literal.

file(READ "${SHIM}" _s)
string(REPLACE "\\" "\\\\" _s "${_s}")
string(REPLACE "\"" "\\\"" _s "${_s}")
string(REPLACE "\r" "" _s "${_s}")
string(REPLACE "\n" "\\n" _s "${_s}")
set(MINCOLOR_SHIM_JS "\"${_s}\"")
set(MINCOLOR_VERSION "${VERSION}")
configure_file("${IN}" "${OUT}" @ONLY)
