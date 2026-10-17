// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
#include "Core/Random.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: _hitFunctions (_hitFunctions@BATTLE/BATTLE.PRG)
int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* data: contents loaded in Step 5 */

// decomp: BATTLE.PRG _getAttackGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9712)
short Battle_AttackGemHitBonus(Battle_ActionDefinition* action, Battle_ActorStats* actor)
{
    int hasGemBuff = 0;
    int gemBuff = 0;

    if (Battle_actors[actor->actorId]->weaponDrawn & 1) {
        int i;
        if (action->type == Battle_ActionType_Spell) {
            hasGemBuff = 0;
            for (i = 0; i < 3; ++i) {
                if (actor->weapon.gems[i].gemEffects == 2) {
                    hasGemBuff = 1;
                }
            }
        } else {
            hasGemBuff = 0;
            for (i = 0; i < 3; ++i) {
                if (actor->weapon.gems[i].gemEffects == 1) {
                    hasGemBuff = 1;
                }
            }
        }
        if (hasGemBuff != 0) {
            gemBuff = 20;
        }
    }
    return gemBuff;
}

// decomp: BATTLE.PRG _getDefenseGemBuff (src/BATTLE/BATTLE.PRG/146C.c:9741)
short Battle_DefenseGemHitBonus(Battle_ActionDefinition* action, Battle_ActorStats* actor, int hitNumber)
{
    int hasGemBuff;
    int i;

    int gemBuff = 0;
    if (Battle_actors[actor->actorId]->weaponDrawn & 1) {
        hasGemBuff = 0;
        if (action->type == 1) {
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 6) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
        } else {
            u_int v = action->id;
            if ((v < 40) || (((v - 54) & 0xFF) < 170)) {
                hasGemBuff = 0;
                for (i = 0; i < 3; ++i) {
                    if (actor->shield.gems[i].gemEffects == 5) {
                        hasGemBuff = 1;
                    }
                }
                if (hasGemBuff != 0) {
                    gemBuff += 20;
                }
            }
        }

        hasGemBuff = 0;
        switch (action->hits[hitNumber].effectId) {
        case 1:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 11) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 2:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 12) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 3:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 13) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 5:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 14) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 11:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 16) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 15:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 18) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 16:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 19) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        case 17:
            for (i = 0; i < 3; ++i) {
                if (actor->shield.gems[i].gemEffects == 21) {
                    hasGemBuff = 1;
                }
            }
            if (hasGemBuff != 0) {
                gemBuff += 20;
            }
            break;
        }
    }
    return gemBuff;
}

// decomp: BATTLE.PRG _invalidHitFunction (src/BATTLE/BATTLE.PRG/146C.c:9860)
int Battle_IgnoreHitCallback(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
}

// decomp: BATTLE.PRG func_8007E97C (src/BATTLE/BATTLE.PRG/146C.c:9867)
int Battle_PreviousHitSuccessThreshold(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    return Battle_previousHitResult != 0 ? 100 : 255;
}

// decomp: BATTLE.PRG _getHitGemModifier (src/BATTLE/BATTLE.PRG/146C.c:9875)
int Battle_GemHitChance(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int arg4 __attribute__((unused)))
{
    int gemModifier = 100;
    if ((source != NULL) && (source->targetsPosition == 0)) {
        gemModifier +=
            Battle_AttackGemHitBonus(action, Battle_actors[source->target.targetActor]->stats);
    }
    if ((target != NULL) && (target->targetsPosition == 0)) {
        gemModifier -= Battle_DefenseGemHitBonus(
            action, Battle_actors[target->target.targetActor]->stats, hitNumber);
    }
    return gemModifier;
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
        int variance = Core_Random_NextAveraged(11);
        int v1 = agiDiff - 5;
        agiDiff = v1 + variance;
    }

    agiDiff += Battle_AttackGemHitBonus(action, sourceActor);
    agiDiff -= Battle_DefenseGemHitBonus(action, targetActor, hitNumber);

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
        int variance = Core_Random_NextAveraged(11);
        int v1 = intDiff - 5;
        intDiff = v1 + variance;
    }

    intDiff += Battle_AttackGemHitBonus(action, sourceActor);
    intDiff -= Battle_DefenseGemHitBonus(action, targetActor, hitNumber);

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
        int variance = Core_Random_NextAveraged(11);
        int v1 = agiDiff - 5;
        agiDiff = v1 + variance;
    }

    agiDiff += Battle_AttackGemHitBonus(action, sourceActor);
    agiDiff -= Battle_DefenseGemHitBonus(action, targetActor, hitNumber);

    if (agiDiff < 0) {
        agiDiff = 0;
    } else if (agiDiff == 255) {
        agiDiff = 254;
    }

    return agiDiff;
}

// decomp: BATTLE.PRG _getRiskModifier (src/BATTLE/BATTLE.PRG/146C.c:10045)
int Battle_RiskHitChance(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* source,
    Battle_HitResult* arg2 __attribute__((unused)), int hitNumber __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    int rate =
        ((Battle_actors[source->target.targetActor]->stats->risk + 150) * 100) / 256;
    if (rate == 255) {
        rate = 254;
    }
    return rate;
}

// decomp: BATTLE.PRG _getChainEvasionModifier (src/BATTLE/BATTLE.PRG/146C.c:10057)
int Battle_ChainHitChance(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* target,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int rate;

    rate = ((255
                - Battle_actors[target->target.targetActor]
                    ->stats->limbs[target->target.targetLimb]
                    .chainEvasion)
               * 100)
         / 255;
    if (rate == 255) {
        rate = 254;
    }
    return rate;
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

    chance = Core_Random_NextBounded(100);

    if (threshold != 255) {
        if ((source->targetsPosition == 0) && (source->target.targetActor == 0)) {
            threshold += 10;
        }
        return chance < threshold;
    }

    return 0;
}

