// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/HitModifiersLive.hpp"
#include "Core/RandomLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: _hitFunctions (_hitFunctions@BATTLE/BATTLE.PRG)
int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: BATTLE.PRG _invalidHitFunction (src/BATTLE/BATTLE.PRG/146C.c:9860)
int Battle_IgnoreHitCallback(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
}

// decomp: BATTLE.PRG _getAgilityDifference (src/BATTLE/BATTLE.PRG/146C.c:9890)
int Battle_AgilityHitChance(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int addVariance)
{
    int agiDiff;
    int i;
    int targetLimb = target->target.targetLimb;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int sourceActorValue = sourceActor->agility + sourceActor->accessory.currentAgility;
    int targetActorValue = targetActor->agility + targetActor->accessory.currentAgility;

    for (i = 0; i < 6; ++i) {
        sourceActorValue += sourceActor->limbs[i].armor.currentAgility;
        targetActorValue += targetActor->limbs[i].armor.currentAgility;
    }

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {
        sourceActorValue +=
            sourceActor->weapon.currentAgility + sourceActor->shield.currentAgility;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
        targetActorValue +=
            targetActor->weapon.currentAgility + targetActor->shield.currentAgility;
    }

    sourceActorValue = sourceActorValue * (100 - sourceActor->risk) / 100;
    targetActorValue += targetActor->limbs[targetLimb].agilityDefenseBonus;
    targetActorValue = targetActorValue * (100 - targetActor->risk) / 100;
    agiDiff = sourceActorValue - targetActorValue;
    agiDiff += 100;

    if (addVariance != 0) {
        int variance = VS::Core::NextLiveRandomAveraged(11);
        int variance_base_chance = agiDiff - 5;
        agiDiff = variance_base_chance + variance;
    }

    agiDiff += VS::Battle::AttackLiveGemHitBonus(action, sourceActor);
    agiDiff -= VS::Battle::DefenseLiveGemHitBonus(action, targetActor, hitNumber);

    if (agiDiff < 0) {
        agiDiff = 0;
    } else if (agiDiff == 255) {
        agiDiff = 254;
    }

    return agiDiff;
}

// decomp: BATTLE.PRG _getIntelligenceDifference (src/BATTLE/BATTLE.PRG/146C.c:9941)
int Battle_IntelligenceHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int hitNumber, int addVariance)
{
    int intDiff;
    int i;
    int targetLimb = target->target.targetLimb;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int sourceActorValue = sourceActor->intelligence + sourceActor->accessory.currentInt;
    int targetActorValue = targetActor->intelligence + targetActor->accessory.currentInt;

    for (i = 0; i < 6; ++i) {
        sourceActorValue += sourceActor->limbs[i].armor.currentInt;
        targetActorValue += targetActor->limbs[i].armor.currentInt;
    }

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {
        sourceActorValue +=
            sourceActor->weapon.currentInt + sourceActor->shield.currentInt;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
        targetActorValue +=
            targetActor->weapon.currentInt + targetActor->shield.currentInt;
    }

    sourceActorValue = (sourceActorValue * (100 - sourceActor->risk)) / 100;
    targetActorValue += targetActor->limbs[targetLimb].agilityDefenseBonus;
    targetActorValue = (targetActorValue * (100 - targetActor->risk) / 100);
    targetActorValue = targetActorValue * targetActor->currentHp / targetActor->maxHp;
    intDiff = sourceActorValue - targetActorValue;

    if (addVariance != 0) {
        int variance = VS::Core::NextLiveRandomAveraged(11);
        int variance_base_chance = intDiff - 5;
        intDiff = variance_base_chance + variance;
    }

    intDiff += VS::Battle::AttackLiveGemHitBonus(action, sourceActor);
    intDiff -= VS::Battle::DefenseLiveGemHitBonus(action, targetActor, hitNumber);

    if (intDiff < 0) {
        intDiff = 0;
    } else if (intDiff == 255) {
        intDiff = 254;
    }

    return intDiff;
}

// decomp: BATTLE.PRG _getAgilityDifference2 (src/BATTLE/BATTLE.PRG/146C.c:9992)
int Battle_CostAdjustedAgilityHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int hitNumber, int addVariance)
{
    int agiDiff;
    int i;
    int v;
    int targetLimb = target->target.targetLimb;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int sourceValue = sourceActor->agility + sourceActor->accessory.currentAgility;
    int targetValue = targetActor->agility + targetActor->accessory.currentAgility;

    for (i = 0; i < 6; ++i) {
        sourceValue += sourceActor->limbs[i].armor.currentAgility;
        targetValue += targetActor->limbs[i].armor.currentAgility;
    }

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {
        sourceValue +=
            sourceActor->weapon.currentAgility + sourceActor->shield.currentAgility;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
        targetValue +=
            targetActor->weapon.currentAgility + targetActor->shield.currentAgility;
    }

    sourceValue = (sourceValue * (100 - sourceActor->risk)) / 100;
    targetValue += targetActor->limbs[targetLimb].agilityDefenseBonus;
    targetValue = targetValue * (100 - targetActor->risk) / 100;
    agiDiff = sourceValue - targetValue;
    v = agiDiff + 130;
    agiDiff = v - action->cost;

    if (addVariance != 0) {
        int variance = VS::Core::NextLiveRandomAveraged(11);
        int variance_base_chance = agiDiff - 5;
        agiDiff = variance_base_chance + variance;
    }

    agiDiff += VS::Battle::AttackLiveGemHitBonus(action, sourceActor);
    agiDiff -= VS::Battle::DefenseLiveGemHitBonus(action, targetActor, hitNumber);

    if (agiDiff < 0) {
        agiDiff = 0;
    } else if (agiDiff == 255) {
        agiDiff = 254;
    }

    return agiDiff;
}

// decomp: BATTLE.PRG _doesAttackHit (src/BATTLE/BATTLE.PRG/146C.c:10075)
int Battle_RollActionHit(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int addVariance)
{
    int chance;
    int threshold;

    if ((target->targetsPosition == 0)
        && (Battle_actors[target->target.targetActor]->stats->actorId == 0x80)) {
        threshold = 100;
    } else {
        threshold = Battle_hitFunctions[action->hits[hitNumber].hitFunctionId](
            action, source, target, hitNumber, addVariance);
        if ((action->type == Battle_ActionType_Spell)
            && (Battle_actors[target->target.targetActor]->stats->statuses < 0)) {
            threshold = 0;
        }
    }

    if (Battle_hitPrerequisiteResult == 0) {
        threshold = 255;
    }

    if (threshold != 255) {
        if (target->targetsPosition == 0) {
            if (Battle_actors[target->target.targetActor]->stats->statuses & 4) {
                threshold *= 2;
            }
        }
        if (threshold > 100) {
            threshold = 100;
        }
    }

    if ((hitNumber == 0) || (target->target.hitThreshold == 255)) {
        target->target.hitThreshold = threshold;
    }

    chance = VS::Core::NextLiveRandomBounded(100);

    if (threshold != 255) {
        if ((source->targetsPosition == 0) && (source->target.targetActor == 0)) {
            threshold += 10;
        }
        return chance < threshold;
    }

    return 0;
}

}  // extern "C"
