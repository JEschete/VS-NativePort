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
int Battle_ResolveActorDefeat(int source_actor_slot, int target_actor_slot, int count_kill)
{
    int ret = 0;
    if (Battle_actors[target_actor_slot] != NULL) {
        if (Battle_actors[target_actor_slot]->stats->currentHp == 0) {
            Battle_ResolveEnemyDefeat(source_actor_slot, target_actor_slot, count_kill);
            if (target_actor_slot == 0) {
                Battle_SuppressAshleyStatusLabels();
                Event_EvaluateRoomEventTriggers(0x40, 0, NULL, 0);
                Event_BeginQueuedCutscene();
                ret = 1;
            } else if (Battle_actors[target_actor_slot]->retainAfterDefeat == 0) {
                Actors_RemoveActorAtSlot(target_actor_slot, 1);
            }
        } else {
            Actors_RestoreActorIdlePose(target_actor_slot);
        }
    }
    return ret;
}

// decomp: BATTLE.PRG func_8007FE5C (src/BATTLE/BATTLE.PRG/146C.c:10534)
short Battle_ApplyDamageGemMultiplier(
    Battle_ActionDefinition* action, short damage_amount, Battle_ActorStats* source_stats, Battle_ActorStats* target_stats)
{
    int has_modifier_gem;
    int damage_percent;
    int i;

    damage_percent = 100;

    if (action->type == Battle_ActionType_Spell) {
        has_modifier_gem = 0;

        for (i = 0; i < 3; ++i) {
            if (source_stats->weapon.gems[i].gemEffects == 4) {
                has_modifier_gem = 1;
            }
        }

        if (has_modifier_gem != 0) {
            damage_percent += 5;
        }

        has_modifier_gem = 0;

        for (i = 0; i < 3; ++i) {
            if (target_stats->shield.gems[i].gemEffects == 8) {
                has_modifier_gem = 1;
            }
        }

    } else {

        has_modifier_gem = 0;

        for (i = 0; i < 3; ++i) {
            if (source_stats->weapon.gems[i].gemEffects == 3) {
                has_modifier_gem = 1;
            }
        }

        if (has_modifier_gem != 0) {
            damage_percent += 5;
        }

        has_modifier_gem = 0;

        for (i = 0; i < 3; ++i) {
            if (target_stats->shield.gems[i].gemEffects == 7) {
                has_modifier_gem = 1;
            }
        }
    }

    if (has_modifier_gem != 0) {
        damage_percent -= 5;
    }

    return (damage_amount * damage_percent) / 100;
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
                int limb_recovery_amount = (amount * (actor->limbs[i].maxHp - actor->limbs[i].hp))
                            / limbDamageTaken;
                Battle_limbArmorDpLoss[i] = limb_recovery_amount;
                entity->limbs[i].effects.opaque0 =
                    entity->limbs[i].effects.opaque0 + ((limb_recovery_amount << 0x10) >> 0xF);
            }
        }
    }
}

// decomp: BATTLE.PRG _statCalculatorNop (src/BATTLE/BATTLE.PRG/146C.c:11230)
void Battle_IgnoreStatCalculator(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
}

// decomp: BATTLE.PRG _getLastValue (src/BATTLE/BATTLE.PRG/146C.c:11237)
short Battle_LastActionStatValue(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    return Battle_lastActionStatValue;
}

// decomp: BATTLE.PRG _getPhysicalAttackDamage (src/BATTLE/BATTLE.PRG/146C.c:11271)
short Battle_CalculatePhysicalHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int withRandMod, int applyStatuses)
{
    Battle_DamageCalculation calculation;
    int attack;
    short maxAffinity;
    short value;
    int i;
    int affinity;
    int defense;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int limb = target->target.targetLimb;

    calculation.sourceClass = sourceActor->enemyClass;
    calculation.targetClass = targetActor->enemyClass;

    if (action->hits[nHit].damageType == 0) {
        calculation.type = sourceActor->weapon.damageType;
        if (!calculation.type) {
            calculation.type = 1;
        }
    } else {
        calculation.type = action->hits[nHit].damageType;
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
        calculation.sourceAffinity = affinity;
    } else {
        calculation.sourceAffinity = action->hits[nHit].affinity - 1;
    }

    affinity = 0;
    maxAffinity = 0;

    for (i = 0; i < 7; ++i) {

        calculation.tempTargetAffinity =
            targetActor->limbs[limb].affinities[i]
            + targetActor->limbs[limb].armor.classAffinityCurrent.affinities[0][i];

        if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
            calculation.tempTargetAffinity +=
                targetActor->shield.classAffinityCurrent.affinities[0][i];
        }

        if (maxAffinity < calculation.tempTargetAffinity) {
            maxAffinity = calculation.tempTargetAffinity;
            affinity = i;
        }
    }

    calculation.targetAffinity = affinity;
    calculation.attackStat = sourceActor->strength;

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {

        calculation.attackStat = calculation.attackStat + sourceActor->weapon.currentStr;
        calculation.typeValue = sourceActor->weapon.damageTypeValue;
        calculation.weaponAffinity =
            sourceActor->weapon.classAffinityCurrent.affinities[0][calculation.sourceAffinity];
        calculation.weaponClass =
            sourceActor->weapon.classAffinityCurrent.classes[0][targetActor->enemyClass];
        calculation.weaponDamage = calculation.weaponClass + (calculation.typeValue + calculation.weaponAffinity);

        if (sourceActor->weapon.maxDp != 0) {
            calculation.weaponDamage =
                calculation.weaponDamage
                - (((sourceActor->weapon.maxDp - sourceActor->weapon.currentDp) * 100)
                    / sourceActor->weapon.maxDp);
        }

        if (sourceActor->weapon.maxPp != 0) {
            calculation.weaponDamage +=
                (sourceActor->weapon.currentPp
                    * *((sourceActor->weapon.blade.category) + Battle_bladePpDamageBonuses))
                / sourceActor->weapon.maxPp;
        }

    } else {
        calculation.weaponClass = 0;
        calculation.weaponAffinity = 0;
        calculation.typeValue = 0;
        calculation.weaponDamage = 0;
    }

    calculation.attackStat += sourceActor->accessory.currentStr;
    calculation.weaponDamage += sourceActor->accessory.types[calculation.type]
                       + sourceActor->accessory.classes[calculation.targetClass]
                       + sourceActor->accessory.affinities[calculation.sourceAffinity];
    calculation.weaponDamage = calculation.weaponDamage / 4 + 100;
    calculation.mediatingStat = targetActor->strength;
    calculation.armorStat = targetActor->limbs[limb].armor.currentStr;
    calculation.shieldStat = 0;
    calculation.armorStatFactor = 100;
    calculation.shieldStatFactor = 100;
    calculation.statFactor = 100;
    calculation.statFactor = targetActor->limbs[limb].types[calculation.type]
                    + targetActor->limbs[limb].affinities[calculation.sourceAffinity] + 100;
    calculation.armorStatFactor =
        targetActor->limbs[limb].armor.classAffinityCurrent.classes[0][calculation.sourceClass]
        + (targetActor->limbs[limb].armor.types[calculation.type]
            + targetActor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][calculation.sourceAffinity])
        + 100;

    if (targetActor->limbs[limb].armor.maxDp != 0) {
        calculation.armorStatFactor -= (((targetActor->limbs[limb].armor.maxDp
                                      - targetActor->limbs[limb].armor.currentDp)
                                     * 100)
                                    / targetActor->limbs[limb].armor.maxDp)
                              / 4;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {

        calculation.shieldStat += targetActor->shield.currentStr;
        calculation.shieldStatFactor =
            calculation.shieldStatFactor
            + ((targetActor->shield.types[calculation.type]
                   + targetActor->shield.classAffinityCurrent
                       .affinities[0][calculation.sourceAffinity])
                + targetActor->shield.classAffinityCurrent.classes[0][calculation.sourceClass]);

        if (targetActor->shield.maxDp != 0) {
            calculation.shieldStatFactor -=
                (((targetActor->shield.maxDp - targetActor->shield.currentDp) * 100)
                    / targetActor->shield.maxDp)
                / 4;
        }
        if (targetActor->shield.maxPp != 0) {
            calculation.shieldStatFactor +=
                (targetActor->shield.currentPp * 100) / targetActor->shield.maxPp / 4;
        }
    }

    calculation.mediatingStat = calculation.mediatingStat + targetActor->accessory.currentStr;
    calculation.statFactor = calculation.statFactor
                    + ((targetActor->accessory.types[calculation.type]
                           + targetActor->accessory.classes[calculation.sourceClass])
                        + targetActor->accessory.affinities[calculation.sourceAffinity]);
    attack = (calculation.attackStat * calculation.weaponDamage) / 100;
    attack = (attack * ((action->hits[nHit].statFactor * 10) + 100)) / 100;
    defense = (calculation.mediatingStat - 20) * calculation.statFactor;
    defense += calculation.shieldStat * calculation.shieldStatFactor;
    defense += calculation.armorStat * calculation.armorStatFactor;
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
                    calculation.targetAffinity + 1);
                Items_GrowDefenderEquipmentRatings(action, targetActor, (sourceActor->enemyClass) + 1,
                    calculation.sourceAffinity + 1, limb);
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
    Battle_DamageCalculation calculation;
    short maxAffinity;
    short value;
    int i;
    int affinity;
    int attack;
    int defense;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int limb = target->target.targetLimb;

    calculation.sourceClass = sourceActor->enemyClass;
    calculation.type = action->hits[nHit].damageType;

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

        calculation.sourceAffinity = affinity;

        affinity = 0;
        maxAffinity = 0;

        for (i = 0; i < 7; ++i) {
            calculation.tempTargetAffinity =
                targetActor->limbs[limb].affinities[i]
                + targetActor->limbs[limb].armor.classAffinityCurrent.affinities[0][i];
            if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {
                calculation.tempTargetAffinity +=
                    targetActor->shield.classAffinityCurrent.affinities[0][i];
            }
            if (maxAffinity < calculation.tempTargetAffinity) {
                maxAffinity = calculation.tempTargetAffinity;
                affinity = i;
            }
        }

        calculation.targetAffinity = affinity;
    } else {
        calculation.sourceAffinity = action->hits[nHit].affinity - 1;
        calculation.targetAffinity = action->hits[nHit].affinity - 1;
    }

    calculation.attackStat = sourceActor->intelligence;

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {

        calculation.attackStat = calculation.attackStat + sourceActor->weapon.currentInt;
        calculation.weaponDamage = calculation.weaponAffinity =
            sourceActor->weapon.classAffinityCurrent.affinities[0][calculation.sourceAffinity];

        if (sourceActor->weapon.maxDp != 0) {
            calculation.weaponDamage -=
                (((sourceActor->weapon.maxDp - sourceActor->weapon.currentDp) * 100)
                    / sourceActor->weapon.maxDp);
        }
        if (sourceActor->weapon.maxPp != 0) {
            calculation.weaponDamage +=
                (sourceActor->weapon.currentPp * 100) / sourceActor->weapon.maxPp;
        }
    } else {
        calculation.weaponClass = 0;
        calculation.weaponAffinity = 0;
        calculation.typeValue = 0;
        calculation.weaponDamage = 0;
    }

    calculation.attackStat += sourceActor->accessory.currentInt;
    calculation.weaponDamage += sourceActor->accessory.types[calculation.type]
                       + sourceActor->accessory.affinities[calculation.sourceAffinity];
    calculation.weaponDamage = calculation.weaponDamage / 4 + 100;
    calculation.mediatingStat = targetActor->intelligence;
    calculation.armorStat = targetActor->limbs[limb].armor.currentInt;
    calculation.shieldStat = 0;
    calculation.armorStatFactor = 100;
    calculation.shieldStatFactor = 100;
    calculation.statFactor = 100;

    if (calculation.type != 0) {
        calculation.statFactor = targetActor->limbs[limb].types[calculation.type] + 100;
        calculation.armorStatFactor = targetActor->limbs[limb].armor.types[calculation.type] + 100;
    }

    calculation.statFactor += targetActor->limbs[limb].affinities[calculation.sourceAffinity];
    calculation.armorStatFactor +=
        targetActor->limbs[limb]
            .armor.classAffinityCurrent.affinities[0][calculation.sourceAffinity];

    if (targetActor->limbs[limb].armor.maxDp != 0) {
        calculation.armorStatFactor -= (((targetActor->limbs[limb].armor.maxDp
                                      - targetActor->limbs[limb].armor.currentDp)
                                     * 100)
                                    / targetActor->limbs[limb].armor.maxDp)
                              / 4;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {

        calculation.shieldStat = targetActor->shield.currentInt;

        if (calculation.type != 0) {
            calculation.shieldStatFactor += targetActor->shield.types[calculation.type];
        }

        calculation.shieldStatFactor =
            calculation.shieldStatFactor
            + targetActor->shield.classAffinityCurrent.affinities[0][calculation.sourceAffinity];

        if (targetActor->shield.maxDp != 0) {
            calculation.shieldStatFactor -=
                (((targetActor->shield.maxDp - targetActor->shield.currentDp) * 100)
                    / targetActor->shield.maxDp)
                / 4;
        }

        if (targetActor->shield.maxPp != 0) {
            calculation.shieldStatFactor +=
                ((targetActor->shield.currentPp * 100) / targetActor->shield.maxPp) / 4;
        }
    }

    calculation.mediatingStat = calculation.mediatingStat + targetActor->accessory.currentInt;
    calculation.statFactor += targetActor->accessory.types[calculation.type]
                     + targetActor->accessory.affinities[calculation.sourceAffinity];
    attack = (calculation.attackStat * calculation.weaponDamage) / 100;
    attack = (attack * (action->hits[nHit].statFactor * 10 + 100)) / 100;
    defense = (calculation.mediatingStat - 20) * calculation.statFactor;
    defense += calculation.shieldStat * calculation.shieldStatFactor;
    defense += calculation.armorStat * calculation.armorStatFactor;
    defense /= (targetActor->risk >> 1) + 100;
    value = Battle_ApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (addRandMod != 0) {
        value += 1 + Core_Random_NextBounded(5);
        if (nHit == 0) {
            if (calculation.sourceAffinity != 0) {
                if (target->targetsPosition == 0) {
                    Items_GrowDefenderEquipmentRatings(action, targetActor, 0, calculation.sourceAffinity + 1, limb);
                }
            }
        }
    }

    return value;
}

// decomp: BATTLE.PRG _previousHitFactor0 (src/BATTLE/BATTLE.PRG/146C.c:11640)
short Battle_ScalePreviousTargetHit(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    return (((Battle_actionState->selection.targets[0].target.hp + Battle_actionState->selection.targets[0].target.mp)
                * action_definition->hits[hit_index].statFactor)
               / 10)
         + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _hpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11650)
short Battle_ScaleSourceMissingHp(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    Battle_ActorStats* actor = Battle_actors[source_hit->target.targetActor]->stats;
    int v = (actor->maxHp - actor->currentHp) * action_definition->hits[hit_index].statFactor;
    return (v / 10) + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _factorCurrentHp (src/BATTLE/BATTLE.PRG/146C.c:11669)
short Battle_ScaleTargetCurrentHp(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    int v = Battle_actors[target_hit->target.targetActor]->stats->currentHp
          * action_definition->hits[hit_index].statFactor;
    return (v / 100) + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _getWeaponPp (src/BATTLE/BATTLE.PRG/146C.c:11678)
short Battle_WeaponPpWithChainOffset(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    return Battle_actors[source_hit->target.targetActor]->stats->weapon.currentPp
         + ((u_short)Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _factorMagicDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:11686)
short Battle_ScaleIncomingSpellDamage(Battle_ActionDefinition* action,
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    short scaled_amount = 0;
    if (((Battle_actionDefinitions[Battle_actionState->selection.actionId].type)) == Battle_ActionType_Spell) {
        if ((Battle_actionState->selection.primaryTarget.hpEffect == 1)
            || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
            scaled_amount = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                         * action->hits[hit_index].statFactor)
                   / 10;
        }
    }
    return scaled_amount;
}

// decomp: BATTLE.PRG _factorPhysicalDamageReceived0 (src/BATTLE/BATTLE.PRG/146C.c:11703)
short Battle_ScaleIncomingPhysicalDamage(Battle_ActionDefinition* action,
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    short scaled_amount = 0;
    if (((Battle_actionDefinitions[Battle_actionState->selection.actionId].type)) != Battle_ActionType_Spell) {
        if ((Battle_actionState->selection.primaryTarget.hpEffect == 1)
            || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
            scaled_amount = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                         * action->hits[hit_index].statFactor)
                   / 10;
        }
    }
    return scaled_amount;
}

// decomp: BATTLE.PRG _mpLostFactor (src/BATTLE/BATTLE.PRG/146C.c:11720)
short Battle_ScaleSourceMissingMp(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    Battle_ActorStats* actor = Battle_actors[source_hit->target.targetActor]->stats;
    return (actor->maxMp - actor->currentMp) * action_definition->hits[hit_index].statFactor / 10
         + (Battle_actionState->queueIndex - 1);
}

// decomp: BATTLE.PRG _hpHealed (src/BATTLE/BATTLE.PRG/146C.c:11850)
short Battle_CalculateSpellHealing(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target, int nHit,
    int withRandMod)
{
    Battle_DamageCalculation calculation;
    short value;
    int randMod;
    int attack;
    int defense;

    Battle_ActorStats* sourceActor = Battle_actors[source->target.targetActor]->stats;
    Battle_ActorStats* targetActor = Battle_actors[target->target.targetActor]->stats;
    int limb = target->target.targetLimb;

    calculation.sourceClass = sourceActor->enemyClass;
    calculation.type = action->hits[nHit].damageType;
    calculation.sourceAffinity = action->hits[nHit].affinity - 1;
    calculation.attackStat = sourceActor->intelligence;

    if (Battle_actors[sourceActor->actorId]->weaponDrawn & 1) {

        calculation.attackStat += sourceActor->weapon.currentInt;

        if (calculation.sourceAffinity) {
            calculation.weaponAffinity =
                sourceActor->weapon.classAffinityCurrent.affinities[0][calculation.sourceAffinity];
        }

        calculation.weaponDamage = calculation.weaponAffinity;

        if (sourceActor->weapon.maxDp != 0) {
            calculation.weaponDamage =
                calculation.weaponAffinity
                - (((sourceActor->weapon.maxDp - sourceActor->weapon.currentDp) * 100)
                    / sourceActor->weapon.maxDp);
        }

        if (sourceActor->weapon.maxPp != 0) {
            calculation.weaponDamage +=
                (sourceActor->weapon.currentPp * 100) / sourceActor->weapon.maxPp;
        }
    } else {
        calculation.weaponClass = 0;
        calculation.weaponAffinity = 0;
        calculation.typeValue = 0;
        calculation.weaponDamage = 0;
    }

    calculation.attackStat += sourceActor->accessory.currentInt;
    calculation.weaponDamage += sourceActor->accessory.types[calculation.type]
                       + sourceActor->accessory.affinities[calculation.sourceAffinity];
    calculation.weaponDamage = (calculation.weaponDamage / 4) + 100;
    calculation.mediatingStat = targetActor->intelligence;
    calculation.armorStat = targetActor->limbs[limb].armor.currentInt;
    calculation.shieldStat = 0;
    calculation.armorStatFactor = 100;
    calculation.shieldStatFactor = 100;
    calculation.statFactor = 100;

    if (calculation.type != 0) {
        calculation.statFactor = targetActor->limbs[limb].types[calculation.type] + 100;
        calculation.armorStatFactor = targetActor->limbs[limb].armor.types[calculation.type] + 100;
    }

    if (calculation.sourceAffinity != 0) {
        calculation.statFactor += targetActor->limbs[limb].affinities[calculation.sourceAffinity];
        calculation.armorStatFactor +=
            targetActor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][calculation.sourceAffinity];
    }

    if (targetActor->limbs[limb].armor.maxDp != 0) {
        calculation.armorStatFactor -= (((targetActor->limbs[limb].armor.maxDp
                                      - targetActor->limbs[limb].armor.currentDp)
                                     * 100)
                                    / targetActor->limbs[limb].armor.maxDp)
                              / 4;
    }

    if (Battle_actors[targetActor->actorId]->weaponDrawn & 1) {

        calculation.shieldStat = targetActor->shield.currentInt;

        if (calculation.type != 0) {
            calculation.shieldStatFactor += targetActor->shield.types[calculation.type];
        }

        if (calculation.sourceAffinity != 0) {
            calculation.shieldStatFactor +=
                targetActor->shield.classAffinityCurrent.affinities[0][calculation.sourceAffinity];
        }

        if (targetActor->shield.maxDp != 0) {
            calculation.shieldStatFactor -=
                (((targetActor->shield.maxDp - targetActor->shield.currentDp) * 100)
                    / targetActor->shield.maxDp)
                / 4;
        }

        if (targetActor->shield.maxPp != 0) {
            calculation.shieldStatFactor +=
                ((targetActor->shield.currentPp * 100) / targetActor->shield.maxPp) / 4;
        }
    }

    calculation.mediatingStat += targetActor->accessory.currentInt;
    calculation.statFactor += targetActor->accessory.types[calculation.type]
                     + targetActor->accessory.affinities[calculation.sourceAffinity];
    attack = (calculation.attackStat * calculation.weaponDamage) / 100;
    attack = (attack * ((action->hits[nHit].statFactor * 10) + 100)) / 100;
    defense = (calculation.mediatingStat - 20) * calculation.statFactor;
    defense += calculation.shieldStat * calculation.shieldStatFactor;
    defense += calculation.armorStat * calculation.armorStatFactor;
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
        if (calculation.sourceAffinity != 0) {
            if (target->targetsPosition == 0) {
                Items_GrowDefenderEquipmentRatings(action, targetActor, 0, calculation.sourceAffinity + 1, limb);
            }
        }
    }

    return value;
}

// decomp: BATTLE.PRG _getRecoverableHp (src/BATTLE/BATTLE.PRG/146C.c:12130)
short Battle_TargetMissingHp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    return Battle_actors[target_hit->target.targetActor]->stats->maxHp
         - Battle_actors[target_hit->target.targetActor]->stats->currentHp;
}

// decomp: BATTLE.PRG _getRecoverableMp (src/BATTLE/BATTLE.PRG/146C.c:12138)
short Battle_TargetMissingMp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    return Battle_actors[target_hit->target.targetActor]->stats->maxMp
         - Battle_actors[target_hit->target.targetActor]->stats->currentMp;
}

// decomp: BATTLE.PRG _factorMagicDamageReceivedOfType (src/BATTLE/BATTLE.PRG/146C.c:12156)
short Battle_ScaleIncomingSpellEffect(Battle_ActionDefinition* action,
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    short scaled_amount = 0;
    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        if ((Battle_actionState->selection.primaryTarget.hpEffect
                == Battle_actionDefinitions[Battle_actionState->selection.actionId].type)
            || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
            scaled_amount = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                         * action->hits[hit_index].statFactor)
                   / 10;
        }
    }
    return scaled_amount;
}

// decomp: BATTLE.PRG _factorSpellCost (src/BATTLE/BATTLE.PRG/146C.c:12191)
short Battle_ScaleIncomingSpellCost(Battle_ActionDefinition* action, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    short scaled_spell_cost;

    if (Battle_actionDefinitions[Battle_actionState->selection.actionId].type == Battle_ActionType_Spell) {
        scaled_spell_cost = (Battle_actionDefinitions[Battle_actionState->selection.actionId].cost
                     * action->hits[hit_index].statFactor)
               / 10;
    } else {
        scaled_spell_cost = 0;
    }

    return scaled_spell_cost;
}

// decomp: BATTLE.PRG _factorDamageReceived (src/BATTLE/BATTLE.PRG/146C.c:12208)
short Battle_ScaleIncomingDamage(Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index,
    int execute_action __attribute__((unused)))
{
    short scaled_amount = 0;
    if ((Battle_actionState->selection.primaryTarget.hpEffect == 1) || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
        scaled_amount = ((Battle_actionState->selection.primaryTarget.target.hp + Battle_actionState->selection.primaryTarget.target.mp)
                     * action_definition->hits[hit_index].statFactor)
               / 10;
    }
    return scaled_amount;
}

// decomp: BATTLE.PRG _getEquippedShieldDp (src/BATTLE/BATTLE.PRG/146C.c:12221)
short Battle_SourceShieldPp(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    return Battle_actors[source_hit->target.targetActor]->stats->shield.currentPp;
}

// decomp: BATTLE.PRG _factorLastValue (src/BATTLE/BATTLE.PRG/146C.c:12228)
short Battle_ScaleLastActionStatValue(Battle_ActionDefinition* action_definition __attribute__((unused)), Battle_HitResult* source_hit,
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    return (Battle_lastActionStatValue * action_definition->hits[hit_index].statFactor) / 10;
}

// decomp: BATTLE.PRG _getPhysicalAttackDamageWithStatusModifiers (src/BATTLE/BATTLE.PRG/146C.c:12235)
short Battle_PhysicalDamageWithDebuffs(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    return Battle_CalculatePhysicalHitDamage(action_definition, source_hit, target_hit, hit_index, execute_action, 1);
}

// decomp: BATTLE.PRG _getRisk (src/BATTLE/BATTLE.PRG/146C.c:12241)
short Battle_TargetRisk(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    return Battle_actors[target_hit->target.targetActor]->stats->risk;
}

// decomp: BATTLE.PRG _calculateStatChange (src/BATTLE/BATTLE.PRG/146C.c:12248)
short Battle_CalculateActionStatChange(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit, int execute_action)
{
    short amount;

    if ((target_hit->targetsPosition == 0)
        && (Battle_actors[target_hit->target.targetActor]->stats->actorId == 128)) {
        amount = 999;
    } else {
        amount = Battle_statCalculators[action->hits[hit].statCalculatorId](
            action, source_hit, target_hit, hit, execute_action);
    }

    Battle_lastActionStatValue = amount;
    return amount;
}

