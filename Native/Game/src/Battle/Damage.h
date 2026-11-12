// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_DAMAGE_H_TYPES
#define VS_BATTLE_DAMAGE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_DAMAGE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_DAMAGE_H_DECLS)
#define VS_BATTLE_DAMAGE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

extern u_short Battle_bladePpDamageBonuses[12];
extern short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern Battle_Actor* Battle_actors[42];
extern u_int Battle_lastActionStatValue;
extern int Battle_criticalHitOccurred;
extern short Battle_limbArmorDpLoss[8];
short Battle_CalculatePhysicalHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int withRandMod, int applyStatuses);
short Battle_PhysicalDamageWithoutDebuffs(Battle_ActionDefinition* action,
    Battle_HitResult* source, Battle_HitResult* target, int nHit, int withRandMod);
short Battle_CalculateSpellHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int addRandMod);
short Battle_CalculateSpellHealing(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target, int nHit,
    int withRandMod);
short Battle_PhysicalDamageWithDebuffs(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);

#endif // VS_BATTLE_DAMAGE_H_DECLS
