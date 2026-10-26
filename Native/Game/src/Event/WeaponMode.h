#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptSetAshleyBattleMode(u_char* command, int16_t thread_id);

#ifdef __cplusplus
}
#endif
