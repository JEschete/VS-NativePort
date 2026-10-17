// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ABILITIES_H_TYPES
#define VS_BATTLE_ABILITIES_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ABILITIES_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ABILITIES_H_DECLS)
#define VS_BATTLE_ABILITIES_H_DECLS
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern Battle_Actor* Battle_characterState;
extern u_short Battle_breakArtPointRequirements[15][4];
extern u_short Battle_battleAbilityPointRequirements[150];
extern short Battle_abilityInputResult;
void Battle_ArmDefenseAbilityTimingWindow(int arg0);
int Battle_PendingBattleAbilityUnlocks(void);
void Battle_BeginPendingAbilityReward(void);
void Battle_GrantPendingAbilityUnlock(void);
void Battle_AdvanceBattleAbilitySequence(void);
int Battle_CanTriggerBattleAbility(int arg0);
void Battle_UpdateBattleAbilityTiming(void);
int Battle_IsActionUnlocked(u_int arg0);
void Battle_UnlockAction(u_int arg0);
int Battle_HasUnlockedSpellClass(int spellClass);
int Battle_HasUnlockedWeaponBreakArt(void);
int Battle_HasUnlockedBattleAbility(int defense);

#endif // VS_BATTLE_ABILITIES_H_DECLS
