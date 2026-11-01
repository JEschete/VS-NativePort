#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slot of VS::Event::ScriptSetAshleyBattleMode
// (Event/WeaponMode.hpp). The opcode table and the BATTLE.PRG module image
// (0x800B9DB4) hold this adapter; it decodes the command bytes and binds the
// typed core to the live event, battle and model state.
#include <stdint.h>

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

intptr_t Event_ScriptSetAshleyBattleModeOpcode(u_char* command,
                                               int16_t thread_id);

#ifdef __cplusplus
}
#endif
