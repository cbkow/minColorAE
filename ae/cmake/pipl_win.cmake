# SPDX-License-Identifier: GPL-3.0-only
# minColorAE. Copyright (C) 2026 cbkow.
# A PiPL .r into a Windows .rc, as the SDK samples' custom build step does:
#   cl /EP (headers on the path) -> .rr ; PiPLtool .rr .rrc ; cl /D MSWindows /EP .rrc -> .rc
# -DCL= -DPIPLTOOL= -DIN= -DOUT= -DWORK=<path prefix for the temporaries> -DINC=<dir|dir|...>
string(REPLACE "|" ";" _dirs "${INC}")
set(_inc)
foreach(d ${_dirs})
  list(APPEND _inc "/I${d}")
endforeach()
execute_process(COMMAND "${CL}" /nologo /EP ${_inc} "${IN}" OUTPUT_FILE "${WORK}.rr" RESULT_VARIABLE r ERROR_VARIABLE e)
if(r)
  message(FATAL_ERROR "PiPL preprocess failed (${r}): ${e}")
endif()
execute_process(COMMAND "${PIPLTOOL}" "${WORK}.rr" "${WORK}.rrc" RESULT_VARIABLE r OUTPUT_VARIABLE o ERROR_VARIABLE e)
if(r)
  message(FATAL_ERROR "PiPLtool failed (${r}): ${o} ${e}")
endif()
execute_process(COMMAND "${CL}" /nologo /D MSWindows /EP "${WORK}.rrc" OUTPUT_FILE "${OUT}" RESULT_VARIABLE r ERROR_VARIABLE e)
if(r)
  message(FATAL_ERROR "PiPL .rc preprocess failed (${r}): ${e}")
endif()
