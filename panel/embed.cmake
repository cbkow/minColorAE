# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# cmake -DIN=panel/minColor.jsx -DCONFIG=... -DSHIM=... -DINJSON=panel/in.json -DPRESETS=<dump_presets output>
#       -DVERSION=x.y.z -DOUT=... -P embed.cmake
# Fills the panel's @...@ tokens: the version, the OCIO config, the viewport shim and the starter in.json as
# JavaScript string literals, and the minColor Input's Gamut and Transfer menu
# entries and the Output's Display Encoding presets (in menu order) as JSON arrays, read from dump_presets' output so they
# come from the same C++ tables the effect builds its menus from.

function(js_string var path)
  file(READ "${path}" _s)
  string(REPLACE "\\" "\\\\" _s "${_s}")
  string(REPLACE "\"" "\\\"" _s "${_s}")
  string(REPLACE "\r" "" _s "${_s}")
  string(REPLACE "\n" "\\n" _s "${_s}")
  set(${var} "\"${_s}\"" PARENT_SCOPE)
endfunction()

js_string(MINCOLOR_CONFIG_JS "${CONFIG}")
js_string(MINCOLOR_SHIM_JS "${SHIM}")
js_string(MINCOLOR_IN_JSON_JS "${INJSON}")
file(READ "${PRESETS}" _p)
string(JSON MINCOLOR_INPUT_GAMUTS GET "${_p}" input_gamuts)
string(JSON MINCOLOR_INPUT_TRANSFERS GET "${_p}" input_transfer_functions)
string(JSON _n LENGTH "${_p}" displays)
set(MINCOLOR_DISPLAYS "")
math(EXPR _last "${_n} - 1")
foreach(_k RANGE ${_last})
  string(JSON _name GET "${_p}" displays ${_k} name)
  if(_k GREATER 0)
    string(APPEND MINCOLOR_DISPLAYS ", ")
  endif()
  string(APPEND MINCOLOR_DISPLAYS "\"${_name}\"")
endforeach()
set(MINCOLOR_DISPLAYS "[${MINCOLOR_DISPLAYS}]")
set(MINCOLOR_VERSION "${VERSION}")
configure_file("${IN}" "${OUT}" @ONLY)
