/* SPDX-License-Identifier: GPL-3.0-only
 * minColorAE. Copyright (C) 2026 cbkow. Part of a work derived from OpenDRT v1.1.0 by Jed Smith (GPLv3); see NOTICE.
 */
#include "AEConfig.h"
#include "AE_EffectVers.h"

#ifndef AE_OS_WIN
	#include "AE_General.r"
#endif

/* Flag words must equal kOutFlags / kOutFlags2 in drt_ae_effect.mm (static_asserted there). */
resource 'PiPL' (16000) {
	{
		Kind { AEEffect },
		Name { "minColor Input" },
		Category { "minColor" },
#ifdef AE_OS_WIN
    #if defined(AE_PROC_INTELx64)
		CodeWin64X86 {"EffectMain"},
    #elif defined(AE_PROC_ARM64)
		CodeWinARM64 {"EffectMain"},
    #endif
#elif defined(AE_OS_MAC)
		CodeMacIntel64 {"EffectMain"},
		CodeMacARM64 {"EffectMain"},
#endif
		AE_PiPL_Version { 2, 0 },
		AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
		AE_Effect_Version { 32769 },               /* 0.1.0 develop build 1 = PF_VERSION(0,1,0,DEVELOP,1); bump with the table */
		AE_Effect_Info_Flags { 0 },
		AE_Effect_Global_OutFlags { 0x6000400 },
		AE_Effect_Global_OutFlags_2 { 0xA001408 },
		AE_Effect_Match_Name { "ski.bialkow minColor Input" },
		AE_Reserved_Info { 0 },
		AE_Effect_Support_URL { "https://github.com/cbkow/minColorAE" }
	}
};
