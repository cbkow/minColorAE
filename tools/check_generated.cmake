# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# cmake -DGEN=<generator> -DOUT=<fresh output> -DREF=<committed file> -P check_generated.cmake
# Runs a generator and fails when its output differs from the committed copy.
execute_process(COMMAND "${GEN}" "${OUT}" RESULT_VARIABLE rc OUTPUT_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "${GEN} failed (${rc})")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${OUT}" "${REF}" RESULT_VARIABLE diff)
if(NOT diff EQUAL 0)
  message(FATAL_ERROR "${REF} is stale: regenerate it with ${GEN} ${REF}")
endif()
