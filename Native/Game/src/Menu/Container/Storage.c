// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Container_discardMenuCursorAnimation = 0;

// decomp: D_80109A82 (D_80109A82@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_transferredMiscCount;

// decomp: _containerItemCapacities (_containerItemCapacities@src/MENU/MENUD.PRG/234.c)
u_short Menu_Container_containerItemCapacities[] = { 32, 64, 64, 32, 64, 192, 256, 2 };

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Container_discardMenuCountTemplate[] = "|>48|000/000\0";

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Container_discardSteps[] = { 1, 10, 100 };

// decomp: MENUD.PRG vs_menuD_initUiWeapon (src/MENU/MENUD.PRG/234.c:21)
void Menu_Container_BuildWeaponStats(Items_WeaponLoadout* target, Items_InventoryWeapon* source,
    Items_ContainerData* container)
{
    int i;
    Items_WeaponSpawnLoadout* temp = Core_Memory_AllocateFromHighEnd(sizeof *temp);
    Core_Memory_ZeroAlignedWords(temp, sizeof *temp);

    if (source != NULL) {
        temp->index = source->index;
        if (source->blade != 0) {
            Items_ReadInventoryBladeStats(
                &temp->blade, &container->blades[source->blade - 1]);
            temp->material = container->blades[source->blade - 1].material;
        }

        if (source->grip != 0) {
            Items_ReadInventoryGripStats(
                &temp->grip, &container->grips[source->grip - 1]);
        }

        for (i = 0; i < 3; ++i) {
            if (source->gems[i] != 0) {
                Items_ReadInventoryGemStats(
                    &temp->gems[i], &container->gems[source->gems[i] - 1]);
            }
        }
    }

    for (i = 0; i < 24; ++i) {
        temp->name[i] = source->name[i];
    }

    Items_BuildWeaponStatsFromParts(target, temp);
    Core_Memory_FreeFromHighEnd(temp);
}

// decomp: MENUD.PRG vs_menuD_initUiShield (src/MENU/MENUD.PRG/234.c:57)
void Menu_Container_BuildShieldStats(Items_ShieldLoadout* target, Items_InventoryShield* source,
    Items_ContainerData* container)
{
    int i;
    Items_ShieldSpawnLoadout* temp = Core_Memory_AllocateFromHighEnd(sizeof *temp);
    Core_Memory_ZeroAlignedWords(temp, sizeof *temp);

    if (source != NULL) {
        temp->index = source->index;
        Items_ReadInventoryArmorStats(&temp->base, &source->base);
        temp->material = source->base.material;

        for (i = 0; i < 3; ++i) {
            if (source->gems[i] != 0) {
                Items_ReadInventoryGemStats(
                    &temp->gems[i], &container->gems[(source->gems[i] & 0x7F) - 1]);
            }
        }
    }
    Items_BuildShieldStatsFromParts(target, temp);
    Core_Memory_FreeFromHighEnd(temp);
}

// decomp: MENUD.PRG _getContainerIndicesOffset (src/MENU/MENUD.PRG/234.c:85)
u_short* Menu_Container_CategoryIndices(int itemCategory, Items_Container* container)
{
    int i;
    int offset;

    offset = 0;
    for (i = 0; i < itemCategory; ++i) {
        offset += Menu_Container_containerItemCapacities[i];
    }
    return (u_short*)&container->indices + offset;
}

// decomp: MENUD.PRG _getContainerItemIndex (src/MENU/MENUD.PRG/234.c:97)
int Menu_Container_FindItemRow(int itemCategory, int index, u_short* indices)
{
    int i;

    for (i = 0; i < Menu_Container_containerItemCapacities[itemCategory]; ++i) {
        if (indices[i] == (index + 1)) {
            return i + 1;
        }
    }
    return 0;
}

// decomp: MENUD.PRG _setContainerItemIndex (src/MENU/MENUD.PRG/234.c:109)
void Menu_Container_AppendItemIndex(int itemCategory, int index, u_short* indices)
{
    int i;

    for (i = 0; i < Menu_Container_containerItemCapacities[itemCategory]; ++i) {
        if (indices[i] == 0) {
            indices[i] = index + 1;
            return;
        }
    }
}

// decomp: MENUD.PRG _initContainerObject (src/MENU/MENUD.PRG/234.c:121)
void Menu_Container_ClearItem(
    int itemCategory, int index, Items_ContainerData* container)
{
    int i;

    switch (itemCategory) {
    case Items_ItemCategory_Weapon: {
        Items_InventoryWeapon* weapon = &container->weapons[index];

        Menu_Container_ClearItem(1, weapon->blade - 1, container);
        Menu_Container_ClearItem(2, weapon->grip - 1, container);

        for (i = 0; i < 3; ++i) {
            int gem = weapon->gems[i];
            if (gem != 0) {
                Menu_Container_ClearItem(Items_ItemCategory_Gem, gem - 1, container);
            }
        }

        Core_Memory_ZeroWordsBackward(weapon, sizeof *weapon);
        weapon->index = index + 1;
        break;
    }
    case Items_ItemCategory_Blade: {
        Items_InventoryBlade* blade = &container->blades[index];
        Core_Memory_ZeroWordsBackward(blade, sizeof *blade);
        blade->index = index + 1;
        break;
    }
    case Items_ItemCategory_Grip: {
        Items_InventoryGrip* grip = &container->grips[index];
        Core_Memory_ZeroWordsBackward(grip, sizeof *grip);
        grip->index = index + 1;
        break;
    }
    case Items_ItemCategory_Shield: {
        Items_InventoryShield* shield = &container->shields[index];

        for (i = 0; i < 3; ++i) {
            int gem = shield->gems[i];
            if (gem != 0) {
                Menu_Container_ClearItem(Items_ItemCategory_Gem, gem - 1, container);
            }
        }

        Core_Memory_ZeroWordsBackward(shield, sizeof *shield);
        shield->index = index + 1;
        break;
    }
    case Items_ItemCategory_Armor: {
        Items_InventoryArmor* armor = &container->armor[index];
        Core_Memory_ZeroWordsBackward(armor, sizeof *armor);
        armor->index = index + 1;
        break;
    }
    case Items_ItemCategory_Gem: {
        Items_InventoryGem* gem = &container->gems[index];
        Core_Memory_ZeroWordsBackward(gem, sizeof *gem);
        gem->index = index + 1;
        break;
    }
    case Items_ItemCategory_Misc: {
        Items_InventoryMisc* item = &container->misc[index];
        Core_Memory_ZeroWordsBackward(item, sizeof *item);
        item->index = index + 1;
        break;
    }
    }
}

// decomp: MENUD.PRG _getContainerItemId (src/MENU/MENUD.PRG/234.c:191)
int Menu_Container_ItemId(int type, int index, Items_ContainerData* container)
{
    int ret = 0;

    switch (type) {
    case Items_ItemCategory_Weapon:
        ret = container->weapons[index].blade;
        break;
    case Items_ItemCategory_Blade:
        ret = container->blades[index].id;
        break;
    case Items_ItemCategory_Grip:
        ret = container->grips[index].id;
        break;
    case Items_ItemCategory_Shield:
        ret = container->shields[index].base.id;
        break;
    case Items_ItemCategory_Armor:
        ret = container->armor[index].id;
        break;
    case Items_ItemCategory_Gem:
        ret = container->gems[index].id;
        break;
    case Items_ItemCategory_Misc:
        ret = container->misc[index].id;
        break;
    }
    return ret;
}

// decomp: MENUD.PRG _countContainerItems (src/MENU/MENUD.PRG/234.c:221)
int Menu_Container_CountItems(int itemCategory, Items_ContainerData* container)
{
    int i;
    int count = 0;

    for (i = 0; i < Menu_Container_containerItemCapacities[itemCategory]; ++i) {
        if (Menu_Container_ItemId(itemCategory, i, container) != 0) {
            ++count;
        }
    }
    return count;
}

// decomp: MENUD.PRG _getEmptyContainerSlot (src/MENU/MENUD.PRG/234.c:234)
int Menu_Container_FindEmptySlot(int itemCategory, Items_ContainerData* container)
{
    int i;

    for (i = 0; i < Menu_Container_containerItemCapacities[itemCategory]; ++i) {
        if (Menu_Container_ItemId(itemCategory, i, container) == 0) {
            break;
        }
    }
    return i;
}

// decomp: MENUD.PRG func_801031A0 (src/MENU/MENUD.PRG/234.c:259)
void Menu_Container_PreviewTransferredMisc(void)
{
    int i;
    int var_a2;
    Items_ContainerData* container = &Menu_Framework_unusedInventoryStorage->transferPreview;

    Core_Memory_CopyHalfwordBlocks(&container->misc, Menu_Container_otherStorage->items.misc, sizeof container->misc);

    for (var_a2 = Menu_Framework_unusedInventoryStorage->itemsToTransfer[0], i = 0; var_a2 != 0;
        ++i, var_a2 = Menu_Framework_unusedInventoryStorage->itemsToTransfer[i]) {
        if ((var_a2 >> 8) == Items_ItemCategory_Misc) {
            Menu_Container_CopyItem(Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy | Items_ItemCategory_Misc, container,
                (var_a2 - 1) & 0xFF, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
        }
    }
}

// decomp: MENUD.PRG func_80103270 (src/MENU/MENUD.PRG/234.c:276)
void Menu_Container_RefreshMiscTransferCount(void)
{
    Menu_Container_PreviewTransferredMisc();
    Menu_Container_transferredMiscCount =
        Menu_Container_CountItems(Items_ItemCategory_Misc, &Menu_Framework_unusedInventoryStorage->transferPreview);
}

// decomp: MENUD.PRG _copyContainerItem (src/MENU/MENUD.PRG/234.c:283)
int Menu_Container_CopyItem(int itemCategory, Items_ContainerData* targetContainer,
    int sourceSlot, Items_ContainerData* sourceContainer)
{
    int copyFlag;
    int targetSlot;
    int parentIndex;
    int itemCount;
    int i;
    int subItemCount;
    int index;

    parentIndex = itemCategory >> 8;
    copyFlag = (itemCategory >> 4) & 1;
    itemCategory = itemCategory & 0xF;
    index = 0;
    itemCount = Menu_Container_CountItems(itemCategory, targetContainer);
    targetSlot = Menu_Container_FindEmptySlot(itemCategory, targetContainer);

    switch (itemCategory) {
    case Items_ItemCategory_Weapon: {
        Items_InventoryWeapon* source = &sourceContainer->weapons[sourceSlot];
        Items_InventoryWeapon* target = &targetContainer->weapons[targetSlot];

        if (((itemCount != 32)
                && (Menu_Container_CountItems(Items_ItemCategory_Blade, targetContainer) != 64))
            && (Menu_Container_CountItems(Items_ItemCategory_Grip, targetContainer) != 64)) {

            subItemCount = 0;

            for (i = 0; i < 3; ++i) {
                if (source->gems[i] != 0) {
                    ++subItemCount;
                }
            }

            if ((Menu_Container_CountItems(Items_ItemCategory_Gem, targetContainer) + subItemCount)
                > 192) {
                break;
            }

            index = targetSlot + 1;

            if (copyFlag != 0) {
                Core_Memory_ZeroWordsBackward(target, sizeof *target);
                target->index = index;
                target->blade = Menu_Container_CopyItem(
                    (index << 8) | Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy | Items_ItemCategory_Blade,
                    targetContainer, source->blade - 1, sourceContainer);
                target->grip = Menu_Container_CopyItem(
                    (index << 8) | Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy | Items_ItemCategory_Grip,
                    targetContainer, source->grip - 1, sourceContainer);
                for (i = 0; i < 3; ++i) {
                    if (source->gems[i] != 0) {
                        target->gems[i] = Menu_Container_CopyItem(
                            (index << 8) | Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy | Items_ItemCategory_Gem,
                            targetContainer, source->gems[i] - 1, sourceContainer);
                    }
                }
                Core_Memory_CopyBytesBackward(target->name, source->name, sizeof target->name);
            }
        }
        break;
    }
    case Items_ItemCategory_Blade: {
        Items_InventoryBlade* source = &sourceContainer->blades[sourceSlot];
        Items_InventoryBlade* target = &targetContainer->blades[targetSlot];

        if (itemCount == 64) {
            break;
        }

        index = targetSlot + 1;

        if (copyFlag != 0) {
            Core_Memory_CopyHalfwords(target, source, sizeof *target);
            target->index = index;
            target->assembledWeaponIndex = parentIndex;
        }
        break;
    }
    case Items_ItemCategory_Grip: {
        Items_InventoryGrip* source = &sourceContainer->grips[sourceSlot];
        Items_InventoryGrip* target = &targetContainer->grips[targetSlot];

        if (itemCount == 64) {
            break;
        }

        index = targetSlot + 1;

        if (copyFlag != 0) {
            Core_Memory_CopyHalfwords(target, source, sizeof *target);
            target->index = index;
            target->assembledWeaponIndex = parentIndex;
        }
        break;
    }
    case Items_ItemCategory_Shield: {
        Items_InventoryShield* source = &sourceContainer->shields[sourceSlot];
        Items_InventoryShield* target = &targetContainer->shields[targetSlot];

        if (itemCount == 32) {
            break;
        }

        subItemCount = 0;

        for (i = 0; i < 3; ++i) {
            if (source->gems[i] != 0) {
                ++subItemCount;
            }
        }

        if ((Menu_Container_CountItems(Items_ItemCategory_Gem, targetContainer) + subItemCount)
            > 192) {
            break;
        }

        index = targetSlot + 1;

        if (copyFlag != 0) {
            Core_Memory_ZeroWordsBackward(target, sizeof *target);
            Core_Memory_CopyHalfwords(&target->base, &source->base, sizeof target->base);
            target->index = index;
            for (i = 0; i < 3; ++i) {
                if (source->gems[i] != 0) {
                    target->gems[i] =
                        Menu_Container_CopyItem(((index | Items_GemTarget_Shield) << 8)
                                               | Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy | Items_ItemCategory_Gem,
                            targetContainer, source->gems[i] - 1, sourceContainer);
                }
            }
        }
        break;
    }
    case Items_ItemCategory_Armor: {
        Items_InventoryArmor* source = &sourceContainer->armor[sourceSlot];
        Items_InventoryArmor* target = &targetContainer->armor[targetSlot];

        if (itemCount == 64) {
            break;
        }

        index = targetSlot + 1;

        if (copyFlag != 0) {
            Core_Memory_CopyHalfwords(target, source, sizeof *target);
            target->index = index;
        }
        break;
    }
    case Items_ItemCategory_Gem: {
        Items_InventoryGem* source = &sourceContainer->gems[sourceSlot];
        Items_InventoryGem* target = &targetContainer->gems[targetSlot];

        if (itemCount == 192) {
            break;
        }

        index = targetSlot + 1;

        if (copyFlag != 0) {
            Core_Memory_CopyHalfwords(target, source, sizeof *target);
            target->index = index;
            target->setItemIndex = parentIndex;
        }
        break;
    }
    case Items_ItemCategory_Misc: {
        Items_InventoryMisc* target = targetContainer->misc;
        Items_InventoryMisc* source = &sourceContainer->misc[sourceSlot];

        subItemCount = source->count;

        for (i = 0; i < 256; ++i) {
            if (target[i].id == source->id) {
                if (copyFlag != 0) {
                    if ((100 - target[i].count) >= subItemCount) {
                        target[i].count = target[i].count + subItemCount;
                        subItemCount = 0;
                    } else {
                        int v0 = subItemCount - 100;
                        subItemCount = v0 + target[i].count;
                        target[i].count = 100;
                    }
                } else {
                    int v0 = subItemCount - 100;
                    subItemCount = v0 + target[i].count;
                    if (subItemCount < 0) {
                        subItemCount = 0;
                    }
                }
            }
        }

        if (subItemCount == 0) {
            index = 1;
            break;
        }
        if (itemCount == 256) {
            break;
        }

        index = targetSlot + 1;

        if (copyFlag != 0) {
            target[targetSlot].id = source->id;
            target[targetSlot].count = subItemCount;
        }
        break;
    }
    }
    return index;
}

// decomp: MENUD.PRG _defragmentContainerItems (src/MENU/MENUD.PRG/234.c:498)
void Menu_Container_RebuildCategoryIndices(int itemCategory, Items_Container* container)
{
    int i;
    int j;
    int var_a1;
    int source;
    int capacity;
    u_short* indexOffset;

    indexOffset = Menu_Container_CategoryIndices(itemCategory, container);
    capacity = Menu_Container_containerItemCapacities[itemCategory];

    for (i = 0; i < capacity; ++i) {
        source = indexOffset[i];
        if ((source != 0)
            && (Menu_Container_ItemId(itemCategory, source - 1, &container->items) == 0)) {
            indexOffset[i] = 0;
        }
    }

    i = 0;

    while (1) {
        if (indexOffset[i] != 0) {
            ++i;
            if (i == (capacity - 1)) {
                break;
            }
        } else {
            for (var_a1 = i + 1; var_a1 < capacity; ++var_a1) {
                if (indexOffset[var_a1] != 0) {
                    break;
                }
            }

            if (var_a1 == capacity) {
                break;
            }

            for (j = var_a1; j < capacity; ++j) {
                indexOffset[(j + i) - var_a1] = indexOffset[j];
            }

            for (j = j + (i - var_a1); j < capacity; ++j) {
                indexOffset[j] = 0;
            }
        }
    }

    for (i = 0; i < capacity; ++i) {
        if ((Menu_Container_ItemId(itemCategory, i, &container->items) != 0)
            && (Menu_Container_FindItemRow(itemCategory, i, indexOffset) == 0)) {
            Menu_Container_AppendItemIndex(itemCategory, i, indexOffset);
        }
    }
}

// decomp: MENUD.PRG _copyInventoryToContainer (src/MENU/MENUD.PRG/234.c:555)
void Menu_Container_SnapshotGadgetBag(void)
{
    int i;

    Items_Inventory* inventory = &Audio_Seq_unusedInventory;
    Items_Container* container = &Menu_Framework_unusedInventoryStorage->gadgetBag;

    Core_Memory_ZeroWordsBackward(&container->items, sizeof container->items);
    Core_Memory_ZeroWordsBackward(&Menu_Framework_unusedInventoryStorage->gadgetBag.indices,
        sizeof Menu_Framework_unusedInventoryStorage->gadgetBag.indices);

    for (i = 0; i < 7; ++i) {
        int j;
        char* sourceIndex = Menu_Inventory_inventoryIndices[i];
        u_short* targetIndex = Menu_Container_CategoryIndices(i, container);

        for (j = 0; j < Items_Inventory_inventoryItemCapacities[i]; ++j) {
            targetIndex[j] = sourceIndex[j];
        }
    }

    Core_Memory_CopyHalfwordBlocks(
        container->items.weapons, inventory->weapons, sizeof container->items.weapons / 4);
    Core_Memory_CopyHalfwordBlocks(
        container->items.blades, inventory->blades, sizeof container->items.blades / 4);
    Core_Memory_CopyHalfwordBlocks(
        container->items.grips, inventory->grips, sizeof container->items.grips / 4);
    Core_Memory_CopyHalfwordBlocks(
        container->items.shields, inventory->shields, sizeof container->items.shields / 4);
    Core_Memory_CopyHalfwordBlocks(
        container->items.armor, inventory->armor, sizeof container->items.armor / 4);
    Core_Memory_CopyHalfwordBlocks(
        container->items.gems, inventory->gems, sizeof container->items.gems / 4);
    Core_Memory_CopyHalfwordBlocks(
        container->items.misc, inventory->misc, sizeof container->items.misc / 4);
}

// decomp: MENUD.PRG _copyContainerToInventory (src/MENU/MENUD.PRG/234.c:592)
void Menu_Container_CommitGadgetBag(void)
{
    int i;

    Items_Container* container = &Menu_Framework_unusedInventoryStorage->gadgetBag;
    Items_Inventory* inventory = &Audio_Seq_unusedInventory;

    for (i = 0; i < 7; ++i) {
        int j;
        int k;
        char* targetIndex = Menu_Inventory_inventoryIndices[i];
        u_short* sourceIndex = Menu_Container_CategoryIndices(i, container);

        for (j = 0; j < 7; ++j) {
            Menu_Container_RebuildCategoryIndices(j, container);
        }

        for (k = 0; k < Items_Inventory_inventoryItemCapacities[i]; ++k) {
            targetIndex[k] = sourceIndex[k];
        }
    }

    Core_Memory_CopyHalfwordBlocks(
        inventory->weapons, container->items.weapons, sizeof inventory->weapons);
    Core_Memory_CopyHalfwordBlocks(inventory->blades, container->items.blades, sizeof inventory->blades);
    Core_Memory_CopyHalfwordBlocks(inventory->grips, container->items.grips, sizeof inventory->grips);
    Core_Memory_CopyHalfwordBlocks(
        inventory->shields, container->items.shields, sizeof inventory->shields);
    Core_Memory_CopyHalfwordBlocks(inventory->armor, container->items.armor, sizeof inventory->armor);
    Core_Memory_CopyHalfwordBlocks(inventory->gems, container->items.gems, sizeof inventory->gems);
    Core_Memory_CopyHalfwordBlocks(inventory->misc, container->items.misc, sizeof inventory->misc);
}

// decomp: MENUD.PRG _getParentItemIndex (src/MENU/MENUD.PRG/234.c:625)
int Menu_Container_ParentWeaponIndex(
    int itemCategory, int index, Items_ContainerData* container)
{
    int parentIndex = 0;

    if (itemCategory == Items_ItemCategory_Blade) {
        parentIndex = container->blades[index].assembledWeaponIndex;
    }
    if (itemCategory == Items_ItemCategory_Grip) {
        parentIndex = container->grips[index].assembledWeaponIndex;
    }
    if (itemCategory == Items_ItemCategory_Gem) {
        parentIndex = container->gems[index].setItemIndex;
        if (parentIndex & Items_GemTarget_Shield) {
            parentIndex = 0;
        }
    }
    return parentIndex;
}

// decomp: MENUD.PRG _getSetShieldIndex (src/MENU/MENUD.PRG/234.c:645)
int Menu_Container_ParentShieldIndex(
    int itemCategory, int index, Items_ContainerData* container)
{
    int parentIndex = 0;

    if (itemCategory == Items_ItemCategory_Gem) {
        parentIndex = container->gems[index].setItemIndex;
        if (!(container->gems[index].setItemIndex & Items_GemTarget_Shield)) {
            parentIndex = 0;
        }
    }
    return parentIndex & 0x7F;
}

// decomp: MENUD.PRG _sortWeaponsByStat (src/MENU/MENUD.PRG/234.c:1306)
void Menu_Container_SortWeapons(int stat)
{
    Items_WeaponLoadout target;
    u_short sortedIndices[32];
    int i;
    int maxStatValue;
    int sortedIndex;
    int sourceIndex;

    Items_InventoryWeapon* sourceWeapons = Menu_Container_containerData->items.weapons;
    u_short* targetIndices =
        Menu_Container_CategoryIndices(Items_ItemCategory_Weapon, Menu_Container_containerData);
    Core_Memory_ZeroWordsBackward(sortedIndices, sizeof sortedIndices);
    sortedIndex = 0;

    while (1) {
        maxStatValue = -INT_MAX - 1;

        for (i = 0; i < 32; ++i) {
            int statValue;
            sourceIndex = targetIndices[i];
            if (sourceIndex == 0) {
                continue;
            }
            Menu_Container_BuildWeaponStats(
                &target, &sourceWeapons[sourceIndex - 1], &Menu_Container_containerData->items);
            statValue = Menu_Framework_WeaponSortValue(stat, &target);
            if (maxStatValue < statValue) {
                maxStatValue = statValue;
            }
        }

        if (maxStatValue == (-INT_MAX - 1)) {
            break;
        }

        for (i = 0; i < 32; ++i) {
            sourceIndex = targetIndices[i];
            if (sourceIndex == 0) {
                continue;
            }
            Menu_Container_BuildWeaponStats(
                &target, &sourceWeapons[sourceIndex - 1], &Menu_Container_containerData->items);
            if (Menu_Framework_WeaponSortValue(stat, &target) == maxStatValue) {
                sortedIndices[sortedIndex++] = sourceIndex;
                targetIndices[i] = 0;
            }
        }
    }
    Core_Memory_CopyHalfwordBlocks(targetIndices, &sortedIndices, sizeof sortedIndices);
}

// decomp: MENUD.PRG _sortShieldsByStat (src/MENU/MENUD.PRG/234.c:1388)
void Menu_Container_SortShields(int arg0)
{
    Items_ShieldLoadout sp10;
    u_short sp178[0x20];
    int temp_v0;
    int i;
    int var_s3;
    int temp_s0;

    Items_InventoryShield* temp_s5 = Menu_Container_containerData->items.shields;
    u_short* temp_s6 = Menu_Container_CategoryIndices(3, Menu_Container_containerData);
    int var_s4 = 0;

    Core_Memory_ZeroWordsBackward(&sp178, sizeof sp178);

    while (1) {
        var_s3 = 0x80000000;

        for (i = 0; i < 32; ++i) {
            temp_s0 = temp_s6[i];
            if (temp_s0 != 0) {
                Menu_Container_BuildShieldStats(
                    &sp10, &temp_s5[temp_s0 - 1], &Menu_Container_containerData->items);
                temp_v0 = Menu_Framework_ShieldSortValue(arg0, &sp10);
                if (var_s3 < temp_v0) {
                    var_s3 = temp_v0;
                }
            }
        }

        if (var_s3 == 0x80000000) {
            break;
        }

        for (i = 0; i < 32; ++i) {
            temp_s0 = temp_s6[i];
            if (temp_s0 != 0) {
                Menu_Container_BuildShieldStats(
                    &sp10, &temp_s5[temp_s0 - 1], &Menu_Container_containerData->items);
                if (Menu_Framework_ShieldSortValue(arg0, &sp10) == var_s3) {
                    sp178[var_s4++] = temp_s0;
                    temp_s6[i] = 0;
                }
            }
        }
    }
    Core_Memory_CopyHalfwordBlocks(temp_s6, &sp178, sizeof sp178);
}

// decomp: MENUD.PRG _sortItemsByStat (src/MENU/MENUD.PRG/234.c:1445)
void Menu_Container_SortMisc(int arg0)
{
    u_short sp10[0x100];
    int temp_v0;
    int i;
    int var_s4;
    int var_s5;
    int temp_s0;
    u_short* temp_s7;
    Items_InventoryMisc* item;

    item = Menu_Container_containerData->items.misc;
    temp_s7 = Menu_Container_CategoryIndices(6, Menu_Container_containerData);
    Core_Memory_ZeroWordsBackward(&sp10, sizeof sp10);
    var_s5 = 0;

    while (1) {
        var_s4 = 0x80000000;

        for (i = 0; i < 256; ++i) {
            temp_s0 = temp_s7[i];
            if (temp_s0 != 0) {
                temp_v0 = Menu_Framework_MiscSortValue(arg0, &item[temp_s0 - 1]);
                if (var_s4 < temp_v0) {
                    var_s4 = temp_v0;
                }
            }
        }

        if (var_s4 == 0x80000000) {
            break;
        }

        for (i = 0; i < 256; ++i) {
            temp_s0 = temp_s7[i];
            if ((temp_s0 != 0) && (Menu_Framework_MiscSortValue(arg0, &item[temp_s0 - 1]) == var_s4)) {
                sp10[var_s5++] = temp_s0;
                temp_s7[i] = 0;
            }
        }
    }
    Core_Memory_CopyHalfwordBlocks(temp_s7, &sp10, sizeof sp10);
}

// decomp: MENUD.PRG _sortEquipment (src/MENU/MENUD.PRG/234.c:1528)
void Menu_Container_CopyEquipmentStats(Items_EquipmentPartStats* item, int type, int index)
{
    switch (type) {
    case Items_ItemCategory_Blade:
        Items_ReadInventoryBladeStats(
            item, &Menu_Container_containerData->items.blades[index]);
        return;
    case Items_ItemCategory_Grip:
        Items_ReadInventoryGripStats(
            item, &Menu_Container_containerData->items.grips[index]);
        return;
    case Items_ItemCategory_Armor:
        Items_ReadInventoryArmorStats(
            item, &Menu_Container_containerData->items.armor[index]);
        return;
    case Items_ItemCategory_Gem:
        Items_ReadInventoryGemStats(item, &Menu_Container_containerData->items.gems[index]);
        return;
    }
}

// decomp: MENUD.PRG _copyEquipmentByStat (src/MENU/MENUD.PRG/234.c:1549)
void Menu_Container_SortEquipment(int arg0, int arg1)
{
    Items_EquipmentPartStats sp10;
    int temp_v0;
    int i;
    int var_s4;
    int var_s6;
    int temp_s0;
    int temp_s5;
    u_short* temp_s7;

    var_s6 = 0;
    temp_s5 = Menu_Container_containerItemCapacities[arg0];

    {
        u_short sp40[temp_s5];
        temp_s7 = Menu_Container_CategoryIndices(arg0, Menu_Container_containerData);
        Core_Memory_ZeroWordsBackward(sp40, temp_s5 * 2);

        while (1) {
            var_s4 = 0x80000000;

            for (i = 0; i < temp_s5; ++i) {
                temp_s0 = temp_s7[i];
                if (temp_s0 != 0) {
                    Menu_Container_CopyEquipmentStats(&sp10, arg0, temp_s0 - 1);
                    temp_v0 = Menu_Framework_EquipmentSortValue(arg1, &sp10);
                    if (var_s4 < temp_v0) {
                        var_s4 = temp_v0;
                    }
                }
            }

            if (var_s4 == 0x80000000) {
                break;
            }

            for (i = 0; i < temp_s5; ++i) {
                temp_s0 = temp_s7[i];
                if (temp_s0 != 0) {
                    Menu_Container_CopyEquipmentStats(&sp10, arg0, temp_s0 - 1);
                    if (Menu_Framework_EquipmentSortValue(arg1, &sp10) == var_s4) {
                        sp40[var_s6++] = temp_s0;
                        temp_s7[i] = 0;
                    }
                }
            }
        }
        Core_Memory_CopyHalfwordBlocks(temp_s7, sp40, temp_s5 * 2);
    }
}

// decomp: MENUD.PRG _displaySortMenu (src/MENU/MENUD.PRG/234.c:1601)
int Menu_Container_UpdateSortMenu(int arg0)
{
    static char Menu_Container_kWeaponSortStats[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23 };
    static char Menu_Container_kGripSortStats[] = { 0, 8, 9, 10, 24, 25, 26 };
    static char Menu_Container_kShieldSortStats[] = { 1, 4, 5, 6, 7, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39 };
    static char Menu_Container_kArmorSortStats[] = { 0, 1, 4, 5, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39 };
    static char Menu_Container_kGemSortStats[] = { 8, 9, 10, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
        39 };
    static char Menu_Container_kMiscSortStats[] = { 0, 56 };
    static char Menu_Container_kSortStatCounts[] = { sizeof Menu_Container_kWeaponSortStats, sizeof Menu_Container_kWeaponSortStats, sizeof Menu_Container_kGripSortStats,
        sizeof Menu_Container_kShieldSortStats, sizeof Menu_Container_kArmorSortStats, sizeof Menu_Container_kGemSortStats, sizeof Menu_Container_kMiscSortStats };
    static char* Menu_Container_kSortStatLists[] = { Menu_Container_kWeaponSortStats, Menu_Container_kWeaponSortStats, Menu_Container_kGripSortStats, Menu_Container_kShieldSortStats,
        Menu_Container_kArmorSortStats, Menu_Container_kGemSortStats, Menu_Container_kMiscSortStats };

    static char Menu_Container_sortMenuStep;
    static u_char Menu_Container_sortCategory;

    enum Menu_Container_SortMenuState { Menu_Container_SortMenuState_Init, Menu_Container_SortMenuState_Display };

    char* sp10[48];
    int spD0[24];
    int i;
    char* new_var;

    if (arg0 != 0) {
        Menu_Container_sortCategory = arg0 - 1;
        Menu_Container_sortMenuStep = 0;
        return 0;
    }

    switch (Menu_Container_sortMenuStep) {
    case Menu_Container_SortMenuState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            new_var = Menu_Container_kSortStatLists[Menu_Container_sortCategory];

            for (i = 0; i < Menu_Container_kSortStatCounts[Menu_Container_sortCategory]; ++i) {
                sp10[i * 2] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [new_var[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortMenu]];
                sp10[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [new_var[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortMenuDesc]];
                spD0[i] = 0;
            }
            Menu_Framework_CreateActionRows(i, Menu_Container_sortCategory + 96, sp10, spD0);
            Menu_Container_sortMenuStep = Menu_Container_SortMenuState_Display;
        }
        break;
    case Menu_Container_SortMenuState_Display:
        Menu_Framework_UpdateActionMenu();
        i = Menu_Framework_ActionSelectionResult();
        if ((i + 1) != 0) {
            if (i >= 0) {
                Menu_PlaySelectSound();
                switch (Menu_Container_sortCategory) {
                case 0:
                    Menu_Container_SortWeapons(Menu_Container_kWeaponSortStats[i]);
                    break;
                case 3:
                    Menu_Container_SortShields(Menu_Container_kShieldSortStats[i]);
                    break;
                case 6:
                    Menu_Container_SortMisc(Menu_Container_kMiscSortStats[i]);
                    break;
                default:
                    Menu_Container_SortEquipment(Menu_Container_sortCategory, Menu_Container_kSortStatLists[Menu_Container_sortCategory][i]);
                    break;
                }
            } else {
                Menu_PlayLeaveSound();
            }
            return i + 1;
        }
        break;
    }
    return 0;
}

// decomp: MENUD.PRG _discardMenu (src/MENU/MENUD.PRG/234.c:1680)
int Menu_Container_UpdateDiscardMenu(int arg0)
{
    enum Menu_Inventory_DiscardMenuState {
        Menu_Inventory_DiscardMenuState_Init,
        Menu_Inventory_DiscardMenuState_DiscardOneInit,
        Menu_Inventory_DiscardMenuState_DiscardMultipleInit,
        Menu_Inventory_DiscardMenuState_DiscardOne,
        Menu_Inventory_DiscardMenuState_ClearTemplate,
        Menu_Inventory_DiscardMenuState_DetermineDiscardAmount,
        Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm,
        Menu_Inventory_DiscardMenuState_DiscardMultiple
    };

#pragma vsstring(start)
#pragma vsstring(end)


    static char Menu_Container_discardMenuState;
    static char Menu_Container_discardMenuItemCategory;
    static u_char Menu_Container_discardMenuSelectedItem;
    static char Menu_Container_discardType;
    static u_char Menu_Container_discardStep;
    static char Menu_Container_discardCount;
    static char Menu_Container_discardMenuMaxAmount;
    static char Menu_Container_discardMenuLayoutPadding[3] __attribute__((unused));

    char* discardOneText[4];
    int discardOneRowTypes[2];
    char* discardMultipleText[4];
    int discardMultipleRowTypes[2];
    int i;
    int temp_v0_7;
    int var_v0;
    int a2;
    Menu_Framework_MenuRow* menuItem;
    int a0;

    if (arg0 != 0) {
        Menu_Container_discardType = arg0 >> 0x10;
        Menu_Container_discardMenuItemCategory = (arg0 >> 0xC) & 0xF;
        Menu_Container_discardMenuSelectedItem = arg0 - 1;
        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Menu_Container_discardType + Text_Menu_ITEMHELP_BIN_Index_discardConfirm]]);
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_Init;
        return 0;
    }
    switch (Menu_Container_discardMenuState) {
    case Menu_Inventory_DiscardMenuState_Init:
        var_v0 = 0;
        if (Menu_textBoxes[7].flags.done) {
            if (Menu_Framework_AreMenuRowsIdle() != 0) {
                Menu_Container_discardMenuState = ((Menu_Container_discardType & 1) + 1);
            }
        }
        break;
    case Menu_Inventory_DiscardMenuState_DiscardOneInit:
        for (i = 0; i < 2; ++i) {
            discardOneText[i * 2] = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[i + Text_Menu_ITEMHELP_BIN_Index_confirmYes]];
            discardOneText[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Menu_Container_discardType + Text_Menu_ITEMHELP_BIN_Index_discardConfirm]];
            discardOneRowTypes[i] = 0;
        }

        Menu_Framework_CreateActionRows(
            2, Menu_Framework_ActionsMenuAction_Discard, discardOneText, discardOneRowTypes);
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardOne;
        break;
    case Menu_Inventory_DiscardMenuState_DiscardMultipleInit:
        Menu_Container_discardMenuMaxAmount = Menu_Container_containerData->items.misc[Menu_Container_discardMenuSelectedItem].count;
        i = Core_Math_SplitDecimalDigit(Menu_Container_discardMenuMaxAmount);
        Menu_Container_discardMenuCountTemplate[8] = i & 0xF;
        i = Core_Math_SplitDecimalDigit(i >> 4);

        Menu_Container_discardMenuCountTemplate[7] = i & 0xF;
        Menu_Container_discardMenuCountTemplate[6] = i >> 4;

        for (i = 2; i < 5; ++i) {
            Menu_Container_discardMenuCountTemplate[i] = 0;
        }

        menuItem = Menu_Framework_SetMenuItem(0x22, -0x7E, 0x82, 0x7E, 0, Menu_Container_discardMenuCountTemplate);
        menuItem->state = 5;
        menuItem->targetPosition0 = 0;
        Menu_Container_discardStep = 0;
        Menu_Container_discardCount = 0;
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_ClearTemplate;
        break;
    case Menu_Inventory_DiscardMenuState_DiscardOne:
        Menu_Framework_UpdateActionMenu();
        i = Menu_Framework_ActionSelectionResult() + 1;
        if (i != 0) {
            if (i == 1) {
                Audio_PlaySfxCentered(0x7E, 0x1C);
                Menu_Container_ClearItem(
                    Menu_Container_discardMenuItemCategory, Menu_Container_discardMenuSelectedItem, &Menu_Container_containerData->items);
            } else {
                Menu_PlayLeaveSound();
            }
            return i;
        }
        break;
    case Menu_Inventory_DiscardMenuState_ClearTemplate:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        for (i = 2; i < 5; ++i) {
#pragma vsstring(start)
            Menu_Container_discardMenuCountTemplate[i] = ' ';
#pragma vsstring(end)
        }

        Menu_Framework_SetMenuItem(0x22, 0, 0x82, 0x7E, 0, Menu_Container_discardMenuCountTemplate);
        Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DetermineDiscardAmount;
        // Fallthrough
    case Menu_Inventory_DiscardMenuState_DetermineDiscardAmount:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRright | PADRdown)) {
            i = Core_Math_SplitDecimalDigit(Menu_Container_discardCount);
            Menu_Container_discardMenuCountTemplate[4] = i & 0xF;
            i = Core_Math_SplitDecimalDigit(i >> 4);
            Menu_Container_discardMenuCountTemplate[3] = i & 0xF;
            Menu_Container_discardMenuCountTemplate[2] = i >> 4;
            Menu_Framework_SetMenuItem(0x22, 0, 0x82, 0x7E, 0, Menu_Container_discardMenuCountTemplate);
            if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {
                Menu_PlayLeaveSound();
                if ((Input_buttonsPressed.allButtons & PADRup)) {
                    return -2;
                }
                return -1;
            }
            if (Menu_Container_discardCount == 0) {
                Menu_PlayLeaveSound();
                return -1;
            }
            Menu_PlaySelectSound();
            Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm;
            break;
        }

        i = 0;
        a0 = a2 = Menu_Container_discardCount;
        if (Input_buttonRepeat & PADLup) {
            i = 1;
        }
        if (Input_buttonRepeat & PADLdown) {
            --i;
        }
        if (i == 1) {
            a0 = Menu_Container_discardCount + Menu_Container_discardSteps[Menu_Container_discardStep];
        } else if (i == -1) {
            a0 = Menu_Container_discardCount - Menu_Container_discardSteps[Menu_Container_discardStep];
        }
        if (a0 >= 0) {
            if (Menu_Container_discardMenuMaxAmount >= a0) {
                Menu_Container_discardCount = a0;
            }
        }
        i = 0;
        a0 = Menu_Container_discardStep;
        if (Input_buttonRepeat & PADLright) {
            i = -1;
        }
        if (Input_buttonRepeat & PADLleft) {
            ++i;
        }
        if (i == 1) {
            if ((a0 & 0xFF) == 2) {
                if (Menu_Container_discardCount != Menu_Container_discardMenuMaxAmount) {
                    Menu_Container_discardCount = Menu_Container_discardMenuMaxAmount;
                }
            } else {
                Menu_Container_discardStep = a0 + 1;
            }
        } else if (i == -1) {
            if (!(a0 & 0xFF)) {
                if (Menu_Container_discardCount != 0) {
                    Menu_Container_discardCount = 0;
                }
            } else {
                Menu_Container_discardStep = a0 - 1;
            }
        }
        if ((a0 != Menu_Container_discardStep) || (a2 != Menu_Container_discardCount)) {
            Menu_PlayChangeSound();
        }
        Render_SetMenuFontStyle(5);
        temp_v0_7 = Core_Math_SplitDecimalDigit(Menu_Container_discardCount);
        Render_DrawVariableWidthGlyph(
            temp_v0_7 & 0xF, 0x42, 0x82, Render_frameScratch.overlayOrderingTable - 0xC);
        temp_v0_7 = Core_Math_SplitDecimalDigit(temp_v0_7 >> 4);
        Render_DrawVariableWidthGlyph(
            temp_v0_7 & 0xF, 0x3C, 0x82, Render_frameScratch.overlayOrderingTable - 0xC);
        Render_DrawVariableWidthGlyph(
            temp_v0_7 >> 4, 0x36, 0x82, Render_frameScratch.overlayOrderingTable - 0xC);
        Menu_Container_discardMenuCursorAnimation =
            Menu_Framework_DrawCursor(Menu_Container_discardMenuCursorAnimation, (0x36 - (Menu_Container_discardStep * 6)) | 0x720000);
        break;
    case Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                discardMultipleText[i * 2] = (char*)&Menu_Framework_itemHelp
                    [Menu_Framework_itemHelp[i + Text_Menu_ITEMHELP_BIN_Index_confirmYes]];
                discardMultipleText[i * 2 + 1] =
                    (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardConfirm];
                discardMultipleRowTypes[i] = 0;
            }

            Menu_Framework_CreateActionRows(
                2, Menu_Framework_ActionsMenuAction_Discard, discardMultipleText, discardMultipleRowTypes);
            Menu_Container_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardMultiple;
        }
        break;
    case Menu_Inventory_DiscardMenuState_DiscardMultiple:
        Menu_Framework_UpdateActionMenu();
        i = Menu_Framework_ActionSelectionResult() + 1;
        if (i != 0) {
            if (i == 1) {
                Audio_PlaySfxCentered(0x7E, 0x1C);
                if (Menu_Container_containerData->items.misc[Menu_Container_discardMenuSelectedItem].count
                    == Menu_Container_discardCount) {
                    Menu_Container_ClearItem(
                        Menu_Container_discardMenuItemCategory, Menu_Container_discardMenuSelectedItem, &Menu_Container_containerData->items);
                } else {
                    Menu_Container_containerData->items.misc[Menu_Container_discardMenuSelectedItem].count -= Menu_Container_discardCount;
                }
            } else {
                Menu_PlayLeaveSound();
            }
            return i;
        }
        break;
    }
    return 0;
}

// decomp: MENUD.PRG _getWeaponsubItemCount (src/MENU/MENUD.PRG/234.c:1959)
int Menu_Container_CountWeaponGems(int arg0)
{
    int i;
    Items_InventoryWeapon* temp = &Menu_Container_containerData->items.weapons[arg0];
    int count = 0;

    for (i = 0; i < Menu_Container_containerData->items.grips[temp->grip - 1].gemSlots; ++i) {
        count += temp->gems[i] != 0;
    }
    return count;
}

// decomp: MENUD.PRG _getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1971)
int Menu_Container_CountShieldGems(int arg0)
{
    int i;
    Items_InventoryShield* shield = &Menu_Container_containerData->items.shields[arg0];
    int count = 0;

    for (i = 0; i < shield->base.gemSlots; ++i) {
        count += shield->gems[i] != 0;
    }
    return count;
}

// decomp: MENUD.PRG func_801066DC (src/MENU/MENUD.PRG/234.c:1983)
int Menu_Container_WouldExceedCapacity(int arg0, int arg1)
{
    int temp_s0 = Menu_Container_CountItems(arg0, &Menu_Container_otherStorage->items);
    int temp_s1 =
        arg1 + (temp_s0 + Menu_Container_CountItems(arg0, &Menu_Framework_unusedInventoryStorage->stagedTransfer));

    if (Menu_Container_browsingGadgetBag != 0) {
        return Menu_Container_containerItemCapacities[arg0] < temp_s1;
    }
    return Items_Inventory_inventoryItemCapacities[arg0] < temp_s1;
}

// decomp: MENUD.PRG func_80106784 (src/MENU/MENUD.PRG/234.c:1995)
int Menu_Container_TransferCapacityError(int arg0, int arg1)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == 0) {
            break;
        }
    }

    if (arg0 == 0) {
        i += 2 + Menu_Container_CountWeaponGems(arg1);
    }

    if (arg0 == 3) {
        i += Menu_Container_CountShieldGems(arg1);
    }

    if (i >= 64) {
        return 1;
    }

    if (Menu_Container_WouldExceedCapacity(arg0, 1) != 0) {
        return 2;
    }

    if (arg0 == 0) {
        if (Menu_Container_WouldExceedCapacity(1, 1) != 0) {
            return 2;
        }
        if (Menu_Container_WouldExceedCapacity(2, 1) != 0) {
            return 2;
        }
        if (Menu_Container_WouldExceedCapacity(5, Menu_Container_CountWeaponGems(arg1)) != 0) {
            return 2;
        }
    }

    if (arg0 == 3 && Menu_Container_WouldExceedCapacity(5, Menu_Container_CountShieldGems(arg1)) != 0) {
        return 2;
    }

    return 0;
}

// decomp: MENUD.PRG func_801068BC (src/MENU/MENUD.PRG/234.c:2040)
void Menu_Container_UndoPendingTransfer(int arg0)
{
    int temp_s0_2;
    int temp_s2;
    int temp_s0;

    temp_s0 = Menu_Framework_unusedInventoryStorage->itemsToTransfer[arg0];
    temp_s2 = temp_s0 >> 8;
    temp_s0_2 = (temp_s0 - 1) & 0xFF;
    Menu_Container_CopyItem(temp_s2 | 0x10, &Menu_Container_containerData->items, temp_s0_2,
        &Menu_Framework_unusedInventoryStorage->stagedTransfer);
    Menu_Container_ClearItem(temp_s2, temp_s0_2, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
}

// decomp: MENUD.PRG func_80106948 (src/MENU/MENUD.PRG/234.c:2054)
void Menu_Container_StageItemTransfer(int arg0, int arg1)
{
    Menu_Container_CopyItem(arg0 | 0x10, &Menu_Framework_unusedInventoryStorage->stagedTransfer, arg1,
        &Menu_Container_containerData->items);
    Menu_Container_ClearItem(arg0, arg1, &Menu_Container_containerData->items);
}

// decomp: MENUD.PRG func_801069B0 (src/MENU/MENUD.PRG/234.c:2061)
int Menu_Container_FindPendingTransfer(int arg0, int arg1)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == ((arg0 << 8) | (arg1 + 1))) {
            return i + 1;
        }
    }
    return 0;
}

// decomp: MENUD.PRG func_80106A04 (src/MENU/MENUD.PRG/234.c:2073)
void Menu_Container_AppendPendingTransfer(int arg0, int arg1)
{
    int i;
    u_short* var_v1;

    i = 0;
    var_v1 = (u_short*)Menu_Framework_unusedInventoryStorage + 0x61D8;
    for (i = 0; i < 64; ++i) {
        if (var_v1[i] == 0) {
            var_v1[i] = ((arg0 << 8) | (arg1 + 1));
            return;
        }
    }
}

// decomp: MENUD.PRG func_80106A50 (src/MENU/MENUD.PRG/234.c:2088)
void Menu_Container_RebuildPendingTransfers(void)
{
    int j;
    int var_a1;
    int i;
    int k;
    u_short* temp_s2 = Menu_Framework_unusedInventoryStorage->itemsToTransfer;

    for (i = 0; i < 64; ++i) {
        int source = temp_s2[i];
        if ((source != 0)
            && (Menu_Container_ItemId((source >> 8), (source - 1) & 0xFF,
                    &Menu_Framework_unusedInventoryStorage->stagedTransfer)
                == 0)) {
            temp_s2[i] = 0;
        }
    }

    i = 0;

    while (1) {
        if (temp_s2[i] != 0) {
            ++i;
            if (i == 0x3F) {
                break;
            }
        } else {
            for (var_a1 = i + 1; var_a1 < 0x40; ++var_a1) {
                if (temp_s2[var_a1] != 0) {
                    break;
                }
            }

            if (var_a1 == 0x40) {
                break;
            }

            for (j = var_a1; j < 0x40; ++j) {
                temp_s2[j + i - var_a1] = temp_s2[j];
            }

            for (j = j + (i - var_a1); j < 64; ++j) {
                temp_s2[j] = 0;
            }
        }
    }

    for (k = 0; k < 7; ++k) {
        for (i = 0; i < Menu_Container_containerItemCapacities[k]; ++i) {
            if ((Menu_Container_ItemId(k, i, &Menu_Framework_unusedInventoryStorage->stagedTransfer) != 0)
                && (Menu_Container_FindPendingTransfer(k, i) == 0)) {
                Menu_Container_AppendPendingTransfer(k, i);
            }
        }
    }
}

// MENUD.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Container_Storage_ResetMenudStatics(void)
{
    {
        static const u_short initial[] = { 32, 64, 64, 32, 64, 192, 256, 2 };
        memcpy((void*)&Menu_Container_containerItemCapacities, &initial, sizeof Menu_Container_containerItemCapacities);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Container_discardMenuCursorAnimation, &initial, sizeof Menu_Container_discardMenuCursorAnimation);
    }
    {
        static const char initial[] = "|>48|000/000\0";
        memcpy((void*)&Menu_Container_discardMenuCountTemplate, &initial, sizeof Menu_Container_discardMenuCountTemplate);
    }
    {
        static const char initial[] = { 1, 10, 100 };
        memcpy((void*)&Menu_Container_discardSteps, &initial, sizeof Menu_Container_discardSteps);
    }
}

