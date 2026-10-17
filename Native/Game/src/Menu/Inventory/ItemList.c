// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
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
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemDetails.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: vs_mainMenu_freezeTabArrows (vs_mainMenu_freezeTabArrows@MENU/MAINMENU.PRG)
char Menu_Inventory_freezeTabArrows = 0;

// decomp: vs_mainMenu_inventoryIndices (vs_mainMenu_inventoryIndices@MENU/MAINMENU.PRG)
u_char* Menu_Inventory_inventoryIndices[] = { Audio_Seq_unusedInventoryIndices.weapons,
    Audio_Seq_unusedInventoryIndices.blades, Audio_Seq_unusedInventoryIndices.grips,
    Audio_Seq_unusedInventoryIndices.shields, Audio_Seq_unusedInventoryIndices.armor,
    Audio_Seq_unusedInventoryIndices.gems, Audio_Seq_unusedInventoryIndices.misc };

// decomp: local static of _itemNavigation, at file scope so the module reset reaches it
static char Menu_Inventory_itemListAvailableActions[8];

// decomp: local static of _sortItems, at file scope so the module reset reaches it
static u_char Menu_Inventory_sortMenuBladeSortStats[] = { 0, 8, 9, 10, 24, 25, 26 };

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Inventory_discardMenuCursorBrightness = 0;

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Inventory_discardSteps[] = { 1, 10, 100 };

// decomp: local static of _discardMenu, at file scope so the module reset reaches it
static char Menu_Inventory_discardTemplate[] = "|>48|000/000\0";

// decomp: local static of _sortItems, at file scope so the module reset reaches it
static u_char Menu_Inventory_sortMenuEquipmentSortStats[] = { 8, 9, 10, 27, 28, 29, 30, 31, 32, 33, 34, 35,
        36, 37, 38, 39 };

// decomp: local static of _sortItems, at file scope so the module reset reaches it
static u_char Menu_Inventory_sortMenuGripSortStats[] = { 1, 4, 5, 6, 7, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39 };

// decomp: local static of _sortItems, at file scope so the module reset reaches it
static u_char Menu_Inventory_sortMenuMiscSortStats[] = { 0, 56 };

// decomp: local static of _sortItems, at file scope so the module reset reaches it
static u_char Menu_Inventory_sortMenuShieldSortStats[] = { 0, 1, 4, 5, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39 };

// decomp: local static of _sortItems, at file scope so the module reset reaches it
static u_char Menu_Inventory_sortMenuWeaponSortStats[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23 };

// decomp: MENU _sortWeapons (src/MENU/items1.h:731)
void Menu_Inventory_SortWeapons(int stat)
{
    Items_WeaponLoadout uiWeapon;
    char sortedIndices[8];
    int i;
    int sortedPos;

    Items_InventoryWeapon* weapon = Audio_Seq_unusedInventory.weapons;
    char* indices = Audio_Seq_unusedInventoryIndices.weapons;

    Core_Memory_ZeroWordsBackward(sortedIndices, 8);

    sortedPos = 0;

    while (1) {
        int index;
        int maxValue = -INT_MAX - 1;

        for (i = 0; i < 8; ++i) {
            index = indices[i];

            if (index != 0) {
                int value;

                Items_LoadInventoryWeaponStats(&uiWeapon, &weapon[index - 1]);

                value = Menu_Framework_WeaponSortValue(stat, &uiWeapon);

                if (maxValue < value) {
                    maxValue = value;
                }
            }
        }

        if (maxValue == -INT_MAX - 1) {
            break;
        }

        for (i = 0; i < 8; ++i) {
            index = indices[i];

            if (index != 0) {

                Items_LoadInventoryWeaponStats(&uiWeapon, &weapon[index - 1]);

                if (Menu_Framework_WeaponSortValue(stat, &uiWeapon) == maxValue) {
                    sortedIndices[sortedPos++] = index;
                    indices[i] = 0;
                }
            }
        }
    }

    Core_Memory_CopyHalfwordBlocks(indices, sortedIndices, 8);
}

// decomp: MENU _sortShields (src/MENU/items1.h:827)
void Menu_Inventory_SortShields(int stat)
{
    Items_ShieldLoadout uiShield;
    char sortedIndices[8];
    int i;
    int sortedPos;

    Items_InventoryShield* shield = &Audio_Seq_unusedInventory.shields[0];
    char* indices = Audio_Seq_unusedInventoryIndices.shields;

    Core_Memory_ZeroWordsBackward(sortedIndices, sizeof sortedIndices);

    sortedPos = 0;

    while (1) {
        int index;
        int maxValue = -INT_MAX - 1;

        for (i = 0; i < 8; ++i) {
            int value;
            index = indices[i];

            if (index != 0) {

                Items_LoadInventoryShieldStats(&uiShield, &shield[index - 1]);

                value = Menu_Framework_ShieldSortValue(stat, &uiShield);

                if (maxValue < value) {
                    maxValue = value;
                }
            }
        }

        if (maxValue == -INT_MAX - 1) {
            break;
        }

        for (i = 0; i < 8; ++i) {
            index = indices[i];

            if (index != 0) {

                Items_LoadInventoryShieldStats(&uiShield, &shield[index - 1]);

                if (Menu_Framework_ShieldSortValue(stat, &uiShield) == maxValue) {
                    sortedIndices[sortedPos++] = index;
                    indices[i] = 0;
                }
            }
        }
    }

    Core_Memory_CopyHalfwordBlocks(indices, sortedIndices, 8);
}

// decomp: MENU _sortMisc (src/MENU/items1.h:899)
void Menu_Inventory_SortMisc(int stat)
{
    char sortedIndices[64];
    int i;
    int sortedPos;

    Items_InventoryMisc* misc = Audio_Seq_unusedInventory.misc;
    char* indices = Audio_Seq_unusedInventoryIndices.misc;

    Core_Memory_ZeroWordsBackward(sortedIndices, sizeof sortedIndices);
    sortedPos = 0;

    while (1) {
        int index;
        int maxValue = -INT_MAX - 1;

        for (i = 0; i < 0x40; ++i) {
            index = indices[i];

            if (index != 0) {
                int value = Menu_Framework_MiscSortValue(stat, &misc[index - 1]);

                if (maxValue < value) {
                    maxValue = value;
                }
            }
        }

        if (maxValue == -INT_MAX - 1) {
            break;
        }

        for (i = 0; i < 0x40; ++i) {
            index = indices[i];

            if ((index != 0) && (Menu_Framework_MiscSortValue(stat, &misc[index - 1]) == maxValue)) {
                sortedIndices[sortedPos++] = index;
                indices[i] = 0;
            }
        }
    }

    Core_Memory_CopyHalfwordBlocks(indices, sortedIndices, sizeof sortedIndices);
}

// decomp: MENU _copyEquipment (src/MENU/items1.h:994)
void Menu_Inventory_CopyEquipmentStats(
    Items_EquipmentPartStats* equipment, int itemCategory, int itemIndex)
{
    switch (itemCategory) {
    case Items_ItemCategory_Blade:
        Items_ReadInventoryBladeStats(
            equipment, &Audio_Seq_unusedInventory.blades[itemIndex]);
        return;
    case Items_ItemCategory_Grip:
        Items_ReadInventoryGripStats(equipment, &Audio_Seq_unusedInventory.grips[itemIndex]);
        return;
    case Items_ItemCategory_Armor:
        Items_ReadInventoryArmorStats(equipment, &Audio_Seq_unusedInventory.armor[itemIndex]);
        return;
    case Items_ItemCategory_Gem:
        Items_ReadInventoryGemStats(equipment, &Audio_Seq_unusedInventory.gems[itemIndex]);
        return;
    }
}

// decomp: MENU _sortEquipment (src/MENU/items1.h:1017)
void Menu_Inventory_SortEquipment(int itemCategory, int stat)
{
    Items_EquipmentPartStats uiEquipment;
    int i;
    int sortedPos = 0;
    int capacity = Items_Inventory_inventoryItemCapacities[itemCategory];
    {
        char sortedIndices[capacity];
        char* indices = Menu_Inventory_inventoryIndices[itemCategory];

        Core_Memory_ZeroWordsBackward(sortedIndices, capacity);

        while (1) {
            int index;
            int maxValue = -INT_MAX - 1;

            for (i = 0; i < capacity; ++i) {
                index = indices[i];

                if (index != 0) {
                    int value;

                    Menu_Inventory_CopyEquipmentStats(&uiEquipment, itemCategory, index - 1);

                    value = Menu_Framework_EquipmentSortValue(stat, &uiEquipment);

                    if (maxValue < value) {
                        maxValue = value;
                    }
                }
            }

            if (maxValue == -INT_MAX - 1) {
                break;
            }

            for (i = 0; i < capacity; ++i) {
                index = indices[i];

                if (index != 0) {

                    Menu_Inventory_CopyEquipmentStats(&uiEquipment, itemCategory, index - 1);

                    if (Menu_Framework_EquipmentSortValue(stat, &uiEquipment) == maxValue) {
                        sortedIndices[sortedPos++] = index;
                        indices[i] = 0;
                    }
                }
            }
        }

        Core_Memory_CopyHalfwordBlocks(indices, sortedIndices, capacity);
    }
}

// decomp: MENU _sortItems (src/MENU/items1.h:1077)
int Menu_Inventory_UpdateSortMenu(int initItemCategory)
{
    enum Menu_Inventory_SortMenuState { Menu_Inventory_SortMenuState_Init, Menu_Inventory_SortMenuState_Sort };


    static u_char Menu_Inventory_sortMenuSortStatSizes[] = { sizeof Menu_Inventory_sortMenuWeaponSortStats, sizeof Menu_Inventory_sortMenuWeaponSortStats,
        sizeof Menu_Inventory_sortMenuBladeSortStats, sizeof Menu_Inventory_sortMenuGripSortStats, sizeof Menu_Inventory_sortMenuShieldSortStats,
        sizeof Menu_Inventory_sortMenuEquipmentSortStats, sizeof Menu_Inventory_sortMenuMiscSortStats };

    static u_char* Menu_Inventory_sortMenuSortStats[] = { Menu_Inventory_sortMenuWeaponSortStats, Menu_Inventory_sortMenuWeaponSortStats, Menu_Inventory_sortMenuBladeSortStats,
        Menu_Inventory_sortMenuGripSortStats, Menu_Inventory_sortMenuShieldSortStats, Menu_Inventory_sortMenuEquipmentSortStats, Menu_Inventory_sortMenuMiscSortStats };

    static u_char Menu_Inventory_sortMenuState;
    static u_char Menu_Inventory_sortMenuItemCategory;

    char* menuText[48];
    int rowTypes[24];
    int i;

    if (initItemCategory != 0) {
        Menu_Inventory_sortMenuItemCategory = initItemCategory - 1;
        Menu_Inventory_sortMenuState = Menu_Inventory_SortMenuState_Init;

        return 0;
    }

    switch (Menu_Inventory_sortMenuState) {
    case Menu_Inventory_SortMenuState_Init:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            u_char* categoryStats = Menu_Inventory_sortMenuSortStats[Menu_Inventory_sortMenuItemCategory];

            for (i = 0; i < Menu_Inventory_sortMenuSortStatSizes[Menu_Inventory_sortMenuItemCategory]; ++i) {
                menuText[i * 2] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [categoryStats[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortRange - 4]];
                menuText[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                        [categoryStats[i] * 2 + Text_Menu_ITEMHELP_BIN_Index_sortRangeDesc - 4]];
                rowTypes[i] = 0;
            }

            Menu_Framework_CreateActionRows(
                i, Menu_Inventory_sortMenuItemCategory + Menu_Framework_ActionsMenuAction_Sort, menuText, rowTypes);

            Menu_Inventory_sortMenuState = 1;
        }
        break;

    case Menu_Inventory_SortMenuState_Sort:
        Menu_Framework_UpdateActionMenu();

        i = Menu_Framework_ActionSelectionResult();

        if ((i + 1) != 0) {
            if (i >= 0) {
                Menu_PlaySelectSound();

                switch (Menu_Inventory_sortMenuItemCategory) {
                case Items_ItemCategory_Weapon:
                    Menu_Inventory_SortWeapons(Menu_Inventory_sortMenuWeaponSortStats[i]);
                    break;

                case Items_ItemCategory_Shield:
                    Menu_Inventory_SortShields(Menu_Inventory_sortMenuGripSortStats[i]);
                    break;

                case Items_ItemCategory_Misc:
                    Menu_Inventory_SortMisc(Menu_Inventory_sortMenuMiscSortStats[i]);
                    break;

                default:
                    Menu_Inventory_SortEquipment(Menu_Inventory_sortMenuItemCategory, Menu_Inventory_sortMenuSortStats[Menu_Inventory_sortMenuItemCategory][i]);
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

// decomp: MENU _discardMenu (src/MENU/items1.h:1185)
int Menu_Inventory_UpdateDiscardMenu(int params)
{
#pragma vsstring(start)

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


    static char Menu_Inventory_discardMenuState;
    static char Menu_Inventory_discardMenuItemCategory;
    static char Menu_Inventory_discardMenuSelectedItem;
    static char Menu_Inventory_discardType;
    static u_char Menu_Inventory_discardStep;
    static char Menu_Inventory_discardCount;
    static char Menu_Inventory_discardMenuMaxAmount;
    static char Menu_Inventory_discardMenuLayoutPadding[3] __attribute__((unused));

    char* menuText[4];
    int rowTypes[2];
    int i;
    Menu_Framework_MenuRow* menuItem;
    int nextStep;
    int nextCount;

    if (params != 0) {
        Menu_Inventory_discardType = params >> 16;
        Menu_Inventory_discardMenuItemCategory = params >> 8;
        Menu_Inventory_discardMenuSelectedItem = params & 0xFF;

        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Menu_Inventory_discardType + Text_Menu_ITEMHELP_BIN_Index_discardConfirm]]);

        Menu_Inventory_discardMenuState = Menu_Inventory_DiscardMenuState_Init;

        return 0;
    }

    switch (Menu_Inventory_discardMenuState) {
    case Menu_Inventory_DiscardMenuState_Init:
        if (Menu_textBoxes[7].flags.done) {
            if (Menu_Framework_AreMenuRowsIdle() != 0) {
                Menu_Inventory_discardMenuState = ((Menu_Inventory_discardType & 1) + Menu_Inventory_DiscardMenuState_DiscardOneInit);
            }
        }
        break;

    case Menu_Inventory_DiscardMenuState_DiscardOneInit:
        for (i = 0; i < 2; ++i) {

            menuText[i * 2] = (char*)&Menu_Framework_itemHelp
                [Menu_Framework_itemHelp[i + Text_Menu_ITEMHELP_BIN_Index_confirmYes]];
            menuText[i * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [Menu_Inventory_discardType + Text_Menu_ITEMHELP_BIN_Index_discardConfirm]];
            rowTypes[i] = 0;
        }

        Menu_Framework_CreateActionRows(2, Menu_Framework_ActionsMenuAction_Discard, menuText, rowTypes);

        Menu_Inventory_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardOne;
        break;

    case Menu_Inventory_DiscardMenuState_DiscardMultipleInit:
        Menu_Inventory_discardMenuMaxAmount = Audio_Seq_unusedInventory.misc[Menu_Inventory_discardMenuSelectedItem - 1].count;

        i = Core_Math_SplitDecimalDigit(Menu_Inventory_discardMenuMaxAmount);

        Menu_Inventory_discardTemplate[8] = i & 0xF;

        i = Core_Math_SplitDecimalDigit(i >> 4);

        Menu_Inventory_discardTemplate[7] = i & 0xF;
        Menu_Inventory_discardTemplate[6] = i >> 4;

        for (i = 2; i < 5; ++i) {
            Menu_Inventory_discardTemplate[i] = '0';
        }

        menuItem = Menu_Framework_SetMenuItem(34, -126, 130, 126, 0, Menu_Inventory_discardTemplate);
        menuItem->state = 5;
        menuItem->targetPosition0 = 0;

        Menu_Inventory_discardStep = 0;
        Menu_Inventory_discardCount = 0;
        Menu_Inventory_discardMenuState = Menu_Inventory_DiscardMenuState_ClearTemplate;
        break;

    case Menu_Inventory_DiscardMenuState_DiscardOne:
        Menu_Framework_UpdateActionMenu();
        i = Menu_Framework_ActionSelectionResult() + 1;

        if (i != 0) {
            if (i == 1) {
                Audio_PlaySfxCentered(0x7E, 28);
                Items_Inventory_ClearItemSlot(Menu_Inventory_discardMenuItemCategory, Menu_Inventory_discardMenuSelectedItem);
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
            Menu_Inventory_discardTemplate[i] = ' ';
        }

        Menu_Framework_SetMenuItem(34, 0, 130, 126, 0, Menu_Inventory_discardTemplate);

        Menu_Inventory_discardMenuState = Menu_Inventory_DiscardMenuState_DetermineDiscardAmount;
        // Fallthrough

    case Menu_Inventory_DiscardMenuState_DetermineDiscardAmount:
        if (Input_buttonsPressed.allButtons & (PADRup | PADRright | PADRdown)) {

            i = Core_Math_SplitDecimalDigit(Menu_Inventory_discardCount);

            Menu_Inventory_discardTemplate[4] = i & 0xF;

            i = Core_Math_SplitDecimalDigit(i >> 4);

            Menu_Inventory_discardTemplate[3] = i & 0xF;
            Menu_Inventory_discardTemplate[2] = i >> 4;

            Menu_Framework_SetMenuItem(34, 0, 130, 126, 0, Menu_Inventory_discardTemplate);

            if (Input_buttonsPressed.allButtons & (PADRup | PADRdown)) {

                Menu_PlayLeaveSound();

                if (Input_buttonsPressed.allButtons & PADRup) {
                    return -2;
                }

                return -1;
            }
            if (Menu_Inventory_discardCount == 0) {
                Menu_PlayLeaveSound();

                return -1;
            }

            Menu_PlaySelectSound();

            Menu_Inventory_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm;
            break;
        }

        i = 0;
        nextStep = nextCount = Menu_Inventory_discardCount;

        if (Input_buttonRepeat & PADLup) {
            i = 1;
        }

        if (Input_buttonRepeat & PADLdown) {
            --i;
        }

        if (i == 1) {
            nextStep = nextCount + Menu_Inventory_discardSteps[Menu_Inventory_discardStep];
        } else if (i == -1) {
            nextStep = nextCount - Menu_Inventory_discardSteps[Menu_Inventory_discardStep];
        }

        if (nextStep >= 0) {
            if (Menu_Inventory_discardMenuMaxAmount >= nextStep) {
                Menu_Inventory_discardCount = nextStep;
            }
        }

        i = 0;
        nextStep = Menu_Inventory_discardStep;

        if (Input_buttonRepeat & PADLright) {
            --i;
        }

        if (Input_buttonRepeat & PADLleft) {
            ++i;
        }

        if (i == 1) {
            if ((nextStep & 0xFF) == 2) {
                if (Menu_Inventory_discardCount != Menu_Inventory_discardMenuMaxAmount) {
                    Menu_Inventory_discardCount = Menu_Inventory_discardMenuMaxAmount;
                }
            } else {
                ++Menu_Inventory_discardStep;
            }
        } else if (i == -1) {
            if (!(nextStep & 0xFF)) {
                if (Menu_Inventory_discardCount != 0) {
                    Menu_Inventory_discardCount = 0;
                }
            } else {
                --Menu_Inventory_discardStep;
            }
        }

        if ((nextStep != Menu_Inventory_discardStep) || (nextCount != Menu_Inventory_discardCount)) {
            Menu_PlayChangeSound();
        }

        Render_SetMenuFontStyle(5);

        i = Core_Math_SplitDecimalDigit(Menu_Inventory_discardCount);
        Render_DrawVariableWidthGlyph(i & 0xF, 66, 130, Render_frameScratch.overlayOrderingTable - 12);
        i = Core_Math_SplitDecimalDigit(i >> 4);
        Render_DrawVariableWidthGlyph(i & 0xF, 60, 130, Render_frameScratch.overlayOrderingTable - 12);
        Render_DrawVariableWidthGlyph(i >> 4, 54, 130, Render_frameScratch.overlayOrderingTable - 12);

        Menu_Inventory_discardMenuCursorBrightness = Menu_Framework_DrawCursor(
            Menu_Inventory_discardMenuCursorBrightness, (54 - (Menu_Inventory_discardStep * 6)) | (114 << 16));

        break;

    case Menu_Inventory_DiscardMenuState_DiscardMultipleConfirm:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {

            char* menuText[4];
            int rowTypes[2];

            for (i = 0; i < 2; ++i) {
                menuText[i * 2] = (char*)&Menu_Framework_itemHelp
                    [Menu_Framework_itemHelp[i + Text_Menu_ITEMHELP_BIN_Index_confirmYes]];
                menuText[i * 2 + 1] =
                    (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardConfirm];
                rowTypes[i] = 0;
            }

            Menu_Framework_CreateActionRows(2, Menu_Framework_ActionsMenuAction_Discard, menuText, rowTypes);
            Menu_Inventory_discardMenuState = Menu_Inventory_DiscardMenuState_DiscardMultiple;
        }

        break;

    case Menu_Inventory_DiscardMenuState_DiscardMultiple:
        Menu_Framework_UpdateActionMenu();
        i = Menu_Framework_ActionSelectionResult() + 1;

        if (i != 0) {
            if (i == 1) {
                Audio_PlaySfxCentered(0x7E, 0x1C);

                if (Audio_Seq_unusedInventory.misc[Menu_Inventory_discardMenuSelectedItem - 1].count == Menu_Inventory_discardCount) {
                    Items_Inventory_ClearItemSlot(Menu_Inventory_discardMenuItemCategory, Menu_Inventory_discardMenuSelectedItem);
                } else {
                    Audio_Seq_unusedInventory.misc[Menu_Inventory_discardMenuSelectedItem - 1].count =
                        Audio_Seq_unusedInventory.misc[Menu_Inventory_discardMenuSelectedItem - 1].count - Menu_Inventory_discardCount;
                }
            } else {
                Menu_PlayLeaveSound();
            }

            return i;
        }

        break;
    }

    return 0;

#pragma vsstring(end)
}

// decomp: MENU _getParentWeaponIndex (src/MENU/items2.h:11)
int Menu_Inventory_ParentWeaponIndex(int itemCategory, int index)
{
    int parentIndex = 0;

    if (itemCategory == Items_ItemCategory_Blade) {
        parentIndex = Audio_Seq_unusedInventory.blades[index].assembledWeaponIndex;
    }

    if (itemCategory == Items_ItemCategory_Grip) {
        parentIndex = Audio_Seq_unusedInventory.grips[index].assembledWeaponIndex;
    }

    if (itemCategory == Items_ItemCategory_Gem) {
        parentIndex = Audio_Seq_unusedInventory.gems[index].setItemIndex;
        if (parentIndex & Items_GemTarget_Shield) {
            parentIndex = 0;
        }
    }

    return parentIndex;
}

// decomp: MENU _getParentShieldIndex (src/MENU/items2.h:38)
int Menu_Inventory_ParentShieldIndex(int itemCategory, int itemIndex)
{
    u_short index = 0;

    if (itemCategory == Items_ItemCategory_Gem) {

        index = Audio_Seq_unusedInventory.gems[itemIndex].setItemIndex;

        if (!(index & Items_GemTarget_Shield)) {
            index = 0;
        }
    }

    return index & 0x7F;
}

// decomp: MENU _populateItemRows (src/MENU/items2.h:60)
int Menu_Inventory_BuildItemRows(
    int itemCategory, char* menuText[], int rowTypes[], char* textBuf)
{
    int i;
    char* indices = Menu_Inventory_inventoryIndices[itemCategory];
    int count = 0;

    for (i = 0; i < Items_Inventory_inventoryItemCapacities[itemCategory];
        ++count, ++i, textBuf += 96) {

        int parentItem;
        int rowType;
        int index = indices[i];

        if (index == 0) {
            break;
        }

        index = indices[i] - 1;
        rowType = 0;

        switch (itemCategory) {
        case Items_ItemCategory_Weapon:
            Menu_Framework_FormatInventoryWeaponRow(&Audio_Seq_unusedInventory.weapons[index],
                &menuText[count * 2], &rowTypes[i], textBuf);

            if (Audio_Seq_unusedInventory.weapons[index].isEquipped != 0) {
                rowType = 0xCA00;
            }
            break;

        case Items_ItemCategory_Blade:
            Menu_Framework_FormatBladeRow(&Audio_Seq_unusedInventory.blades[index],
                &menuText[count * 2], &rowTypes[i], textBuf);
            break;

        case Items_ItemCategory_Grip:
            Menu_Framework_FormatGripRow(&Audio_Seq_unusedInventory.grips[index], &menuText[count * 2],
                &rowTypes[i], textBuf);
            break;

        case Items_ItemCategory_Shield:
            Menu_Framework_FormatInventoryShieldRow(&Audio_Seq_unusedInventory.shields[index],
                &menuText[count * 2], &rowTypes[i], textBuf);

            if (Audio_Seq_unusedInventory.shields[index].isEquipped != 0) {
                rowType = 0xCA00;
            }
            break;

        case Items_ItemCategory_Armor:
            Menu_Framework_FormatInventoryArmorRow(&Audio_Seq_unusedInventory.armor[index],
                &menuText[count * 2], &rowTypes[i], textBuf);

            if (Audio_Seq_unusedInventory.armor[index].limb != 0) {
                rowType = 0xCA00;
            }
            break;

        case Items_ItemCategory_Gem:
            Menu_Framework_FormatGemRow(&Audio_Seq_unusedInventory.gems[index], &menuText[count * 2],
                &rowTypes[i], textBuf);
            break;

        case Items_ItemCategory_Misc: {
            u_int actionId;
            Menu_Framework_FormatMiscItemRow(&Audio_Seq_unusedInventory.misc[index], &menuText[count * 2],
                &rowTypes[i], textBuf);
            actionId =
                Menu_Inventory_miscItemToActionMap[Audio_Seq_unusedInventory.misc[index].id - 323];

            if ((actionId - 106) < 28) {
                int j;
                for (j = 0; j < 3; ++j) {
                    if (Battle_actionDefinitions[actionId].unlocked) {
                        ++actionId;
                    }
                }
            }

            Menu_Inventory_unusedMiscActions[count] = actionId;
            break;
        }
        }

        parentItem = Menu_Inventory_ParentWeaponIndex(itemCategory, index);

        if (parentItem != 0) {
            rowType = 0xCA00;
            if (Audio_Seq_unusedInventory.weapons[parentItem - 1].isEquipped == 0) {
                rowType = 0xCC00;
            }
        }

        parentItem = Menu_Inventory_ParentShieldIndex(itemCategory, index);

        if (parentItem != 0) {
            rowType = 0xCA00;
            if (Audio_Seq_unusedInventory.shields[parentItem - 1].isEquipped == 0) {
                rowType = 0xCC00;
            }
        }

        rowTypes[i] |= rowType;
    }

    return count;
}

// decomp: MENU _restoreItemMenu (src/MENU/items2.h:175)
void Menu_Inventory_RestoreItemRows(int itemCategory)
{
    int i;

    while (Menu_Framework_itemsListRow < 16) {
        Menu_Framework_UpdateRowSelection();
    }

    for (i = 20; i < 40; ++i) {
        Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(i);

        if (menuItem->state == 2) {
            menuItem->state = 1;
            menuItem->x = menuItem->targetPosition0;
        }

        menuItem->selected =
            (i ^ (Menu_Framework_menuMemory.cursorMemories[(itemCategory + 30) * 2] + 20)) == 0;
    }
}

// decomp: MENU _itemNavigation (src/MENU/items2.h:201)
int Menu_Inventory_UpdateItemList(int initialize)
{
    enum Menu_Inventory_ItemListState {
        Menu_Inventory_ItemListState_None,
        Menu_Inventory_ItemListState_Init,
        Menu_Inventory_ItemListState_WaitInit = 5,
        Menu_Inventory_ItemListState_ViewDetails = 7,
        Menu_Inventory_ItemListState_PopulateActions,
        Menu_Inventory_ItemListState_ProcessAction,
        Menu_Inventory_ItemListState_Search,
        Menu_Inventory_ItemListState_Discard,
        Menu_Inventory_ItemListState_Sort,
    };

    enum Menu_Inventory_ItemAction {
        Menu_Inventory_ItemAction_UseItem = 1,
        Menu_Inventory_ItemAction_SearchItem,
        Menu_Inventory_ItemAction_DiscardItem,
        Menu_Inventory_ItemAction_DiscardAllItems,
        Menu_Inventory_ItemAction_SortItems,
    };

    static char Menu_Inventory_itemListState;
    static char Menu_Inventory_itemListNoItems;
    static char Menu_Inventory_itemListIgnoreCursorMemory;
    static char Menu_Inventory_itemListLayoutPadding __attribute__((unused));
    static int Menu_Inventory_itemListSelectedRow;

    char** menuText;
    int* rowTypes;
    char** sp310;
    int rowCount;
    char* indices;
    int i;
    u_char itemCategory;
    int var_a1_2;
    int row;
    int index;

loop_1:
    if (initialize != 0) {
        Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(3);

        if (menuItem->x > 180) {
            if (menuItem->x > 204) {
                menuItem->x -= 24;
            }
            menuItem->state = 2;
            menuItem->targetPosition0 = 180;
        }

        Menu_Inventory_SetPanelVisibility(1);

        Menu_Inventory_secondaryTitleAnimationStep = 0;
        Menu_Inventory_itemListIgnoreCursorMemory = initialize & 1;

        if (Menu_Inventory_itemListIgnoreCursorMemory) {
            if (Input_settings.cursorMemory == 0) {
                memset(&Menu_Framework_menuMemory.opaque3A, 0, sizeof Menu_Framework_menuMemory.opaque3A);
            }
        }

        Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Init;
        return 0;
    }

    itemCategory = (Menu_Framework_menuMemory.opaque3A.currentCategory - 1) & 7;
    indices = Menu_Inventory_inventoryIndices[itemCategory];

    switch (Menu_Inventory_itemListState) {
    case Menu_Inventory_ItemListState_None:
    case Menu_Inventory_ItemListState_Init: {
        void* textBuf = Core_Memory_AllocateFromHighEnd(72 * 96);
        rowTypes = textBuf + 0x1800;
        menuText = textBuf + 0x1900;

        Menu_Inventory_freezeTabArrows = 1;

        for (i = 0; i < 7; ++i) {
            Items_Inventory_RebuildDisplayIndices(i);
        }

        if (itemCategory == Items_ItemCategory_Manage) {
            for (i = 0; i < 4; ++i) {
                menuText[i] = (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_equip + i]];
                rowTypes[i] = Audio_Seq_unusedStateFlags.introState < 3;
            }

            if (Battle_characterState->stats->statuses & 0x80000) {
                for (i = 0; i < 2; ++i) {
                    menuText[i * 2 + 1] =
                        (char*)&Menu_Inventory_menuText[Text_Menu3_menuText_Offset_equipmentTarnished];
                    rowTypes[i] = 1;
                }
            }
            rowCount = 2;
        } else {
            rowCount = Menu_Inventory_BuildItemRows(itemCategory, menuText, rowTypes, textBuf);
        }

        if (Menu_Framework_actionRestrictions & 7) {
            for (i = 0; i < 64; ++i) {
                rowTypes[i] |= 1;
            }
        }

        Menu_Inventory_itemListNoItems = rowCount == 0;

        if (Menu_Inventory_itemListNoItems) {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noItemsInCategory]);
        } else {

            i = Input_settings.cursorMemory;

            if (Menu_Inventory_itemListIgnoreCursorMemory) {
                Menu_Inventory_itemListIgnoreCursorMemory = 0;
            } else {
                Input_settings.cursorMemory = 1;
            }

            var_a1_2 = itemCategory + 30;

            if (itemCategory == Items_ItemCategory_Manage) {
                var_a1_2 |= 0x200;
            } else {
                var_a1_2 |= 0x19200;
            }

            Menu_Framework_CreateMenuRows(rowCount, var_a1_2, menuText, rowTypes);

            if (Menu_Inventory_itemListState == Menu_Inventory_ItemListState_None) {
                Menu_Inventory_RestoreItemRows(itemCategory);
            }

            Input_settings.cursorMemory = i;
        }

        Core_Memory_FreeFromHighEnd(textBuf);

        Menu_Inventory_itemListState = 2;
        break;
    }

    case 2:
        Menu_Inventory_statusCommandState = 1;

        if (Menu_Inventory_itemListNoItems) {
            if (Input_buttonsPressed.allButtons & PADRup) {
                Menu_PlayLeaveSound();
                return -2;
            }

            if (Input_buttonsPressed.allButtons & PADRdown) {
                Menu_PlayLeaveSound();
                return -1;
            }

            if (Input_buttonsPressed.allButtons & (PADRright | PADRleft)) {
                Menu_PlayInvalidSound();
            }
        } else {

            if (!(Menu_Framework_actionRestrictions & 7)) {
                u_int rowCount = itemCategory;
                rowCount = rowCount < Items_ItemCategory_Misc;
                Menu_Inventory_statusCommandState = rowCount + 1;
            }

            Menu_Inventory_itemListSelectedRow = Menu_Framework_GetSelectedRow() + 1;

#define offset 1

            if (Menu_Inventory_itemListSelectedRow == 0) {
                if (Input_buttonsPressed.allButtons & PADRleft) {
                    if ((Menu_Inventory_statusCommandState >= 2) && Menu_Framework_AreMenuRowsIdle()) {

                        Menu_PlaySelectSound();

                        row = Menu_Framework_CloseRowsAndRememberCursor();

                        Menu_Framework_MenuItem(row >> 8)->outsetIcon = 0;
                        Menu_Inventory_UpdateItemDetails(itemCategory | ((row + offset) << 4));
#undef offset
                        Menu_Inventory_secondaryTitleEnabled = 0;
                        Menu_Inventory_itemListState = 7;
                        break;
                    }

                    Menu_PlayInvalidSound();
                }
            } else {
                void* textBuf;
                int* rowTypes;
                int count;

                if ((Menu_Inventory_itemListSelectedRow <= 0) || (itemCategory == Items_ItemCategory_Manage)) {
                    return Menu_Inventory_itemListSelectedRow;
                }

                textBuf = Core_Memory_AllocateFromHighEnd(72 * 96);
                sp310 = textBuf + 0x1900;
                rowTypes = textBuf + 0x1800;

                count = Menu_Inventory_BuildItemRows(itemCategory, sp310, rowTypes, textBuf);

                rowTypes[Menu_Inventory_itemListSelectedRow - 1] |= 8;
                i = Input_settings.cursorMemory;
                Input_settings.cursorMemory = 1;

                Menu_Framework_CreateMenuRows(
                    count, (itemCategory + 30) | 0x19200, sp310, rowTypes);

                Input_settings.cursorMemory = i;

                Menu_Inventory_RestoreItemRows(itemCategory);

                Menu_Inventory_statusCommandState = 1;
                Menu_Inventory_freezeTabArrows = 0;

                Core_Memory_FreeFromHighEnd(textBuf);

                Menu_Inventory_itemListState = 6;
                break;
            }
        }
        // Fallthrough

    case 3: {
        int moveBack;

        i = Input_buttonsState & (PADL1 | PADLleft);
        rowCount = Input_buttonsState & (PADR1 | PADLright);
        moveBack = i != 0;

        if (rowCount != 0) {
            row = moveBack ^ 1;
        } else {
            row = moveBack & 1;
        }

        if (row != 0) {

            var_a1_2 = PADL1 | PADLleft;

            if (i == (PADL1 | PADLleft)) {
                var_a1_2 = PADLleft;
            }

            if (Input_buttonRepeat & var_a1_2) {
                itemCategory = (itemCategory - 1) & 7;
            }

            var_a1_2 = (PADR1 | PADLright);

            if (rowCount == (PADR1 | PADLright)) {
                var_a1_2 = PADLright;
            }

            if (Input_buttonRepeat & var_a1_2) {
                itemCategory = (itemCategory + 1) & 7;
            }

            if (itemCategory != ((Menu_Framework_menuMemory.opaque3A.currentCategory - 1) & 7)) {

                Menu_PlayChangeSound();

                Menu_Framework_menuMemory.opaque3A.currentCategory = (itemCategory + 1) & 7;

                if (Menu_Inventory_itemListState == 2) {
                    if (Menu_Inventory_itemListNoItems == 0) {
                        Menu_Framework_CloseRowsAndRememberCursor();
                        Menu_Framework_ClearMenuExcept(3);
                    }
                    Menu_Inventory_itemListState = 3;
                }
            }
        }

        if ((Menu_Inventory_itemListState == 3) && Menu_Framework_AreMenuRowsIdle()) {
            if ((Input_buttonsState & (PADL1 | PADR1 | PADLleft | PADLright)) && row) {
                break;
            }

            Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Init;

            initialize = 0;
            goto loop_1;
        }
        break;
    }

    case 4: {
        Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(3);

        if (menuItem->x > 180) {
            if (menuItem->x > 204) {
                menuItem->x -= 24;
            }

            menuItem->state = 2;
            menuItem->targetPosition0 = 180;
        }

        Menu_Inventory_itemListState = Menu_Inventory_ItemListState_WaitInit;
    }
        /* fallthrough */

    case Menu_Inventory_ItemListState_WaitInit:
        if (Menu_Framework_AreMenuRowsIdle()) {
            Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Init;
        }
        break;

    case 6:
        if (Input_buttonsPressed.allButtons & PADRleft) {
            Menu_PlayInvalidSound();
        }

        i = Menu_Framework_GetSelectedRow() + 1;

        if (i == 0) {
            break;
        }

        rowCount = Menu_Inventory_itemListSelectedRow;

        if (rowCount == i) {
            Menu_Inventory_itemListState = Menu_Inventory_ItemListState_PopulateActions;
            break;
        }

        if (i > 0) {

            row = indices[i - 1];
            var_a1_2 = indices[rowCount - 1];

            if ((itemCategory == Items_ItemCategory_Misc)
                && (Audio_Seq_unusedInventory.misc[row - 1].id
                    == Audio_Seq_unusedInventory.misc[var_a1_2 - 1].id)) {

                Audio_Seq_unusedInventory.misc[var_a1_2 - 1].count +=
                    Audio_Seq_unusedInventory.misc[row - 1].count;

                if (Audio_Seq_unusedInventory.misc[var_a1_2 - 1].count > 100) {
                    Audio_Seq_unusedInventory.misc[row - 1].count =
                        Audio_Seq_unusedInventory.misc[var_a1_2 - 1].count - 100;
                    Audio_Seq_unusedInventory.misc[var_a1_2 - 1].count = 100;
                } else {
                    Items_Inventory_ClearItemSlot(Items_ItemCategory_Misc, row);
                }
            } else {
                indices[i - 1] = var_a1_2;
                indices[rowCount - 1] = row;
            }

            Menu_Framework_ClearMenuExcept(3);

            Menu_Inventory_itemListState = 4;
            break;
        }

        if (i != -1) {
            return -2;
        }

        Menu_Inventory_itemListState = Menu_Inventory_ItemListState_None;
        break;

    case Menu_Inventory_ItemListState_ViewDetails:
        i = Menu_Inventory_UpdateItemDetails(0);

        if (i == 0) {
            break;
        }

        if (i == -2) {
            return -2;
        }

        Menu_Inventory_secondaryTitleEnabled = 1;

        {
            Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(3);

            if (menuItem->x > 180) {
                if (menuItem->x > 204) {
                    menuItem->x -= 24;
                }
                menuItem->state = 2;
                menuItem->targetPosition0 = 180;
            }
        }

        Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Init;
        break;

    case Menu_Inventory_ItemListState_PopulateActions: {
        char* sp310[12];
        int rowType;

        rowCount = 0;
        index = indices[Menu_Inventory_itemListSelectedRow - 1] - 1;

        Core_Memory_ZeroWordsBackward(Menu_Framework_rowTypeBuf, sizeof(int) * 6);

        if (itemCategory == Items_ItemCategory_Misc) {

            sp310[0] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_useItem];
            sp310[1] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_useItemDesc];

            if (Audio_Seq_unusedInventory.misc[index].id > 457) {
                *Menu_Framework_rowTypeBuf = 1;
                sp310[1] = (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_cannotUse];
            } else if (Battle_ActorActionRestrictionFlags(0, Menu_Inventory_unusedMiscActions[Menu_Inventory_itemListSelectedRow - 1]) & 2) {
                *Menu_Framework_rowTypeBuf = 1;
                sp310[1] = (char*)&Menu_Inventory_menuText[Text_Menu3_menuText_Offset_insufficientMp];
            }

            ++rowCount;
            Menu_Inventory_itemListAvailableActions[rowCount] = 1;
        }

        if (((char)(itemCategory - 1) < 2) || (itemCategory == Items_ItemCategory_Gem)) {

            int var_s2 = itemCategory & 0xFF;
            sp310[rowCount * 2] =
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_searchItem];
            sp310[rowCount * 2 + 1] =
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_searchItemDesc];

            if ((Menu_Inventory_ParentWeaponIndex(var_s2, index) == 0)
                && (Menu_Inventory_ParentShieldIndex(var_s2, index) == 0)) {
                sp310[rowCount * 2 + 1] =
                    (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_invalidSearch];
                Menu_Framework_rowTypeBuf[rowCount] = 1;
            }

            ++rowCount;
            Menu_Inventory_itemListAvailableActions[rowCount] = 2;
        }

        rowType = 0;
        sp310[rowCount * 2] =
            (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardItem];
        sp310[rowCount * 2 + 1] =
            (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardItemDesc];

        switch (itemCategory) {
        case Items_ItemCategory_Weapon:
            rowType = Audio_Seq_unusedInventory.weapons[index].isEquipped != 0;
            break;
        case Items_ItemCategory_Shield:
            rowType = Audio_Seq_unusedInventory.shields[index].isEquipped != 0;
            break;
        case Items_ItemCategory_Armor:
            rowType = Audio_Seq_unusedInventory.armor[index].limb != 0;
            break;
        case Items_ItemCategory_Misc:
            if (Audio_Seq_unusedInventory.misc[index].id > 457) {
                rowType = 3;
            }
            break;
        }

        var_a1_2 = Menu_Inventory_ParentWeaponIndex(itemCategory, index);

        if (var_a1_2 != 0) {
            rowType = 1;
            if (Audio_Seq_unusedInventory.weapons[var_a1_2 - 1].isEquipped == 0) {
                rowType = 2;
            }
        }

        var_a1_2 = Menu_Inventory_ParentShieldIndex(itemCategory, index);

        if (var_a1_2 != 0) {
            rowType = 1;
            if (Audio_Seq_unusedInventory.shields[var_a1_2 - 1].isEquipped == 0) {
                rowType = 2;
            }
        }

        if (rowType != 0) {
            sp310[rowCount * 2 + 1] = (char*)&Menu_Framework_itemHelp[Menu_Framework_itemHelp
                    [rowType + Text_Menu_ITEMHELP_BIN_Index_cannotDiscardEquipped - 1]];
            rowType = 1;
        }

        Menu_Framework_rowTypeBuf[rowCount] = rowType;
        ++rowCount;
        Menu_Inventory_itemListAvailableActions[rowCount] = Menu_Inventory_ItemAction_DiscardItem;

        if (itemCategory == Items_ItemCategory_Misc) {

            sp310[rowCount * 2] =
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardAll];
            sp310[rowCount * 2 + 1] =
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_discardAllDesc];

            if (Audio_Seq_unusedInventory.misc[index].id > 457) {
                sp310[rowCount * 2 + 1] =
                    (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_cannotDiscard];
                Menu_Framework_rowTypeBuf[rowCount] = 1;
            }

            ++rowCount;
            Menu_Inventory_itemListAvailableActions[rowCount] = Menu_Inventory_ItemAction_DiscardAllItems;
        }

        sp310[rowCount * 2] =
            (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_sortItem];
        sp310[rowCount * 2 + 1] =
            (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_sortItemDesc];

        ++rowCount;
        Menu_Inventory_itemListAvailableActions[rowCount] = Menu_Inventory_ItemAction_SortItems;

        Menu_Framework_CreateActionRows(
            rowCount, itemCategory + 38, sp310, Menu_Framework_rowTypeBuf);

        Menu_Inventory_itemListState = Menu_Inventory_ItemListState_ProcessAction;
        break;
    }

    case Menu_Inventory_ItemListState_ProcessAction:
        Menu_Framework_UpdateInformationVisibility(
            Menu_Framework_HighlightedActionRow(), Menu_Framework_actionMenuState);

        Menu_Framework_UpdateActionMenu();
        index = Menu_Framework_ActionSelectionResult() + 1;

        if (index != 0) {

            Menu_Framework_FlyOutRowsLeftExcept(0x28);

            if (index > 0) {

                Menu_PlaySelectSound();


                switch (Menu_Inventory_itemListAvailableActions[index]) {
                case Menu_Inventory_ItemAction_UseItem:
                    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

                    Items_miscItemInvoked = indices[Menu_Inventory_itemListSelectedRow - 1] - 1;
                    Menu_Framework_actionSelection.executeAbility.halves[0] =
                        (short)Menu_Inventory_unusedMiscActions[Menu_Inventory_itemListSelectedRow - 1];
                    Menu_Framework_actionSelection.executeAbility.halves[1] =
                        (short)Audio_Seq_unusedInventory.misc[Items_miscItemInvoked].id;

                    return 3;

                case Menu_Inventory_ItemAction_SearchItem:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Framework_ClearMenuExcept(3);

                    Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Search;
                    break;

                case Menu_Inventory_ItemAction_DiscardItem:
                    rowCount = indices[Menu_Inventory_itemListSelectedRow - 1] | (itemCategory << 8);

                    if ((itemCategory == Items_ItemCategory_Misc)
                        && (Audio_Seq_unusedInventory.misc[indices[Menu_Inventory_itemListSelectedRow - 1] - 1].count
                            >= 2)) {
                        rowCount |= 0x10000;
                    }

                    Menu_Inventory_UpdateDiscardMenu(rowCount);

                    Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Discard;
                    break;

                case Menu_Inventory_ItemAction_DiscardAllItems:
                    Menu_Inventory_UpdateDiscardMenu(
                        indices[Menu_Inventory_itemListSelectedRow - 1] | (itemCategory << 8) | 0x20000);

                    Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Discard;
                    break;

                case Menu_Inventory_ItemAction_SortItems:
                    Menu_Framework_InitializeInformationBox();
                    Menu_Inventory_UpdateSortMenu(itemCategory + 1);

                    Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Sort;
                    break;
                }
            } else {

                Menu_PlayLeaveSound();


                if (index == -2) {
                    Menu_Framework_ClearMenuExcept(3);

                    return -2;
                }

                Menu_Framework_InitializeInformationBox();

                Menu_Inventory_itemListState = Menu_Inventory_ItemListState_None;
            }
        }

#undef index

        break;

    case Menu_Inventory_ItemListState_Search:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        i = Menu_Inventory_ParentWeaponIndex(itemCategory, indices[Menu_Inventory_itemListSelectedRow - 1] - 1);

        if (i == 0) {
            i = Menu_Inventory_ParentShieldIndex(itemCategory, indices[Menu_Inventory_itemListSelectedRow - 1] - 1);
            itemCategory = 3;
        } else {
            itemCategory = 0;
        }

        Menu_Framework_menuMemory.opaque3A.currentCategory = (itemCategory + 1) & 7;
        (&Menu_Framework_menuMemory.opaque3A)->categoryCursors[itemCategory].word =
            Items_Inventory_FindDisplayPosition(itemCategory, i - 1) - 1;

        Menu_Inventory_itemListState = Menu_Inventory_ItemListState_Init;
        break;

    case Menu_Inventory_ItemListState_Discard:
        i = Menu_Inventory_UpdateDiscardMenu(0);

        if (i != 0) {

            Menu_Framework_InitializeInformationBox();
            Menu_Framework_FlyOutRowsLeftExcept(40);
            Menu_Framework_ClearMenuExcept(3);

            if (i == -2) {
                return -2;
            }

            Menu_Inventory_itemListState = Menu_Inventory_ItemListState_WaitInit;
        }

        break;

    case Menu_Inventory_ItemListState_Sort:
        i = Menu_Inventory_UpdateSortMenu(0);

        if (i != 0) {

            Menu_Framework_FlyOutRowsLeftExcept(40);
            Menu_Framework_ClearMenuExcept(3);

            if (i == -2) {
                return -2;
            }

            Menu_Inventory_itemListState = Menu_Inventory_ItemListState_WaitInit;
        }

        break;
    }

    return 0;
}

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ItemList_ResetMainMenuStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_freezeTabArrows, &initial, sizeof Menu_Inventory_freezeTabArrows);
    }
    {
        static const u_char* initial[] = { Audio_Seq_unusedInventoryIndices.weapons,
    Audio_Seq_unusedInventoryIndices.blades, Audio_Seq_unusedInventoryIndices.grips,
    Audio_Seq_unusedInventoryIndices.shields, Audio_Seq_unusedInventoryIndices.armor,
    Audio_Seq_unusedInventoryIndices.gems, Audio_Seq_unusedInventoryIndices.misc };
        memcpy((void*)&Menu_Inventory_inventoryIndices, &initial, sizeof Menu_Inventory_inventoryIndices);
    }
}

// MENU3.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ItemList_ResetMenu3Statics(void)
{
    {
        static const u_char initial[] = { 0, 8, 9, 10, 24, 25, 26 };
        memcpy((void*)&Menu_Inventory_sortMenuBladeSortStats, &initial, sizeof Menu_Inventory_sortMenuBladeSortStats);
    }
    {
        static const u_char initial[] = { 8, 9, 10, 27, 28, 29, 30, 31, 32, 33, 34, 35,
        36, 37, 38, 39 };
        memcpy((void*)&Menu_Inventory_sortMenuEquipmentSortStats, &initial, sizeof Menu_Inventory_sortMenuEquipmentSortStats);
    }
    {
        static const u_char initial[] = { 1, 4, 5, 6, 7, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39 };
        memcpy((void*)&Menu_Inventory_sortMenuGripSortStats, &initial, sizeof Menu_Inventory_sortMenuGripSortStats);
    }
    {
        static const u_char initial[] = { 0, 56 };
        memcpy((void*)&Menu_Inventory_sortMenuMiscSortStats, &initial, sizeof Menu_Inventory_sortMenuMiscSortStats);
    }
    {
        static const u_char initial[] = { 0, 1, 4, 5, 8, 9, 10, 40, 41, 42, 27, 28, 29, 30,
        31, 32, 33, 34, 35, 36, 37, 38, 39 };
        memcpy((void*)&Menu_Inventory_sortMenuShieldSortStats, &initial, sizeof Menu_Inventory_sortMenuShieldSortStats);
    }
    {
        static const u_char initial[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23 };
        memcpy((void*)&Menu_Inventory_sortMenuWeaponSortStats, &initial, sizeof Menu_Inventory_sortMenuWeaponSortStats);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_discardMenuCursorBrightness, &initial, sizeof Menu_Inventory_discardMenuCursorBrightness);
    }
    {
        static const char initial[] = { 1, 10, 100 };
        memcpy((void*)&Menu_Inventory_discardSteps, &initial, sizeof Menu_Inventory_discardSteps);
    }
    {
        static const char initial[] = "|>48|000/000\0";
        memcpy((void*)&Menu_Inventory_discardTemplate, &initial, sizeof Menu_Inventory_discardTemplate);
    }
    {
        static const char initial[8];
        memcpy((void*)&Menu_Inventory_itemListAvailableActions, &initial, sizeof Menu_Inventory_itemListAvailableActions);
    }
}

