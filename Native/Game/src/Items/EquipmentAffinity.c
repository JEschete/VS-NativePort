// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Core/Random.h"
#include "Items/Equipment.h"
#include "Items/EquipmentAffinity.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG _calculateArmorClassAffinity (src/BATTLE/BATTLE.PRG/146C.c:9582)
void Items_RebuildArmorClassAffinity(Battle_ActorStats* actor_stats, int limb_index)
{
    int i;

    for (i = 0; i < 6; ++i) {
        u_short v = actor_stats->limbs[limb_index].armor.armor.classes[i];
        actor_stats->limbs[limb_index].armor.classAffinityCurrent.classes[1][i] = v;
        actor_stats->limbs[limb_index].armor.classAffinityCurrent.classes[0][i] = v;
    }

    for (i = 0; i < 7; ++i) {
        u_short v = actor_stats->limbs[limb_index].armor.armor.affinities[i];
        actor_stats->limbs[limb_index].armor.classAffinityCurrent.affinities[1][i] = v;
        actor_stats->limbs[limb_index].armor.classAffinityCurrent.affinities[0][i] =
            v + actor_stats->limbs[limb_index].armor.affinityModifiers[i];
    }
}

// decomp: BATTLE.PRG _calculateShieldClassAffinity (src/BATTLE/BATTLE.PRG/146C.c:9600)
void Items_RebuildShieldClassAffinity(Battle_ActorStats* actor_stats)
{
    short combined_rating;
    int i;

    for (i = 0; i < 6; ++i) {
        combined_rating = actor_stats->shield.base.classes[i] + actor_stats->shield.gems[0].classes[i]
                  + actor_stats->shield.gems[1].classes[i] + actor_stats->shield.gems[2].classes[i];
        actor_stats->shield.classAffinityCurrent.classes[1][i] = combined_rating;
        actor_stats->shield.classAffinityCurrent.classes[0][i] = combined_rating;
    }

    for (i = 0; i < 7; ++i) {
        combined_rating = actor_stats->shield.base.affinities[i] + actor_stats->shield.gems[0].affinities[i]
                  + actor_stats->shield.gems[1].affinities[i]
                  + actor_stats->shield.gems[2].affinities[i];
        actor_stats->shield.classAffinityCurrent.affinities[1][i] = combined_rating;
        actor_stats->shield.classAffinityCurrent.affinities[0][i] =
            combined_rating + actor_stats->shield.affinityModifiers[i];
    }
}

// decomp: BATTLE.PRG _calculateWeaponClassAffinity (src/BATTLE/BATTLE.PRG/146C.c:9622)
void Items_RebuildWeaponClassAffinity(Battle_ActorStats* actor_stats)
{
    short combined_rating;
    int i;

    for (i = 0; i < 6; ++i) {
        combined_rating = actor_stats->weapon.blade.classes[i] + actor_stats->weapon.gems[0].classes[i]
                  + actor_stats->weapon.gems[1].classes[i] + actor_stats->weapon.gems[2].classes[i];
        actor_stats->weapon.classAffinityCurrent.classes[1][i] = combined_rating;
        actor_stats->weapon.classAffinityCurrent.classes[0][i] = combined_rating;
    }

    for (i = 0; i < 7; ++i) {
        combined_rating = actor_stats->weapon.blade.affinities[i] + actor_stats->weapon.gems[0].affinities[i]
                  + actor_stats->weapon.gems[1].affinities[i]
                  + actor_stats->weapon.gems[2].affinities[i];
        actor_stats->weapon.classAffinityCurrent.affinities[1][i] = combined_rating;
        actor_stats->weapon.classAffinityCurrent.affinities[0][i] =
            combined_rating + actor_stats->weapon.affinityModifiers[i];
    }
}

// decomp: BATTLE.PRG _triggerArmorAffinityChange (src/BATTLE/BATTLE.PRG/146C.c:10773)
void Items_GrowOpposingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int equipment_label_id, int actorId)
{
    int targetAffinity;
    int i;
    int physical;
    int opposingAffinity;
    int randVal;

    if (action->hits[0].affinity != 0) {
        affinity = action->hits[0].affinity - 1;
    }

    switch (affinity) {
    case 0:
        targetAffinity = 0;
        break;

    case 1:
        targetAffinity = 3;
        physical = 0;
        opposingAffinity = 1;
        break;

    case 2:
        targetAffinity = 4;
        physical = 0;
        opposingAffinity = 2;
        break;

    case 3:
        targetAffinity = 1;
        physical = 0;
        opposingAffinity = 3;
        break;

    case 4:
        targetAffinity = 2;
        physical = 0;
        opposingAffinity = 4;
        break;

    case 5:
        targetAffinity = 6;
        physical = 0;
        opposingAffinity = 5;
        break;

    case 6:
        targetAffinity = 5;
        physical = 0;
        opposingAffinity = 6;
        break;
    }

    if (equipment->affinities[targetAffinity] >= 100) {
        return;
    }

    randVal = Core_Random_NextBounded(256);

    if ((70 - ((equipment->affinities[targetAffinity] + 100) / 4)) >= randVal) {

        ++equipment->affinities[targetAffinity];

        if (actorId == 0) {
            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, targetAffinity + 6, 1);
        }

        if (targetAffinity == 0) {
            for (i = 1; i < 7; ++i) {
                if (equipment->affinities[i] > -100) {

                    int randVal = Core_Random_NextBounded(256);

                    if ((((equipment->affinities[i] + 100) / 4) + 20) >= randVal) {

                        --equipment->affinities[i];

                        if (actorId == 0) {
                            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, i + 6, 0x80000001);
                        }
                    }
                }
            }
            return;
        }

        if (equipment->affinities[physical] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if ((((equipment->affinities[physical] + 100) / 4) + 20) >= randVal) {

                --equipment->affinities[physical];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, physical + 6, 0x80000001);
                }
            }
        }

        if (equipment->affinities[opposingAffinity] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if ((((equipment->affinities[opposingAffinity] + 100) / 4) + 0x14)
                >= randVal) {

                --equipment->affinities[opposingAffinity];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, opposingAffinity + 6, 0x80000001);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG _triggerWeaponAffinityChange (src/BATTLE/BATTLE.PRG/146C.c:10892)
void Items_GrowMatchingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int equipment_label_id, int suppressNotification)
{
    int targetAffinity;
    int i;
    int physical;
    int opposingAffinity;
    int randVal;

    if (action->hits[0].affinity != 0) {
        affinity = action->hits[0].affinity - 1;
    }

    switch (affinity) {
    case 0:
        targetAffinity = 0;
        break;

    case 1:
        targetAffinity = 1;
        physical = 0;
        opposingAffinity = 3;
        break;

    case 2:
        targetAffinity = 2;
        physical = 0;
        opposingAffinity = 4;
        break;

    case 3:
        targetAffinity = 3;
        physical = 0;
        opposingAffinity = 1;
        break;

    case 4:
        targetAffinity = 4;
        physical = 0;
        opposingAffinity = 2;
        break;

    case 5:
        targetAffinity = 5;
        physical = 0;
        opposingAffinity = 6;
        break;

    case 6:
        targetAffinity = 6;
        physical = 0;
        opposingAffinity = 5;
        break;
    }

    if (equipment->affinities[targetAffinity] >= 100) {
        return;
    }

    randVal = Core_Random_NextBounded(256);

    if ((70 - ((equipment->affinities[targetAffinity] + 100) / 4)) < randVal) {
        return;
    }

    ++equipment->affinities[targetAffinity];

    if (suppressNotification == 0) {
        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, targetAffinity + 6, 1);
    }

    if (targetAffinity == 0) {
        for (i = 1; i < 7; ++i) {
            if (equipment->affinities[i] > -100) {

                int randVal = Core_Random_NextBounded(256);

                if ((((equipment->affinities[i] + 100) / 4) + 20) >= randVal) {

                    --equipment->affinities[i];

                    if (suppressNotification == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, i + 6, 0x80000001);
                    }
                }
            }
        }
        return;
    }

    if (equipment->affinities[physical] > -100) {

        int randVal = Core_Random_NextBounded(256);

        if ((((equipment->affinities[physical] + 100) / 4) + 20) >= randVal) {

            --equipment->affinities[physical];

            if (suppressNotification == 0) {
                ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, physical + 6, 0x80000001);
            }
        }
    }

    if (equipment->affinities[opposingAffinity] > -100) {

        int randVal = Core_Random_NextBounded(256);

        if ((((equipment->affinities[opposingAffinity] + 100) / 4) + 20) >= randVal) {

            --equipment->affinities[opposingAffinity];

            if (suppressNotification == 0) {
                ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, opposingAffinity + 6, 0x80000001);
            }
        }
    }
}

// decomp: BATTLE.PRG _triggerClassChange (src/BATTLE/BATTLE.PRG/146C.c:11011)
void Items_GrowEquipmentClass(Battle_ActionDefinition* action __attribute__((unused)),
    Items_EquipmentPartStats* equipment, int class, int equipment_label_id, int actorId)
{
    int opposingClass0;
    int opposingClass1;
    int randVal;
    int targetClass = class;

    switch (class) {
    case 0:
        opposingClass0 = 1;
        opposingClass1 = 2;
        break;
    case 1:
        opposingClass0 = 2;
        opposingClass1 = 3;
        break;
    case 2:
        opposingClass0 = 3;
        opposingClass1 = 4;
        break;
    case 3:
        opposingClass0 = 4;
        opposingClass1 = 5;
        break;
    case 4:
        opposingClass0 = 5;
        opposingClass1 = 0;
        break;
    case 5:
        opposingClass0 = 0;
        opposingClass1 = 1;
        break;
    }

    if (equipment->classes[targetClass] >= 100) {
        return;
    }

    randVal = Core_Random_NextBounded(256);

    if ((70 - (equipment->classes[targetClass] + 100) / 4) >= randVal) {

        ++equipment->classes[targetClass];

        if (actorId == 0) {
            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, targetClass, 1);
        }

        if (equipment->classes[opposingClass0] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if (((equipment->classes[opposingClass0] + 100) / 4 + 20) >= randVal) {

                --equipment->classes[opposingClass0];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, opposingClass0, 0x80000001);
                }
            }
        }

        if (equipment->classes[opposingClass1] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if (((equipment->classes[opposingClass1] + 100) / 4 + 20) >= randVal) {

                --equipment->classes[opposingClass1];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(equipment_label_id, opposingClass1, 0x80000001);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_80080C9C (src/BATTLE/BATTLE.PRG/146C.c:11090)
void Items_GrowDefenderEquipmentRatings(
    Battle_ActionDefinition* action, Battle_ActorStats* defender_stats, int attacker_class_id, int attacker_affinity_id, int hit_limb_index)
{
    int eligible_equipment_slots[8];
    int selected_equipment_slot;
    int i;
    Items_EquipmentPartStats* equipment;
    int eligible_equipment_count = 0;

    switch (action->category) {
    case 1:
    case 2:
    case 3:
    case 4:
        if ((Battle_actors[defender_stats->actorId]->weaponDrawn & 1)
            && (defender_stats->shield.base.id != 0)) {
            eligible_equipment_slots[eligible_equipment_count++] = 0xFF;
        }

        for (i = 0; i < 6; ++i) {
            if ((defender_stats->limbs[i].maxHp != 0) && (defender_stats->limbs[i].armor.armor.id != 0)) {
                eligible_equipment_slots[eligible_equipment_count++] = i;
            }
        }

        break;

    case 5:
    case 7:
    case 14:
        if (Battle_actors[defender_stats->actorId]->weaponDrawn & 1) {
            if (defender_stats->shield.base.id != 0) {
                eligible_equipment_slots[eligible_equipment_count++] = 0xFF;
            }
        }

    case 6:
    case 8:
    case 11:
    case 12:
        if (defender_stats->limbs[hit_limb_index].maxHp && (defender_stats->limbs[hit_limb_index].armor.armor.id != 0)) {
            eligible_equipment_slots[eligible_equipment_count++] = hit_limb_index;
        }

        break;
    }

    if (eligible_equipment_count == 0) {
        return;
    }

    selected_equipment_slot = eligible_equipment_slots[Core_Random_NextBounded(eligible_equipment_count)];

    if (selected_equipment_slot == 0xFF) {
        Items_EquipmentPartStats* equipment = &defender_stats->shield.base;

        if (defender_stats->shield.base.id != 0) {

            if (attacker_class_id != 0) {
                Items_GrowEquipmentClass(action, equipment, attacker_class_id - 1, 0xF1, defender_stats->actorId);
            }
            if (attacker_affinity_id != 0) {
                Items_GrowMatchingEquipmentAffinity(
                    action, equipment, attacker_affinity_id - 1, 0xF1, defender_stats->actorId);
            }
        }
        Items_RebuildShieldClassAffinity(defender_stats);
    } else {

        equipment = &defender_stats->limbs[selected_equipment_slot].armor.armor;

        if (equipment->id != 0) {
            if (attacker_class_id != 0) {
                do {
                    Items_GrowEquipmentClass(action, equipment, attacker_class_id - 1,
                        defender_stats->limbs[selected_equipment_slot].nameIndex, defender_stats->actorId);
                } while (0);
            }
            if (attacker_affinity_id != 0) {
                Items_GrowMatchingEquipmentAffinity(action, equipment, attacker_affinity_id - 1,
                    defender_stats->limbs[selected_equipment_slot].nameIndex, defender_stats->actorId);
            }
        }

        Items_RebuildArmorClassAffinity(defender_stats, selected_equipment_slot);
    }
}

// decomp: BATTLE.PRG func_80080F78 (src/BATTLE/BATTLE.PRG/146C.c:11178)
void Items_GrowAttackerWeaponRatings(Battle_ActionDefinition* action, Battle_ActorStats* attacker_stats, int defender_class_id, int defender_affinity_id)
{
    if (action->type != Battle_ActionType_Spell) {
        Items_EquipmentPartStats* weapon_blade = &attacker_stats->weapon.blade;
        if (attacker_stats->weapon.blade.id != 0) {
            if (defender_class_id != 0) {
                Items_GrowEquipmentClass(action, weapon_blade, defender_class_id - 1, 0xF0, attacker_stats->actorId);
            }
            if (defender_affinity_id != 0) {
                Items_GrowOpposingEquipmentAffinity(
                    action, weapon_blade, defender_affinity_id - 1, 0xF0, attacker_stats->actorId);
            }
        }
        Items_RebuildWeaponClassAffinity(attacker_stats);
    }
}

