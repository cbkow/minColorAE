# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# The ScriptUI panel: panel/minColor.jsx with the version and the viewport shim
# embedded (embed.cmake), so the shim the panel writes is always this build's.
# Needs no SDK.

set(_panel_out "${CMAKE_CURRENT_BINARY_DIR}/minColor.jsx")
add_custom_command(
  OUTPUT "${_panel_out}"
  COMMAND ${CMAKE_COMMAND}
          -DIN=${CMAKE_CURRENT_SOURCE_DIR}/minColor.jsx
          -DSHIM=${CMAKE_SOURCE_DIR}/ocio/mincolor-viewport-shim.ocio
          -DVERSION=${PROJECT_VERSION}
          -DOUT=${_panel_out}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake
  DEPENDS minColor.jsx embed.cmake ${CMAKE_SOURCE_DIR}/ocio/mincolor-viewport-shim.ocio
  COMMENT "Embedding the viewport shim into the panel"
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
