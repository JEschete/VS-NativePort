// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/WordCopy.h"
#include "Items/Inventory.h"
#include "Items/Loot.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Loot/LootInventory.h"
#include "Menu/Loot/LootScreen.h"
#include "State/Flags.h"
#include "State/StateFlags.h"

// decomp: MENUB.PRG _copyBladeToInventory (src/MENU/MENUB.PRG/260.c:712)
int Menu_Loot_StageBlade(Items_InventoryBlade* source, int weapon)
{
    int index = 1;
    Items_InventoryBlade* blade = Menu_Loot_inventory->blades;

    while (blade->id != 0) {
        ++blade;
        ++index;
        if (index == 16) {
            return 0;
        }
    }

    Core_Memory_CopyHalfwords(blade, source, sizeof *blade);

    blade->assembledWeaponIndex = weapon;
    blade->index = index;
    return index;
}

// decomp: MENUB.PRG _copyGripToInventory (src/MENU/MENUB.PRG/260.c:739)
int Menu_Loot_StageGrip(Items_InventoryGrip* source, int weaponIndex)
{
    int index = 1;
    Items_InventoryGrip* grip = Menu_Loot_inventory->grips;

    while (grip->id != 0) {
        ++grip;
        ++index;
        if (index == 16) {
            return 0;
        }
    }

    Core_Memory_CopyHalfwords(grip, source, sizeof *grip);

    grip->assembledWeaponIndex = weaponIndex;
    grip->index = index;
    return index;
}

// decomp: MENUB.PRG _copyGemToInventory (src/MENU/MENUB.PRG/260.c:766)
int Menu_Loot_StageGem(Items_InventoryGem* source, int item)
{
    int index = 1;
    Items_InventoryGem* gem = Menu_Loot_inventory->gems;

    while (gem->id != 0) {
        ++gem;
        ++index;
        if (index == 0x30) {
            return 0;
        }
    }

    Core_Memory_CopyHalfwords(gem, source, sizeof *gem);

    gem->setItemIndex = item;
    gem->index = index;
    return index;
}

// decomp: MENUB.PRG _copyWeaponToInventory (src/MENU/MENUB.PRG/260.c:792)
int Menu_Loot_StageWeapon(Items_LootedWeapon* arg0)
{
    int i;
    Items_InventoryWeapon* slot = Menu_Loot_inventory->weapons;
    int index = 1;

    while (slot->blade != 0) {
        ++slot;
        if (++index == 8) {
            return 0;
        }
    }

    Core_Memory_ZeroWordsBackward(slot, sizeof *slot);

    slot->index = index;
    slot->blade = Menu_Loot_StageBlade(&arg0->blade, index);
    slot->grip = Menu_Loot_StageGrip(&arg0->grip, index);

    for (i = 0; i < 3; i++) {
        if (arg0->gems[i].id != 0) {
            slot->gems[i] = Menu_Loot_StageGem(&arg0->gems[i], index);
        }
    }

    Core_Memory_CopyBytesBackward(slot->name, arg0->name, sizeof slot->name);

    return index;
}

// decomp: MENUB.PRG _copyArmorToInventory (src/MENU/MENUB.PRG/260.c:827)
int Menu_Loot_StageArmor(Items_InventoryArmor* source)
{
    int index = 1;
    Items_InventoryArmor* armor = Menu_Loot_inventory->armor;

    while (armor->id != 0) {
        ++armor;
        ++index;
        if (index == 16) {
            return 0;
        }
    }

    Core_Memory_CopyHalfwords(armor, source, sizeof *armor);

    armor->index = index;
    return index;
}

// decomp: MENUB.PRG _copyShieldToInventory (src/MENU/MENUB.PRG/260.c:852)
int Menu_Loot_StageShield(Items_LootedShield* arg0)
{
    int i;

    Items_InventoryShield* shield = Menu_Loot_inventory->shields;
    int index = 1;

    while (shield->base.id != 0) {
        ++shield;
        ++index;
        if (index == 8) {
            return 0;
        }
    }

    Core_Memory_ZeroWordsBackward(shield, sizeof *shield);
    Core_Memory_CopyHalfwords(&shield->base, &arg0->shield, sizeof shield->base);

    shield->base.limb = index;

    for (i = 0; i < 3; ++i) {
        if (arg0->gems[i].id != 0) {
            shield->gems[i] = Menu_Loot_StageGem(&arg0->gems[i], index | 0x80);
        }
    }

    return index;
}

// decomp: MENUB.PRG _copyMiscToInventory (src/MENU/MENUB.PRG/260.c:886)
int Menu_Loot_StageMiscItem(Items_LootedItem* arg0)
{
    int index = 1;
    Items_InventoryMisc* item = Menu_Loot_inventory->misc;

    while (item->id != 0) {
        ++index;
        ++item;
        if (index == 64) {
            return 0;
        }
    }

    item->id = arg0->itemId;
    item->count = arg0->count;
    return index;
}

// decomp: MENUB.PRG _updateLootList (src/MENU/MENUB.PRG/260.c:907)
void Menu_Loot_RefreshClaimableRows(void)
{
    int i;
    int newOffset;
    Menu_Loot_LootListItem* loot = Menu_Loot_lootList;

    for (i = 0; i < Menu_Loot_lootListCount; ++i, ++loot) {
        loot->state = Items_Inventory_CopyItem(loot->itemCategory, &Audio_Seq_unusedInventory,
                          loot->itemIndex, Menu_Loot_inventory)
                   != 0;
    }

    i = Menu_Loot_selectedLoot + Menu_Loot_lootListOffset;

    if (i >= Menu_Loot_lootListCount) {
        i = Menu_Loot_lootListCount - 1;
        Menu_Loot_lootListOffset = Menu_Loot_lootListCount - 8;
        Menu_Loot_selectedLoot = 7;
    }

    if (Menu_Loot_lootListCount < 9) {
        Menu_Loot_selectedLoot = i;
        Menu_Loot_lootListOffset = 0;
        return;
    }

    newOffset = Menu_Loot_lootListCount - 8;
    if (newOffset < Menu_Loot_lootListOffset) {
        Menu_Loot_lootListOffset = newOffset;
        Menu_Loot_selectedLoot = i - newOffset;
    }

    if ((Menu_Loot_selectedLoot == 7) && (i != (Menu_Loot_lootListCount - 1))) {
        Menu_Loot_selectedLoot -= 1;
        Menu_Loot_lootListOffset = Menu_Loot_lootListOffset + 1;
    }
}

// decomp: MENUB.PRG _addLootToList (src/MENU/MENUB.PRG/260.c:950)
void Menu_Loot_AppendLootRow(int itemCategory, int itemIndex)
{
    Menu_Loot_LootListItem* item = &Menu_Loot_lootList[Menu_Loot_lootListCount++];
    item->itemCategory = itemCategory;
    item->itemIndex = itemIndex - 1;
    item->opaque2 = 0;
}

// decomp: MENUB.PRG _itemIncludesItemsInCategory (src/MENU/MENUB.PRG/260.c:968)
int Menu_Loot_CountContainedCategory(int arg0, Menu_Loot_LootListItem* arg1)
{
    int i;
    Items_InventoryShield* shield;
    Items_InventoryWeapon* weapon;

    int category = arg1->itemCategory;
    int count = 0;

    switch (arg0) {
    case Items_ItemCategory_Blade:
    case Items_ItemCategory_Grip:
        count += category == Items_ItemCategory_Weapon;
        // Fallthrough

    case Items_ItemCategory_Weapon:
    case Items_ItemCategory_Shield:
    case Items_ItemCategory_Armor:
    case Items_ItemCategory_Misc:
        count += category == arg0;
        break;

    case Items_ItemCategory_Gem:
        count += category == Items_ItemCategory_Gem;

        if (category == Items_ItemCategory_Weapon) {
            weapon = &Menu_Loot_inventory->weapons[arg1->itemIndex];
            for (i = 0; i < Menu_Loot_inventory->grips[weapon->grip - 1].gemSlots; ++i) {
                count += weapon->gems[i] != 0;
            }
        }

        if (category == Items_ItemCategory_Shield) {
            shield = &Menu_Loot_inventory->shields[arg1->itemIndex];
            for (i = 0; i < shield->base.gemSlots; ++i) {
                count += shield->gems[i] != 0;
            }
        }

        break;
    }
    return count;
}

// decomp: MENUB.PRG _droppableUnderCurrentDifficulty (src/MENU/MENUB.PRG/260.c:1113)
int Menu_Loot_IsDropAllowed(int arg0)
{
    return arg0 & (Audio_Seq_unusedStateFlags.difficulty + 1);
}

// decomp: MENUB.PRG _applyLootList (src/MENU/MENUB.PRG/260.c:1124)
void Menu_Loot_StageDropRecord(Items_BattleLoot* arg0)
{
    int i;

    if (Menu_Loot_IsDropAllowed(arg0->weapon.difficultyFlags) != 0) {
        Menu_Loot_AppendLootRow(Items_ItemCategory_Weapon, Menu_Loot_StageWeapon(&arg0->weapon));
    }

    if (Menu_Loot_IsDropAllowed(arg0->blade.difficultyFlags) != 0) {
        Menu_Loot_AppendLootRow(Items_ItemCategory_Blade, Menu_Loot_StageBlade(&arg0->blade.blade, 0));
    }

    if (Menu_Loot_IsDropAllowed(arg0->grip.difficultyFlags) != 0) {
        Menu_Loot_AppendLootRow(Items_ItemCategory_Grip, Menu_Loot_StageGrip(&arg0->grip.grip, 0));
    }

    if (Menu_Loot_IsDropAllowed(arg0->shield.difficultyFlags) != 0) {
        Menu_Loot_AppendLootRow(Items_ItemCategory_Shield, Menu_Loot_StageShield(&arg0->shield));
    }

    for (i = 0; i < 2; ++i) {
        if (Menu_Loot_IsDropAllowed(arg0->armor[i].difficultyFlags) != 0) {
            Menu_Loot_AppendLootRow(
                Items_ItemCategory_Armor, Menu_Loot_StageArmor(&arg0->armor[i].armor));
        }
    }

    if (Menu_Loot_IsDropAllowed(arg0->accessory.difficultyFlags) != 0) {
        Menu_Loot_AppendLootRow(
            Items_ItemCategory_Armor, Menu_Loot_StageArmor(&arg0->accessory.accessory));
    }

    if (Menu_Loot_IsDropAllowed(arg0->gem.difficultyFlags) != 0) {
        Menu_Loot_AppendLootRow(Items_ItemCategory_Gem, Menu_Loot_StageGem(&arg0->gem.gem, 0));
    }

    for (i = 0; i < 3; ++i) {
        if ((Menu_Loot_IsDropAllowed(arg0->misc[i].difficultyFlags) != 0)
            && (arg0->misc[i].itemId != 0) && (arg0->misc[i].count != 0)) {
            Menu_Loot_AppendLootRow(Items_ItemCategory_Misc, Menu_Loot_StageMiscItem(&arg0->misc[i]));
        }
    }

    Menu_Loot_RefreshClaimableRows();
}

// decomp: MENUB.PRG _applyAllLootLists (src/MENU/MENUB.PRG/260.c:1174)
void Menu_Loot_BuildStagedInventory(Items_LootListNode* node)
{
    Core_Memory_ZeroWordsBackward(Menu_Loot_inventory, sizeof *Menu_Loot_inventory);

    if (node != NULL) {
        while (node != NULL) {
            Menu_Loot_StageDropRecord(&node->loot);
            node = node->next;
        }
    } else {
        Menu_Loot_StageDropRecord(Menu_Framework_lootEntry);
    }
}

// decomp: MENUB.PRG _setKeyFlag (src/MENU/MENUB.PRG/260.c:2108)
void Menu_Loot_RecordClaimedKey(int arg0)
{
    if (arg0 > 457) {
        State_SetScriptFlag(arg0 + 438, 1);
    }
}

