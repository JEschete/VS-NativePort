// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Core/Random.h"
#include "Field/RoomLoading.h"
#include "Field/Traps.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_8007F434 (src/BATTLE/BATTLE.PRG/146C.c:10124)
int Field_RollRiskLockBreak(void)
{
    int risk_derived_percent = ((Battle_characterState->stats->risk + 150) * 100 / 256);
    if (risk_derived_percent > 100) {
        risk_derived_percent = 100;
    }
    return Core_Random_NextBounded(100) < (risk_derived_percent + 20);
}

// decomp: BATTLE.PRG _damageFromTrapPanel (src/BATTLE/BATTLE.PRG/146C.c:11730)
short Field_CalculateTrapPanelDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* hit, int nHit,
    int withRandMod)
{
    Battle_DamageCalculation damage_calculation;
    short damage;
    int randMod;
    int dpMod;
    int defense;
    int trap_attack_value;
    Battle_ActorStats* actor = Battle_actors[hit->target.targetActor]->stats;
    int limb = hit->target.targetLimb;

    damage_calculation.attackStat = actor->maxHp / 10;
    damage_calculation.attackStat *= action->hits[nHit].statFactor;
    damage_calculation.attackStat += (Field_RoomTrapLevel() + 1) * 10;
    damage_calculation.type = action->hits[nHit].damageType;
    damage_calculation.sourceAffinity = action->hits[nHit].affinity - 1;
    damage_calculation.mediatingStat = actor->strength;
    damage_calculation.armorStat = actor->limbs[limb].armor.currentStr;
    damage_calculation.shieldStat = 0;
    damage_calculation.armorStatFactor = 100;
    damage_calculation.shieldStatFactor = 100;
    damage_calculation.statFactor = 100;

    if (damage_calculation.type != 0) {
        damage_calculation.statFactor = actor->limbs[limb].types[damage_calculation.type] + 100;
        damage_calculation.armorStatFactor = actor->limbs[limb].armor.types[damage_calculation.type] + 100;
    }

    if (damage_calculation.sourceAffinity) {
        damage_calculation.statFactor += actor->limbs[limb].affinities[damage_calculation.sourceAffinity];
        damage_calculation.armorStatFactor +=
            actor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][damage_calculation.sourceAffinity];
    }

    if (actor->limbs[limb].armor.maxDp != 0) {

        dpMod =
            ((actor->limbs[limb].armor.maxDp - actor->limbs[limb].armor.currentDp) * 100)
            / actor->limbs[limb].armor.maxDp;

        if (dpMod < 0) {
            dpMod += 3;
        }

        damage_calculation.armorStatFactor -= dpMod >> 2;
    }

    if (Battle_actors[actor->actorId]->weaponDrawn & 1) {

        damage_calculation.shieldStat = actor->shield.currentStr;

        if (damage_calculation.type != 0) {
            damage_calculation.shieldStatFactor += actor->shield.types[damage_calculation.type];
        }

        if (damage_calculation.sourceAffinity != 0) {
            damage_calculation.shieldStatFactor +=
                actor->shield.classAffinityCurrent.affinities[0][damage_calculation.sourceAffinity];
        }

        if (actor->shield.maxDp != 0) {

            dpMod = ((actor->shield.maxDp - actor->shield.currentDp) * 100)
                  / actor->shield.maxDp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            damage_calculation.shieldStatFactor -= dpMod >> 2;
        }

        if (actor->shield.maxPp != 0) {

            dpMod = (actor->shield.currentPp * 100) / actor->shield.maxPp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            damage_calculation.shieldStatFactor += dpMod >> 2;
        }
    }

    damage_calculation.mediatingStat += actor->accessory.currentStr;
    damage_calculation.statFactor += actor->accessory.types[damage_calculation.type]
                     + actor->accessory.affinities[damage_calculation.sourceAffinity];
    defense = (damage_calculation.mediatingStat - 20) * damage_calculation.statFactor;
    defense += damage_calculation.shieldStat * damage_calculation.shieldStatFactor;
    defense += damage_calculation.armorStat * damage_calculation.armorStatFactor;
    defense /= (actor->risk / 2) + 100;
    trap_attack_value = damage_calculation.attackStat;
    damage = trap_attack_value - defense;

    if (damage < 0) {
        damage = 0;
    }

    if (withRandMod != 0) {
        randMod = Core_Random_NextAveraged(11) - 5;
    } else {
        randMod = 0;
    }

    damage += randMod;

    if (damage < 0) {
        damage = 0;
    }

    if (damage >= actor->currentHp) {
        damage = actor->currentHp - 1;
    }

    return damage;
}

// decomp: BATTLE.PRG _hpHealedFromHealPanel (src/BATTLE/BATTLE.PRG/146C.c:11992)
short Field_CalculateHealingPanelRecovery(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* hit, int nHit,
    int withRandMod)
{
    Battle_DamageCalculation recovery_calculation;
    short hpHealed;
    int randMod;
    int dpMod;
    int defense;
    int panel_attack_value;

    Battle_ActorStats* actor = Battle_actors[hit->target.targetActor]->stats;
    int limb = hit->target.targetLimb;

    recovery_calculation.attackStat = actor->maxHp / 10;
    recovery_calculation.attackStat *= action->hits[nHit].statFactor;
    recovery_calculation.attackStat += (Field_RoomTrapLevel() + 1) * 10;
    recovery_calculation.type = action->hits[nHit].damageType;
    recovery_calculation.sourceAffinity = action->hits[nHit].affinity - 1;
    recovery_calculation.mediatingStat = actor->strength;
    recovery_calculation.armorStat = actor->limbs[limb].armor.currentStr;
    recovery_calculation.shieldStat = 0;
    recovery_calculation.armorStatFactor = 100;
    recovery_calculation.shieldStatFactor = 100;
    recovery_calculation.statFactor = 100;

    if (recovery_calculation.type != 0) {
        recovery_calculation.statFactor = actor->limbs[limb].types[recovery_calculation.type] + 100;
        recovery_calculation.armorStatFactor = actor->limbs[limb].armor.types[recovery_calculation.type] + 100;
    }

    if (recovery_calculation.sourceAffinity) {
        recovery_calculation.statFactor += actor->limbs[limb].affinities[recovery_calculation.sourceAffinity];
        recovery_calculation.armorStatFactor +=
            actor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][recovery_calculation.sourceAffinity];
    }

    if (actor->limbs[limb].armor.maxDp != 0) {

        dpMod =
            ((actor->limbs[limb].armor.maxDp - actor->limbs[limb].armor.currentDp) * 100)
            / actor->limbs[limb].armor.maxDp;

        if (dpMod < 0) {
            dpMod += 3;
        }

        recovery_calculation.armorStatFactor -= dpMod >> 2;
    }

    if (Battle_actors[actor->actorId]->weaponDrawn & 1) {

        recovery_calculation.shieldStat = actor->shield.currentStr;

        if (recovery_calculation.type != 0) {
            recovery_calculation.shieldStatFactor += actor->shield.types[recovery_calculation.type];
        }

        if (recovery_calculation.sourceAffinity != 0) {
            recovery_calculation.shieldStatFactor +=
                actor->shield.classAffinityCurrent.affinities[0][recovery_calculation.sourceAffinity];
        }

        if (actor->shield.maxDp != 0) {

            dpMod = ((actor->shield.maxDp - actor->shield.currentDp) * 100)
                  / actor->shield.maxDp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            recovery_calculation.shieldStatFactor -= dpMod >> 2;
        }

        if (actor->shield.maxPp != 0) {

            dpMod = (actor->shield.currentPp * 100) / actor->shield.maxPp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            recovery_calculation.shieldStatFactor += dpMod >> 2;
        }
    }

    recovery_calculation.mediatingStat += actor->accessory.currentStr;
    recovery_calculation.statFactor += actor->accessory.types[recovery_calculation.type]
                     + actor->accessory.affinities[recovery_calculation.sourceAffinity];
    defense = (recovery_calculation.mediatingStat - 20) * recovery_calculation.statFactor;
    defense += recovery_calculation.shieldStat * recovery_calculation.shieldStatFactor;
    defense += recovery_calculation.armorStat * recovery_calculation.armorStatFactor;
    defense /= (actor->risk / 2) + 100;
    panel_attack_value = recovery_calculation.attackStat;
    hpHealed = panel_attack_value - defense;

    if (hpHealed < 0) {
        hpHealed = 0;
    }

    if (withRandMod != 0) {
        randMod = Core_Random_NextAveraged(11) - 5;
    } else {
        randMod = 0;
    }

    hpHealed += randMod;

    if (hpHealed < 0) {
        hpHealed = 0;
    }

    return hpHealed;
}

}  // extern "C"
