# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
cmake_minimum_required(VERSION 3.20)
project(minColorAE VERSION 0.1.0 LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# No fast-math anywhere: the probe compares the core against the upstream DCTL to
# 1e-5, and the whole point of the shared core is that hosts get the same floats.
if(MSVC)
  add_compile_options(/fp:precise /W4)
else()
  add_compile_options(-fno-fast-math -Wall -Wextra)
endif()

# ---- the core: headers + preset tables -------------------------------------
add_library(drt_core STATIC core/opendrt_presets.cpp)
target_include_directories(drt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)

# ---- the reference: upstream DCTL compiled as C++ --------------------------
add_library(dctl_ref STATIC tools/dctl_ref.cpp)
target_include_directories(dctl_ref
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/tools
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core ${CMAKE_CURRENT_SOURCE_DIR}/upstream)
# The DCTL is written for Resolve's compiler; keep our warnings out of it.
if(NOT MSVC)
  target_compile_options(dctl_ref PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter)
endif()

# ---- tools ------------------------------------------------------------------
add_executable(probe_core tools/probe_core.cpp)
target_link_libraries(probe_core PRIVATE drt_core dctl_ref)

add_executable(dump_presets tools/dump_presets.cpp)
target_link_libraries(dump_presets PRIVATE drt_core)

enable_testing()
add_test(NAME probe_core COMMAND probe_core)

# ---- After Effects plugins (needs the Adobe SDK; skipped when absent) --------
add_subdirectory(ae)

# ---- ScriptUI panel (no SDK needed) -------------------------------------------
add_subdirectory(panel)

# ---- MSL compile check (macOS with Xcode's Metal toolchain) ----------------
if(APPLE)
  find_program(XCRUN xcrun)
  if(XCRUN)
    # `xcrun -f metal` succeeds even when the Metal Toolchain component is not
    # downloaded (the stub then fails at run time), so try an actual compile.
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/metal_probe.metal "#include <metal_stdlib>\nkernel void k() {}\n")
    execute_process(COMMAND ${XCRUN} -sdk macosx metal -c ${CMAKE_CURRENT_BINARY_DIR}/metal_probe.metal
                            -o ${CMAKE_CURRENT_BINARY_DIR}/metal_probe.air
                    RESULT_VARIABLE METAL_RES OUTPUT_QUIET ERROR_QUIET)
    if(METAL_RES EQUAL 0)
      set(MSL_AIR ${CMAKE_CURRENT_BINARY_DIR}/check_msl.air)
      add_custom_command(
        OUTPUT ${MSL_AIR}
        COMMAND ${XCRUN} -sdk macosx metal -c ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_msl.metal
                -I ${CMAKE_CURRENT_SOURCE_DIR}/core -o ${MSL_AIR}
        DEPENDS tools/check_msl.metal core/opendrt_shim_msl.h core/opendrt_params.h core/opendrt_kernel.h
        COMMENT "Compiling the core as Metal (dialect check)")
      add_custom_target(check_msl ALL DEPENDS ${MSL_AIR})
      add_test(NAME check_msl COMMAND ${XCRUN} -sdk macosx metal -c ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_msl.metal
               -I ${CMAKE_CURRENT_SOURCE_DIR}/core -o ${CMAKE_CURRENT_BINARY_DIR}/check_msl_test.air)
    else()
      message(STATUS "Metal compiler unavailable; check_msl skipped. "
                     "Xcode's Metal Toolchain installs with: xcodebuild -downloadComponent MetalToolchain")
    endif()
  endif()
endif()
