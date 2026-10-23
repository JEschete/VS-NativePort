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
void Items_RebuildArmorClassAffinity(Battle_ActorStats* arg0, int arg1)
{
    int i;

    for (i = 0; i < 6; ++i) {
        u_short v = arg0->limbs[arg1].armor.armor.classes[i];
        arg0->limbs[arg1].armor.classAffinityCurrent.classes[1][i] = v;
        arg0->limbs[arg1].armor.classAffinityCurrent.classes[0][i] = v;
    }

    for (i = 0; i < 7; ++i) {
        u_short v = arg0->limbs[arg1].armor.armor.affinities[i];
        arg0->limbs[arg1].armor.classAffinityCurrent.affinities[1][i] = v;
        arg0->limbs[arg1].armor.classAffinityCurrent.affinities[0][i] =
            v + arg0->limbs[arg1].armor.affinityModifiers[i];
    }
}

// decomp: BATTLE.PRG _calculateShieldClassAffinity (src/BATTLE/BATTLE.PRG/146C.c:9600)
void Items_RebuildShieldClassAffinity(Battle_ActorStats* arg0)
{
    short temp_v0_2;
    int i;

    for (i = 0; i < 6; ++i) {
        temp_v0_2 = arg0->shield.base.classes[i] + arg0->shield.gems[0].classes[i]
                  + arg0->shield.gems[1].classes[i] + arg0->shield.gems[2].classes[i];
        arg0->shield.classAffinityCurrent.classes[1][i] = temp_v0_2;
        arg0->shield.classAffinityCurrent.classes[0][i] = temp_v0_2;
    }

    for (i = 0; i < 7; ++i) {
        temp_v0_2 = arg0->shield.base.affinities[i] + arg0->shield.gems[0].affinities[i]
                  + arg0->shield.gems[1].affinities[i]
                  + arg0->shield.gems[2].affinities[i];
        arg0->shield.classAffinityCurrent.affinities[1][i] = temp_v0_2;
        arg0->shield.classAffinityCurrent.affinities[0][i] =
            temp_v0_2 + arg0->shield.affinityModifiers[i];
    }
}

// decomp: BATTLE.PRG _calculateWeaponClassAffinity (src/BATTLE/BATTLE.PRG/146C.c:9622)
void Items_RebuildWeaponClassAffinity(Battle_ActorStats* arg0)
{
    short temp_v0_2;
    int i;

    for (i = 0; i < 6; ++i) {
        temp_v0_2 = arg0->weapon.blade.classes[i] + arg0->weapon.gems[0].classes[i]
                  + arg0->weapon.gems[1].classes[i] + arg0->weapon.gems[2].classes[i];
        arg0->weapon.classAffinityCurrent.classes[1][i] = temp_v0_2;
        arg0->weapon.classAffinityCurrent.classes[0][i] = temp_v0_2;
    }

    for (i = 0; i < 7; ++i) {
        temp_v0_2 = arg0->weapon.blade.affinities[i] + arg0->weapon.gems[0].affinities[i]
                  + arg0->weapon.gems[1].affinities[i]
                  + arg0->weapon.gems[2].affinities[i];
        arg0->weapon.classAffinityCurrent.affinities[1][i] = temp_v0_2;
        arg0->weapon.classAffinityCurrent.affinities[0][i] =
            temp_v0_2 + arg0->weapon.affinityModifiers[i];
    }
}

// decomp: BATTLE.PRG _triggerArmorAffinityChange (src/BATTLE/BATTLE.PRG/146C.c:10773)
void Items_GrowOpposingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int arg3, int actorId)
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
            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, targetAffinity + 6, 1);
        }

        if (targetAffinity == 0) {
            for (i = 1; i < 7; ++i) {
                if (equipment->affinities[i] > -100) {

                    int randVal = Core_Random_NextBounded(256);

                    if ((((equipment->affinities[i] + 100) / 4) + 20) >= randVal) {

                        --equipment->affinities[i];

                        if (actorId == 0) {
                            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, i + 6, 0x80000001);
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
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, physical + 6, 0x80000001);
                }
            }
        }

        if (equipment->affinities[opposingAffinity] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if ((((equipment->affinities[opposingAffinity] + 100) / 4) + 0x14)
                >= randVal) {

                --equipment->affinities[opposingAffinity];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, opposingAffinity + 6, 0x80000001);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG _triggerWeaponAffinityChange (src/BATTLE/BATTLE.PRG/146C.c:10892)
void Items_GrowMatchingEquipmentAffinity(Battle_ActionDefinition* action, Items_EquipmentPartStats* equipment,
    int affinity, int arg3, int suppressNotification)
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
        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, targetAffinity + 6, 1);
    }

    if (targetAffinity == 0) {
        for (i = 1; i < 7; ++i) {
            if (equipment->affinities[i] > -100) {

                int randVal = Core_Random_NextBounded(256);

                if ((((equipment->affinities[i] + 100) / 4) + 20) >= randVal) {

                    --equipment->affinities[i];

                    if (suppressNotification == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, i + 6, 0x80000001);
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
                ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, physical + 6, 0x80000001);
            }
        }
    }

    if (equipment->affinities[opposingAffinity] > -100) {

        int randVal = Core_Random_NextBounded(256);

        if ((((equipment->affinities[opposingAffinity] + 100) / 4) + 20) >= randVal) {

            --equipment->affinities[opposingAffinity];

            if (suppressNotification == 0) {
                ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, opposingAffinity + 6, 0x80000001);
            }
        }
    }
}

// decomp: BATTLE.PRG _triggerClassChange (src/BATTLE/BATTLE.PRG/146C.c:11011)
void Items_GrowEquipmentClass(Battle_ActionDefinition* action __attribute__((unused)),
    Items_EquipmentPartStats* equipment, int class, int arg3, int actorId)
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
            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, targetClass, 1);
        }

        if (equipment->classes[opposingClass0] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if (((equipment->classes[opposingClass0] + 100) / 4 + 20) >= randVal) {

                --equipment->classes[opposingClass0];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, opposingClass0, 0x80000001);
                }
            }
        }

        if (equipment->classes[opposingClass1] > -100) {

            int randVal = Core_Random_NextBounded(256);

            if (((equipment->classes[opposingClass1] + 100) / 4 + 20) >= randVal) {

                --equipment->classes[opposingClass1];

                if (actorId == 0) {
                    ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg3, opposingClass1, 0x80000001);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_80080C9C (src/BATTLE/BATTLE.PRG/146C.c:11090)
void Items_GrowDefenderEquipmentRatings(
    Battle_ActionDefinition* arg0, Battle_ActorStats* arg1, int arg2, int arg3, int arg4)
{
    int sp10[8];
    int temp_s2;
    int i;
    Items_EquipmentPartStats* equipment;
    int var_a0 = 0;

    switch (arg0->category) {
    case 1:
    case 2:
    case 3:
    case 4:
        if ((Battle_actors[arg1->actorId]->weaponDrawn & 1)
            && (arg1->shield.base.id != 0)) {
            sp10[var_a0++] = 0xFF;
        }

        for (i = 0; i < 6; ++i) {
            if ((arg1->limbs[i].maxHp != 0) && (arg1->limbs[i].armor.armor.id != 0)) {
                sp10[var_a0++] = i;
            }
        }

        break;

    case 5:
    case 7:
    case 14:
        if (Battle_actors[arg1->actorId]->weaponDrawn & 1) {
            if (arg1->shield.base.id != 0) {
                sp10[var_a0++] = 0xFF;
            }
        }

    case 6:
    case 8:
    case 11:
    case 12:
        if (arg1->limbs[arg4].maxHp && (arg1->limbs[arg4].armor.armor.id != 0)) {
            sp10[var_a0++] = arg4;
        }

        break;
    }

    if (var_a0 == 0) {
        return;
    }

    temp_s2 = sp10[Core_Random_NextBounded(var_a0)];

    if (temp_s2 == 0xFF) {
        Items_EquipmentPartStats* equipment = &arg1->shield.base;

        if (arg1->shield.base.id != 0) {

            if (arg2 != 0) {
                Items_GrowEquipmentClass(arg0, equipment, arg2 - 1, 0xF1, arg1->actorId);
            }
            if (arg3 != 0) {
                Items_GrowMatchingEquipmentAffinity(
                    arg0, equipment, arg3 - 1, 0xF1, arg1->actorId);
            }
        }
        Items_RebuildShieldClassAffinity(arg1);
    } else {

        equipment = &arg1->limbs[temp_s2].armor.armor;

        if (equipment->id != 0) {
            if (arg2 != 0) {
                do {
                    Items_GrowEquipmentClass(arg0, equipment, arg2 - 1,
                        arg1->limbs[temp_s2].nameIndex, arg1->actorId);
                } while (0);
            }
            if (arg3 != 0) {
                Items_GrowMatchingEquipmentAffinity(arg0, equipment, arg3 - 1,
                    arg1->limbs[temp_s2].nameIndex, arg1->actorId);
            }
        }

        Items_RebuildArmorClassAffinity(arg1, temp_s2);
    }
}

// decomp: BATTLE.PRG func_80080F78 (src/BATTLE/BATTLE.PRG/146C.c:11178)
void Items_GrowAttackerWeaponRatings(Battle_ActionDefinition* action, Battle_ActorStats* arg1, int arg2, int arg3)
{
    if (action->type != Battle_ActionType_Spell) {
        Items_EquipmentPartStats* temp_s1 = &arg1->weapon.blade;
        if (arg1->weapon.blade.id != 0) {
            if (arg2 != 0) {
                Items_GrowEquipmentClass(action, temp_s1, arg2 - 1, 0xF0, arg1->actorId);
            }
            if (arg3 != 0) {
                Items_GrowOpposingEquipmentAffinity(
                    action, temp_s1, arg3 - 1, 0xF0, arg1->actorId);
            }
        }
        Items_RebuildWeaponClassAffinity(arg1);
    }
}

