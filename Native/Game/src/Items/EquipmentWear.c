// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
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
short Items_ApplyPpGemMultiplier(int arg0, short base_pp_amount, char* gem_scan_bytes)
{
    int i;
    int pp_percent = 100;
    int has_pp_gem = 0;

    for (i = 0; i < 3; ++i) {
        if (gem_scan_bytes[i * 0x30 + 0xCA] == 9) {
            has_pp_gem = 1;
        }
    }

    if (has_pp_gem != 0) {
        pp_percent += 5;
    }
    return (base_pp_amount * pp_percent) / 100;
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
    Battle_ActionDefinition* action, Battle_ActorStats* source_stats, Battle_ActorStats* target_stats, int dp_amount)
{
    int has_pp_gem;
    int pp_percent_or_gain;
    int i;

    pp_percent_or_gain = 100;

    if (action->type != Battle_ActionType_Spell) {
        has_pp_gem = 0;

        for (i = 0; i < 3; ++i) {
            if (source_stats->weapon.gems[i].gemEffects == 9) {
                has_pp_gem = 1;
            }
        }

        if (has_pp_gem != 0) {
            pp_percent_or_gain += 5;
        }

        pp_percent_or_gain = (((dp_amount / 10) * pp_percent_or_gain) / 100);
        if (source_stats->weapon.currentPp + pp_percent_or_gain < source_stats->weapon.maxPp) {
            source_stats->weapon.currentPp = source_stats->weapon.currentPp + pp_percent_or_gain;
        } else {
            source_stats->weapon.currentPp = source_stats->weapon.maxPp;
        }
    }

    pp_percent_or_gain = 100;

    if (Battle_actors[target_stats->actorId]->weaponDrawn & 1) {
        has_pp_gem = 0;
        for (i = 0; i < 3; ++i) {
            if (target_stats->shield.gems[i].gemEffects == 9) {
                has_pp_gem = 1;
            }
        }
        if (has_pp_gem != 0) {
            pp_percent_or_gain -= 5;
        }

        pp_percent_or_gain = (((dp_amount / 10) * pp_percent_or_gain) / 100);
        if (target_stats->shield.currentPp + pp_percent_or_gain < target_stats->shield.maxPp != 0) {
            target_stats->shield.currentPp = target_stats->shield.currentPp + pp_percent_or_gain;
        } else {
            target_stats->shield.currentPp = target_stats->shield.maxPp;
        }
    }
}

