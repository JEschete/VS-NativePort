// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Items/EquipmentLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionEffects.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_80084390 (src/BATTLE/BATTLE.PRG/146C.c:12606)
int Battle_QueueSpellTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084440 (src/BATTLE/BATTLE.PRG/146C.c:12617)
int Battle_QueuePhysicalTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type != Battle_ActionType_Spell) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800844F0 (src/BATTLE/BATTLE.PRG/146C.c:12628)
int Battle_QueuePhysicalAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 0) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084570 (src/BATTLE/BATTLE.PRG/146C.c:12639)
int Battle_QueueAirAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 1) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800845F0 (src/BATTLE/BATTLE.PRG/146C.c:12650)
int Battle_QueueFireAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 2) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084670 (src/BATTLE/BATTLE.PRG/146C.c:12661)
int Battle_QueueEarthAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 3) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800846F0 (src/BATTLE/BATTLE.PRG/146C.c:12672)
int Battle_QueueWaterAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 4) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084770 (src/BATTLE/BATTLE.PRG/146C.c:12683)
int Battle_QueueLightAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 5) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800847F0 (src/BATTLE/BATTLE.PRG/146C.c:12694)
int Battle_QueueDarkAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 6) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084870 (src/BATTLE/BATTLE.PRG/146C.c:12705)
int Battle_QueueDamageTriggeredRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    int incoming_hp_effect = Battle_actionState->selection.primaryTarget.hpEffect;
    if ((incoming_hp_effect == 1) || ((incoming_hp_effect == 3))) {
        short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        VS::Battle::LiveDistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12789)
int Battle_QueueHitDamageAndEquipmentWear(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short damage_amount;
    Battle_ActorStats* source_stats = Battle_actors[source_hit->target.targetActor]->stats;
    Battle_ActorStats* target_stats = Battle_actors[target_hit->target.targetActor]->stats;
    Battle_criticalHitOccurred = 0;
    damage_amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->target.hp += damage_amount;

    if (Battle_criticalHitOccurred != 0) {
        target_hit->hpEffect = 3;
    } else {
        target_hit->hpEffect = 1;
    }

    if (target_hit->targetsPosition == 0) {

        VS::Battle::LiveDistributeHitLimbDamage(action_definition, target_hit, damage_amount);

        if (target_stats->actorId != 0x80) {
            target_hit->inflictStatuses |= target_stats->statuses & 0x8000;
        }

        if (execute_action != 0) {
            VS::Items::ConsumeLiveHitEquipmentDp(action_definition, source_stats, target_stats, damage_amount);
            VS::Items::AccumulateLiveHitEquipmentPp(action_definition, source_stats, target_stats, damage_amount);
        }
    }
}

}  // extern "C"
