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
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

extern u_short Battle_bladePpDamageBonuses[12];
extern short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern Battle_Actor* Battle_actors[42];
extern u_int Battle_lastActionStatValue;
extern int Battle_criticalHitOccurred;
extern short Battle_limbArmorDpLoss[28];
int Battle_ResolveActorDefeat(int source_actor_slot, int target_actor_slot, int count_kill);
short Battle_ApplyDamageGemMultiplier(
    Battle_ActionDefinition* action, short damage_amount, Battle_ActorStats* source_stats, Battle_ActorStats* target_stats);
void Battle_DistributeHitLimbDamage(Battle_ActionDefinition* action, Battle_HitResult* entity, short baseValue);
void Battle_DistributeLimbRecovery(Battle_ActionDefinition* action, Battle_HitResult* entity, short amount);
void Battle_IgnoreStatCalculator(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
short Battle_LastActionStatValue(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
short Battle_CalculatePhysicalHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int withRandMod, int applyStatuses);
short Battle_PhysicalDamageWithoutDebuffs(Battle_ActionDefinition* action,
    Battle_HitResult* source, Battle_HitResult* target, int nHit, int withRandMod);
short Battle_CalculateSpellHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int addRandMod);
short Battle_ScalePreviousTargetHit(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
short Battle_ScaleSourceMissingHp(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_ScaleTargetCurrentHp(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_WeaponPpWithChainOffset(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
short Battle_ScaleIncomingSpellDamage(Battle_ActionDefinition* action,
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_ScaleIncomingPhysicalDamage(Battle_ActionDefinition* action,
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_ScaleSourceMissingMp(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
short Battle_CalculateSpellHealing(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target, int nHit,
    int withRandMod);
short Battle_TargetMissingHp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
short Battle_TargetMissingMp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
short Battle_ScaleIncomingSpellEffect(Battle_ActionDefinition* action,
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_ScaleIncomingSpellCost(Battle_ActionDefinition* action, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_ScaleIncomingDamage(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)));
short Battle_SourceShieldPp(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
short Battle_ScaleLastActionStatValue(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
short Battle_PhysicalDamageWithDebuffs(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
short Battle_TargetRisk(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
short Battle_CalculateActionStatChange(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit, int execute_action);

#endif // VS_BATTLE_DAMAGE_H_DECLS
