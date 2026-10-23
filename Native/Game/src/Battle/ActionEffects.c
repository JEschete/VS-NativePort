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
int Battle_MarkSourceActionSuccess(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2, int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    arg1->sourceResult = 2;
}

// decomp: BATTLE.PRG func_80084248 (src/BATTLE/BATTLE.PRG/146C.c:12554)
int Battle_QueueInstantDeath(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueStatusRemovalMask(0x1E, arg2);
    arg2->target.hp = Battle_actors[arg2->target.targetActor]->stats->currentHp;
    arg2->hpEffect = 1;
}

// decomp: BATTLE.PRG func_800842AC (src/BATTLE/BATTLE.PRG/146C.c:12563)
int Battle_QueueDamageBasedArmorWear(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    arg2->armorDpChange = arg2->target.hp * 2;
    arg2->limbs[arg2->target.targetLimb].effects.opaque2 = arg2->target.hp * 2;
    arg2->armorDpEffect = 1;
}

// decomp: BATTLE.PRG func_80084310 (src/BATTLE/BATTLE.PRG/146C.c:12579)
int Battle_SetActionFlag64(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4)
{
    if (arg4 != 0) {
        State_SetScriptFlag(0x40, 1);
    }
}

// decomp: BATTLE.PRG func_80084340 (src/BATTLE/BATTLE.PRG/146C.c:12589)
int Battle_SetActionFlag32(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4)
{
    if (arg4 != 0) {
        State_SetScriptFlag(0x20, 1);
    }
}

// decomp: BATTLE.PRG func_80084390 (src/BATTLE/BATTLE.PRG/146C.c:12606)
int Battle_QueueSpellTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        short amount = Battle_CalculateActionStatChange(action, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(action, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_80084440 (src/BATTLE/BATTLE.PRG/146C.c:12617)
int Battle_QueuePhysicalTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type != Battle_ActionType_Spell) {
        short amount = Battle_CalculateActionStatChange(action, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(action, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_800844F0 (src/BATTLE/BATTLE.PRG/146C.c:12628)
int Battle_QueuePhysicalAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 0) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_80084570 (src/BATTLE/BATTLE.PRG/146C.c:12639)
int Battle_QueueAirAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 1) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_800845F0 (src/BATTLE/BATTLE.PRG/146C.c:12650)
int Battle_QueueFireAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 2) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_80084670 (src/BATTLE/BATTLE.PRG/146C.c:12661)
int Battle_QueueEarthAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 3) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_800846F0 (src/BATTLE/BATTLE.PRG/146C.c:12672)
int Battle_QueueWaterAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 4) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_80084770 (src/BATTLE/BATTLE.PRG/146C.c:12683)
int Battle_QueueLightAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 5) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_800847F0 (src/BATTLE/BATTLE.PRG/146C.c:12694)
int Battle_QueueDarkAffinityRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    if (Battle_actionState->selection.effectVariant == 6) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_80084870 (src/BATTLE/BATTLE.PRG/146C.c:12705)
int Battle_QueueDamageTriggeredRecovery(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    int temp_v1 = Battle_actionState->selection.primaryTarget.hpEffect;
    if ((temp_v1 == 1) || ((temp_v1 == 3))) {
        short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
        arg2->target.hp += amount;
        arg2->hpEffect = 2;
        Battle_DistributeLimbRecovery(arg0, arg2, amount);
    }
}

// decomp: BATTLE.PRG _increaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12717)
int Battle_QueueRandomStrengthIncrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->strengthChange += amount;
    arg2->strengthEffect = 2;
}

// decomp: BATTLE.PRG _increaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12726)
int Battle_QueueRandomIntelligenceIncrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->intelligenceChange += amount;
    arg2->intelligenceEffect = 2;
}

// decomp: BATTLE.PRG _increaseAgilityRand (src/BATTLE/BATTLE.PRG/146C.c:12735)
int Battle_QueueRandomAgilityIncrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->agilityChange += amount;
    arg2->agilityEffect = 2;
}

// decomp: BATTLE.PRG _increaseMaxHpRand (src/BATTLE/BATTLE.PRG/146C.c:12744)
int Battle_QueueRandomMaximumHpIncrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->maxHpChange += amount;
    arg2->maxHpEffect = 2;
}

// decomp: BATTLE.PRG _increaseMaxMpRand (src/BATTLE/BATTLE.PRG/146C.c:12753)
int Battle_QueueRandomMaximumMpIncrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->maxMpChange += amount;
    arg2->maxMpEffect = 2;
}

// decomp: BATTLE.PRG _decreaseStrengthRand (src/BATTLE/BATTLE.PRG/146C.c:12762)
int Battle_QueueRandomStrengthDecrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->strengthChange += amount;
    arg2->strengthEffect = 1;
}

// decomp: BATTLE.PRG _decreaseIntelligenceRand (src/BATTLE/BATTLE.PRG/146C.c:12771)
int Battle_QueueRandomIntelligenceDecrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->intelligenceChange += amount;
    arg2->intelligenceEffect = 1;
}

// decomp: BATTLE.PRG _decreaseAglityRand (src/BATTLE/BATTLE.PRG/146C.c:12780)
int Battle_QueueRandomAgilityDecrease(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int amount = Core_Random_NextBounded(4) + 1;
    arg2->agilityChange += amount;
    arg2->agilityEffect = 1;
}

// decomp: BATTLE.PRG func_80084B70 (src/BATTLE/BATTLE.PRG/146C.c:12789)
int Battle_QueueHitDamageAndEquipmentWear(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    short temp_v0;
    Battle_ActorStats* temp_s4 = Battle_actors[arg1->target.targetActor]->stats;
    Battle_ActorStats* temp_s2 = Battle_actors[arg2->target.targetActor]->stats;
    Battle_criticalHitOccurred = 0;
    temp_v0 = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
    arg2->target.hp += temp_v0;

    if (Battle_criticalHitOccurred != 0) {
        arg2->hpEffect = 3;
    } else {
        arg2->hpEffect = 1;
    }

    if (arg2->targetsPosition == 0) {

        Battle_DistributeHitLimbDamage(arg0, arg2, temp_v0);

        if (temp_s2->actorId != 0x80) {
            arg2->inflictStatuses |= temp_s2->statuses & 0x8000;
        }

        if (arg4 != 0) {
            Items_ConsumeHitEquipmentDp(arg0, temp_s4, temp_s2, temp_v0);
            Items_AccumulateHitEquipmentPp(arg0, temp_s4, temp_s2, temp_v0);
        }
    }
}

