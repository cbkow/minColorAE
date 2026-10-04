# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# cmake -DIN=panel/minColor.jsx -DSHIM=... -DINJSON=panel/in.json -DPRESETS=<dump_presets output>
#       -DVERSION=x.y.z -DOUT=... -P embed.cmake
# Fills the panel's @...@ tokens: the version, the shim and the starter in.json as
# JavaScript string literals, and the minColor Input's Gamut and Transfer menu
# entries (in menu order) as JSON arrays, read from dump_presets' output so they
# come from the same C++ tables the effect builds its menus from.

function(js_string var path)
  file(READ "${path}" _s)
  string(REPLACE "\\" "\\\\" _s "${_s}")
  string(REPLACE "\"" "\\\"" _s "${_s}")
  string(REPLACE "\r" "" _s "${_s}")
  string(REPLACE "\n" "\\n" _s "${_s}")
  set(${var} "\"${_s}\"" PARENT_SCOPE)
endfunction()

js_string(MINCOLOR_SHIM_JS "${SHIM}")
js_string(MINCOLOR_IN_JSON_JS "${INJSON}")
file(READ "${PRESETS}" _p)
string(JSON MINCOLOR_INPUT_GAMUTS GET "${_p}" input_gamuts)
string(JSON MINCOLOR_INPUT_TRANSFERS GET "${_p}" input_transfer_functions)
set(MINCOLOR_VERSION "${VERSION}")
configure_file("${IN}" "${OUT}" @ONLY)
