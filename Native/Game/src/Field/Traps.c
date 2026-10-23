// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
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
    int var_s0 = ((Battle_characterState->stats->risk + 150) * 100 / 256);
    if (var_s0 > 100) {
        var_s0 = 100;
    }
    return Core_Random_NextBounded(100) < (var_s0 + 20);
}

// decomp: BATTLE.PRG _damageFromTrapPanel (src/BATTLE/BATTLE.PRG/146C.c:11730)
short Field_CalculateTrapPanelDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* hit, int nHit,
    int withRandMod)
{
    Battle_DamageCalculation sp10;
    short damage;
    int randMod;
    int dpMod;
    int defense;
    int new_var2;
    Battle_ActorStats* actor = Battle_actors[hit->target.targetActor]->stats;
    int limb = hit->target.targetLimb;

    sp10.attackStat = actor->maxHp / 10;
    sp10.attackStat *= action->hits[nHit].statFactor;
    sp10.attackStat += (Field_RoomTrapLevel() + 1) * 10;
    sp10.type = action->hits[nHit].damageType;
    sp10.sourceAffinity = action->hits[nHit].affinity - 1;
    sp10.mediatingStat = actor->strength;
    sp10.armorStat = actor->limbs[limb].armor.currentStr;
    sp10.shieldStat = 0;
    sp10.armorStatFactor = 100;
    sp10.shieldStatFactor = 100;
    sp10.statFactor = 100;

    if (sp10.type != 0) {
        sp10.statFactor = actor->limbs[limb].types[sp10.type] + 100;
        sp10.armorStatFactor = actor->limbs[limb].armor.types[sp10.type] + 100;
    }

    if (sp10.sourceAffinity) {
        sp10.statFactor += actor->limbs[limb].affinities[sp10.sourceAffinity];
        sp10.armorStatFactor +=
            actor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][sp10.sourceAffinity];
    }

    if (actor->limbs[limb].armor.maxDp != 0) {

        dpMod =
            ((actor->limbs[limb].armor.maxDp - actor->limbs[limb].armor.currentDp) * 100)
            / actor->limbs[limb].armor.maxDp;

        if (dpMod < 0) {
            dpMod += 3;
        }

        sp10.armorStatFactor -= dpMod >> 2;
    }

    if (Battle_actors[actor->actorId]->weaponDrawn & 1) {

        sp10.shieldStat = actor->shield.currentStr;

        if (sp10.type != 0) {
            sp10.shieldStatFactor += actor->shield.types[sp10.type];
        }

        if (sp10.sourceAffinity != 0) {
            sp10.shieldStatFactor +=
                actor->shield.classAffinityCurrent.affinities[0][sp10.sourceAffinity];
        }

        if (actor->shield.maxDp != 0) {

            dpMod = ((actor->shield.maxDp - actor->shield.currentDp) * 100)
                  / actor->shield.maxDp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            sp10.shieldStatFactor -= dpMod >> 2;
        }

        if (actor->shield.maxPp != 0) {

            dpMod = (actor->shield.currentPp * 100) / actor->shield.maxPp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            sp10.shieldStatFactor += dpMod >> 2;
        }
    }

    sp10.mediatingStat += actor->accessory.currentStr;
    sp10.statFactor += actor->accessory.types[sp10.type]
                     + actor->accessory.affinities[sp10.sourceAffinity];
    defense = (sp10.mediatingStat - 20) * sp10.statFactor;
    defense += sp10.shieldStat * sp10.shieldStatFactor;
    defense += sp10.armorStat * sp10.armorStatFactor;
    defense /= (actor->risk / 2) + 100;
    new_var2 = sp10.attackStat;
    damage = new_var2 - defense;

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
    Battle_DamageCalculation sp10;
    short hpHealed;
    int randMod;
    int dpMod;
    int defense;
    int new_var2;

    Battle_ActorStats* actor = Battle_actors[hit->target.targetActor]->stats;
    int limb = hit->target.targetLimb;

    sp10.attackStat = actor->maxHp / 10;
    sp10.attackStat *= action->hits[nHit].statFactor;
    sp10.attackStat += (Field_RoomTrapLevel() + 1) * 10;
    sp10.type = action->hits[nHit].damageType;
    sp10.sourceAffinity = action->hits[nHit].affinity - 1;
    sp10.mediatingStat = actor->strength;
    sp10.armorStat = actor->limbs[limb].armor.currentStr;
    sp10.shieldStat = 0;
    sp10.armorStatFactor = 100;
    sp10.shieldStatFactor = 100;
    sp10.statFactor = 100;

    if (sp10.type != 0) {
        sp10.statFactor = actor->limbs[limb].types[sp10.type] + 100;
        sp10.armorStatFactor = actor->limbs[limb].armor.types[sp10.type] + 100;
    }

    if (sp10.sourceAffinity) {
        sp10.statFactor += actor->limbs[limb].affinities[sp10.sourceAffinity];
        sp10.armorStatFactor +=
            actor->limbs[limb]
                .armor.classAffinityCurrent.affinities[0][sp10.sourceAffinity];
    }

    if (actor->limbs[limb].armor.maxDp != 0) {

        dpMod =
            ((actor->limbs[limb].armor.maxDp - actor->limbs[limb].armor.currentDp) * 100)
            / actor->limbs[limb].armor.maxDp;

        if (dpMod < 0) {
            dpMod += 3;
        }

        sp10.armorStatFactor -= dpMod >> 2;
    }

    if (Battle_actors[actor->actorId]->weaponDrawn & 1) {

        sp10.shieldStat = actor->shield.currentStr;

        if (sp10.type != 0) {
            sp10.shieldStatFactor += actor->shield.types[sp10.type];
        }

        if (sp10.sourceAffinity != 0) {
            sp10.shieldStatFactor +=
                actor->shield.classAffinityCurrent.affinities[0][sp10.sourceAffinity];
        }

        if (actor->shield.maxDp != 0) {

            dpMod = ((actor->shield.maxDp - actor->shield.currentDp) * 100)
                  / actor->shield.maxDp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            sp10.shieldStatFactor -= dpMod >> 2;
        }

        if (actor->shield.maxPp != 0) {

            dpMod = (actor->shield.currentPp * 100) / actor->shield.maxPp;

            if (dpMod < 0) {
                dpMod += 3;
            }

            sp10.shieldStatFactor += dpMod >> 2;
        }
    }

    sp10.mediatingStat += actor->accessory.currentStr;
    sp10.statFactor += actor->accessory.types[sp10.type]
                     + actor->accessory.affinities[sp10.sourceAffinity];
    defense = (sp10.mediatingStat - 20) * sp10.statFactor;
    defense += sp10.shieldStat * sp10.shieldStatFactor;
    defense += sp10.armorStat * sp10.armorStatFactor;
    defense /= (actor->risk / 2) + 100;
    new_var2 = sp10.attackStat;
    hpHealed = new_var2 - defense;

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

