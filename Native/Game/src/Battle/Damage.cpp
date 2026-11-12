// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/DamagePoliciesLive.hpp"
#include "Core/RandomLive.hpp"
#include "Items/EquipmentLive.hpp"
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
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: D_800E82F4 (D_800E82F4@BATTLE/BATTLE.PRG)
u_short Battle_bladePpDamageBonuses[12]; /* contents: module image (ModuleData.cpp) */

// decomp: _statCalculators (_statCalculators@BATTLE/BATTLE.PRG)
short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: vs_battle_actors (vs_battle_actors@BATTLE/BATTLE.PRG)
Battle_Actor* Battle_actors[42];

// decomp: _lastValue (_lastValue@BATTLE/BATTLE.PRG)
u_int Battle_lastActionStatValue;

// decomp: D_800F1A0C (D_800F1A0C@BATTLE/BATTLE.PRG)
int Battle_criticalHitOccurred;

// decomp: _armorDpAdjustmentAmounts (_armorDpAdjustmentAmounts@BATTLE/BATTLE.PRG)
short Battle_limbArmorDpLoss[8];

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
    value = VS::Battle::LiveApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (withRandMod != 0) {
        value += 1 + VS::Core::NextLiveRandomBounded(5);
    }

    if ((applyStatuses != 0) && (sourceActor->statuses & 2)) {
        value /= 2;
    }

    if (withRandMod != 0) {

        if ((action->id < 6) && (VS::Core::NextLiveRandomBounded(100) < (sourceActor->risk / 10) + 10)) {
            value = (short)((u_int)(value << 0x10) >> 0xF);
            Battle_criticalHitOccurred = 1;
        }

        if (nHit == 0) {
            if (target->targetsPosition == 0) {
                VS::Items::GrowLiveAttackerWeaponRatings(action, sourceActor, (targetActor->enemyClass) + 1,
                    calculation.targetAffinity + 1);
                VS::Items::GrowLiveDefenderEquipmentRatings(action, targetActor, (sourceActor->enemyClass) + 1,
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
    value = VS::Battle::LiveApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (addRandMod != 0) {
        value += 1 + VS::Core::NextLiveRandomBounded(5);
        if (nHit == 0) {
            if (calculation.sourceAffinity != 0) {
                if (target->targetsPosition == 0) {
                    VS::Items::GrowLiveDefenderEquipmentRatings(action, targetActor, 0, calculation.sourceAffinity + 1, limb);
                }
            }
        }
    }

    return value;
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
    value = VS::Battle::LiveApplyDamageGemMultiplier(action, attack - defense, sourceActor, targetActor);

    if (value < 0) {
        value = 0;
    }

    if (withRandMod != 0) {
        randMod = VS::Core::NextLiveRandomAveraged(11) - 5;
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
                VS::Items::GrowLiveDefenderEquipmentRatings(action, targetActor, 0, calculation.sourceAffinity + 1, limb);
            }
        }
    }

    return value;
}

// decomp: BATTLE.PRG _getPhysicalAttackDamageWithStatusModifiers (src/BATTLE/BATTLE.PRG/146C.c:12235)
short Battle_PhysicalDamageWithDebuffs(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    return Battle_CalculatePhysicalHitDamage(action_definition, source_hit, target_hit, hit_index, execute_action, 1);
}

}  // extern "C"
