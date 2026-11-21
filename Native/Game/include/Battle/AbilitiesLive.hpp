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
// decomp: BATTLE.PRG func_80069C6C (src/BATTLE/BATTLE.PRG/146C.c:386)
void LiveArmDefenseAbilityTimingWindow(int target_actor_slot);
// decomp: BATTLE.PRG isArtOrAbilityUnlocked (src/BATTLE/BATTLE.PRG/146C.c:2974;
// was _isArtOrAbilityUnlocked): the pending unlocks packed as the original
// returns them, the break art in bit 16 and the battle ability in bit 0.
int LivePendingBattleAbilityUnlocks();
// decomp: BATTLE.PRG func_8006F89C (src/BATTLE/BATTLE.PRG/146C.c:3016)
void LiveBeginPendingAbilityReward();
// decomp: BATTLE.PRG func_8006FA20 (src/BATTLE/BATTLE.PRG/146C.c:3056)
void LiveGrantPendingAbilityUnlock();
// decomp: BATTLE.PRG isActionUnlocked (src/BATTLE/BATTLE.PRG/146C.c:9232; was
// vs_battle_isActionUnlocked)
int LiveIsActionUnlocked(unsigned action_id);
// decomp: BATTLE.PRG setActionUnlocked (src/BATTLE/BATTLE.PRG/146C.c:9240; was
// vs_battle_setActionUnlocked)
void LiveUnlockAction(unsigned action_id);
// decomp: BATTLE.PRG vs_battle_isSpellClassUnlocked
// (src/BATTLE/BATTLE.PRG/5BF94.c:3197)
int LiveHasUnlockedSpellClass(int spell_class);
// decomp: BATTLE.PRG breakArtsUnlocked (src/BATTLE/BATTLE.PRG/5BF94.c:3209; was
// _breakArtsUnlocked)
int LiveHasUnlockedWeaponBreakArt();
// decomp: BATTLE.PRG vs_battle_chainAbilitiesUnlocked
// (src/BATTLE/BATTLE.PRG/5BF94.c:3223)
int LiveHasUnlockedBattleAbility(int defense);
// decomp: BATTLE.PRG func_8006FD1C (src/BATTLE/BATTLE.PRG/146C.c:3125)
void LiveAdvanceBattleAbilitySequence();
// decomp: BATTLE.PRG func_80078828 (src/BATTLE/BATTLE.PRG/146C.c:6891): 1 when
// the ability input slot may trigger its battle ability now.
int LiveCanTriggerBattleAbility(int ability_input_slot);
// decomp: BATTLE.PRG func_80078AB4 (src/BATTLE/BATTLE.PRG/146C.c:7147)
void LiveUpdateBattleAbilityTiming();
}  // namespace VS::Battle
