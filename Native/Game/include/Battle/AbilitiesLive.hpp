#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Abilities.hpp"

// Bindings from the generated reward, script VM, HUD, menu and effect
// timeline callers (tools/port/copy_fixups.py OWNED_CALL_BINDINGS) and from
// the hand-written gameplay loop, loot, menu and diagnostics code to the
// typed battle abilities (Abilities.hpp), in the original call shapes. A call
// without an action state or character state is refused with a warning on
// the "battle.ability" channel.
namespace VS::Battle {
// decomp: BATTLE.PRG func_80069C6C (src/BATTLE/BATTLE.PRG/146C.c:735)
void LiveArmDefenseAbilityTimingWindow(int target_actor_slot);
// decomp: BATTLE.PRG _isArtOrAbilityUnlocked (146C.c:3270): the pending
// unlocks packed as the original returns them, the break art in bit 16 and
// the battle ability in bit 0.
int LivePendingBattleAbilityUnlocks();
// decomp: BATTLE.PRG func_8006F89C (146C.c:3312)
void LiveBeginPendingAbilityReward();
// decomp: BATTLE.PRG func_8006FA20 (146C.c:3352)
void LiveGrantPendingAbilityUnlock();
// decomp: BATTLE.PRG vs_battle_isActionUnlocked (146C.c:9241)
int LiveIsActionUnlocked(unsigned action_id);
// decomp: BATTLE.PRG vs_battle_setActionUnlocked (146C.c:9249)
void LiveUnlockAction(unsigned action_id);
// decomp: BATTLE.PRG vs_battle_isSpellClassUnlocked (5BF94.c:3631)
int LiveHasUnlockedSpellClass(int spell_class);
// decomp: BATTLE.PRG _breakArtsUnlocked (5BF94.c:3643)
int LiveHasUnlockedWeaponBreakArt();
// decomp: BATTLE.PRG vs_battle_chainAbilitiesUnlocked (5BF94.c:3657)
int LiveHasUnlockedBattleAbility(int defense);
// decomp: BATTLE.PRG func_8006FD1C (146C.c:3420)
void LiveAdvanceBattleAbilitySequence();
// decomp: BATTLE.PRG func_80078828 (146C.c:7043): 1 when the ability input
// slot may trigger its battle ability now.
int LiveCanTriggerBattleAbility(int ability_input_slot);
// decomp: BATTLE.PRG func_80078AB4 (146C.c:7147)
void LiveUpdateBattleAbilityTiming();
}  // namespace VS::Battle
