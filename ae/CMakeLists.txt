# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
# After Effects plugins. Needs the Adobe After Effects SDK, which is Adobe's,
# proprietary, obtained separately, and never enters this repo: point
# MINCOLOR_AE_SDK at it, or drop a symlink at private/sdk/AfterEffectsSDK
# (private/ is gitignored).
#
# Bundle mechanics: Rez the PiPL, copy the .rsrc into the bundle, THEN codesign,
# all on relink; copying after the linker seals the bundle ships an invalid
# signature and AE loads nothing, silently.

set(MINCOLOR_AE_SDK "" CACHE PATH "After Effects SDK root (contains Examples/Headers/AE_Effect.h)")
if(NOT MINCOLOR_AE_SDK)
  foreach(_cand
      "${CMAKE_SOURCE_DIR}/private/sdk/AfterEffectsSDK")
    if(EXISTS "${_cand}/Examples/Headers/AE_Effect.h")
      set(MINCOLOR_AE_SDK "${_cand}")
      break()
    endif()
  endforeach()
endif()

if(NOT APPLE)
  message(STATUS "ae/: only the macOS build exists so far; skipped")
  return()
endif()
if(NOT EXISTS "${MINCOLOR_AE_SDK}/Examples/Headers/AE_Effect.h")
  message(STATUS "ae/: After Effects SDK not found (set MINCOLOR_AE_SDK); skipped")
  return()
endif()
message(STATUS "ae/: After Effects SDK at ${MINCOLOR_AE_SDK}")

set(_ae_h "${MINCOLOR_AE_SDK}/Examples/Headers")
set(_ae_u "${MINCOLOR_AE_SDK}/Examples/Util")
set(_core "${CMAKE_SOURCE_DIR}/core")
find_program(REZ_TOOL Rez REQUIRED)

set(DRT_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity for the AE bundles (- = ad-hoc)")
if(DRT_CODESIGN_IDENTITY STREQUAL "-")
  set(_sign_flags --force --deep --sign -)
else()
  set(_sign_flags --force --deep --options runtime --timestamp --sign "${DRT_CODESIGN_IDENTITY}")
endif()

# Where AE (and Premiere) load from without admin rights; group-admin writable.
set(DRT_AE_PLUGIN_DIR "/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore/minColor"
    CACHE PATH "Install directory for the AE bundles")

set(_gen "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY "${_gen}")

# ---- shared: turn a MODULE target into an AE effect bundle ------------------
function(drt_ae_effect_bundle name outname pipl plist)
  target_include_directories(${name} PRIVATE "${_ae_h}" "${_ae_h}/SP" "${_ae_u}")
  target_link_libraries(${name} PRIVATE "-framework Metal" "-framework Foundation")
  set_target_properties(${name} PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION "plugin"
    OUTPUT_NAME "${outname}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${plist}")
  # -dead_strip would drop the entry points the PiPL names; export them explicitly.
  target_link_options(${name} PRIVATE
    "-Wl,-exported_symbol,_EffectMain"
    "-Wl,-exported_symbol,_PluginDataEntryFunction2"
    "-Wl,-dead_strip")
  # Adobe's headers and utils are not ours to make warning-clean.
  target_compile_options(${name} PRIVATE -Wno-unused-parameter -Wno-deprecated-declarations -Wno-missing-field-initializers)

  set(_rsrc "${CMAKE_CURRENT_BINARY_DIR}/${outname}.rsrc")
  add_custom_command(OUTPUT "${_rsrc}"
    COMMAND ${REZ_TOOL} "${pipl}" -o "${_rsrc}" -useDF
            -i "${_ae_h}" -i "${MINCOLOR_AE_SDK}/Examples/Resources" -i "${_ae_u}"
    DEPENDS "${pipl}"
    COMMENT "Rez PiPL (${outname})")
  add_custom_target(${name}_pipl DEPENDS "${_rsrc}")
  add_dependencies(${name} ${name}_pipl)
  set_property(TARGET ${name} APPEND PROPERTY LINK_DEPENDS "${_rsrc}" "${plist}")
  add_custom_command(TARGET ${name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:${name}>/Contents/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy "${_rsrc}" "$<TARGET_BUNDLE_DIR:${name}>/Contents/Resources/${outname}.rsrc"
    COMMAND codesign ${_sign_flags} "$<TARGET_BUNDLE_DIR:${name}>"
    COMMAND codesign --verify --deep --strict "$<TARGET_BUNDLE_DIR:${name}>"
    COMMENT "PiPL into bundle, then codesign (${outname})" VERBATIM)

  # `cmake --build build --target install_<name>` copies the bundle where AE loads it.
  add_custom_target(install_${name}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DRT_AE_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DRT_AE_PLUGIN_DIR}/${outname}.plugin"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${name}>" "${DRT_AE_PLUGIN_DIR}/${outname}.plugin"
    COMMAND ${CMAKE_COMMAND} -E echo "installed ${DRT_AE_PLUGIN_DIR}/${outname}.plugin"
    DEPENDS ${name}
    COMMENT "Install ${outname}.plugin into MediaCore" VERBATIM)
endfunction()

# ---- the embedded Metal source shared by both effects ------------------------
# Order matters: prefix (metal_stdlib), shim, params, kernel, then the AE wrapper kernels.
set(_msl_parts
  "${CMAKE_CURRENT_SOURCE_DIR}/common/drt_ae_msl_prefix.metal"
  "${_core}/opendrt_shim_msl.h"
  "${_core}/opendrt_params.h"
  "${_core}/opendrt_kernel.h"
  "${_core}/opendrt_grade.h"
  "${_core}/mincolor_knee.h"
  "${_core}/mincolor_agx.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/common/drt_ae_kernels.metal")
# A CMake list would split into separate arguments on the command line; join with '|'.
string(REPLACE ";" "|" _msl_joined "${_msl_parts}")
add_custom_command(OUTPUT "${_gen}/drt_ae_msl.h"
  COMMAND ${CMAKE_COMMAND} "-DIN=${_msl_joined}" -DOUT=${_gen}/drt_ae_msl.h -DNAME=kDrtAeMsl
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_text.cmake
  DEPENDS ${_msl_parts} cmake/embed_text.cmake
  COMMENT "Embed the Metal source (core + AE kernels)" VERBATIM)

# ---- the effects ---------------------------------------------------------
function(drt_ae_effect name role outname subdir pipl)
  add_library(${name} MODULE
    common/drt_ae_effect.mm
    "${_gen}/drt_ae_msl.h"
    "${_ae_u}/AEFX_SuiteHelper.c"
    "${_ae_u}/Smart_Utils.cpp")
  target_compile_definitions(${name} PRIVATE ${role}=1)
  target_include_directories(${name} PRIVATE "${_gen}" common)
  target_link_libraries(${name} PRIVATE drt_core)
  # The SDK samples use manual retain/release around Metal; stay off ARC like they do.
  set_source_files_properties(common/drt_ae_effect.mm PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")
  drt_ae_effect_bundle(${name} "${outname}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/${pipl}"
    "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}/Info.plist.in")
endfunction()

drt_ae_effect(mincolor_output DRT_ROLE_OUTPUT "minColor Output" output minColor_Output_PiPL.r)
drt_ae_effect(mincolor_input  DRT_ROLE_INPUT  "minColor Input"  input  minColor_Input_PiPL.r)
drt_ae_effect(mincolor_grade  DRT_ROLE_GRADE  "minColor Grade"  grade  minColor_Grade_PiPL.r)
drt_ae_effect(mincolor_macfix DRT_ROLE_MACFIX "minColor macOS Fix" macfix minColor_macOS_Fix_PiPL.r)
drt_ae_effect(mincolor_knee   DRT_ROLE_KNEE   "minColor Knee"   knee   minColor_Knee_PiPL.r)
drt_ae_effect(mincolor_agx    DRT_ROLE_AGX    "minColor AgX"    agx    minColor_AgX_PiPL.r)

add_custom_target(install_ae DEPENDS install_mincolor_output install_mincolor_input install_mincolor_grade install_mincolor_macfix install_mincolor_knee install_mincolor_agx)
