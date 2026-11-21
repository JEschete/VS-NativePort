#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/BattleMode.hpp"

// Binding from the generated action flow caller to the typed battle mode
// (BattleMode.hpp), in the original argument order
// (tools/port/copy_fixups.py OWNED_CALL_BINDINGS). An absent actor is refused
// with a warning on the "battle.targeting" channel.
namespace VS::Battle {
// decomp: BATTLE.PRG isWeaponDrawn (src/BATTLE/BATTLE.PRG/146C.c:8598; was
// func_8007C088); an absent actor is refused with a warning and reads as
// sheathed.
[[nodiscard]] bool LiveIsActorWeaponDrawn(int actor_slot);
// decomp: BATTLE.PRG setWeaponDrawn (src/BATTLE/BATTLE.PRG/146C.c:8600; was
// func_8007C0AC)
void LiveSetActorBattleMode(int actor_slot, int battle_mode);
}  // namespace VS::Battle
