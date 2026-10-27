// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Core/Random.h"
#include "Items/Equipment.h"
#include "Items/EquipmentWear.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: BATTLE.PRG func_80084228 (src/BATTLE/BATTLE.PRG/146C.c:12547)
int Battle_MarkSourceActionSuccess(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit, int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    source_hit->sourceResult = 2;
}

// decomp: BATTLE.PRG func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12554)
int Battle_QueueInstantDeath(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueStatusRemovalMask(0x1E, target_hit);
    target_hit->target.hp = Battle_actors[target_hit->target.targetActor]->stats->currentHp;
    target_hit->hpEffect = 1;
}

// decomp: BATTLE.PRG func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12563)
int Battle_QueueDamageBasedArmorWear(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    target_hit->armorDpChange = target_hit->target.hp * 2;
    target_hit->limbs[target_hit->target.targetLimb].effects.opaque2 = target_hit->target.hp * 2;
    target_hit->armorDpEffect = 1;
}

// decomp: BATTLE.PRG func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12579)
int Battle_SetActionFlag64(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action)
{
    if (execute_action != 0) {
        State_SetScriptFlag(0x40, 1);
    }
}

// decomp: BATTLE.PRG func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12589)
int Battle_SetActionFlag32(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action)
{
    if (execute_action != 0) {
        State_SetScriptFlag(0x20, 1);
    }
}

// decomp: BATTLE.PRG func_80084390 (src/BATTLE/BATTLE.PRG/146C.c:12606)
int Battle_QueueSpellTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        short amount = Battle_CalculateActionStatChange(action, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084440 (src/BATTLE/BATTLE.PRG/146C.c:12617)
int Battle_QueuePhysicalTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type != Battle_ActionType_Spell) {
        short amount = Battle_CalculateActionStatChange(action, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800844F0 (src/BATTLE/BATTLE.PRG/146C.c:12628)
int Battle_QueuePhysicalAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 0) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084570 (src/BATTLE/BATTLE.PRG/146C.c:12639)
int Battle_QueueAirAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 1) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800845F0 (src/BATTLE/BATTLE.PRG/146C.c:12650)
int Battle_QueueFireAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 2) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084670 (src/BATTLE/BATTLE.PRG/146C.c:12661)
int Battle_QueueEarthAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 3) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800846F0 (src/BATTLE/BATTLE.PRG/146C.c:12672)
int Battle_QueueWaterAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 4) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084770 (src/BATTLE/BATTLE.PRG/146C.c:12683)
int Battle_QueueLightAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 5) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_800847F0 (src/BATTLE/BATTLE.PRG/146C.c:12694)
int Battle_QueueDarkAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    if (Battle_actionState->selection.effectVariant == 6) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084870 (src/BATTLE/BATTLE.PRG/146C.c:12705)
int Battle_QueueDamageTriggeredRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    int incoming_hp_effect = Battle_actionState->selection.primaryTarget.hpEffect;
    if ((incoming_hp_effect == 1) || ((incoming_hp_effect == 3))) {
        short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
        target_hit->target.hp += amount;
        target_hit->hpEffect = 2;
        Battle_DistributeLimbRecovery(action_definition, target_hit, amount);
    }
}

// decomp: BATTLE.PRG _increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12717)
int Battle_QueueRandomStrengthIncrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->strengthChange += amount;
    target_hit->strengthEffect = 2;
}

// decomp: BATTLE.PRG _increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12726)
int Battle_QueueRandomIntelligenceIncrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->intelligenceChange += amount;
    target_hit->intelligenceEffect = 2;
}

// decomp: BATTLE.PRG _increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12735)
int Battle_QueueRandomAgilityIncrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->agilityChange += amount;
    target_hit->agilityEffect = 2;
}

// decomp: BATTLE.PRG _increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12744)
int Battle_QueueRandomMaximumHpIncrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->maxHpChange += amount;
    target_hit->maxHpEffect = 2;
}

// decomp: BATTLE.PRG _increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12753)
int Battle_QueueRandomMaximumMpIncrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->maxMpChange += amount;
    target_hit->maxMpEffect = 2;
}

// decomp: BATTLE.PRG _decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12762)
int Battle_QueueRandomStrengthDecrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->strengthChange += amount;
    target_hit->strengthEffect = 1;
}

// decomp: BATTLE.PRG _decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12771)
int Battle_QueueRandomIntelligenceDecrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->intelligenceChange += amount;
    target_hit->intelligenceEffect = 1;
}

// decomp: BATTLE.PRG _decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12780)
int Battle_QueueRandomAgilityDecrease(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    target_hit->agilityChange += amount;
    target_hit->agilityEffect = 1;
}

// decomp: BATTLE.PRG func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12789)
int Battle_QueueHitDamageAndEquipmentWear(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short damage_amount;
    Battle_ActorStats* source_stats = Battle_actors[source_hit->target.targetActor]->stats;
    Battle_ActorStats* target_stats = Battle_actors[target_hit->target.targetActor]->stats;
    Battle_criticalHitOccurred = 0;
    damage_amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->target.hp += damage_amount;

    if (Battle_criticalHitOccurred != 0) {
        target_hit->hpEffect = 3;
    } else {
        target_hit->hpEffect = 1;
    }

    if (target_hit->targetsPosition == 0) {

        Battle_DistributeHitLimbDamage(action_definition, target_hit, damage_amount);

        if (target_stats->actorId != 0x80) {
            target_hit->inflictStatuses |= target_stats->statuses & 0x8000;
        }

        if (execute_action != 0) {
            Items_ConsumeHitEquipmentDp(action_definition, source_stats, target_stats, damage_amount);
            Items_AccumulateHitEquipmentPp(action_definition, source_stats, target_stats, damage_amount);
        }
    }
}

