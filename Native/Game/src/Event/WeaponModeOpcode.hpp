#pragma once
// Hand-maintained (tools/port/owned.csv).
// The event VM opcode slot of VS::Event::ScriptSetAshleyBattleMode
// (Event/WeaponMode.hpp). The opcode table and the BATTLE.PRG module image
// (0x800B9DB4) hold this adapter; it decodes the command bytes and binds the
// typed core to the live event, battle and model state.
#include <cstdint>

#include "Legacy/Psx.h"

namespace VS::Event {

std::intptr_t ScriptSetAshleyBattleModeOpcode(u_char* command,
                                              std::int16_t thread_id);

}  // namespace VS::Event
