# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# The ScriptUI panel: panel/minColor.jsx with the version, the OCIO config, the
# starter in.json and the Input's menu entries embedded (embed.cmake), so what the
# panel writes and sets is always this build's.
# Needs no SDK.

set(_panel_out "${CMAKE_CURRENT_BINARY_DIR}/minColor.jsx")
set(_presets "${CMAKE_CURRENT_BINARY_DIR}/drt_presets.json")
add_custom_command(
  OUTPUT "${_presets}"
  COMMAND dump_presets "${_presets}"
  DEPENDS dump_presets
  COMMENT "Input menu entries for the panel"
  VERBATIM)
add_custom_command(
  OUTPUT "${_panel_out}"
  COMMAND ${CMAKE_COMMAND}
          -DIN=${CMAKE_CURRENT_SOURCE_DIR}/minColor.jsx
          -DCONFIG=${CMAKE_SOURCE_DIR}/ocio/mincolor.ocio
          -DUNMANAGED=${CMAKE_SOURCE_DIR}/ocio/mincolor-unmanaged.ocio
          -DINJSON=${CMAKE_CURRENT_SOURCE_DIR}/in.json
          -DPRESETS=${_presets}
          -DVERSION=${PROJECT_VERSION}
          -DOUT=${_panel_out}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake
  DEPENDS minColor.jsx embed.cmake in.json "${_presets}" ${CMAKE_SOURCE_DIR}/ocio/mincolor.ocio ${CMAKE_SOURCE_DIR}/ocio/mincolor-unmanaged.ocio
  COMMENT "Embedding the OCIO config into the panel"
  VERBATIM)
add_custom_target(mincolor_panel ALL DEPENDS "${_panel_out}")

# Dockable panels load from the per-user ScriptUI Panels folder (no admin rights).
set(MINCOLOR_PANEL_DIR "$ENV{HOME}/Library/Preferences/Adobe/After Effects/26.5/Scripts/ScriptUI Panels"
    CACHE PATH "Install directory for the ScriptUI panel")
set(MINCOLOR_PANEL_NAME "minColor.jsx" CACHE STRING "Installed file name (the Window menu entry)")
add_custom_target(install_panel
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MINCOLOR_PANEL_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy "${_panel_out}" "${MINCOLOR_PANEL_DIR}/${MINCOLOR_PANEL_NAME}"
  COMMAND ${CMAKE_COMMAND} -E echo "installed ${MINCOLOR_PANEL_DIR}/${MINCOLOR_PANEL_NAME}"
  DEPENDS mincolor_panel
  VERBATIM)

# The animation presets (presets/ae/minColor/*.ffx: mCin_* set minColor Input, mCout_*
# minColor Output, mCagx_* minColor AgX, each the whole effect, all on working space ACEScg). AE lists the
# user presets folder under Effects & Presets > Animation Presets; refresh the list
# (or restart AE) after installing.
set(MINCOLOR_PRESETS_DIR "$ENV{HOME}/Documents/Adobe/After Effects 2026/User Presets/minColor"
    CACHE PATH "Install directory for the animation presets")
file(GLOB _ffx CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/presets/ae/minColor/*.ffx")
add_custom_target(install_presets
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MINCOLOR_PRESETS_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy ${_ffx} "${MINCOLOR_PRESETS_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "installed the animation presets to ${MINCOLOR_PRESETS_DIR}"
  VERBATIM)
