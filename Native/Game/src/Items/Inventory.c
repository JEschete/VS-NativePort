// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Actor.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/WordCopy.h"
#include "Items/Categories.h"
#include "Items/Equipment.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Inventory/ItemList.h"
#include "Render/Palette.h"

// decomp: vs_battle_miscItemInvoked (vs_battle_miscItemInvoked@BATTLE/BATTLE.PRG)
u_char Items_miscItemInvoked;

// decomp: vs_mainMenu_inventoryItemCapacities (vs_mainMenu_inventoryItemCapacities@MENU/MAINMENU.PRG)
u_char Items_Inventory_inventoryItemCapacities[] = { 8, 16, 16, 8, 16, 48, 64, 2 };

// decomp: BATTLE.PRG vs_battle_itemIdIsInInventory (src/BATTLE/BATTLE.PRG/146C.c:9646)
int Items_InventoryItemCount(int id)
{
    int ret;
    int i;

    if (id != 0) {
        for (i = 0; i < 8; ++i) {
            if (id < Items_itemCategoryUpperIds[i]) {
                break;
            }
        }

        ret = 0;

        if (i < 8) {
            switch (i) {
            case 0:
                for (i = 0; i < 16; ++i) {
                    if (Audio_Seq_unusedInventory.blades[i].id == id) {
                        ret = 1;
                        break;
                    }
                }
                break;
            case 1:
                for (i = 0; i < 16; ++i) {
                    if (Audio_Seq_unusedInventory.grips[i].id == id) {
                        ret = 1;
                        break;
                    }
                }
                break;
            case 2:
                for (i = 0; i < 16; ++i) {
                    if (Audio_Seq_unusedInventory.armor[i].id == id) {
                        ret = 1;
                        break;
                    }
                }
                break;
            case 4:
                for (i = 0; i < 48; ++i) {
                    if (Audio_Seq_unusedInventory.gems[i].id == id) {
                        ret = 1;
                        break;
                    }
                }
                break;
            case 5:
            case 6:
            case 7:
                for (i = 0; i < 64; ++i) {
                    if ((Audio_Seq_unusedInventory.misc[i].id == id)
                        && (Audio_Seq_unusedInventory.misc[i].count != 0)) {
                        ret = Audio_Seq_unusedInventory.misc[i].count;
                        break;
                    }
                }
                break;
            }
        }
        return ret;
    }
    return 1;
}

// decomp: BATTLE.PRG vs_battle_decreaseMiscCount (src/BATTLE/BATTLE.PRG/5BF94.c:4454)
int Items_ConsumeMiscItem(int miscId)
{
    int i;

    if (miscId < 0x143) {
        Items_miscItemInvoked = 0;
    }

    for (i = 0; i < 64; ++i) {
        int miscIndex = (Items_miscItemInvoked + i) & 0x3F;
        if (Audio_Seq_unusedInventory.misc[miscIndex].id == miscId) {
            int count = Audio_Seq_unusedInventory.misc[miscIndex].count - 1;
            Audio_Seq_unusedInventory.misc[miscIndex].count = count;
            if (count == 0) {
                Audio_Seq_unusedInventory.misc[miscIndex].id = 0;
            }
            return 1;
        }
    }
    return 0;
}

// decomp: MAINMENU.PRG vs_mainMenu_unequipWeapon (src/MENU/MAINMENU.PRG/2D10.c:1008)
void Items_Inventory_UnequipWeapon(void)
{
    int i;

    Items_EquipAshleyWeapon(NULL);

    for (i = 0; i < 8; ++i) {
        Audio_Seq_unusedInventory.weapons[i].isEquipped = 0;
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_unequipShield (src/MENU/MAINMENU.PRG/2D10.c:1019)
void Items_Inventory_UnequipShield(void)
{
    int i;

    Items_EquipAshleyShield(NULL);

    for (i = 0; i < 8; ++i) {
        Audio_Seq_unusedInventory.shields[i].isEquipped = 0;
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_initItem (src/MENU/MAINMENU.PRG/2D10.c:1030)
void Items_Inventory_ClearItemSlot(int itemCategory, int index)
{
    int i;

    switch (itemCategory) {
    case Items_ItemCategory_Weapon: {
        Items_InventoryWeapon* weapon = &Audio_Seq_unusedInventory.weapons[index - 1];

        Items_Inventory_ClearItemSlot(Items_ItemCategory_Blade, weapon->blade);
        Items_Inventory_ClearItemSlot(Items_ItemCategory_Grip, weapon->grip);

        for (i = 0; i < 3; ++i) {
            int gem = weapon->gems[i];

            if (gem != 0) {
                Items_Inventory_ClearItemSlot(Items_ItemCategory_Gem, gem);
            }
        }

        Core_Memory_ZeroWordsBackward(weapon, sizeof *weapon);

        weapon->index = index;
        break;
    }

    case Items_ItemCategory_Blade: {
        Items_InventoryBlade* blade = &Audio_Seq_unusedInventory.blades[index - 1];

        Core_Memory_ZeroWordsBackward(blade, sizeof *blade);

        blade->index = index;
        break;
    }

    case Items_ItemCategory_Grip: {
        Items_InventoryGrip* grip = &Audio_Seq_unusedInventory.grips[index - 1];

        Core_Memory_ZeroWordsBackward(grip, sizeof *grip);

        grip->index = index;
        break;
    }

    case Items_ItemCategory_Shield: {
        Items_InventoryShield* shield = &Audio_Seq_unusedInventory.shields[index - 1];

        for (i = 0; i < 3; ++i) {
            int gem = shield->gems[i];

            if (gem != 0) {
                Items_Inventory_ClearItemSlot(Items_ItemCategory_Gem, gem);
            }
        }

        Core_Memory_ZeroWordsBackward(shield, sizeof *shield);

        shield->index = index;
        break;
    }

    case Items_ItemCategory_Armor: {
        Items_InventoryArmor* armor = &Audio_Seq_unusedInventory.armor[index - 1];

        Core_Memory_ZeroWordsBackward(armor, sizeof *armor);

        armor->index = index;
        break;
    }

    case Items_ItemCategory_Gem: {
        Items_InventoryGem* gem = &Audio_Seq_unusedInventory.gems[index - 1];

        Core_Memory_ZeroWordsBackward(gem, sizeof *gem);

        gem->index = index;
        break;
    }

    case Items_ItemCategory_Misc: {
        Items_InventoryMisc* misc = &Audio_Seq_unusedInventory.misc[index - 1];

        Core_Memory_ZeroWordsBackward(misc, sizeof *misc);

        misc->index = index;
        break;
    }
    }
}

// decomp: MAINMENU.PRG _getItemId (src/MENU/MAINMENU.PRG/2D10.c:1181)
int Items_Inventory_ItemOccupancyId(int category, int index, Items_Inventory* inventory)
{
    int id = 0;

    if (inventory == NULL) {
        inventory = &Audio_Seq_unusedInventory;
    }

    switch (category) {
    case 0:
        id = inventory->weapons[index].blade;
        break;

    case 1:
        id = inventory->blades[index].id;
        break;

    case 2:
        id = inventory->grips[index].id;
        break;

    case 3:
        id = inventory->shields[index].base.id;
        break;

    case 4:
        id = inventory->armor[index].id;
        break;

    case 5:
        id = inventory->gems[index].id;
        break;

    case 6:
        id = inventory->misc[index].id;
        break;
    }

    return id;
}

// decomp: MAINMENU.PRG vs_mainMenu_findItem (src/MENU/MAINMENU.PRG/2D10.c:1222)
int Items_Inventory_FindDisplayPosition(int category, int id)
{
    int i;

    u_char* index = Menu_Inventory_inventoryIndices[category];
    u_char capacity = Items_Inventory_inventoryItemCapacities[category];

    for (i = 0; i < capacity; ++i) {
        if (index[i] == (id + 1)) {
            return i + 1;
        }
    }

    return 0;
}

// decomp: MAINMENU.PRG _setItemIndex (src/MENU/MAINMENU.PRG/2D10.c:1241)
void Items_Inventory_AppendDisplayIndex(int itemCategory, int id)
{
    int i;
    u_char* index = Menu_Inventory_inventoryIndices[itemCategory];

    for (i = 0; i < Items_Inventory_inventoryItemCapacities[itemCategory]; ++i) {
        if (index[i] == 0) {
            index[i] = id + 1;
            return;
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_rebuildInventory (src/MENU/MAINMENU.PRG/2D10.c:1254)
void Items_Inventory_RebuildDisplayIndices(int itemCategory)
{
    int i;
    u_char* indices = Menu_Inventory_inventoryIndices[itemCategory];
    int capacity = Items_Inventory_inventoryItemCapacities[itemCategory];

    for (i = 0; i < capacity; ++i) {
        int index = indices[i];
        if ((index != 0) && (Items_Inventory_ItemOccupancyId(itemCategory, index - 1, NULL) == 0)) {
            indices[i] = 0;
        }
    }

    i = 0;

    while (1) {
        if (indices[i] != 0) {
            i += 1;
            if (i == (capacity - 1)) {
                break;
            }
        } else {
            int j;
            int k;

            for (j = i + 1; j < capacity; ++j) {
                if (indices[j] != 0) {
                    break;
                }
            }

            if (j == capacity) {
                break;
            }

            for (k = j; k < capacity; ++k) {
                indices[k + i - j] = indices[k];
            }

            for (k = k + (i - j); k < capacity; ++k) {
                indices[k] = 0;
            }
        }
    }

    for (i = 0; i < capacity; ++i) {
        if ((Items_Inventory_ItemOccupancyId(itemCategory, i, NULL) != 0)
            && (Items_Inventory_FindDisplayPosition(itemCategory, i) == 0)) {
            Items_Inventory_AppendDisplayIndex(itemCategory, i);
        }
    }
}

// decomp: MAINMENU.PRG vs_mainMenu_getItemCount (src/MENU/MAINMENU.PRG/2D10.c:1307)
int Items_Inventory_CountItems(int itemCategory, Items_Inventory* inventory)
{
    int i;
    int count = 0;

    for (i = 0; i < Items_Inventory_inventoryItemCapacities[itemCategory]; ++i) {
        if (Items_Inventory_ItemOccupancyId(itemCategory, i, inventory) != 0) {
            ++count;
        }
    }

    return count;
}

// decomp: MAINMENU.PRG vs_mainMenu_getFirstEmptyItemSlot (src/MENU/MAINMENU.PRG/2D10.c:1321)
int Items_Inventory_FirstEmptySlot(int itemCategory, Items_Inventory* inventory)
{
    int i;

    for (i = 0; i < Items_Inventory_inventoryItemCapacities[itemCategory]; ++i) {
        if (Items_Inventory_ItemOccupancyId(itemCategory, i, inventory) == 0) {
            break;
        }
    }

    return i;
}

// decomp: MAINMENU.PRG vs_mainMenu_copyItem (src/MENU/MAINMENU.PRG/2D10.c:1334)
int Items_Inventory_CopyItem(int itemCategory, Items_Inventory* targetInventory,
    int sourceItemIndex, Items_Inventory* sourceInventory)
{
    Items_InventoryMisc* temp_a0;
    int emptySlot;
    int itemCount;
    int i;
    int gemCount;
    int index;

    int parentIndex = itemCategory >> 8;
    int write = (itemCategory >> 4) & 1;
    itemCategory &= 0xF;
    index = 0;
    itemCount = Items_Inventory_CountItems(itemCategory, targetInventory);
    emptySlot = Items_Inventory_FirstEmptySlot(itemCategory, targetInventory);

    switch (itemCategory) {
    case Items_ItemCategory_Weapon: {
        Items_InventoryWeapon* source = &sourceInventory->weapons[sourceItemIndex];
        Items_InventoryWeapon* target = &targetInventory->weapons[emptySlot];

        if ((itemCount != 8)
            && (Items_Inventory_CountItems(Items_ItemCategory_Blade, targetInventory) != 16)) {
            if (Items_Inventory_CountItems(Items_ItemCategory_Grip, targetInventory) != 16) {

                gemCount = 0;

                for (i = 0; i < 3; ++i) {
                    if (source->gems[i] != 0) {
                        ++gemCount;
                    }
                }

                if ((Items_Inventory_CountItems(Items_ItemCategory_Gem, targetInventory)
                        + gemCount)
                    < 49) {

                    index = emptySlot + 1;

                    if (write != 0) {

                        Core_Memory_ZeroWordsBackward(target, sizeof *target);

                        target->index = index;
                        target->blade = Items_Inventory_CopyItem(
                            (index << 8) | Items_ItemCategory_Blade | Menu_Framework_CopyItemFlag_Write,
                            targetInventory, source->blade - 1, sourceInventory);
                        target->grip = Items_Inventory_CopyItem(
                            (index << 8) | Items_ItemCategory_Grip | Menu_Framework_CopyItemFlag_Write,
                            targetInventory, source->grip - 1, sourceInventory);

                        for (i = 0; i < 3; ++i) {
                            if (source->gems[i] != 0) {
                                target->gems[i] = Items_Inventory_CopyItem(
                                    (index << 8) | 0x15, targetInventory,
                                    source->gems[i] - 1, sourceInventory);
                            }
                        }

                        Core_Memory_CopyBytesBackward(target->name, source->name, 0x18);
                    }
                }
            }
        }
        break;
    }

    case Items_ItemCategory_Blade: {
        Items_InventoryBlade* source = &sourceInventory->blades[sourceItemIndex];
        Items_InventoryBlade* target = &targetInventory->blades[emptySlot];

        if (itemCount != 0x10) {

            index = emptySlot + 1;

            if (write != 0) {

                Core_Memory_CopyHalfwords(target, source, sizeof *target);

                target->index = index;
                target->assembledWeaponIndex = parentIndex;
            }
        }
        break;
    }

    case Items_ItemCategory_Grip: {
        Items_InventoryGrip* source = &sourceInventory->grips[sourceItemIndex];
        Items_InventoryGrip* target = &targetInventory->grips[emptySlot];

        if (itemCount != 0x10) {
            index = emptySlot + 1;
            if (write != 0) {

                Core_Memory_CopyHalfwords(target, source, sizeof *target);

                target->index = index;
                target->assembledWeaponIndex = parentIndex;
            }
        }
        break;
    }

    case Items_ItemCategory_Shield: {
        Items_InventoryShield* target = &sourceInventory->shields[sourceItemIndex];
        Items_InventoryShield* source = &targetInventory->shields[emptySlot];

        if (itemCount != 8) {
            gemCount = 0;
            for (i = 0; i < 3; ++i) {
                if (target->gems[i] != 0) {
                    ++gemCount;
                }
            }
            if ((Items_Inventory_CountItems(5, targetInventory) + gemCount) < 49) {

                index = emptySlot + 1;

                if (write != 0) {
                    Core_Memory_ZeroWordsBackward(source, 0x30);
                    Core_Memory_CopyHalfwords(
                        &source->base, &target->base, sizeof source->base);

                    source->index = index;

                    for (i = 0; i < 3; ++i) {
                        if (target->gems[i] != 0) {
                            source->gems[i] = Items_Inventory_CopyItem(
                                ((index | 0x80) << 8) | Items_ItemCategory_Gem
                                    | Menu_Framework_CopyItemFlag_Write,
                                targetInventory, target->gems[i] - 1, sourceInventory);
                        }
                    }
                }
            }
        }
        break;
    }

    case Items_ItemCategory_Armor: {
        Items_InventoryArmor* source = &sourceInventory->armor[sourceItemIndex];
        Items_InventoryArmor* target = &targetInventory->armor[emptySlot];

        if (itemCount != 16) {

            index = emptySlot + 1;

            if (write != 0) {

                Core_Memory_CopyHalfwords(target, source, sizeof *target);

                target->index = index;
            }
        }
        break;
    }

    case Items_ItemCategory_Gem: {
        Items_InventoryGem* source = &sourceInventory->gems[sourceItemIndex];
        Items_InventoryGem* target = &targetInventory->gems[emptySlot];

        if (itemCount != 0x30) {
            index = emptySlot + 1;
            if (write != 0) {

                Core_Memory_CopyHalfwords(target, source, sizeof *target);

                target->index = index;
                target->setItemIndex = parentIndex;
            }
        }
        break;
    }

    case Items_ItemCategory_Misc: {
        Items_InventoryMisc* source = &sourceInventory->misc[sourceItemIndex];
        Items_InventoryMisc* target = targetInventory->misc;
        int count = source->count;

        /* Probe before merging: rejected claims retain their whole loot row. */
        if ((write != 0) && (Items_Inventory_CopyItem(Items_ItemCategory_Misc,
                targetInventory, sourceItemIndex, sourceInventory) == 0)) {
            return 0;
        }

        Items_Inventory_RebuildDisplayIndices(6);

        for (i = 0; i < 0x40; ++i) {
            int slot = Audio_Seq_unusedInventoryIndices.misc[i];

            if (slot == 0) {
                continue;
            }

            temp_a0 = &target[slot - 1];

            if (temp_a0->id != source->id) {
                continue;
            }

            if (write != 0) {
                int space_left = 100 - temp_a0->count;

                if (space_left >= count) {
                    temp_a0->count += count;
                    count = 0;
                } else {
                    count = count + (temp_a0->count - 100);
                    temp_a0->count = 100;
                }
            } else {
                count = count - (100 - temp_a0->count);

                if (count < 0) {
                    count = 0;
                }
            }
        }

        if (count == 0) {
            index = 1;
            break;
        }

        if (itemCount != 0x40) {

            index = emptySlot + 1;

            if (write != 0) {
                target[emptySlot].id = source->id;
                target[emptySlot].count = count;
            }
        }
        break;
    }
    }

    return index;
}

// decomp: MAINMENU.PRG vs_mainMenu_initInventory (src/MENU/MAINMENU.PRG/C48.c:44)
void Items_Inventory_StoreEquippedStats(void)
{
    int i;
    Battle_ActorStats* actor = Battle_characterState->stats;

    if (actor->weapon.index != 0) {
        Items_StoreInventoryWeaponStats(
            &Audio_Seq_unusedInventory.weapons[actor->weapon.index - 1], &actor->weapon);
    }

    if (actor->shield.index != 0) {
        Items_StoreInventoryShieldStats(
            &Audio_Seq_unusedInventory.shields[actor->shield.index - 1], &actor->shield);
    }

    for (i = 0; i < 6; ++i) {
        if (actor->limbs[i].armor.index != 0) {
            Items_StoreInventoryArmorStats(
                &Audio_Seq_unusedInventory.armor[actor->limbs[i].armor.index - 1],
                &actor->limbs[i].armor);
        }
    }

    if (actor->accessory.index != 0) {
        Items_StoreInventoryAccessoryStats(
            &Audio_Seq_unusedInventory.armor[actor->accessory.index - 1], &actor->accessory);
    }
}

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Items_Inventory_ResetMainMenuStatics(void)
{
    {
        static const u_char initial[] = { 8, 16, 16, 8, 16, 48, 64, 2 };
        memcpy((void*)&Items_Inventory_inventoryItemCapacities, &initial, sizeof Items_Inventory_inventoryItemCapacities);
    }
}

