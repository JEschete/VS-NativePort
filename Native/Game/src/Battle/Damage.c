// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/Animation.h"
#include "Actors/Lifecycle.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#include "Core/Random.h"
#include "Event/Cutscene.h"
#include "Event/Triggers.h"
#include "Items/Equipment.h"
#include "Items/EquipmentAffinity.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: D_800E82F4 (D_800E82F4@BATTLE/BATTLE.PRG)
u_short Battle_bladePpDamageBonuses[12]; /* contents: module image (ModuleData.c) */

// decomp: _statCalculators (_statCalculators@BATTLE/BATTLE.PRG)
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.c) */

// decomp: vs_battle_actors (vs_battle_actors@BATTLE/BATTLE.PRG)
Battle_Actor* Battle_actors[42];

// decomp: _lastValue (_lastValue@BATTLE/BATTLE.PRG)
u_int Battle_lastActionStatValue;

// decomp: D_800F1A0C (D_800F1A0C@BATTLE/BATTLE.PRG)
int Battle_criticalHitOccurred;

// decomp: _armorDpAdjustmentAmounts (_armorDpAdjustmentAmounts@BATTLE/BATTLE.PRG)
short Battle_limbArmorDpLoss[28];

// decomp: BATTLE.PRG func_800792E4 (src/BATTLE/BATTLE.PRG/146C.c:7413)
int Battle_ResolveActorDefeat(int arg0, int arg1, int arg2)
{
    int ret = 0;
    if (Battle_actors[arg1] != NULL) {
        if (Battle_actors[arg1]->stats->currentHp == 0) {
            Battle_ResolveEnemyDefeat(arg0, arg1, arg2);
            if (arg1 == 0) {
                Battle_SuppressAshleyStatusLabels();
                Event_EvaluateRoomEventTriggers(0x40, 0, NULL, 0);
                Event_BeginQueuedCutscene();
                ret = 1;
            } else if (Battle_actors[arg1]->retainAfterDefeat == 0) {
                Actors_RemoveActorAtSlot(arg1, 1);
            }
        } else {
            Actors_RestoreActorIdlePose(arg1);
        }
    }
    return ret;
}

// decomp: BATTLE.PRG func_8007FE5C (src/BATTLE/BATTLE.PRG/146C.c:10534)
short Battle_ApplyDamageGemMultiplier(
    Battle_ActionDefinition* action, short arg1, Battle_ActorStats* arg2, Battle_ActorStats* arg3)
{
    int var_a0;
    int var_t0;
    int i;

    var_t0 = 100;

    if (action->type == Battle_ActionType_Spell) {
        var_a0 = 0;

        for (i = 0; i < 3; ++i) {
            if (arg2->weapon.gems[i].gemEffects == 4) {
                var_a0 = 1;
            }
        }

        if (var_a0 != 0) {
            var_t0 += 5;
        }

        var_a0 = 0;

        for (i = 0; i < 3; ++i) {
            if (arg3->shield.gems[i].gemEffects == 8) {
                var_a0 = 1;
            }
        }

    } else {

        var_a0 = 0;

        for (i = 0; i < 3; ++i) {
            if (arg2->weapon.gems[i].gemEffects == 3) {
                var_a0 = 1;
            }
        }

        if (var_a0 != 0) {
            var_t0 += 5;
        }

        var_a0 = 0;

        for (i = 0; i < 3; ++i) {
            if (arg3->shield.gems[i].gemEffects == 7) {
                var_a0 = 1;
            }
        }
    }

    if (var_a0 != 0) {
        var_t0 -= 5;
    }

    return (arg1 * var_t0) / 100;
}

// decomp: BATTLE.PRG func_80080000 (src/BATTLE/BATTLE.PRG/146C.c:10612)
void Battle_DistributeHitLimbDamage(Battle_ActionDefinition* action, Battle_HitResult* entity, short baseValue)
{
    int i;
    int limbs;
    Battle_ActorStats* actor;

    for (i = 0; i < 6; ++i) {
        Battle_limbArmorDpLoss[i] = 0;
    }

    actor = Battle_actors[entity->target.targetActor]->stats;

    switch (action->category) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
        limbs = 0;

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].maxHp != 0) {
                ++limbs;
            }
        }

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].maxHp != 0) {
                Battle_limbArmorDpLoss[i] = baseValue / limbs;
                entity->limbs[i].effects.opaque0 += Battle_limbArmorDpLoss[i];
            }
        }
        break;
    case 5:
    case 7:
    case 14:
        for (i = 0; i < 6; ++i) {
            Battle_limbArmorDpLoss[i] =
                (baseValue * actor->dpSplash[entity->target.targetLimb][i]) / 100;
            entity->limbs[i].effects.opaque0 += Battle_limbArmorDpLoss[i];
        }
        break;
    case 6:
    case 8:
    case 11:
    case 12:
        Battle_limbArmorDpLoss[entity->target.targetLimb] = baseValue;
        entity->limbs[entity->target.targetLimb].effects.opaque0 += baseValue;
        break;
    }
}

// decomp: BATTLE.PRG func_800801E0 (src/BATTLE/BATTLE.PRG/146C.c:10664)
void Battle_DistributeLimbRecovery(Battle_ActionDefinition* action, Battle_HitResult* entity, short amount)
{
    int i;
    int limbDamageTaken = 0;
    Battle_ActorStats* actor = Battle_actors[entity->target.targetActor]->stats;

    for (i = 0; i < 6; ++i) {
        if (actor->limbs[i].maxHp != 0) {
            limbDamageTaken += actor->limbs[i].maxHp - actor->limbs[i].hp;
        }
    }

    if (limbDamageTaken != 0) {
        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].maxHp != 0) {
                int temp_lo = (amount * (actor->limbs[i].maxHp - actor->limbs[i].hp))
                            / limbDamageTaken;
                Battle_limbArmorDpLoss[i] = temp_lo;
                entity->limbs[i].effects.opaque0 =
                    entity->limbs[i].effects.opaque0 + ((temp_lo << 0x10) >> 0xF);
            }
        }
    }
}

// decomp: BATTLE.PRG _statCalculatorNop (src/BATTLE/BATTLE.PRG/146C.c:11230)
void Battle_IgnoreStatCalculator(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
}

// decomp: BATTLE.PRG _getLastValue (src/BATTLE/BATTLE.PRG/146C.c:11237)
short Battle_LastActionStatValue(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    return Battle_lastActionStatValue;
}

// decomp: BATTLE.PRG _getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11271)
short Battle_CalculatePhysicalHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int withRandMod, int applyStatuses)
{
    Battle_DamageCalculation sp18;
    int attack;
    short maxAffinity;
    short value;
    int i;
    int affinity;
    int defense;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int limb = target->target.targetLimb;

    sp18.sourceClass = sourceActor->enemyClass;
    sp18.targetClass = targetActor->enemyClass;

    if (action->hits[nHit].damageType == 0) {
        sp18.type = sourceActor->weapon.damageType;
        if (!sp18.type) {
            sp18.type = 1;
        }
    } else {
        sp18.type = action->hits[nHit].damageType;
    }

    if (action->hits[nHit].affinity == 0) {
        if (!(Battle_actors[sourceActor->actorId]->weaponDrawn & 1)
            || sourceActor->weapon.blade.id == 0) {
            affinity = 0;
        } else {
            affinity = 0;
            maxAffinity = 0;
            for (i = 0; i < 7; ++i) {
                if (maxAffinity
                    < sourceActor->weapon.classAffinityCurrent.affinities[0][i]) {
                    maxAffinity = sourceActor->weapon.classAffinityCurrent.affinities[0][i];
                    affinity = i;
                }
            }
        }
        sp18.sourceAffinity = affinity;
    } else {
        sp18.sourceAffinity = action->hits[nHit].affinity - 1;
    }

    affinity = 0;
    maxAffinity = 0;

    for (i = 0; i < 7; ++i) {

        sp18.tempTargetAffinity =
            targetActor->limbs[limb].affinities[i]
            + targetActor->limbs[limb].armor.classAffinityCurrent.affinities[0][i];

        if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
            sp18.tempTargetAffinity +=
                targetActor->shield.classAffinityCurrent.affinities[0][i];
        }

        if (maxAffinity < sp18.tempTargetAffinity) {
            maxAffinity = sp18.tempTargetAffinity;
            affinity = i;
        }
    }

    sp18.targetAffinity = affinity;
    sp18.attackStat = sourceActor->strength;

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {

        sp18.attackStat = sp18.attackStat + sourceActor->weapon.currentStr;
        sp18.typeValue = sourceActor->weapon.damageTypeValue;
        sp18.weaponAffinity =
            sourceActor->weapon.classAffinityCurrent.affinities[0][sp18.sourceAffinity];
        sp18.weaponClass =
            sourceActor->weapon.classAffinityCurrent.classes[0][targetActor->enemyClass];
        sp18.weaponDamage = sp18.weaponClass + (sp18.typeValue + sp18.weaponAffinity);

        if (sourceActor->weapon.maxDp != 0) {
            sp18.weaponDamage =
                sp18.weaponDamage
                - (((sourceActor->weapon.maxDp - sourceActor->weapon.currentDp) * 100)
                    / sourceActor->weapon.maxDp);
        }

        if (sourceActor->weapon.maxPp != 0) {
            sp18.weaponDamage +=
                (sourceActor->weapon.currentPp
                    * *((sourceActor->weapon.blade.category) + Battle_bladePpDamageBonuses))
                / sourceActor->weapon.maxPp;
        }

    } else {
        sp18.weaponClass = 0;
        sp18.weaponAffinity = 0;
        sp18.typeValue = 0;
        sp18.weaponDamage = 0;
    }

    sp18.attackStat += sourceActor->accessory.currentStr;
    sp18.weaponDamage += sourceActor->accessory.types[sp18.type]
                       + sourceActor->accessory.classes[sp18.targetClass]
                       + sourceActor->accessory.affinities[sp18.sourceAffinity];
    sp18.weaponDamage = sp18.weaponDamage / 4 + 100;
    sp18.mediatingStat = targetActor->strength;
    sp18.armorStat = targetActor->limbs[limb].armor.currentStr;
    sp18.shieldStat = 0;
    sp18.armorStatFactor = 100;
    sp18.shieldStatFactor = 100;
    sp18.statFactor = 100;
    sp18.statFactor = targetActor->limbs[limb].types[sp18.type]
                    + targetActor->limbs[limb].affinities[sp18.sourceAffinity] + 100;
    sp18.armorStatFactor =
        targetActor->limbs[limb].armor.classAffinityCurrent.classes[0][sp18.sourceClass]
        + (targetActor->limbs[limb].armor.types[sp18.type]
            + targetActor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][sp18.sourceAffinity])
        + 100;

    if (targetActor->limbs[limb].armor.maxDp != 0) {
        sp18.armorStatFactor -= (((targetActor->limbs[limb].armor.maxDp
                                      - targetActor->limbs[limb].armor.currentDp)
                                     * 100)
                                    / targetActor->limbs[limb].armor.maxDp)
                              / 4;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {

        sp18.shieldStat += targetActor->shield.currentStr;
        sp18.shieldStatFactor =
            sp18.shieldStatFactor
            + ((targetActor->shield.types[sp18.type]
                   + targetActor->shield.classAffinityCurrent
                       .affinities[0][sp18.sourceAffinity])
                + targetActor->shield.classAffinityCurrent.classes[0][sp18.sourceClass]);

        if (targetActor->shield.maxDp != 0) {
            sp18.shieldStatFactor -=
                (((targetActor->shield.maxDp - targetActor->shield.currentDp) * 100)
                    / targetActor->shield.maxDp)
                / 4;
        }
        if (targetActor->shield.maxPp != 0) {
            sp18.shieldStatFactor +=
                (targetActor->shield.currentPp * 100) / targetActor->shield.maxPp / 4;
        }
    }

    sp18.mediatingStat = sp18.mediatingStat + targetActor->accessory.currentStr;
    sp18.statFactor = sp18.statFactor
                    + ((targetActor->accessory.types[sp18.type]
                           + targetActor->accessory.classes[sp18.sourceClass])
                        + targetActor->accessory.affinities[sp18.sourceAffinity]);
    attack = (sp18.attackStat * sp18.weaponDamage) / 100;
    attack = (attack * ((action->hits[nHit].statFactor * 10) + 100)) / 100;
    defense = (sp18.mediatingStat - 20) * sp18.statFactor;
    defense += sp18.shieldStat * sp18.shieldStatFactor;
    defense += sp18.armorStat * sp18.armorStatFactor;
    defense /= (targetActor->risk / 2) + 100;
    value = Battle_ApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (withRandMod != 0) {
        value += 1 + Core_Random_NextBounded(5);
    }

    if ((applyStatuses != 0) && (sourceActor->statuses & 2)) {
        value /= 2;
    }

    if (withRandMod != 0) {

        if ((action->id < 6) && (Core_Random_NextBounded(100) < (sourceActor->risk / 10) + 10)) {
            value = (short)((u_int)(value << 0x10) >> 0xF);
            Battle_criticalHitOccurred = 1;
        }

        if (nHit == 0) {
            if (target->targetsPosition == 0) {
                Items_GrowAttackerWeaponRatings(action, sourceActor, (targetActor->enemyClass) + 1,
                    sp18.targetAffinity + 1);
                Items_GrowDefenderEquipmentRatings(action, targetActor, (sourceActor->enemyClass) + 1,
                    sp18.sourceAffinity + 1, limb);
            }
        }
    }

    return value;
}

// decomp: BATTLE.PRG _getPhysicalAttackDamageWithoutStatusModifiers (src/BATTLE/BATTLE.PRG/146C.c:11467)
short Battle_PhysicalDamageWithoutDebuffs(Battle_ActionDefinition* action,
    Battle_HitResult* source, Battle_HitResult* target, int nHit, int withRandMod)
{
    return Battle_CalculatePhysicalHitDamage(action, source, target, nHit, withRandMod, 0);
}

// decomp: BATTLE.PRG _getMagicAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11473)
short Battle_CalculateSpellHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int addRandMod)
{
    Battle_DamageCalculation sp18;
    short maxAffinity;
    short value;
    int i;
    int affinity;
    int attack;
    int defense;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int limb = target->target.targetLimb;

    sp18.sourceClass = sourceActor->enemyClass;
    sp18.type = action->hits[nHit].damageType;

    if (action->hits[nHit].affinity == 0) {

        if (!(Battle_actors[sourceActor->actorId]->weaponDrawn & 1)
            || sourceActor->weapon.blade.id == 0) {
            affinity = 0;
        } else {
            affinity = 0;
            maxAffinity = 0;
            for (i = 0; i < 7; ++i) {
                if (maxAffinity
                    < sourceActor->weapon.classAffinityCurrent.affinities[0][i]) {
                    maxAffinity = sourceActor->weapon.classAffinityCurrent.affinities[0][i];
                    affinity = i;
                }
            }
        }

        sp18.sourceAffinity = affinity;

        affinity = 0;
        maxAffinity = 0;

        for (i = 0; i < 7; ++i) {
            sp18.tempTargetAffinity =
                targetActor->limbs[limb].affinities[i]
                + targetActor->limbs[limb].armor.classAffinityCurrent.affinities[0][i];
            if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
                sp18.tempTargetAffinity +=
                    targetActor->shield.classAffinityCurrent.affinities[0][i];
            }
            if (maxAffinity < sp18.tempTargetAffinity) {
                maxAffinity = sp18.tempTargetAffinity;
                affinity = i;
            }
        }

        sp18.targetAffinity = affinity;
    } else {
        sp18.sourceAffinity = action->hits[nHit].affinity - 1;
        sp18.targetAffinity = action->hits[nHit].affinity - 1;
    }

    sp18.attackStat = sourceActor->intelligence;

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {

        sp18.attackStat = sp18.attackStat + sourceActor->weapon.currentInt;
        sp18.weaponDamage = sp18.weaponAffinity =
            sourceActor->weapon.classAffinityCurrent.affinities[0][sp18.sourceAffinity];

        if (sourceActor->weapon.maxDp != 0) {
            sp18.weaponDamage -=
                (((sourceActor->weapon.maxDp - sourceActor->weapon.currentDp) * 100)
                    / sourceActor->weapon.maxDp);
        }
        if (sourceActor->weapon.maxPp != 0) {
            sp18.weaponDamage +=
                (sourceActor->weapon.currentPp * 100) / sourceActor->weapon.maxPp;
        }
    } else {
        sp18.weaponClass = 0;
        sp18.weaponAffinity = 0;
        sp18.typeValue = 0;
        sp18.weaponDamage = 0;
    }

    sp18.attackStat += sourceActor->accessory.currentInt;
    sp18.weaponDamage += sourceActor->accessory.types[sp18.type]
                       + sourceActor->accessory.affinities[sp18.sourceAffinity];
    sp18.weaponDamage = sp18.weaponDamage / 4 + 100;
    sp18.mediatingStat = targetActor->intelligence;
    sp18.armorStat = targetActor->limbs[limb].armor.currentInt;
    sp18.shieldStat = 0;
    sp18.armorStatFactor = 100;
    sp18.shieldStatFactor = 100;
    sp18.statFactor = 100;

    if (sp18.type != 0) {
        sp18.statFactor = targetActor->limbs[limb].types[sp18.type] + 100;
        sp18.armorStatFactor = targetActor->limbs[limb].armor.types[sp18.type] + 100;
    }

    sp18.statFactor += targetActor->limbs[limb].affinities[sp18.sourceAffinity];
    sp18.armorStatFactor +=
        targetActor->limbs[limb]
            .armor.classAffinityCurrent.affinities[0][sp18.sourceAffinity];

    if (targetActor->limbs[limb].armor.maxDp != 0) {
        sp18.armorStatFactor -= (((targetActor->limbs[limb].armor.maxDp
                                      - targetActor->limbs[limb].armor.currentDp)
                                     * 100)
                                    / targetActor->limbs[limb].armor.maxDp)
                              / 4;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {

        sp18.shieldStat = targetActor->shield.currentInt;

        if (sp18.type != 0) {
            sp18.shieldStatFactor += targetActor->shield.types[sp18.type];
        }

        sp18.shieldStatFactor =
            sp18.shieldStatFactor
            + targetActor->shield.classAffinityCurrent.affinities[0][sp18.sourceAffinity];

        if (targetActor->shield.maxDp != 0) {
            sp18.shieldStatFactor -=
                (((targetActor->shield.maxDp - targetActor->shield.currentDp) * 100)
                    / targetActor->shield.maxDp)
                / 4;
        }

        if (targetActor->shield.maxPp != 0) {
            sp18.shieldStatFactor +=
                ((targetActor->shield.currentPp * 100) / targetActor->shield.maxPp) / 4;
        }
    }

    sp18.mediatingStat = sp18.mediatingStat + targetActor->accessory.currentInt;
    sp18.statFactor += targetActor->accessory.types[sp18.type]
                     + targetActor->accessory.affinities[sp18.sourceAffinity];
    attack = (sp18.attackStat * sp18.weaponDamage) / 100;
    attack = (attack * (action->hits[nHit].statFactor * 10 + 100)) / 100;
    defense = (sp18.mediatingStat - 20) * sp18.statFactor;
    defense += sp18.shieldStat * sp18.shieldStatFactor;
    defense += sp18.armorStat * sp18.armorStatFactor;
    defense /= (targetActor->risk >> 1) + 100;
    value = Battle_ApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (addRandMod != 0) {
        value += 1 + Core_Random_NextBounded(5);
        if (nHit == 0) {
            if (sp18.sourceAffinity != 0) {
                if (target->targetsPosition == 0) {
                    Items_GrowDefenderEquipmentRatings(action, targetActor, 0, sp18.sourceAffinity + 1, limb);
                }
            }
        }
    }

    return value;
}

// decomp: BATTLE.PRG _previousHitFactor0 (src/BATTLE/BATTLE.PRG/146C.c:11640)
short Battle_ScalePreviousTargetHit(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    return (((Battle_actionState->selection.targets[0].target.hp + Battle_actionState->selection.targets[0].target.mp)
                * arg0->hits[arg3].statFactor)
               / 10)
         + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _hpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11650)
short Battle_ScaleSourceMissingHp(Battle_ActionDefinition* arg0, Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    Battle_ActorStats* actor = Battle_actors[arg1->target.targetActor]->stats;
    int v = (actor->maxHp - actor->currentHp) * arg0->hits[arg3].statFactor;
    return (v / 10) + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _factorCurrentHp (src/BATTLE/BATTLE.PRG/146C.c:11669)
short Battle_ScaleTargetCurrentHp(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    int v = Battle_actors[arg2->target.targetActor]->stats->currentHp
          * arg0->hits[arg3].statFactor;
    return (v / 100) + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _getWeaponPp (src/BATTLE/BATTLE.PRG/146C.c:11678)
short Battle_WeaponPpWithChainOffset(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    return Battle_actors[arg1->target.targetActor]->stats->weapon.currentPp
         + ((u_short)Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _factorMagicDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:11686)
short Battle_ScaleIncomingSpellDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    short var_a2 = 0;
    if (((Battle_actionDefinitions[Battle_actionState->selection.actionId].type)) == Battle_ActionType_Spell) {
        if ((Battle_actionState->selection.primaryTarget.hpEffect == 1)
            || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
            var_a2 = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                         * action->hits[arg3].statFactor)
                   / 10;
        }
    }
    return var_a2;
}

// decomp: BATTLE.PRG _factorPhysicalDamageReceived0 (src/BATTLE/BATTLE.PRG/146C.c:11703)
short Battle_ScaleIncomingPhysicalDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    short var_a2 = 0;
    if (((Battle_actionDefinitions[Battle_actionState->selection.actionId].type)) != Battle_ActionType_Spell) {
        if ((Battle_actionState->selection.primaryTarget.hpEffect == 1)
            || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
            var_a2 = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                         * action->hits[arg3].statFactor)
                   / 10;
        }
    }
    return var_a2;
}

// decomp: BATTLE.PRG _mpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11720)
short Battle_ScaleSourceMissingMp(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    Battle_ActorStats* actor = Battle_actors[arg1->target.targetActor]->stats;
    return (actor->maxMp - actor->currentMp) * arg0->hits[arg3].statFactor / 10
         + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _hpHealed (src/BATTLE/BATTLE.PRG/146C.c:11850)
short Battle_CalculateSpellHealing(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target, int nHit,
    int withRandMod)
{
    Battle_DamageCalculation sp18;
    short value;
    int randMod;
    int attack;
    int defense;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int limb = target->target.targetLimb;

    sp18.sourceClass = sourceActor->enemyClass;
    sp18.type = action->hits[nHit].damageType;
    sp18.sourceAffinity = action->hits[nHit].affinity - 1;
    sp18.attackStat = sourceActor->intelligence;

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {

        sp18.attackStat += sourceActor->weapon.currentInt;

        if (sp18.sourceAffinity) {
            sp18.weaponAffinity =
                sourceActor->weapon.classAffinityCurrent.affinities[0][sp18.sourceAffinity];
        }

        sp18.weaponDamage = sp18.weaponAffinity;

        if (sourceActor->weapon.maxDp != 0) {
            sp18.weaponDamage =
                sp18.weaponAffinity
                - (((sourceActor->weapon.maxDp - sourceActor->weapon.currentDp) * 100)
                    / sourceActor->weapon.maxDp);
        }

        if (sourceActor->weapon.maxPp != 0) {
            sp18.weaponDamage +=
                (sourceActor->weapon.currentPp * 100) / sourceActor->weapon.maxPp;
        }
    } else {
        sp18.weaponClass = 0;
        sp18.weaponAffinity = 0;
        sp18.typeValue = 0;
        sp18.weaponDamage = 0;
    }

    sp18.attackStat += sourceActor->accessory.currentInt;
    sp18.weaponDamage += sourceActor->accessory.types[sp18.type]
                       + sourceActor->accessory.affinities[sp18.sourceAffinity];
    sp18.weaponDamage = (sp18.weaponDamage / 4) + 100;
    sp18.mediatingStat = targetActor->intelligence;
    sp18.armorStat = targetActor->limbs[limb].armor.currentInt;
    sp18.shieldStat = 0;
    sp18.armorStatFactor = 100;
    sp18.shieldStatFactor = 100;
    sp18.statFactor = 100;

    if (sp18.type != 0) {
        sp18.statFactor = targetActor->limbs[limb].types[sp18.type] + 100;
        sp18.armorStatFactor = targetActor->limbs[limb].armor.types[sp18.type] + 100;
    }

    if (sp18.sourceAffinity != 0) {
        sp18.statFactor += targetActor->limbs[limb].affinities[sp18.sourceAffinity];
        sp18.armorStatFactor +=
            targetActor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][sp18.sourceAffinity];
    }

    if (targetActor->limbs[limb].armor.maxDp != 0) {
        sp18.armorStatFactor -= (((targetActor->limbs[limb].armor.maxDp
                                      - targetActor->limbs[limb].armor.currentDp)
                                     * 100)
                                    / targetActor->limbs[limb].armor.maxDp)
                              / 4;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {

        sp18.shieldStat = targetActor->shield.currentInt;

        if (sp18.type != 0) {
            sp18.shieldStatFactor += targetActor->shield.types[sp18.type];
        }

        if (sp18.sourceAffinity != 0) {
            sp18.shieldStatFactor +=
                targetActor->shield.classAffinityCurrent.affinities[0][sp18.sourceAffinity];
        }

        if (targetActor->shield.maxDp != 0) {
            sp18.shieldStatFactor -=
                (((targetActor->shield.maxDp - targetActor->shield.currentDp) * 100)
                    / targetActor->shield.maxDp)
                / 4;
        }

        if (targetActor->shield.maxPp != 0) {
            sp18.shieldStatFactor +=
                ((targetActor->shield.currentPp * 100) / targetActor->shield.maxPp) / 4;
        }
    }

    sp18.mediatingStat += targetActor->accessory.currentInt;
    sp18.statFactor += targetActor->accessory.types[sp18.type]
                     + targetActor->accessory.affinities[sp18.sourceAffinity];
    attack = (sp18.attackStat * sp18.weaponDamage) / 100;
    attack = (attack * ((action->hits[nHit].statFactor * 10) + 100)) / 100;
    defense = (sp18.mediatingStat - 20) * sp18.statFactor;
    defense += sp18.shieldStat * sp18.shieldStatFactor;
    defense += sp18.armorStat * sp18.armorStatFactor;
    defense /= (targetActor->risk / 2) + 100;
    value = Battle_ApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (withRandMod != 0) {
        randMod = Core_Random_NextAveraged(11) - 5;
    } else {
        randMod = 0;
    }

    value += randMod;

    if (value < 0) {
        value = 0;
    }

    if ((withRandMod != 0) && (nHit == 0)) {
        if (sp18.sourceAffinity != 0) {
            if (target->targetsPosition == 0) {
                Items_GrowDefenderEquipmentRatings(action, targetActor, 0, sp18.sourceAffinity + 1, limb);
            }
        }
    }

    return value;
}

// decomp: BATTLE.PRG _getRecoverableHp (src/BATTLE/BATTLE.PRG/146C.c:12130)
short Battle_TargetMissingHp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    return Battle_actors[arg2->target.targetActor]->stats->maxHp
         - Battle_actors[arg2->target.targetActor]->stats->currentHp;
}

// decomp: BATTLE.PRG _getRecoverableMp (src/BATTLE/BATTLE.PRG/146C.c:12138)
short Battle_TargetMissingMp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    return Battle_actors[arg2->target.targetActor]->stats->maxMp
         - Battle_actors[arg2->target.targetActor]->stats->currentMp;
}

// decomp: BATTLE.PRG _factorMagicDamageReceivedOfType (src/BATTLE/BATTLE.PRG/146C.c:12156)
short Battle_ScaleIncomingSpellEffect(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    short var_a2 = 0;
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        if ((Battle_actionState->selection.primaryTarget.hpEffect
                == Battle_actionDefinitions[Battle_actionState->selection.actionId].type)
            || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
            var_a2 = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                         * action->hits[arg3].statFactor)
                   / 10;
        }
    }
    return var_a2;
}

// decomp: BATTLE.PRG _factorSpellCost (src/BATTLE/BATTLE.PRG/146C.c:12191)
short Battle_ScaleIncomingSpellCost(Battle_ActionDefinition* action, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    short var_v0;

    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        var_v0 = (Battle_actionDefinitions[Battle_actionState->selection.actionId].cost
                     * action->hits[arg3].statFactor)
               / 10;
    } else {
        var_v0 = 0;
    }

    return var_v0;
}

// decomp: BATTLE.PRG _factorDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:12208)
short Battle_ScaleIncomingDamage(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)))
{
    short var_a2 = 0;
    if ((Battle_actionState->selection.primaryTarget.hpEffect == 1) || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
        var_a2 = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                     * arg0->hits[arg3].statFactor)
               / 10;
    }
    return var_a2;
}

// decomp: BATTLE.PRG _getEquippedShieldDp (src/BATTLE/BATTLE.PRG/146C.c:12221)
short Battle_SourceShieldPp(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    return Battle_actors[arg1->target.targetActor]->stats->shield.currentPp;
}

// decomp: BATTLE.PRG _factorLastValue (src/BATTLE/BATTLE.PRG/146C.c:12228)
short Battle_ScaleLastActionStatValue(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    return (Battle_lastActionStatValue * arg0->hits[arg3].statFactor) / 10;
}

// decomp: BATTLE.PRG _getPhysicalAttackDamageWithStatusModifiers (src/BATTLE/BATTLE.PRG/146C.c:12235)
short Battle_PhysicalDamageWithDebuffs(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    return Battle_CalculatePhysicalHitDamage(arg0, arg1, arg2, arg3, arg4, 1);
}

// decomp: BATTLE.PRG _getRisk (src/BATTLE/BATTLE.PRG/146C.c:12241)
short Battle_TargetRisk(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    return Battle_actors[arg2->target.targetActor]->stats->risk;
}

// decomp: BATTLE.PRG _calculateStatChange (src/BATTLE/BATTLE.PRG/146C.c:12248)
short Battle_CalculateActionStatChange(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int hit, int arg4)
{
    short amount;

    if ((arg2->targetsPosition == 0)
        && (Battle_actors[arg2->target.targetActor]->stats->actorId == 128)) {
        amount = 999;
    } else {
        amount = Battle_statCalculators[action->hits[hit].statCalculatorId](
            action, arg1, arg2, hit, arg4);
    }

    Battle_lastActionStatValue = amount;
    return amount;
}

