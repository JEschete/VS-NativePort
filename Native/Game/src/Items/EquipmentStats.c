// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/StatusEffects.h"
#include "Core/Callbacks.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Items/Equipment.h"
#include "Items/EquipmentInit.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG vs_battle_applyWeaponStats (src/BATTLE/BATTLE.PRG/146C.c:968)
void Items_BuildWeaponStatsFromParts(
    Items_WeaponLoadout* target, Items_WeaponSpawnLoadout* source)
{
    int i;

    Core_Memory_CopyAlignedWords(&target->blade, &source->blade, sizeof target->blade);
    Core_Memory_CopyAlignedWords(&target->grip, &source->grip, sizeof target->grip);

    for (i = 0; i < 3; ++i) {
        Core_Memory_CopyAlignedWords(&target->gems[i], &source->gems[i], sizeof source->gems[i]);
    }

    target->wepId = source->blade.weaponModelId;
    target->dropRate = source->dropRate;
    target->index = source->index;
    target->blade.material = source->material;
    target->damageType = source->blade.damageType;
    target->actionType = source->blade.costType;
    target->damageTypeValue = source->grip.types[target->damageType];
    target->risk = source->blade.cost;
    target->recoveryDelay = source->blade.opaque14;
    target->range = source->blade.range;
    target->currentDp = source->blade.currentDp;
    target->maxDp = source->blade.maxDp;

    target->currentPp = source->blade.currentPp;
    target->maxPp = source->blade.maxPp;
    target->currentStr = target->baseStr =
        source->blade.strength + source->grip.strength + source->gems[0].strength
        + source->gems[1].strength + source->gems[2].strength;
    target->currentInt = target->baseInt =
        source->blade.intelligence + source->grip.intelligence
        + source->gems[0].intelligence + source->gems[1].intelligence
        + source->gems[2].intelligence;
    target->currentAgility = target->baseAgility =
        source->blade.agility + source->grip.agility + source->gems[0].agility
        + source->gems[1].agility + source->gems[2].agility;

    for (i = 0; i < 6; ++i) {
        target->classAffinityBaseline.classes[i] =
            target->classAffinityCurrent.classes[0][i] =
                target->classAffinityCurrent.classes[1][i] =
                    source->blade.classes[i] + source->gems[0].classes[i]
                    + source->gems[1].classes[i] + source->gems[2].classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->classAffinityBaseline.affinities[i] =
            target->classAffinityCurrent.affinities[0][i] =
                target->classAffinityCurrent.affinities[1][i] =
                    source->blade.affinities[i] + source->gems[0].affinities[i]
                    + source->gems[1].affinities[i] + source->gems[2].affinities[i];
        target->affinityModifiers[i] = 0;
    }

    for (i = 0; i < 24; ++i) {
        target->name[i] = source->name[i];
    }
}

// decomp: BATTLE.PRG vs_battle_applyShieldStats (src/BATTLE/BATTLE.PRG/146C.c:1028)
void Items_BuildShieldStatsFromParts(
    Items_ShieldLoadout* target, Items_ShieldSpawnLoadout* source)
{
    int i;

    Core_Memory_CopyAlignedWords(&target->base, &source->base, sizeof source->base);

    for (i = 0; i < 3; ++i) {
        Core_Memory_CopyAlignedWords(&target->gems[i], &source->gems[i], sizeof target->gems[i]);
    }

    target->wepId = source->base.weaponModelId;
    target->dropRate = source->dropRate;
    target->index = source->index;
    target->base.material = source->material;

    for (i = 0; i < 4; ++i) {
        target->types[i] = source->base.types[i];
    }

    target->currentDp = source->base.currentDp;
    target->maxDp = source->base.maxDp;
    target->currentPp = source->base.currentPp;
    target->maxPp = source->base.maxPp;
    target->currentStr = target->baseStr =
        source->base.strength + source->gems[0].strength + source->gems[1].strength
        + source->gems[2].strength;
    target->currentInt = target->baseInt =
        source->base.intelligence + source->gems[0].intelligence
        + source->gems[1].intelligence + source->gems[2].intelligence;
    target->currentAgility = target->baseAgility =
        source->base.agility + source->gems[0].agility + source->gems[1].agility
        + source->gems[2].agility;

    for (i = 0; i < 6; ++i) {
        target->classAffinityBaseline.classes[i] =
            target->classAffinityCurrent.classes[0][i] =
                target->classAffinityCurrent.classes[1][i] =
                    source->base.classes[i] + source->gems[0].classes[i]
                    + source->gems[1].classes[i] + source->gems[2].classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->classAffinityBaseline.affinities[i] =
            target->classAffinityCurrent.affinities[0][i] =
                target->classAffinityCurrent.affinities[1][i] =
                    source->base.affinities[i] + source->gems[0].affinities[i]
                    + source->gems[1].affinities[i] + source->gems[2].affinities[i];
        target->affinityModifiers[i] = 0;
    }
}

// decomp: BATTLE.PRG _applyAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:1080)
void Items_BuildAccessoryStatsFromPart(Items_AccessorySpawnLoadout* target, Items_AccessoryInit* source)
{
    int i;

    Core_Memory_CopyAlignedWords(target, &source->equipment, sizeof target->accessory);

    target->currentStr = source->equipment.strength;
    target->currentInt = source->equipment.intelligence;
    target->currentAgility = source->equipment.agility;
    target->dropRate = source->dropRate;
    target->index = source->index;

    for (i = 0; i < 4; ++i) {
        target->types[i] = source->equipment.types[i];
    }

    for (i = 0; i < 6; ++i) {
        target->classes[i] = source->equipment.classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->affinities[i] = source->equipment.affinities[i];
    }
}

// decomp: BATTLE.PRG _applyArmorStats (src/BATTLE/BATTLE.PRG/146C.c:1105)
void Items_BuildArmorStatsFromPart(Items_ArmorLoadout* target, Items_ArmorInit* source)
{
    int i;

    Core_Memory_CopyAlignedWords(&target->armor, &source->equipment, sizeof target->armor);

    target->armor.material = source->material;
    target->currentDp = source->equipment.currentDp;
    target->maxDp = source->equipment.maxDp;
    target->currentStr = target->baseStr = source->equipment.strength;
    target->currentInt = target->baseInt = source->equipment.intelligence;
    target->currentAgility = target->baseAgility = source->equipment.agility;
    target->index = source->index;

    for (i = 0; i < 4; ++i) {
        target->types[i] = source->equipment.types[i];
    }

    for (i = 0; i < 6; ++i) {
        signed char v = source->equipment.classes[i];
        target->classAffinityCurrent.classes[1][i] = v;
        target->classAffinityCurrent.classes[0][i] = v;
        target->classAffinityBaseline.classes[i] = v;
    }

    for (i = 0; i < 7; ++i) {
        signed char v = source->equipment.affinities[i];
        target->affinityModifiers[i] = 0;
        target->classAffinityCurrent.affinities[1][i] = v;
        target->classAffinityCurrent.affinities[0][i] = v;
        target->classAffinityBaseline.affinities[i] = v;
    }

    target->dropRate = source->dropRate;
    target->opaque99 = source->opaque32;
}

// decomp: BATTLE.PRG vs_battle_copyInventoryBladeStats (src/BATTLE/BATTLE.PRG/146C.c:1142)
void Items_ReadInventoryBladeStats(
    Items_EquipmentPartStats* target, Items_InventoryBlade* source)
{
    int i;
    Items_InventoryBlade* tempBlade;

    target->id = source->id;
    target->subId = source->subId;
    target->weaponModelId = source->wepId;
    target->category = source->category;
    target->currentDp = source->currentDp;
    target->maxDp = source->maxDp;
    target->currentPp = source->currentPp;
    target->maxPp = source->maxPp;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;
    target->cost = source->cost;
    target->damageType = source->damageType;
    target->costType = source->costType;
    target->opaque14 = source->opaque12;
    target->range = source->range;
    target->index = source->index;

    tempBlade = source;

    for (i = 0; i < 6; ++i) {
        target->classes[i] = tempBlade->classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->affinities[i] = tempBlade->affinities[i];
    }

    target->material = source->material;
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGripStats (src/BATTLE/BATTLE.PRG/146C.c:1179)
void Items_ReadInventoryGripStats(
    Items_EquipmentPartStats* target, Items_InventoryGrip* source)
{
    int i;

    target->id = source->id;
    target->subId = source->subId;
    target->category = source->category;
    target->gemSlots = source->gemSlots;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;
    target->index = source->index;

    for (i = 0; i < 4; ++i) {
        target->types[i] = source->types[i];
    }
}

// decomp: BATTLE.PRG vs_battle_copyInventoryArmorStats (src/BATTLE/BATTLE.PRG/146C.c:1198)
void Items_ReadInventoryArmorStats(
    Items_EquipmentPartStats* target, Items_InventoryArmor* source)
{
    int i;
    Items_InventoryArmor* tempArmor;

    target->id = source->id;
    target->subId = source->subId;
    target->weaponModelId = source->wepId;
    target->category = source->category;
    target->currentDp = source->currentDp;
    target->maxDp = source->maxDp;
    target->currentPp = source->currentPp;
    target->maxPp = source->maxPp;
    target->gemSlots = source->gemSlots;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;
    target->index = source->index;

    tempArmor = source;

    for (i = 0; i < 4; ++i) {
        target->types[i] = tempArmor->types[i];
    }

    for (i = 0; i < 6; ++i) {
        target->classes[i] = tempArmor->classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->affinities[i] = tempArmor->affinities[i];
    }

    target->material = source->material;
}

// decomp: BATTLE.PRG vs_battle_copyInventoryGemStats (src/BATTLE/BATTLE.PRG/146C.c:1235)
void Items_ReadInventoryGemStats(
    Items_EquipmentPartStats* target, Items_InventoryGem* source)
{
    int i;

    target->id = source->id;
    target->subId = source->subId;
    target->gemEffects = source->gemEffects;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;
    target->index = source->index;

    for (i = 0; i < 6; ++i) {
        target->classes[i] = source->classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->affinities[i] = source->affinities[i];
    }
}

// decomp: BATTLE.PRG vs_battle_applyWeapon (src/BATTLE/BATTLE.PRG/146C.c:1257)
void Items_LoadInventoryWeaponStats(Items_WeaponLoadout* target, Items_InventoryWeapon* source)
{
    int i;

    Items_WeaponSpawnLoadout* tempWeapon = Core_Memory_AllocateFromHighEnd(sizeof *tempWeapon);
    Core_Memory_ZeroAlignedWords(tempWeapon, sizeof *tempWeapon);

    if (source != NULL) {

        tempWeapon->index = source->index;

        if (source->blade != 0) {
            Items_ReadInventoryBladeStats(
                &tempWeapon->blade, &Audio_Seq_unusedInventory.blades[source->blade - 1]);
            tempWeapon->material = Audio_Seq_unusedInventory.blades[source->blade - 1].material;
        }

        if (source->grip != 0) {
            Items_ReadInventoryGripStats(
                &tempWeapon->grip, &Audio_Seq_unusedInventory.grips[source->grip - 1]);
        }

        for (i = 0; i < 3; ++i) {
            if (source->gems[i] != 0) {
                Items_ReadInventoryGemStats(
                    &tempWeapon->gems[i], &Audio_Seq_unusedInventory.gems[source->gems[i] - 1]);
            }
        }

        for (i = 0; i < 24; ++i) {
            tempWeapon->name[i] = source->name[i];
        }
    }

    Items_BuildWeaponStatsFromParts(target, tempWeapon);
    Core_Memory_FreeFromHighEnd(tempWeapon);
}

// decomp: BATTLE.PRG vs_battle_applyShield (src/BATTLE/BATTLE.PRG/146C.c:1295)
void Items_LoadInventoryShieldStats(Items_ShieldLoadout* target, Items_InventoryShield* source)
{
    Items_ShieldSpawnLoadout* tempShield = Core_Memory_AllocateFromHighEnd(sizeof *tempShield);
    Core_Memory_ZeroAlignedWords(tempShield, sizeof *tempShield);

    if (source != NULL) {
        int i;

        tempShield->index = source->index;

        Items_ReadInventoryArmorStats(&tempShield->base, &source->base);

        tempShield->material = source->base.material;

        for (i = 0; i < 3; ++i) {
            if (source->gems[i] != 0) {
                Items_ReadInventoryGemStats(&tempShield->gems[i],
                    &Audio_Seq_unusedInventory.gems[(source->gems[i] & 0x7F) - 1]);
            }
        }
    }

    Items_BuildShieldStatsFromParts(target, tempShield);
    Core_Memory_FreeFromHighEnd(tempShield);
}

// decomp: BATTLE.PRG vs_battle_applyArmor (src/BATTLE/BATTLE.PRG/146C.c:1321)
void Items_LoadInventoryArmorStats(Items_ArmorLoadout* target, Items_InventoryArmor* source)
{
    Items_ArmorInit* tempArmor = Core_Memory_AllocateFromHighEnd(sizeof *tempArmor);
    Core_Memory_ZeroAlignedWords(tempArmor, sizeof *tempArmor);

    if (source != NULL) {
        tempArmor->index = source->index;

        Items_ReadInventoryArmorStats(&tempArmor->equipment, source);

        tempArmor->material = source->material;
    }

    Items_BuildArmorStatsFromPart(target, tempArmor);
    Core_Memory_FreeFromHighEnd(tempArmor);
}

// decomp: BATTLE.PRG vs_battle_applyAccessory (src/BATTLE/BATTLE.PRG/146C.c:1338)
void Items_LoadInventoryAccessoryStats(
    Items_AccessorySpawnLoadout* target, Items_InventoryArmor* source)
{
    Items_AccessoryInit* tempAccessory = Core_Memory_AllocateFromHighEnd(sizeof *tempAccessory);
    Core_Memory_ZeroAlignedWords(tempAccessory, sizeof *tempAccessory);

    if (source != NULL) {
        tempAccessory->index = source->index;

        Items_ReadInventoryArmorStats(&tempAccessory->equipment, source);
    }

    Items_BuildAccessoryStatsFromPart(target, tempAccessory);
    Core_Memory_FreeFromHighEnd(tempAccessory);
}

// decomp: BATTLE.PRG func_8006B214 (src/BATTLE/BATTLE.PRG/146C.c:1356)
void Items_RemoveEquipmentBoostOnEquip(void)
{
    Battle_ActorStats* ashley_stats = Battle_characterState->stats;

    if (ashley_stats->statuses & 0x100000) {
        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(ashley_stats->actorId, 0, 0x100000, 1);
        Battle_RemoveActorStatuses(0x100000, ashley_stats);
    }
}

// decomp: BATTLE.PRG func_8006B270 (src/BATTLE/BATTLE.PRG/146C.c:1366)
void Items_RemoveWeaponEnchantmentOnEquip(void)
{
    Battle_ActorStats* ashley_stats = Battle_characterState->stats;

    if (ashley_stats->statuses & 0x01E00000) {
        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(ashley_stats->actorId, 0, ashley_stats->statuses & 0x01E00000, 1);
        Battle_RemoveActorStatuses(ashley_stats->statuses & 0x01E00000, ashley_stats);
    }
}

// decomp: BATTLE.PRG func_8006B2D4 (src/BATTLE/BATTLE.PRG/146C.c:1376)
void Items_RemoveArmorEnchantmentOnEquip(void)
{
    Battle_ActorStats* ashley_stats = Battle_characterState->stats;

    if (ashley_stats->statuses & 0x1E000000) {
        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(ashley_stats->actorId, 0, ashley_stats->statuses & 0x1E000000, 1);
        Battle_RemoveActorStatuses(ashley_stats->statuses & 0x1E000000, ashley_stats);
    }
}

// decomp: BATTLE.PRG vs_battle_equipWeapon (src/BATTLE/BATTLE.PRG/146C.c:1386)
void Items_EquipAshleyWeapon(Items_InventoryWeapon* weapon)
{
    Battle_ActorStats* actor;

    Items_LoadInventoryWeaponStats(&Battle_characterState->stats->weapon, weapon);
    Core_IgnoreCallback(Battle_characterState->stats);

    if (weapon == NULL) {
        Battle_characterState->equippedWeaponCategory = 10;
    } else {
        Battle_characterState->equippedWeaponCategory =
            Audio_Seq_unusedInventory.blades[weapon->blade - 1].category;
    }

    actor = Battle_characterState->stats;
    actor->currentRange = actor->weapon.range.range + actor->reach;
    actor->currentRangeY = actor->weapon.range.secondRange + actor->reach;
    actor->currentRangeZ = actor->weapon.range.thirdRange + actor->reach;
    actor->currentAttackShapeAngle = actor->weapon.range.attackShapeAngle;
    actor->currentAttackShape = 1;

    Items_RemoveEquipmentBoostOnEquip();
    Items_RemoveWeaponEnchantmentOnEquip();
}

// decomp: BATTLE.PRG vs_battle_equipShield (src/BATTLE/BATTLE.PRG/146C.c:1411)
void Items_EquipAshleyShield(Items_InventoryShield* shield)
{
    Items_LoadInventoryShieldStats(&Battle_characterState->stats->shield, shield);
    Core_IgnoreCallback(Battle_characterState->stats);
    Items_RemoveEquipmentBoostOnEquip();
    Items_RemoveArmorEnchantmentOnEquip();
}

// decomp: BATTLE.PRG vs_battle_equipArmor (src/BATTLE/BATTLE.PRG/146C.c:1419)
void Items_EquipAshleyArmor(int bodyPart, Items_InventoryArmor* armor)
{
    Items_LoadInventoryArmorStats(&Battle_characterState->stats->limbs[bodyPart].armor, armor);
    Core_IgnoreCallback(Battle_characterState->stats);
    Items_RemoveEquipmentBoostOnEquip();
    Items_RemoveArmorEnchantmentOnEquip();
}

// decomp: BATTLE.PRG vs_battle_equipAccessory (src/BATTLE/BATTLE.PRG/146C.c:1427)
void Items_EquipAshleyAccessory(Items_InventoryArmor* accessory)
{
    Items_LoadInventoryAccessoryStats(&Battle_characterState->stats->accessory, accessory);
    Core_IgnoreCallback(Battle_characterState->stats);
    Items_RemoveEquipmentBoostOnEquip();
    Items_RemoveArmorEnchantmentOnEquip();
}

// decomp: BATTLE.PRG vs_battle_copyUiBladeStats (src/BATTLE/BATTLE.PRG/146C.c:1435)
void Items_WriteInventoryBladeStats(
    Items_InventoryBlade* target, Items_EquipmentPartStats* source)
{
    int i;
    Items_InventoryBlade* tempBlade;

    target->id = source->id;
    target->subId = source->subId;
    target->wepId = source->weaponModelId;
    target->category = source->category;
    target->currentDp = source->currentDp;
    target->maxDp = source->maxDp;
    target->currentPp = source->currentPp;
    target->maxPp = source->maxPp;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;
    target->cost = source->cost;
    target->damageType = source->damageType & 3;
    target->costType = source->costType & 7;
    target->opaque12 = source->opaque14;
    target->range = source->range;

    tempBlade = target;

    for (i = 0; i < 6; ++i) {
        tempBlade->classes[i] = source->classes[i];
    }

    for (i = 0; i < 7; ++i) {
        tempBlade->affinities[i] = source->affinities[i];
    }

    target->material = source->material;
}

// decomp: BATTLE.PRG vs_battle_copyUiGripStats (src/BATTLE/BATTLE.PRG/146C.c:1471)
void Items_WriteInventoryGripStats(
    Items_InventoryGrip* target, Items_EquipmentPartStats* source)
{
    int i;

    target->id = source->id;
    target->subId = source->subId;
    target->category = source->category;
    target->gemSlots = source->gemSlots;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;

    for (i = 0; i < 4; ++i) {
        target->types[i] = source->types[i];
    }
}

// decomp: BATTLE.PRG vs_battle_copyUiGemStats (src/BATTLE/BATTLE.PRG/146C.c:1489)
void Items_WriteInventoryGemStats(Items_InventoryGem* target, Items_EquipmentPartStats* source)
{
    int i;

    target->id = source->id;
    target->subId = source->subId;
    target->gemEffects = source->gemEffects;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;

    for (i = 0; i < 6; ++i) {
        target->classes[i] = source->classes[i];
    }

    for (i = 0; i < 7; ++i) {
        target->affinities[i] = source->affinities[i];
    }
}

// decomp: BATTLE.PRG vs_battle_copyUiEquipmentStats (src/BATTLE/BATTLE.PRG/146C.c:1509)
void Items_WriteInventoryArmorStats(
    Items_InventoryArmor* target, Items_EquipmentPartStats* source)
{
    int i;
    Items_InventoryArmor* tempArmor;

    target->id = source->id;
    target->subId = source->subId;
    target->wepId = source->weaponModelId;
    target->category = source->category;
    target->currentDp = source->currentDp;
    target->maxDp = source->maxDp;
    target->currentPp = source->currentPp;
    target->maxPp = source->maxPp;
    target->gemSlots = source->gemSlots;
    target->strength = source->strength;
    target->intelligence = source->intelligence;
    target->agility = source->agility;

    tempArmor = target;

    for (i = 0; i < 4; ++i) {
        tempArmor->types[i] = source->types[i];
    }

    for (i = 0; i < 6; ++i) {
        tempArmor->classes[i] = source->classes[i];
    }

    for (i = 0; i < 7; ++i) {
        tempArmor->affinities[i] = source->affinities[i];
    }

    target->material = source->material;
}

// decomp: BATTLE.PRG vs_battle_copyUiWeaponStats (src/BATTLE/BATTLE.PRG/146C.c:1545)
void Items_StoreInventoryWeaponStats(
    Items_InventoryWeapon* target, Items_WeaponLoadout* source)
{
    if (source->blade.id != 0) {
        int i;

        source->blade.currentDp = source->currentDp;
        source->blade.currentPp = source->currentPp;

        Items_WriteInventoryBladeStats(
            &Audio_Seq_unusedInventory.blades[target->blade - 1], &source->blade);
        Items_WriteInventoryGripStats(
            &Audio_Seq_unusedInventory.grips[target->grip - 1], &source->grip);

        for (i = 0; i < 3; ++i) {
            if (source->gems[i].id != 0) {
                Items_WriteInventoryGemStats(
                    &Audio_Seq_unusedInventory.gems[target->gems[i] - 1], &source->gems[i]);
            }
        }

        for (i = 0; i < 24; ++i) {
            target->name[i] = source->name[i];
        }
    }
}

// decomp: BATTLE.PRG vs_battle_copyUiShieldStats (src/BATTLE/BATTLE.PRG/146C.c:1572)
void Items_StoreInventoryShieldStats(
    Items_InventoryShield* target, Items_ShieldLoadout* source)
{
    if (source->base.id != 0) {
        int i;

        source->base.currentDp = source->currentDp;
        source->base.currentPp = source->currentPp;

        Items_WriteInventoryArmorStats(&target->base, &source->base);

        for (i = 0; i < 3; ++i) {
            if (source->gems[i].id != 0) {
                Items_WriteInventoryGemStats(
                    &Audio_Seq_unusedInventory.gems[target->gems[i] - 1], &source->gems[i]);
            }
        }
    }
}

// decomp: BATTLE.PRG vs_battle_copyUiArmorStats (src/BATTLE/BATTLE.PRG/146C.c:1592)
void Items_StoreInventoryArmorStats(Items_InventoryArmor* target, Items_ArmorLoadout* source)
{
    if (source->armor.id != 0) {
        source->armor.currentDp = source->currentDp;
        Items_WriteInventoryArmorStats(target, &source->armor);
    }
}

// decomp: BATTLE.PRG vs_battle_copyUiAccessoryStats (src/BATTLE/BATTLE.PRG/146C.c:1600)
void Items_StoreInventoryAccessoryStats(
    Items_InventoryArmor* target, Items_AccessorySpawnLoadout* source)
{
    if (source->accessory.id != 0) {
        Items_WriteInventoryArmorStats(target, &source->accessory);
    }
}

