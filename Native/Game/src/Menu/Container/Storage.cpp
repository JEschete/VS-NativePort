// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
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
    Items_WeaponSpawnLoadout* temp = (Items_WeaponSpawnLoadout *)(Core_Memory_AllocateFromHighEnd(sizeof *temp));
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
    Items_ShieldSpawnLoadout* temp = (Items_ShieldSpawnLoadout *)(Core_Memory_AllocateFromHighEnd(sizeof *temp));
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
    int packed_transfer_item;
    Items_ContainerData* container = &Menu_Framework_unusedInventoryStorage->transferPreview;

    Core_Memory_CopyHalfwordBlocks(&container->misc, Menu_Container_otherStorage->items.misc, sizeof container->misc);

    for (packed_transfer_item = Menu_Framework_unusedInventoryStorage->itemsToTransfer[0], i = 0; packed_transfer_item != 0;
        ++i, packed_transfer_item = Menu_Framework_unusedInventoryStorage->itemsToTransfer[i]) {
        if ((packed_transfer_item >> 8) == Items_ItemCategory_Misc) {
            Menu_Container_CopyItem((int)Menu_Container_CopyContainerFlag_CopyContainerFlagsCopy | Items_ItemCategory_Misc, container,
                (packed_transfer_item - 1) & 0xFF, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
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
    int next_occupied_index;
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
            for (next_occupied_index = i + 1; next_occupied_index < capacity; ++next_occupied_index) {
                if (indexOffset[next_occupied_index] != 0) {
                    break;
                }
            }

            if (next_occupied_index == capacity) {
                break;
            }

            for (j = next_occupied_index; j < capacity; ++j) {
                indexOffset[(j + i) - next_occupied_index] = indexOffset[j];
            }

            for (j = j + (i - next_occupied_index); j < capacity; ++j) {
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
        char* sourceIndex = (char *)(Menu_Inventory_inventoryIndices[i]);
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
        char* targetIndex = (char *)(Menu_Inventory_inventoryIndices[i]);
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
void Menu_Container_SortShields(int sort_stat_id)
{
    Items_ShieldLoadout shield_stats;
    u_short sorted_shield_indices[0x20];
    int shield_sort_value;
    int i;
    int highest_remaining_sort_value;
    int shield_id;

    Items_InventoryShield* shield_items = Menu_Container_containerData->items.shields;
    u_short* shield_indices = Menu_Container_CategoryIndices(3, Menu_Container_containerData);
    int sorted_shield_count = 0;

    Core_Memory_ZeroWordsBackward(&sorted_shield_indices, sizeof sorted_shield_indices);

    while (1) {
        highest_remaining_sort_value = 0x80000000;

        for (i = 0; i < 32; ++i) {
            shield_id = shield_indices[i];
            if (shield_id != 0) {
                Menu_Container_BuildShieldStats(
                    &shield_stats, &shield_items[shield_id - 1], &Menu_Container_containerData->items);
                shield_sort_value = Menu_Framework_ShieldSortValue(sort_stat_id, &shield_stats);
                if (highest_remaining_sort_value < shield_sort_value) {
                    highest_remaining_sort_value = shield_sort_value;
                }
            }
        }

        if (highest_remaining_sort_value == 0x80000000) {
            break;
        }

        for (i = 0; i < 32; ++i) {
            shield_id = shield_indices[i];
            if (shield_id != 0) {
                Menu_Container_BuildShieldStats(
                    &shield_stats, &shield_items[shield_id - 1], &Menu_Container_containerData->items);
                if (Menu_Framework_ShieldSortValue(sort_stat_id, &shield_stats) == highest_remaining_sort_value) {
                    sorted_shield_indices[sorted_shield_count++] = shield_id;
                    shield_indices[i] = 0;
                }
            }
        }
    }
    Core_Memory_CopyHalfwordBlocks(shield_indices, &sorted_shield_indices, sizeof sorted_shield_indices);
}

// decomp: MENUD.PRG _sortItemsByStat (src/MENU/MENUD.PRG/234.c:1445)
void Menu_Container_SortMisc(int sort_stat_id)
{
    u_short sorted_misc_indices[0x100];
    int misc_sort_value;
    int i;
    int highest_remaining_sort_value;
    int sorted_misc_count;
    int misc_item_id;
    u_short* misc_indices;
    Items_InventoryMisc* item;

    item = Menu_Container_containerData->items.misc;
    misc_indices = Menu_Container_CategoryIndices(6, Menu_Container_containerData);
    Core_Memory_ZeroWordsBackward(&sorted_misc_indices, sizeof sorted_misc_indices);
    sorted_misc_count = 0;

    while (1) {
        highest_remaining_sort_value = 0x80000000;

        for (i = 0; i < 256; ++i) {
            misc_item_id = misc_indices[i];
            if (misc_item_id != 0) {
                misc_sort_value = Menu_Framework_MiscSortValue(sort_stat_id, &item[misc_item_id - 1]);
                if (highest_remaining_sort_value < misc_sort_value) {
                    highest_remaining_sort_value = misc_sort_value;
                }
            }
        }

        if (highest_remaining_sort_value == 0x80000000) {
            break;
        }

        for (i = 0; i < 256; ++i) {
            misc_item_id = misc_indices[i];
            if ((misc_item_id != 0) && (Menu_Framework_MiscSortValue(sort_stat_id, &item[misc_item_id - 1]) == highest_remaining_sort_value)) {
                sorted_misc_indices[sorted_misc_count++] = misc_item_id;
                misc_indices[i] = 0;
            }
        }
    }
    Core_Memory_CopyHalfwordBlocks(misc_indices, &sorted_misc_indices, sizeof sorted_misc_indices);
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
void Menu_Container_SortEquipment(int category_id, int sort_stat_id)
{
    Items_EquipmentPartStats equipment_stats;
    int equipment_sort_value;
    int i;
    int highest_remaining_sort_value;
    int sorted_equipment_count;
    int equipment_id;
    int category_capacity;
    u_short* category_indices;

    sorted_equipment_count = 0;
    category_capacity = Menu_Container_containerItemCapacities[category_id];

    {
        u_short sorted_equipment_indices[category_capacity];
        category_indices = Menu_Container_CategoryIndices(category_id, Menu_Container_containerData);
        Core_Memory_ZeroWordsBackward(sorted_equipment_indices, category_capacity * 2);

        while (1) {
            highest_remaining_sort_value = 0x80000000;

            for (i = 0; i < category_capacity; ++i) {
                equipment_id = category_indices[i];
                if (equipment_id != 0) {
                    Menu_Container_CopyEquipmentStats(&equipment_stats, category_id, equipment_id - 1);
                    equipment_sort_value = Menu_Framework_EquipmentSortValue(sort_stat_id, &equipment_stats);
                    if (highest_remaining_sort_value < equipment_sort_value) {
                        highest_remaining_sort_value = equipment_sort_value;
                    }
                }
            }

            if (highest_remaining_sort_value == 0x80000000) {
                break;
            }

            for (i = 0; i < category_capacity; ++i) {
                equipment_id = category_indices[i];
                if (equipment_id != 0) {
                    Menu_Container_CopyEquipmentStats(&equipment_stats, category_id, equipment_id - 1);
                    if (Menu_Framework_EquipmentSortValue(sort_stat_id, &equipment_stats) == highest_remaining_sort_value) {
                        sorted_equipment_indices[sorted_equipment_count++] = equipment_id;
                        category_indices[i] = 0;
                    }
                }
            }
        }
        Core_Memory_CopyHalfwordBlocks(category_indices, sorted_equipment_indices, category_capacity * 2);
    }
}

// decomp: MENUD.PRG _displaySortMenu (src/MENU/MENUD.PRG/234.c:1601)
int Menu_Container_UpdateSortMenu(int category_selection)
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

    char* sort_row_text[48];
    int sort_row_types[24];
    int i;
    char* sort_stat_ids;

    if (category_selection != 0) {
        Menu_Container_sortCategory = category_selection - 1;
        Menu_Container_sortMenuStep = 0;
        return 0;
    }

    switch (Menu_Container_sortMenuStep) {
    case Menu_Container_SortMenuState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            sort_stat_ids = Menu_Container_kSortStatLists[Menu_Container_sortCategory];

            for (i = 0; i < Menu_Container_kSortStatCounts[Menu_Container_sortCategory]; ++i) {
                sort_row_text[i * 2] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [sort_stat_ids[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortMenu]];
                sort_row_text[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [sort_stat_ids[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortMenuDesc]];
                sort_row_types[i] = 0;
            }
            Menu_Framework_CreateActionRows(i, Menu_Container_sortCategory + 96, sort_row_text, sort_row_types);
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
int Menu_Container_UpdateDiscardMenu(int packed_discard_selection)
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
    int packed_discard_digits;
    int var_v0;
    int previous_discard_count;
    Menu_Framework_MenuRow* menuItem;
    int a0;

    if (packed_discard_selection != 0) {
        Menu_Container_discardType = packed_discard_selection >> 0x10;
        Menu_Container_discardMenuItemCategory = (packed_discard_selection >> 0xC) & 0xF;
        Menu_Container_discardMenuSelectedItem = packed_discard_selection - 1;
        Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(1));
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
        a0 = previous_discard_count = Menu_Container_discardCount;
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
        if ((a0 != Menu_Container_discardStep) || (previous_discard_count != Menu_Container_discardCount)) {
            Menu_PlayChangeSound();
        }
        Render_SetMenuFontStyle(5);
        packed_discard_digits = Core_Math_SplitDecimalDigit(Menu_Container_discardCount);
        Render_DrawVariableWidthGlyph(
            packed_discard_digits & 0xF, 0x42, 0x82, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 0xC));
        packed_discard_digits = Core_Math_SplitDecimalDigit(packed_discard_digits >> 4);
        Render_DrawVariableWidthGlyph(
            packed_discard_digits & 0xF, 0x3C, 0x82, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 0xC));
        Render_DrawVariableWidthGlyph(
            packed_discard_digits >> 4, 0x36, 0x82, (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) - 0xC));
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
int Menu_Container_CountWeaponGems(int weapon_index)
{
    int i;
    Items_InventoryWeapon* temp = &Menu_Container_containerData->items.weapons[weapon_index];
    int count = 0;

    for (i = 0; i < Menu_Container_containerData->items.grips[temp->grip - 1].gemSlots; ++i) {
        count += temp->gems[i] != 0;
    }
    return count;
}

// decomp: MENUD.PRG _getShieldsubItemCount (src/MENU/MENUD.PRG/234.c:1971)
int Menu_Container_CountShieldGems(int shield_index)
{
    int i;
    Items_InventoryShield* shield = &Menu_Container_containerData->items.shields[shield_index];
    int count = 0;

    for (i = 0; i < shield->base.gemSlots; ++i) {
        count += shield->gems[i] != 0;
    }
    return count;
}

// decomp: MENUD.PRG func_801066DC (src/MENU/MENUD.PRG/234.c:1983)
int Menu_Container_WouldExceedCapacity(int category_id, int additional_item_count)
{
    int destination_item_count = Menu_Container_CountItems(category_id, &Menu_Container_otherStorage->items);
    int projected_item_count =
        additional_item_count + (destination_item_count + Menu_Container_CountItems(category_id, &Menu_Framework_unusedInventoryStorage->stagedTransfer));

    if (Menu_Container_browsingGadgetBag != 0) {
        return Menu_Container_containerItemCapacities[category_id] < projected_item_count;
    }
    return Items_Inventory_inventoryItemCapacities[category_id] < projected_item_count;
}

// decomp: MENUD.PRG func_80106784 (src/MENU/MENUD.PRG/234.c:1995)
int Menu_Container_TransferCapacityError(int category_id, int item_index)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == 0) {
            break;
        }
    }

    if (category_id == 0) {
        i += 2 + Menu_Container_CountWeaponGems(item_index);
    }

    if (category_id == 3) {
        i += Menu_Container_CountShieldGems(item_index);
    }

    if (i >= 64) {
        return 1;
    }

    if (Menu_Container_WouldExceedCapacity(category_id, 1) != 0) {
        return 2;
    }

    if (category_id == 0) {
        if (Menu_Container_WouldExceedCapacity(1, 1) != 0) {
            return 2;
        }
        if (Menu_Container_WouldExceedCapacity(2, 1) != 0) {
            return 2;
        }
        if (Menu_Container_WouldExceedCapacity(5, Menu_Container_CountWeaponGems(item_index)) != 0) {
            return 2;
        }
    }

    if (category_id == 3 && Menu_Container_WouldExceedCapacity(5, Menu_Container_CountShieldGems(item_index)) != 0) {
        return 2;
    }

    return 0;
}

// decomp: MENUD.PRG func_801068BC (src/MENU/MENUD.PRG/234.c:2040)
void Menu_Container_UndoPendingTransfer(int pending_transfer_index)
{
    int staged_item_index;
    int category_id;
    int packed_transfer_item;

    packed_transfer_item = Menu_Framework_unusedInventoryStorage->itemsToTransfer[pending_transfer_index];
    category_id = packed_transfer_item >> 8;
    staged_item_index = (packed_transfer_item - 1) & 0xFF;
    Menu_Container_CopyItem(category_id | 0x10, &Menu_Container_containerData->items, staged_item_index,
        &Menu_Framework_unusedInventoryStorage->stagedTransfer);
    Menu_Container_ClearItem(category_id, staged_item_index, &Menu_Framework_unusedInventoryStorage->stagedTransfer);
}

// decomp: MENUD.PRG func_80106948 (src/MENU/MENUD.PRG/234.c:2054)
void Menu_Container_StageItemTransfer(int category_id, int item_index)
{
    Menu_Container_CopyItem(category_id | 0x10, &Menu_Framework_unusedInventoryStorage->stagedTransfer, item_index,
        &Menu_Container_containerData->items);
    Menu_Container_ClearItem(category_id, item_index, &Menu_Container_containerData->items);
}

// decomp: MENUD.PRG func_801069B0 (src/MENU/MENUD.PRG/234.c:2061)
int Menu_Container_FindPendingTransfer(int category_id, int item_index)
{
    int i;

    for (i = 0; i < 64; ++i) {
        if (Menu_Framework_unusedInventoryStorage->itemsToTransfer[i] == ((category_id << 8) | (item_index + 1))) {
            return i + 1;
        }
    }
    return 0;
}

// decomp: MENUD.PRG func_80106A04 (src/MENU/MENUD.PRG/234.c:2073)
void Menu_Container_AppendPendingTransfer(int category_id, int item_index)
{
    int i;
    u_short* pending_transfer_items;

    i = 0;
    pending_transfer_items = (u_short*)Menu_Framework_unusedInventoryStorage + 0x61D8;
    for (i = 0; i < 64; ++i) {
        if (pending_transfer_items[i] == 0) {
            pending_transfer_items[i] = ((category_id << 8) | (item_index + 1));
            return;
        }
    }
}

// decomp: MENUD.PRG func_80106A50 (src/MENU/MENUD.PRG/234.c:2088)
void Menu_Container_RebuildPendingTransfers(void)
{
    int j;
    int next_occupied_transfer_index;
    int i;
    int k;
    u_short* pending_transfer_items = Menu_Framework_unusedInventoryStorage->itemsToTransfer;

    for (i = 0; i < 64; ++i) {
        int source = pending_transfer_items[i];
        if ((source != 0)
            && (Menu_Container_ItemId((source >> 8), (source - 1) & 0xFF,
                    &Menu_Framework_unusedInventoryStorage->stagedTransfer)
                == 0)) {
            pending_transfer_items[i] = 0;
        }
    }

    i = 0;

    while (1) {
        if (pending_transfer_items[i] != 0) {
            ++i;
            if (i == 0x3F) {
                break;
            }
        } else {
            for (next_occupied_transfer_index = i + 1; next_occupied_transfer_index < 0x40; ++next_occupied_transfer_index) {
                if (pending_transfer_items[next_occupied_transfer_index] != 0) {
                    break;
                }
            }

            if (next_occupied_transfer_index == 0x40) {
                break;
            }

            for (j = next_occupied_transfer_index; j < 0x40; ++j) {
                pending_transfer_items[j + i - next_occupied_transfer_index] = pending_transfer_items[j];
            }

            for (j = j + (i - next_occupied_transfer_index); j < 64; ++j) {
                pending_transfer_items[j] = 0;
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

}  // extern "C"
