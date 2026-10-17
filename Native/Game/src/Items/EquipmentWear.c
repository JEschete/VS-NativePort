// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Items/Equipment.h"
#include "Items/EquipmentWear.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_8007FF8C (src/BATTLE/BATTLE.PRG/146C.c:10594)
short Items_ApplyPpGemMultiplier(int arg0, short arg1, char* arg2)
{
    int i;
    int var_t0 = 100;
    int var_a3 = 0;

    for (i = 0; i < 3; ++i) {
        if (arg2[i * 0x30 + 0xCA] == 9) {
            var_a3 = 1;
        }
    }

    if (var_a3 != 0) {
        var_t0 += 5;
    }
    return (arg1 * var_t0) / 100;
}

// decomp: BATTLE.PRG _adjustDpPp (src/BATTLE/BATTLE.PRG/146C.c:10689)
void Items_ConsumeHitEquipmentDp(
    Battle_ActionDefinition* action, Battle_ActorStats* source, Battle_ActorStats* target, int amount)
{
    int i;

    if (action->type != Battle_ActionType_Spell) {
        if (source->weapon.currentDp >= amount) {
            source->weapon.currentDp -= amount;
        } else {
            source->weapon.currentDp = 0;
        }
    }

    if (Battle_actors[target->actorId]->weaponDrawn & 1) {
        if (target->shield.currentDp >= amount) {
            target->shield.currentDp -= amount;
        } else {
            target->shield.currentDp = 0;
        }
    }

    for (i = 0; i < 6; ++i) {
        if (Battle_limbArmorDpLoss[i] != 0) {
            if (target->limbs[i].armor.currentDp >= Battle_limbArmorDpLoss[i]) {
                target->limbs[i].armor.currentDp -= Battle_limbArmorDpLoss[i];
            } else {
                target->limbs[i].armor.currentDp = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_800803A4 (src/BATTLE/BATTLE.PRG/146C.c:10721)
void Items_AccumulateHitEquipmentPp(
    Battle_ActionDefinition* action, Battle_ActorStats* arg1, Battle_ActorStats* arg2, int arg3)
{
    int var_t0;
    int var_t1;
    int i;

    var_t1 = 100;

    if (action->type != Battle_ActionType_Spell) {
        var_t0 = 0;

        for (i = 0; i < 3; ++i) {
            if (arg1->weapon.gems[i].gemEffects == 9) {
                var_t0 = 1;
            }
        }

        if (var_t0 != 0) {
            var_t1 += 5;
        }

        var_t1 = (((arg3 / 10) * var_t1) / 100);
        if (arg1->weapon.currentPp + var_t1 < arg1->weapon.maxPp) {
            arg1->weapon.currentPp = arg1->weapon.currentPp + var_t1;
        } else {
            arg1->weapon.currentPp = arg1->weapon.maxPp;
        }
    }

    var_t1 = 100;

    if (Battle_actors[arg2->actorId]->weaponDrawn & 1) {
        var_t0 = 0;
        for (i = 0; i < 3; ++i) {
            if (arg2->shield.gems[i].gemEffects == 9) {
                var_t0 = 1;
            }
        }
        if (var_t0 != 0) {
            var_t1 -= 5;
        }

        var_t1 = (((arg3 / 10) * var_t1) / 100);
        if (arg2->shield.currentPp + var_t1 < arg2->shield.maxPp != 0) {
            arg2->shield.currentPp = arg2->shield.currentPp + var_t1;
        } else {
            arg2->shield.currentPp = arg2->shield.maxPp;
        }
    }
}

