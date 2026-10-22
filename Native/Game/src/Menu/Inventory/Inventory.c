// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/SequencePlayback.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/WeaponModel.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Battle/Hud.h"
#include "Battle/StatusEffects.h"
#include "Content/StringTables.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Field/Actors.h"
#include "Field/RoomRendering.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/KeyItems.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/MenuRows.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemDetails.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Sounds.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Settings.h"

// decomp: _ashleyRenderState (_ashleyRenderState@src/MENU/items1.h)
int Menu_Inventory_ashleyRenderState = 0;

// decomp: _ashleyYOffset (_ashleyYOffset@src/MENU/items1.h)
int Menu_Inventory_ashleyYOffset = 0;

// decomp: _exitToBattle (_exitToBattle@src/MENU/items1.h)
int Menu_Inventory_exitToBattle;

// decomp: _itemCategoryIconsAnimationStep (_itemCategoryIconsAnimationStep@src/MENU/items1.h)
char Menu_Inventory_itemCategoryIconsAnimationStep;

// decomp: _itemCategoryIconsEnabled (_itemCategoryIconsEnabled@src/MENU/_toggleUi.h)
char Menu_Inventory_itemCategoryIconsEnabled;

// decomp: _secondaryTitleAnimationStep (_secondaryTitleAnimationStep@src/MENU/items1.h)
char Menu_Inventory_secondaryTitleAnimationStep;

// decomp: _secondaryTitleEnabled (_secondaryTitleEnabled@src/MENU/_toggleUi.h)
char Menu_Inventory_secondaryTitleEnabled;

// decomp: _selectedItemCategoryIconOnTop (_selectedItemCategoryIconOnTop@src/MENU/items1.h)
char Menu_Inventory_selectedItemCategoryIconOnTop;

// decomp: _statusCommandAnimationStep (_statusCommandAnimationStep@src/MENU/items1.h)
char Menu_Inventory_statusCommandAnimationStep;

// decomp: _statusCommandState (_statusCommandState@src/MENU/_toggleUi.h)
char Menu_Inventory_statusCommandState;

// decomp: vs_battle_loadObjectDataState (vs_battle_loadObjectDataState@BATTLE/BATTLE.PRG)
char Menu_Inventory_equipmentModelLoadStep; /* contents: module image (ModuleData.c) */

// decomp: local static of vs_mainMenu_miscItemsShortcutMenu, at file scope so the module reset reaches it
static char Menu_Inventory_miscItemShortcutMisc[] = "\x16\x12\x1C\x0C\xE7\x00\x00\xF8";

// decomp: vs_mainMenu_miscItemToActionMap (vs_mainMenu_miscItemToActionMap@MENU/MAINMENU.PRG)
u_char Menu_Inventory_miscItemToActionMap[] = { 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
    0xE7, 0xF3, 0xF4, 0xF5, 0xF6, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEF, 0xEE, 0xF0,
    0xF2, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xF1, 0x00,
    0xFE, 0xFD, 0xFB, 0xF4, 0xFE, 0xFB, 0xF6, 0xF3, 0xF3, 0xFC, 0xFD, 0xFF, 0xFB, 0xF5,
    0xFE, 0xFD, 0xFF, 0xFD, 0xFD, 0xFF, 0xFD, 0xFE, 0xFE, 0xFF, 0xFE, 0xFC, 0xFF, 0xFD,
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x00, 0x00, 0x00, 0x6E,
    0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x7A, 0x00, 0x00,
    0x00, 0x7E, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x5E, 0x5F, 0x60, 0x57, 0x58,
    0x5A, 0x5B, 0x4D, 0x3C, 0x36, 0x3D, 0x37, 0x3E, 0x38, 0x3F, 0x39, 0x4B, 0x4C, 0x52,
    0x53, 0x55, 0x4F, 0x50, 0x5C, 0x5D, 0x3A, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49 };

// decomp: local static of _processEquipSubMenu, at file scope so the module reset reaches it
static char Menu_Inventory_equipmentChoicesCursorAnimation = 0;

// decomp: local static of vs_menu3_exec, at file scope so the module reset reaches it
static char Menu_Inventory_restoreItemCursor = 0;

// decomp: _equippedItem (_equippedItem@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_equippedItem;

// decomp: _itemsFirstRow (_itemsFirstRow@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_itemsFirstRow;

// decomp: _itemsText (_itemsText@src/MENU/MENU3.PRG/16C.c)
char* Menu_Inventory_itemsText;

// decomp: _menuRowCount (_menuRowCount@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_menuRowCount;

// decomp: _menuText (_menuText@src/MENU/items0.h)
u_short Menu_Inventory_menuText[472]; /* contents: disc string table (ModuleData.c) */

// decomp: _miscActions (_miscActions@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_unusedMiscActions[64];

// decomp: _selectedItemRow (_selectedItemRow@src/MENU/MENU3.PRG/16C.c)
int Menu_Inventory_selectedItemRow;

// decomp: local static of _equipArmorMenu, at file scope so the module reset reaches it
static char Menu_Inventory_armorMenuBodyPartCategories[] = { 0, 5, 5, 2, 3, 4, 6, 7 };

// decomp: local static of _dispatchEquipMenu, at file scope so the module reset reaches it
static int (*Menu_Inventory_dispatchEquipmentMenuEquipMenuHandlers[])(int) = { Menu_Inventory_UpdateArmorMenu, Menu_Inventory_UpdateWeaponMenu,
        Menu_Inventory_UpdateShieldMenu, Menu_Inventory_UpdateAccessoryMenu };

// decomp: local static of _equipMenu, at file scope so the module reset reaches it
static char Menu_Inventory_equipMenuRowTypes[] = { 0x78, 0x79, 0x69, 0x6A, 0x68, 0x6D, 0x6B, 0x7A };

// decomp: state (state@src/MENU/MENU3.PRG/16C.c)
u_char Menu_Inventory_equipListStep;

// decomp: MAINMENU.PRG vs_mainMenu_miscItemsShortcutMenu (src/MENU/MAINMENU.PRG/C48.c:480)
void Menu_Inventory_UpdateMiscItemShortcut(int initialize)
{
    enum Menu_Inventory_MiscItemShortcutState { Menu_Inventory_MiscItemShortcutState_Init, Menu_Inventory_MiscItemShortcutState_WaitTitle, Menu_Inventory_MiscItemShortcutState_InitItemRows, Menu_Inventory_MiscItemShortcutState_WaitForInput, Menu_Inventory_MiscItemShortcutState_Quit };

#pragma vsstring(start)
#pragma vsstring(end)

    static int Menu_Inventory_miscItemShortcutLayoutPadding __attribute__((unused)); // Garbage, patched in MAINMENU_PRG.py
    static char Menu_Inventory_miscItemShortcutActions[64];
    static int Menu_Inventory_miscItemShortcutSelectedRow;
    static char Menu_Inventory_miscItemShortcutState;
    static char Menu_Inventory_miscItemShortcutNoItems;
    static char Menu_Inventory_miscItemShortcutNoForceCursorMemory;
    static char Menu_Inventory_miscItemShortcutLayoutPadding2 __attribute__((unused));

    int j;
    int i;
    int rowCount;
    int* rowTypes;
    char** menuText;
    void* buf;
    Menu_Framework_MenuRow* menuItem;

    if (initialize != 0) {
        Menu_Framework_InitializeInformationBox();

        if (initialize & 1) {
            Menu_Inventory_miscItemShortcutNoForceCursorMemory = 1;
        }

        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_Init;
        return;
    }

    switch (Menu_Inventory_miscItemShortcutState) {
    case Menu_Inventory_MiscItemShortcutState_Init:
        menuItem = Menu_Framework_SetMenuItem(0, 320, 18, 140, 8,
            (char*)&Menu_menuStrings
                [Menu_menuStrings[Text_Battle_menuStrings_Index_items]]);
        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        menuItem->selected = 1;
        menuItem->targetPosition0 = 180;
        menuItem->unselectable = (Menu_Framework_actionRestrictions & 7) != 0;
        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_WaitTitle;

        Menu_Framework_LoadItemText(1);

        break;

    case Menu_Inventory_MiscItemShortcutState_WaitTitle:
        if (!Menu_Framework_AreMenuRowsIdle() || !Menu_Framework_LoadItemText(0)) {
            break;
        }

        menuItem = Menu_Framework_SetMenuItem(10, 320, 34, 140, 8, Menu_Inventory_miscItemShortcutMisc);
        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_InitItemRows;
        break;

    case Menu_Inventory_MiscItemShortcutState_InitItemRows:
        buf = Core_Memory_AllocateFromHighEnd(0x1900 + 64 * 2 * sizeof(*menuText));
        rowCount = 0;

        Items_Inventory_RebuildDisplayIndices(Items_ItemCategory_Misc);

        rowTypes = buf + 0x1800;
        menuText = buf + 0x1900;

        for (i = 0; i < 64; ++i, ++rowCount) {

            Items_InventoryMisc* misc;
            int index = Audio_Seq_unusedInventoryIndices.misc[i];

            if (index == 0) {
                break;
            }

            misc = &Audio_Seq_unusedInventory.misc[index - 1];

            Menu_Framework_FormatMiscItemRow(misc, &menuText[i * 2], rowTypes - -i, buf + i * 96);

            if (misc->id < Items_KeyItemId_BronzeKey) {

                u_int action = Menu_Inventory_miscItemToActionMap[misc->id - 323];

                if ((action - 106) < 28) {
                    for (j = 0; j < 3; ++j) {
                        if (Battle_actionDefinitions[action].unlocked) {
                            ++action;
                        }
                    }
                }

                Menu_Inventory_miscItemShortcutActions[i] = action;
            }

            if ((Menu_Framework_actionRestrictions & 7) || (misc->id >= Items_KeyItemId_BronzeKey)
                || (Battle_ActorActionRestrictionFlags(0, Menu_Inventory_miscItemShortcutActions[i]) & 2)) {
                rowTypes[i] |= Menu_Framework_RowTypeFlag_Disabled;
            }
        }

        Menu_Inventory_miscItemShortcutNoItems = rowCount == 0;

        if (Menu_Inventory_miscItemShortcutNoItems != 0) {
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noItemsInCategory]);
        } else {

            i = Input_settings.cursorMemory;

            if (Menu_Inventory_miscItemShortcutNoForceCursorMemory != 0) {
                Menu_Inventory_miscItemShortcutNoForceCursorMemory = 0;
            } else {
                Input_settings.cursorMemory = 1;
            }

            Menu_Framework_CreateMenuRows(
                rowCount, (25 << 12) | (2 << 8) | 36, menuText, rowTypes);

            Input_settings.cursorMemory = i;
        }

        Core_Memory_FreeFromHighEnd(buf);

        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_WaitForInput;
        break;

    case Menu_Inventory_MiscItemShortcutState_WaitForInput:
        if (Menu_Inventory_miscItemShortcutNoItems) {
            if (Input_buttonsPressed.allButtons & (PADRup | PADRright | PADRdown)) {
                if (Input_buttonsPressed.allButtons & PADRright) {

                    Menu_PlayInvalidSound();

                } else {

                    Menu_PlayLeaveSound();
                    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                    Menu_Framework_DismissInformationBox();

                    Menu_Inventory_miscItemShortcutSelectedRow = -1;
                    Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_Quit;
                }
            }
        } else {
            Menu_Inventory_miscItemShortcutSelectedRow = Menu_Framework_GetSelectedRow() + 1;

            if (Menu_Inventory_miscItemShortcutSelectedRow != 0) {

                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();

                if (Menu_Inventory_miscItemShortcutSelectedRow > 0) {
                    Items_miscItemInvoked =
                        Audio_Seq_unusedInventoryIndices.misc[Menu_Inventory_miscItemShortcutSelectedRow - 1] - 1;
                    Menu_Framework_actionSelection.executeAbility.halves[0] = Menu_Inventory_miscItemShortcutActions[Menu_Inventory_miscItemShortcutSelectedRow - 1];
                    Menu_Framework_actionSelection.executeAbility.halves[1] =
                        Audio_Seq_unusedInventory.misc[Items_miscItemInvoked].id;
                }

                Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_Quit;
            }
        }
        break;

    case Menu_Inventory_MiscItemShortcutState_Quit:
        if (!Menu_Framework_AreMenuRowsIdle() || !Menu_Framework_UnloadItemTextWhenReady()) {
            break;
        }

        if (Menu_Inventory_miscItemShortcutSelectedRow > 0) {
            Menu_Framework_actionSelection.selectionKind = 8;
        }

        Menu_Framework_menuState.currentMenu = Menu_Framework_MenuState_NoneClosed;
        Menu_Framework_actionSelection.menuStatus = 2;
        return;
    }

    Menu_Framework_UpdateScreen();

    return;
}

// decomp: MENU3.PRG _populateMenuRows (src/MENU/MENU3.PRG/16C.c:39)
void Menu_Inventory_CreateEquipmentRows(int count, u_short** menuText, int* rowTypes)
{
    int i;
    int j;
    Menu_Framework_MenuRow* menuItem;

    Menu_Inventory_menuRowCount = count;
    Menu_Inventory_itemsFirstRow = 0;
    Menu_Inventory_selectedItemRow = 0;
    Menu_Inventory_equipListStep = 1;

    if (Menu_Inventory_itemsText && Menu_Inventory_itemsText) { }

    Menu_Inventory_itemsText = Core_Memory_AllocateFromHighEnd(count * 128);

    for (i = 0; i < count; ++i) {
        u_short* text;

        ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[i].menuRowFlags = rowTypes[i];

        text = menuText[i * 2];

        if (text != NULL) {
            for (j = 0; j < 13; j++) {
                ((u_short*)Menu_Inventory_itemsText)[i * 64 + j] = text[j];
            }
            ((u_short*)(i * 128 + Menu_Inventory_itemsText))[13] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            ((u_short*)(i * 128 + Menu_Inventory_itemsText))[0] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }

        text = menuText[i * 2 + 1];

        if (text != NULL) {
            ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[i].description[0] = Core_TextCode_ChunkSize;
            for (j = 0; j < 46; ++j) {
                ((u_short*)(Menu_Inventory_itemsText))[i * 64 + j + 17] = text[j];
            }
            ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[i].description[47] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[i].description[0] =
                Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }
    }

    j = rowTypes[0];
    menuItem = Menu_Framework_SetMenuItem(20, 155, 18, 165, 0, (char*)Menu_Inventory_itemsText);
    menuItem->unselectable = j & 1;
    menuItem->rowIcon = j >> 26;
    menuItem->material = (j >> 16) & 7;
    menuItem->selected = 1;
    Menu_Inventory_equippedItem = (j >> 19) & 0x7F;
}

// decomp: MENU3.PRG _getSelectedItemRow (src/MENU/MENU3.PRG/16C.c:101)
int Menu_Inventory_GetSelectedEquipmentRow(void)
{
    return Menu_Inventory_itemsText != NULL ? -1 : Menu_Inventory_selectedItemRow;
}

// decomp: MENU3.PRG _processEquipSubMenu (src/MENU/MENU3.PRG/16C.c:111)
void Menu_Inventory_UpdateEquipmentChoices(int hasSfx)
{

    int temp_s0;
    int temp_s4;
    int i;
    int temp_s6;
    Menu_Framework_MenuRow* menuItem;

    if (Menu_Inventory_equipListStep < 6) {
        if (Menu_Inventory_equipListStep == Menu_Inventory_menuRowCount) {
            Menu_Inventory_equipListStep = 16;
            return;
        }

        menuItem = Menu_Framework_SetMenuItem(
            Menu_Inventory_equipListStep + 20, 320, (Menu_Inventory_equipListStep * 16) + 18, 151, 0, Menu_Inventory_itemsText + (Menu_Inventory_equipListStep << 7));
        menuItem->state = 2;
        menuItem->targetPosition0 = 0xA9;
        temp_s0 = ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[Menu_Inventory_equipListStep].menuRowFlags;
        menuItem->unselectable = temp_s0 & 1;
        menuItem->rowIcon = (temp_s0 >> 0x1A);

        if (((temp_s0 >> 9) & 0x7F) != 0) {
            menuItem->outsetIcon = ((temp_s0 >> 9) & 0x7F) - 100;
        }

        menuItem->material = (temp_s0 >> 0x10) & 7;
        ++Menu_Inventory_equipListStep;

        if (Menu_Inventory_equipListStep == 6) {
            if (Menu_Inventory_menuRowCount >= 7) {
                menuItem->isScrollable = 2;
            }

            Menu_Inventory_equipListStep = 16;
        }

        return;
    }

    temp_s6 = Menu_Inventory_itemsFirstRow;
    temp_s4 = Menu_Inventory_selectedItemRow + temp_s6;

    menuItem = Menu_Framework_MenuItem(Menu_Inventory_selectedItemRow + 20);
    Menu_Framework_SetInformationMessage(Menu_Inventory_itemsText + ((temp_s4 << 7) + 0x20));

    switch (Menu_Inventory_equipListStep) {
    case 16:
        if (Menu_Framework_AreMenuRowsIdle()) {
            Menu_Inventory_equipListStep = 17;
        }
        break;

    case 17:
        if (Input_buttonsPressed.allButtons & PADRup) {

            // Pointless check, value is always 0 in practice
            if (!(hasSfx & 2)) {
                Menu_PlayLeaveSound();
            }

            Core_Memory_FreeFromHighEnd(Menu_Inventory_itemsText);

            Menu_Inventory_itemsText = NULL;
            Menu_Inventory_selectedItemRow = -3;

            break;
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (menuItem->unselectable) {
                Menu_PlayInvalidSound();
            } else {

                Core_Memory_FreeFromHighEnd(Menu_Inventory_itemsText);

                Menu_Inventory_itemsText = NULL;
                Menu_Inventory_selectedItemRow = temp_s4;

                if (hasSfx != 0) {
                    if (temp_s4 == 0) {
                        Audio_PlaySfxCentered(0x7E, 0x22);
                    } else {
                        Audio_PlaySfxCentered(0x7E, 0x21);
                    }
                }
                break;
            }
        }

        menuItem->selected = 0;

        if (Input_buttonsPressed.allButtons & PADRdown) {

            Menu_PlayLeaveSound();
            Core_Memory_FreeFromHighEnd(Menu_Inventory_itemsText);

            Menu_Inventory_itemsText = NULL;
            Menu_Inventory_selectedItemRow = -2;

            break;
        }

        if (Input_buttonRepeat & PADLup) {
            if (Menu_Inventory_menuRowCount < 7 || Menu_Inventory_itemsFirstRow == 0) {
                if (Menu_Inventory_selectedItemRow != 0) {
                    --Menu_Inventory_selectedItemRow;
                }
            } else if (Menu_Inventory_selectedItemRow == 2) {
                --Menu_Inventory_itemsFirstRow;
            } else {
                --Menu_Inventory_selectedItemRow;
            }
        }

        if (Input_buttonRepeat & PADLdown) {
            if (Menu_Inventory_menuRowCount < 7) {
                if (Menu_Inventory_selectedItemRow < (Menu_Inventory_menuRowCount - 1)) {
                    ++Menu_Inventory_selectedItemRow;
                }
            } else if (Menu_Inventory_itemsFirstRow == (Menu_Inventory_menuRowCount - 6)) {
                if (Menu_Inventory_selectedItemRow < 5) {
                    ++Menu_Inventory_selectedItemRow;
                }
            } else if (Menu_Inventory_selectedItemRow == 4) {
                ++Menu_Inventory_itemsFirstRow;
            } else {
                ++Menu_Inventory_selectedItemRow;
            }
        }

        if (temp_s4 != (Menu_Inventory_selectedItemRow + Menu_Inventory_itemsFirstRow)) {
            Menu_PlayChangeSound();

            if (Menu_Inventory_itemsFirstRow != temp_s6) {
                char buf[Menu_Inventory_menuRowCount];
                for (i = 0; i < Menu_Inventory_menuRowCount; ++i) {
                    buf[i] = 0;
                }
                temp_s0 = Menu_Inventory_menuRowCount;

                if (Menu_Inventory_menuRowCount >= 7) {
                    temp_s0 = 6;
                }
                for (i = 1; i < temp_s0; ++i) {
                    buf[i + temp_s6] = Menu_Framework_MenuItem(i + 0x14)->gradientState;
                }

                for (i = 1;;) {
                    menuItem = Menu_Framework_SetMenuItem(i + 0x14, 0xA9, 0x12 + i * 0x10,
                        0x97, 0, Menu_Inventory_itemsText + ((i + Menu_Inventory_itemsFirstRow) << 7));
                    menuItem->gradientState = buf[i + Menu_Inventory_itemsFirstRow];
                    temp_s0 =
                        ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[i + Menu_Inventory_itemsFirstRow].menuRowFlags;
                    menuItem->unselectable = temp_s0 & 1;
                    menuItem->rowIcon = temp_s0 >> 0x1A;

                    if (((temp_s0 >> 9) & 0x7F) != 0) {
                        menuItem->outsetIcon = ((temp_s0 >> 9) & 0x7F) - 0x64;
                    }

                    menuItem->material = (temp_s0 >> 0x10) & 7;
                    i++;

                    if (i == Menu_Inventory_menuRowCount) {
                        break;
                    }
                    if (i == 6) {
                        if ((Menu_Inventory_itemsFirstRow + 6) < Menu_Inventory_menuRowCount) {
                            menuItem->isScrollable = 2;
                        }
                        break;
                    }
                }

                if (Menu_Inventory_itemsFirstRow != 0) {
                    Menu_Framework_MenuItem(0x15)->isScrollable = 1;
                }
            }

            temp_s0 = ((Menu_Framework_ItemText*)Menu_Inventory_itemsText)[Menu_Inventory_selectedItemRow + Menu_Inventory_itemsFirstRow]
                          .menuRowFlags;
            Menu_Inventory_equippedItem = (temp_s0 >> 19) & 0x7F;
        }

        Menu_Framework_MenuItem(Menu_Inventory_selectedItemRow + 0x14)->selected = 1;

        i = (((Menu_Inventory_selectedItemRow * 0x10) + 0xA) << 0x10) | 0x9B;

        if (Menu_Inventory_selectedItemRow == 0) {
            i -= 0xE;
        }

        Menu_Inventory_equipmentChoicesCursorAnimation = Menu_Framework_DrawCursor(Menu_Inventory_equipmentChoicesCursorAnimation, i);
        break;
    }
}

// decomp: MENU3.PRG _populateUiAccessory (src/MENU/MENU3.PRG/16C.c:311)
void Menu_Inventory_SetAccessoryPreviewStats(int accessoryId)
{

    Menu_Framework_ResetEquipmentStats();

    if (accessoryId != 0) {
        int i;
        Items_AccessorySpawnLoadout accessory;

        Items_LoadInventoryAccessoryStats(&accessory, &Menu_Framework_armor[accessoryId - 1]);

        for (i = 0; i < 16; ++i) {
            Menu_Framework_equipmentStats[i] = accessory.classes[i & 7];
            Menu_Framework_equipmentStats[i + 16] = accessory.affinities[i & 7];
            Menu_Framework_equipmentStats[i + 32] = accessory.types[i & 7];
        }

        Menu_Framework_SetPrimaryStats(
            accessory.currentStr, accessory.currentInt, accessory.currentAgility, 2);
    }

    Menu_Framework_equipmentSubtype = 32;
    Menu_Framework_currentStatusViewItem = accessoryId;

    Menu_Framework_SetEnabledStatPages(7);
}

// decomp: MENU3.PRG _equipAccessoryMenu (src/MENU/MENU3.PRG/16C.c:343)
int Menu_Inventory_UpdateAccessoryMenu(int initialize)
{
    enum Menu_Inventory_EquipmentSlotMenuState { Menu_Inventory_EquipmentSlotMenuState_Init, Menu_Inventory_EquipmentSlotMenuState_HandleInput, Menu_Inventory_EquipmentSlotMenuState_LoadObject, Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady };

    static int Menu_Inventory_accessoryMenuSelectedAccessory;
    static char Menu_Inventory_accessoryMenuState;
    static char Menu_Inventory_accessoryMenuEquippedAccessory;
    static char Menu_Inventory_accessoryMenuLayoutPadding[2] __attribute__((unused));

    int i;
    int index;

    if (initialize) {

        for (i = 0; i < 16; ++i) {
            index = Audio_Seq_unusedInventoryIndices.armor[i];
            if (index && (Audio_Seq_unusedInventory.armor[index - 1].limb == 7)) {
                break;
            }
        }

        if (i == 16) {
            index = 0;
        }

        Menu_Inventory_accessoryMenuEquippedAccessory = index;

        Menu_Inventory_SetAccessoryPreviewStats(index);
        Menu_Framework_UpdateEquipmentStatPages(7);
        Menu_Framework_UpdatePrimaryStatPanel(1);

        Menu_Inventory_accessoryMenuState = Menu_Inventory_EquipmentSlotMenuState_Init;
        return 0;
    }

    switch (Menu_Inventory_accessoryMenuState) {
    case Menu_Inventory_EquipmentSlotMenuState_Init: {
        int hasAccessory;
        int itemCount;

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        itemCount = Items_Inventory_CountItems(4, NULL);
        hasAccessory = 0;

        for (i = 0; i < 16; ++i) {
            if (Audio_Seq_unusedInventory.armor[i].limb == 7) {
                hasAccessory = 1;
            }
        }
        {
            int count = itemCount + 1;
            char textBuf[count * 96];
            u_short* menuText[count * 2];
            int rowTypes[count];
            int rowCount;

            if (!hasAccessory) {
                for (i = 0; i < 2; ++i) {
                    menuText[i] = &Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_none + i]];
                }
                rowTypes[0] = 1;
            } else {
                for (i = 0; i < itemCount; ++i) {

                    index = Audio_Seq_unusedInventoryIndices.armor[i];

                    if (index && (Audio_Seq_unusedInventory.armor[index - 1].limb == 7)) {

                        Menu_Framework_FormatInventoryArmorRow(
                            &Audio_Seq_unusedInventory.armor[index - 1], (char**)menuText,
                            rowTypes, textBuf);

                        rowTypes[0] |= index << 19;
                    }
                }
            }

            rowCount = 1;

            for (i = 0; i < itemCount; ++i) {

                index = Audio_Seq_unusedInventoryIndices.armor[i];

                if ((index != 0) && (Audio_Seq_unusedInventory.armor[index - 1].category == 7)
                    && (Audio_Seq_unusedInventory.armor[index - 1].limb == 0)) {

                    Menu_Framework_FormatInventoryArmorRow(
                        &Audio_Seq_unusedInventory.armor[index - 1],
                        (char**)&menuText[rowCount * 2], &rowTypes[rowCount],
                        textBuf + rowCount * 96);

                    rowTypes[rowCount] |= index << 19;
                    ++rowCount;
                }
            }

            Menu_Inventory_CreateEquipmentRows(rowCount, menuText, rowTypes);

            Menu_Inventory_accessoryMenuState = Menu_Inventory_EquipmentSlotMenuState_HandleInput;
        }
        break;
    }
    case Menu_Inventory_EquipmentSlotMenuState_HandleInput:
        Menu_Inventory_SetAccessoryPreviewStats(Menu_Inventory_equippedItem);
        Menu_Inventory_UpdateEquipmentChoices(1);

        Menu_Inventory_accessoryMenuSelectedAccessory = Menu_Inventory_GetSelectedEquipmentRow() + 1;

        if (!Menu_Inventory_accessoryMenuSelectedAccessory) {
            break;
        }

        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Framework_FlyOutRowLeft(-1);
        Menu_Framework_UpdateEquipmentStatPages(-1);
        Menu_Framework_UpdatePrimaryStatPanel(2);

        if (Menu_Inventory_accessoryMenuSelectedAccessory < 0) {
            Menu_Inventory_accessoryMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        } else {
            if (Menu_Inventory_accessoryMenuEquippedAccessory) {
                Audio_Seq_unusedInventory.armor[Menu_Inventory_accessoryMenuEquippedAccessory - 1].limb = 0;
            }

            if (Menu_Inventory_accessoryMenuSelectedAccessory != 1) {
                Items_InventoryArmor* armor =
                    &Audio_Seq_unusedInventory.armor[Menu_Inventory_equippedItem - 1];
                armor->limb = 7;

                Items_EquipAshleyAccessory(armor);
            } else {
                Items_EquipAshleyAccessory(NULL);
            }

            Menu_Inventory_accessoryMenuState = Menu_Inventory_EquipmentSlotMenuState_LoadObject;
        }
        break;

    case Menu_Inventory_EquipmentSlotMenuState_LoadObject:
        Menu_Inventory_accessoryMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        /* fallthrough */

    case Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady:
        if (Menu_Framework_AreMenuRowsIdle()) {
            return Menu_Inventory_accessoryMenuSelectedAccessory;
        }

        break;
    }
    return 0;
}

// decomp: MENU3.PRG _equipWeaponMenu (src/MENU/MENU3.PRG/16C.c:503)
int Menu_Inventory_UpdateWeaponMenu(int initialize)
{
    enum Menu_Inventory_EquipmentSlotMenuState { Menu_Inventory_EquipmentSlotMenuState_Init, Menu_Inventory_EquipmentSlotMenuState_HandleInput, Menu_Inventory_EquipmentSlotMenuState_LoadObject, Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady };

    static int Menu_Inventory_weaponMenuSelectedWeapon;
    static char Menu_Inventory_weaponMenuState;
    static char Menu_Inventory_weaponMenuCategory;
    static char Menu_Inventory_weaponMenuWepId;
    static char Menu_Inventory_weaponMenuMaterial;

    int i;
    int index;
    Items_InventoryWeapon* weapon;

    if (initialize != 0) {
        for (i = 0; i < 8; ++i) {
            index = Audio_Seq_unusedInventoryIndices.weapons[i];
            if ((index != 0) && (Audio_Seq_unusedInventory.weapons[index - 1].isEquipped != 0)) {
                break;
            }
        }

        if (i == 8) {
            index = 0;
        }

        Menu_Framework_SetWeaponStats(index);
        Menu_Framework_UpdateEquipmentStatPages(7);
        Menu_Framework_UpdateDpPpBars(3);
        Menu_Framework_UpdatePrimaryStatPanel(1);

        Menu_Inventory_weaponMenuState = Menu_Inventory_EquipmentSlotMenuState_Init;
        return 0;
    }

    switch (Menu_Inventory_weaponMenuState) {
    case Menu_Inventory_EquipmentSlotMenuState_Init: {
        int count;
        int hasWeapon;
        int rowCount;

        if (!Menu_Framework_AreMenuRowsIdle()) {
            break;
        }

        count = Items_Inventory_CountItems(0, NULL);
        hasWeapon = 0;

        for (i = 0; i < 8; ++i) {
            if (Audio_Seq_unusedInventory.weapons[i].isEquipped != 0) {
                hasWeapon = 1;
            }
        }
        {
            char textBuf[(count + 1) * 96];
            u_short* menuText[(count + 1) * 2];
            int rowTypes[(count + 1)];

            if (!hasWeapon) {
                for (i = 0; i < 2; ++i) {
                    menuText[i] = &Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_none + i]];
                }
                rowTypes[0] = 1;
            } else {
                for (i = 0; i < count; ++i) {

                    index = Audio_Seq_unusedInventoryIndices.weapons[i];

                    if ((index != 0)
                        && (Audio_Seq_unusedInventory.weapons[index - 1].isEquipped != 0)) {

                        Menu_Framework_FormatInventoryWeaponRow(
                            &Audio_Seq_unusedInventory.weapons[index - 1], (char**)menuText,
                            rowTypes, textBuf);

                        rowTypes[0] |= index << 0x13;
                    }
                }
            }

            rowCount = 1;

            for (i = 0; i < count; ++i) {

                index = Audio_Seq_unusedInventoryIndices.weapons[i];

                if (index != 0) {

                    Items_InventoryWeapon* weapon =
                        &Audio_Seq_unusedInventory.weapons[index - 1];

                    if (weapon->isEquipped == 0) {

                        Menu_Framework_FormatInventoryWeaponRow(weapon,
                            (char**)&menuText[rowCount * 2], &rowTypes[rowCount],
                            textBuf + rowCount * 96);

                        rowTypes[rowCount] |= index << 0x13;
                        ++rowCount;
                    }
                }
            }

            Menu_Inventory_CreateEquipmentRows(rowCount, menuText, rowTypes);

            Menu_Inventory_weaponMenuState = Menu_Inventory_EquipmentSlotMenuState_HandleInput;
        }
        break;
    }
    case Menu_Inventory_EquipmentSlotMenuState_HandleInput:
        Menu_Framework_SetWeaponStats(Menu_Inventory_equippedItem);
        Menu_Inventory_UpdateEquipmentChoices(0);

        Menu_Inventory_weaponMenuSelectedWeapon = Menu_Inventory_GetSelectedEquipmentRow() + 1;

        if (Menu_Inventory_weaponMenuSelectedWeapon == 0) {
            break;
        }

        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Framework_FlyOutRowLeft(-1);
        Menu_Framework_UpdateEquipmentStatPages(-1);
        Menu_Framework_UpdateDpPpBars(4);
        Menu_Framework_UpdatePrimaryStatPanel(2);

        if (Menu_Inventory_weaponMenuSelectedWeapon < 0) {
            Menu_Inventory_weaponMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        } else {

            Items_Inventory_UnequipWeapon();

            if (Menu_Inventory_weaponMenuSelectedWeapon == 1) {
                Menu_Inventory_weaponMenuCategory = 10;
                Menu_Inventory_weaponMenuWepId = 0;
                Menu_Inventory_weaponMenuMaterial = 0;
            } else {
                Items_InventoryBlade* blade;

                weapon = &Audio_Seq_unusedInventory.weapons[Menu_Inventory_equippedItem - 1];
                weapon->isEquipped = 1;

                blade = &Audio_Seq_unusedInventory.blades[weapon->blade - 1];
                Menu_Inventory_weaponMenuCategory = blade->category;
                Menu_Inventory_weaponMenuWepId = blade->wepId;
                Menu_Inventory_weaponMenuMaterial = blade->material;

                if (Menu_Framework_weaponHands[Menu_Inventory_weaponMenuCategory - 1] != 0) {
                    Items_Inventory_UnequipShield();
                }

                Items_EquipAshleyWeapon(weapon);
            }
            Menu_Inventory_weaponMenuState = Menu_Inventory_EquipmentSlotMenuState_LoadObject;
        }
        break;

    case Menu_Inventory_EquipmentSlotMenuState_LoadObject:
        if (Menu_Inventory_ReloadEquipmentModels(Menu_Inventory_weaponMenuCategory, Menu_Inventory_weaponMenuWepId, Menu_Inventory_weaponMenuMaterial)) {
            break;
        }

        Menu_Inventory_weaponMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        // Fallthrough

    case Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Inventory_weaponMenuSelectedWeapon;
        }

        break;
    }

    return 0;
}

// decomp: MENU3.PRG _equipShieldMenu (src/MENU/MENU3.PRG/16C.c:683)
int Menu_Inventory_UpdateShieldMenu(int initialize)
{
    enum Menu_Inventory_EquipmentSlotMenuState { Menu_Inventory_EquipmentSlotMenuState_Init, Menu_Inventory_EquipmentSlotMenuState_HandleInput, Menu_Inventory_EquipmentSlotMenuState_LoadObject, Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady };

    static int Menu_Inventory_shieldMenuSelectedShield;
    static char Menu_Inventory_shieldMenuState;
    static char Menu_Inventory_shieldMenuWepId;
    static char Menu_Inventory_shieldMenuMaterial;
    static char Menu_Inventory_shieldMenuLayoutPadding __attribute__((unused));

    int i;
    int index;

    if (initialize != 0) {
        for (i = 0; i < 8; ++i) {
            index = Audio_Seq_unusedInventoryIndices.shields[i];

            if (index && Audio_Seq_unusedInventory.shields[index - 1].isEquipped) {
                break;
            }
        }

        if (i == 8) {
            index = 0;
        }

        Menu_Framework_SetShieldStats(index);
        Menu_Framework_UpdateEquipmentStatPages(7);
        Menu_Framework_UpdateDpPpBars(3);
        Menu_Framework_UpdatePrimaryStatPanel(1);

        Menu_Inventory_shieldMenuState = Menu_Inventory_EquipmentSlotMenuState_Init;

        return 0;
    }

    switch (Menu_Inventory_shieldMenuState) {
    case Menu_Inventory_EquipmentSlotMenuState_Init: {
        int count;
        int hasShield;
        int rowCount;

        if (!Menu_Framework_AreMenuRowsIdle()) {
            break;
        }

        count = Items_Inventory_CountItems(3, NULL);
        hasShield = 0;

        for (i = 0; i < 8; ++i) {
            if (Audio_Seq_unusedInventory.shields[i].isEquipped != 0) {
                hasShield = 1;
            }
        }
        {
            char textBuf[(count + 1) * 96];
            u_short* menuText[(count + 1) * 2];
            int rowTypes[(count + 1)];

            if (!hasShield) {
                for (i = 0; i < 2; ++i) {
                    menuText[i] = &Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_none + i]];
                }
                rowTypes[0] = 1;
            } else {
                for (i = 0; i < count; ++i) {

                    index = Audio_Seq_unusedInventoryIndices.shields[i];

                    if ((index != 0)
                        && (Audio_Seq_unusedInventory.shields[index - 1].isEquipped != 0)) {

                        Menu_Framework_FormatInventoryShieldRow(
                            &Audio_Seq_unusedInventory.shields[index - 1], (char**)menuText,
                            rowTypes, textBuf);

                        rowTypes[0] |= index << 0x13;
                    }
                }
            }

            rowCount = 1;

            for (i = 0; i < count; ++i) {
                index = Audio_Seq_unusedInventoryIndices.shields[i];
                if (index != 0) {
                    if (Audio_Seq_unusedInventory.shields[index - 1].isEquipped == 0) {
                        Menu_Framework_FormatInventoryShieldRow(
                            &Audio_Seq_unusedInventory.shields[index - 1],
                            (char**)&menuText[rowCount * 2], &rowTypes[rowCount],
                            textBuf + rowCount * 96);
                        rowTypes[rowCount] |= index << 0x13;
                        ++rowCount;
                    }
                }
            }

            Menu_Inventory_CreateEquipmentRows(rowCount, menuText, rowTypes);

            Menu_Inventory_shieldMenuState = Menu_Inventory_EquipmentSlotMenuState_HandleInput;
        }
        break;
    }

    case Menu_Inventory_EquipmentSlotMenuState_HandleInput:
        Menu_Framework_SetShieldStats(Menu_Inventory_equippedItem);
        Menu_Inventory_UpdateEquipmentChoices(0);

        Menu_Inventory_shieldMenuSelectedShield = Menu_Inventory_GetSelectedEquipmentRow() + 1;

        if (Menu_Inventory_shieldMenuSelectedShield == 0) {
            break;
        }

        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Framework_FlyOutRowLeft(-1);
        Menu_Framework_UpdateEquipmentStatPages(-1);
        Menu_Framework_UpdateDpPpBars(4);
        Menu_Framework_UpdatePrimaryStatPanel(2);

        if (Menu_Inventory_shieldMenuSelectedShield < 0) {
            Menu_Inventory_shieldMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        } else {

            Items_Inventory_UnequipShield();

            if (Menu_Inventory_shieldMenuSelectedShield == 1) {
                Menu_Inventory_shieldMenuWepId = 0;
                Menu_Inventory_shieldMenuMaterial = 0;
            } else {
                Battle_ActorStats* actor;
                Items_InventoryShield* shield =
                    &Audio_Seq_unusedInventory.shields[Menu_Inventory_equippedItem - 1];
                shield->isEquipped = 1;

                actor = Battle_characterState->stats;
                Menu_Inventory_shieldMenuWepId = shield->base.wepId;
                Menu_Inventory_shieldMenuMaterial = shield->base.material;

                if (actor->weapon.blade.id
                    && Menu_Framework_weaponHands[actor->weapon.blade.category - 1]) {
                    Items_Inventory_UnequipWeapon();
                }

                Items_EquipAshleyShield(shield);
            }
            Menu_Inventory_shieldMenuState = Menu_Inventory_EquipmentSlotMenuState_LoadObject;
        }
        break;

    case Menu_Inventory_EquipmentSlotMenuState_LoadObject:
        if (Menu_Inventory_ReloadEquipmentModels(0, Menu_Inventory_shieldMenuWepId, Menu_Inventory_shieldMenuMaterial)) {
            break;
        }
        Menu_Inventory_shieldMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        // Fallthrough

    case Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady:
        if (Menu_Framework_AreMenuRowsIdle()) {
            return Menu_Inventory_shieldMenuSelectedShield;
        }
        break;
    }

    return 0;
}

// decomp: MENU3.PRG _equipArmorMenu (src/MENU/MENU3.PRG/16C.c:856)
int Menu_Inventory_UpdateArmorMenu(int initBodyPart)
{
    enum Menu_Inventory_EquipmentSlotMenuState { Menu_Inventory_EquipmentSlotMenuState_Init, Menu_Inventory_EquipmentSlotMenuState_HandleInput, Menu_Inventory_EquipmentSlotMenuState_LoadObject, Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady };


    static int Menu_Inventory_armorMenuSelectedArmor;
    static char Menu_Inventory_armorMenuState;
    static char Menu_Inventory_armorIndex;
    static u_char Menu_Inventory_armorMenuBodyPart;

    int i;
    int index;

    if (initBodyPart != 0) {

        Menu_Inventory_armorMenuBodyPart = initBodyPart;

        for (i = 0; i < 16; ++i) {

            index = Audio_Seq_unusedInventoryIndices.armor[i];

            if ((index != 0)
                && (Audio_Seq_unusedInventory.armor[index - 1].limb == initBodyPart)) {
                break;
            }
        }

        if (i == 16) {
            index = 0;
        }

        Menu_Inventory_armorIndex = index;

        Menu_Framework_SetArmorStats(index);
        Menu_Framework_UpdateEquipmentStatPages(7);
        Menu_Framework_UpdateDpPpBars(1);
        Menu_Framework_UpdatePrimaryStatPanel(1);

        Menu_Inventory_armorMenuState = Menu_Inventory_EquipmentSlotMenuState_Init;

        return 0;
    }

    switch (Menu_Inventory_armorMenuState) {
    case Menu_Inventory_EquipmentSlotMenuState_Init: {
        int hasArmor;
        int count;
        int rowCount;

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        count = Items_Inventory_CountItems(4, NULL);
        hasArmor = 0;

        for (i = 0; i < 16; ++i) {
            if (Audio_Seq_unusedInventory.armor[i].limb == Menu_Inventory_armorMenuBodyPart) {
                hasArmor = 1;
            }
        }
        {
            char textBuf[(count + 1) * 96];
            u_short* menuText[(count + 1) * 2];
            int rowTypes[(count + 1)];

            if (!hasArmor) {
                for (i = 0; i < 2; ++i) {
                    menuText[i] = &Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_none + i]];
                }
                rowTypes[0] = 1;
            } else {
                for (i = 0; i < count; ++i) {

                    index = Audio_Seq_unusedInventoryIndices.armor[i];

                    if ((index != 0)
                        && (Audio_Seq_unusedInventory.armor[index - 1].limb == Menu_Inventory_armorMenuBodyPart)) {

                        Menu_Framework_FormatInventoryArmorRow(
                            &Audio_Seq_unusedInventory.armor[index - 1], (char**)menuText,
                            rowTypes, textBuf);

                        rowTypes[0] |= index << 0x13;
                    }
                }
            }

            rowCount = 1;

            for (i = 0; i < count; ++i) {

                index = Audio_Seq_unusedInventoryIndices.armor[i];

                if ((index != 0)
                    && (Audio_Seq_unusedInventory.armor[index - 1].category
                        == Menu_Inventory_armorMenuBodyPartCategories[Menu_Inventory_armorMenuBodyPart])
                    && (Audio_Seq_unusedInventory.armor[index - 1].limb == 0)) {

                    Menu_Framework_FormatInventoryArmorRow(
                        &Audio_Seq_unusedInventory.armor[index - 1],
                        (char**)&menuText[rowCount * 2], &rowTypes[rowCount],
                        textBuf + rowCount * 0x60);

                    rowTypes[rowCount] |= index << 0x13;
                    ++rowCount;
                }
            }
            Menu_Inventory_CreateEquipmentRows(rowCount, menuText, rowTypes);

            Menu_Inventory_armorMenuState = Menu_Inventory_EquipmentSlotMenuState_HandleInput;
        }

        break;
    }

    case Menu_Inventory_EquipmentSlotMenuState_HandleInput:
        Menu_Framework_SetArmorStats(Menu_Inventory_equippedItem);
        Menu_Inventory_UpdateEquipmentChoices(1);

        Menu_Inventory_armorMenuSelectedArmor = Menu_Inventory_GetSelectedEquipmentRow() + 1;

        if (Menu_Inventory_armorMenuSelectedArmor == 0) {
            break;
        }

        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Framework_FlyOutRowLeft(-1);
        Menu_Framework_UpdateEquipmentStatPages(-1);
        Menu_Framework_UpdateDpPpBars(4);
        Menu_Framework_UpdatePrimaryStatPanel(2);

        if (Menu_Inventory_armorMenuSelectedArmor < 0) {
            Menu_Inventory_armorMenuState = 3;
        } else {
            if (Menu_Inventory_armorIndex != 0) {
                Audio_Seq_unusedInventory.armor[Menu_Inventory_armorIndex - 1].limb = 0;
            }
            if (Menu_Inventory_armorMenuSelectedArmor == 1) {
                Items_EquipAshleyArmor(Menu_Inventory_armorMenuBodyPart - 1, NULL);
            } else {
                Items_InventoryArmor* armor =
                    &Audio_Seq_unusedInventory.armor[Menu_Inventory_equippedItem - 1];
                armor->limb = Menu_Inventory_armorMenuBodyPart;

                Items_EquipAshleyArmor(Menu_Inventory_armorMenuBodyPart - 1, armor);
            }

            Menu_Inventory_armorMenuState = Menu_Inventory_EquipmentSlotMenuState_LoadObject;
        }
        break;

    case Menu_Inventory_EquipmentSlotMenuState_LoadObject:
        Menu_Inventory_armorMenuState = Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady;
        // Fallthrough

    case Menu_Inventory_EquipmentSlotMenuState_WaitMenuReady:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Inventory_armorMenuSelectedArmor;
        }
        break;
    }

    return 0;
}

// decomp: MENU3.PRG _dispatchEquipMenu (src/MENU/MENU3.PRG/16C.c:1028)
int Menu_Inventory_DispatchEquipmentMenu(int row)
{

    static u_char Menu_Inventory_dispatchEquipmentMenuCurrentEquipMenu;

    if (row != 0) {
        if (row == 8) {
            Menu_Inventory_dispatchEquipmentMenuCurrentEquipMenu = 3;
        } else if (row > 2) {
            row -= 2;
            Menu_Inventory_dispatchEquipmentMenuCurrentEquipMenu = 0;
        } else {
            Menu_Inventory_dispatchEquipmentMenuCurrentEquipMenu = row;
        }
    }

    return Menu_Inventory_dispatchEquipmentMenuEquipMenuHandlers[Menu_Inventory_dispatchEquipmentMenuCurrentEquipMenu](row);
}

// decomp: MENU3.PRG _equipMenu (src/MENU/MENU3.PRG/16C.c:1054)
int Menu_Inventory_UpdateEquipmentMenu(int initialize)
{
    enum Menu_Inventory_EquipmentMenuState { Menu_Inventory_EquipmentMenuState_Init, Menu_Inventory_EquipmentMenuState_InitMenu, Menu_Inventory_EquipmentMenuState_TopMenu, Menu_Inventory_EquipmentMenuState_Quit, Menu_Inventory_EquipmentMenuState_InitEquipSubMenu, Menu_Inventory_EquipmentMenuState_EquipSubMenu };


    static int Menu_Inventory_equipmentMenuSelectedRow;
    static char Menu_Inventory_equipmentMenuState;
    static char Menu_Inventory_equipmentMenuLayoutPadding[3] __attribute__((unused));

    char* rowStrings[16];
    int rowTypes[8];
    char buf[768];
    int j;
    int i;
    Menu_Framework_MenuRow* menuItem;
    Items_InventoryShield* shield;
    Items_InventoryWeapon* weapon;

    if (initialize != 0) {
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

        Menu_Inventory_selectedItemCategoryIconOnTop = 1;
        Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_Init;

        return 0;
    }

    switch (Menu_Inventory_equipmentMenuState) {
    case Menu_Inventory_EquipmentMenuState_Init:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        menuItem = Menu_Framework_SetMenuItem(
            31, 320, 18, 126, 8, (char*)&Menu_Inventory_menuText[Text_Menu3_menuText_Offset_equip]);
        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_InitMenu;
        // Fallthrough

    case Menu_Inventory_EquipmentMenuState_InitMenu:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Inventory_selectedItemCategoryIconOnTop = 1;

        for (i = 0; i < 8; ++i) {
            for (j = 0; j < 2; ++j) {
                rowStrings[i * 2 + j] =
                    (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_none + j]];
            }
            rowTypes[i] = 0;
        }

        weapon = Audio_Seq_unusedInventory.weapons;

        for (j = 0; j < 8; ++j, ++weapon) {
            if (weapon->isEquipped != 0) {
                Menu_Framework_FormatInventoryWeaponRow(
                    weapon, (char**)rowStrings, rowTypes, buf);
                break;
            }
        }

        shield = Audio_Seq_unusedInventory.shields;

        for (j = 0; j < 8; ++j, ++shield) {
            if (shield->isEquipped != 0) {
                Menu_Framework_FormatInventoryShieldRow(
                    shield, &rowStrings[2], &rowTypes[1], buf + 96);
                break;
            }
        }

        for (i = 2; i < 8; ++i) {
            Items_InventoryArmor* armor = Audio_Seq_unusedInventory.armor;

            for (j = 0; j < 16; ++j, ++armor) {
                if (((i != 7) + armor->limb) == i) {
                    Menu_Framework_FormatInventoryArmorRow(
                        armor, &rowStrings[i * 2], &rowTypes[i], &buf[i * 96]);
                    break;
                }
            }
        }

        for (i = 0; i < 8; ++i) {
            rowTypes[i] |= Menu_Inventory_equipMenuRowTypes[i] << 9;
        }

        Menu_Framework_CreateMenuRows(8, menuRowInfo(4, 3, 401), &rowStrings[0], rowTypes);
        Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_TopMenu;

        break;

    case Menu_Inventory_EquipmentMenuState_TopMenu:
        if (Input_buttonsPressed.allButtons & PADRleft) {
            Menu_PlayInvalidSound();
        }

        Menu_Inventory_equipmentMenuSelectedRow = Menu_Framework_GetSelectedRow() + 1;

        if (Menu_Inventory_equipmentMenuSelectedRow == Menu_Framework_MenuSelection_Confirm) {
            break;
        }

        if (Menu_Inventory_equipmentMenuSelectedRow < 0) {
            Menu_Inventory_selectedItemCategoryIconOnTop = -Menu_Inventory_equipmentMenuSelectedRow;

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_Quit;
        } else {
            Menu_Inventory_BeginDetailsTransition(Menu_Inventory_equipmentMenuSelectedRow - 1 + 10);

            Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_InitEquipSubMenu;
        }
        break;

    case Menu_Inventory_EquipmentMenuState_Quit:
        if ((Menu_Inventory_equipmentMenuSelectedRow == Menu_Framework_MenuSelection_Quit) || Menu_Framework_AreMenuRowsIdle()) {
            return Menu_Inventory_equipmentMenuSelectedRow;
        }

        break;

    case Menu_Inventory_EquipmentMenuState_InitEquipSubMenu:
        if (!Menu_Framework_AreMenuRowsIdle()) {
            break;
        }

        Menu_Inventory_ashleyRenderState = 1;

        Menu_Framework_ContinueItemDetailsTransition(Menu_Inventory_equipmentMenuSelectedRow + 9);

        Menu_Inventory_itemCategoryIconsEnabled = 0;
        Menu_Inventory_statusCommandState = 0;

        Menu_Inventory_DispatchEquipmentMenu(Menu_Inventory_equipmentMenuSelectedRow);

        Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_EquipSubMenu;
        break;

    case Menu_Inventory_EquipmentMenuState_EquipSubMenu:
        Menu_Inventory_equipmentMenuSelectedRow = Menu_Inventory_DispatchEquipmentMenu(0);

        if (Menu_Inventory_equipmentMenuSelectedRow == 0) {
            break;
        }

        if (Menu_Inventory_equipmentMenuSelectedRow == Menu_Framework_MenuSelection_Quit) {
            Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_Quit;
            break;
        }

        Menu_Inventory_ashleyRenderState = 0;
        Menu_Inventory_itemCategoryIconsEnabled = 1;
        Menu_Inventory_selectedItemCategoryIconOnTop = 2;
        Menu_Inventory_statusCommandState = 1;
        Menu_Inventory_equipmentMenuState = Menu_Inventory_EquipmentMenuState_Init;

        break;
    }

    if (Menu_Inventory_equipmentMenuState == Menu_Inventory_EquipmentMenuState_TopMenu) {
        menuItem = Menu_Framework_SetMenuItem(31, 180, 18, 140, 8,
            (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[Text_Menu3_menuText_Index_equipWeapon
                                        + Menu_Framework_HighlightedRow()]]);
        menuItem->selected = 1;
    }

    return 0;
}

// decomp: MENU3.PRG vs_menu3_exec (src/MENU/MENU3.PRG/16C.c:1236)
int Menu_Inventory_Update(u_char* state)
{
    enum Menu_Inventory_InventoryMenuState {
        Menu_Inventory_InventoryMenuState_None,
        Menu_Inventory_InventoryMenuState_Init = 3,
        Menu_Inventory_InventoryMenuState_LoadItemNamesWait,
        Menu_Inventory_InventoryMenuState_ItemsTop,
        Menu_Inventory_InventoryMenuState_Equip,
        Menu_Inventory_InventoryMenuState_Quit = 9,
        Menu_Inventory_InventoryMenuState_QuitToBattle
    };

    static char Menu_Inventory_closeDelay;

    int category;
    int row;
    char temp_a1;
    char temp_a1_2;

    switch (*state) {
    case Menu_Inventory_InventoryMenuState_Init:
        Menu_Inventory_SetPanelVisibility(0);
        Menu_Framework_SetActorComparisonStats(16);

        Menu_Inventory_secondaryTitleAnimationStep = 0;
        Menu_Inventory_itemCategoryIconsAnimationStep = 0;
        Menu_Inventory_statusCommandAnimationStep = 0;
        Menu_Inventory_selectedItemCategoryIconOnTop = 0;

        if (Menu_Framework_itemNames == NULL) {
            Menu_Framework_LoadItemText(1);
        }

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Field_SetFieldRenderingEnabled(0);
        Menu_Framework_InitializeInformationBox();
        Battle_SetHudHidden(1);

        Battle_hudSlideOffset = 0x200000;

        Menu_Framework_RenderAshley(1);
        Menu_Inventory_UpdateItemList(Menu_Inventory_restoreItemCursor + 1);

        Menu_Inventory_restoreItemCursor = 0;
        *state = Menu_Inventory_InventoryMenuState_LoadItemNamesWait;
        // Fallthrough

    case Menu_Inventory_InventoryMenuState_LoadItemNamesWait:
        if (Menu_Framework_LoadItemText(0) == 0) {
            break;
        }

        *state = Menu_Inventory_InventoryMenuState_ItemsTop;
        // Fallthrough

    case Menu_Inventory_InventoryMenuState_ItemsTop:
        row = Menu_Inventory_UpdateItemList(0);

        if (row == 0) {
            break;
        }

        if (row > 0) {
            switch (row) {
            case 1:
                Menu_Inventory_UpdateEquipmentMenu(1);

                Menu_Inventory_freezeTabArrows = 0;
                *state = Menu_Inventory_InventoryMenuState_Equip;

                break;

            case 2:
                Menu_Inventory_ashleyRenderState = 3;

                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

                Menu_Inventory_secondaryTitleEnabled = 0;
                Menu_Inventory_itemCategoryIconsEnabled = 0;
                Menu_Inventory_statusCommandState = 0;
                *state = 7;

                break;

            case 3:
                Menu_Inventory_SetPanelVisibility(0);
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Field_SetFieldRenderingEnabled(1);

                Menu_Framework_menuState.executeAbilityType = 13;
                *state = 12;

                Battle_SetHudHidden(0);

                Battle_hudSlideOffset = 0;

                Menu_Framework_RenderAshley(2);

                break;
            }
        } else {
            if (row == Menu_Framework_MenuSelection_Quit) {
                *state = Menu_Inventory_InventoryMenuState_QuitToBattle;
            } else {
                *state = Menu_Inventory_InventoryMenuState_Quit;

                Menu_Inventory_SetPanelVisibility(0);
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Field_SetFieldRenderingEnabled(1);
                Battle_SetHudHidden(0);

                Battle_hudSlideOffset = 0;

                Menu_Framework_RenderAshley(2);
            }
        }
        break;

    case Menu_Inventory_InventoryMenuState_Equip:
        row = Menu_Inventory_UpdateEquipmentMenu(0);

        if (row != 0) {
            if (row == Menu_Framework_MenuSelection_Quit) {
                Menu_Inventory_secondaryTitleEnabled = 0;
                Menu_Inventory_secondaryTitleAnimationStep = 0;
                *state = Menu_Inventory_InventoryMenuState_QuitToBattle;
                break;
            }

            Menu_Inventory_itemCategoryIconsEnabled = 1;
            Menu_Inventory_selectedItemCategoryIconOnTop = 0;
            Menu_Inventory_statusCommandState = 1;

            Menu_Inventory_UpdateItemList(1);

            *state = Menu_Inventory_InventoryMenuState_ItemsTop;
        }

        break;

    case 7:
        if (Menu_Framework_AreMenuRowsIdle()) {
            Menu_Framework_RenderAshley(2);

            *state = 8;
            temp_a1 = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 13;
            Menu_Framework_menuState.returnState = temp_a1;
            Menu_Framework_actionRestrictions |= 0x200;
        }
        break;

    case 8:
        if (Menu_Framework_menuState.returnState != 0) {
            Menu_Framework_SetBackgroundPriority(-4, 128);
            Field_SetFieldRenderingEnabled(1);
            Menu_Inventory_SetPanelVisibility(0);
            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Battle_SetHudHidden(0);

            Battle_hudSlideOffset = 0;
            Menu_Inventory_closeDelay = 10;

            *state = 11;
        } else if (Menu_Framework_AreMenuRowsIdle()) {
            Menu_Inventory_SetPanelVisibility(0);

            Menu_Inventory_secondaryTitleAnimationStep = 0;
            Menu_Inventory_itemCategoryIconsAnimationStep = 0;
            Menu_Inventory_statusCommandAnimationStep = 0;
            Menu_Inventory_selectedItemCategoryIconOnTop = 0;

            Menu_Framework_RenderAshley(1);
            Menu_Inventory_UpdateItemList(1);

            *state = Menu_Inventory_InventoryMenuState_ItemsTop;
        }
        break;

    case Menu_Inventory_InventoryMenuState_Quit:
        Menu_Framework_SetBackgroundFade(0);
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() && Menu_Framework_UnloadItemTextWhenReady()) {
            *state = Menu_Inventory_InventoryMenuState_None;
            return 1;
        }

        break;

    case Menu_Inventory_InventoryMenuState_QuitToBattle:
        Field_SetFieldRenderingEnabled(1);
        Menu_Inventory_SetPanelVisibility(0);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Battle_SetHudHidden(0);

        Battle_hudSlideOffset = 0;

        Menu_Framework_RenderAshley(2);

        Menu_Inventory_closeDelay = 10;
        *state = 11;

        break;

    case 11:
        Menu_Framework_SetBackgroundFade(0);
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Inventory_closeDelay != 0) {
            --Menu_Inventory_closeDelay;
            break;
        }

        if (Menu_Framework_AreMenuRowsIdle() && Menu_Framework_UnloadItemTextWhenReady()) {
            Menu_Framework_menuState.currentMenu = 4;
            *state = Menu_Inventory_InventoryMenuState_None;
            return 1;
        }

        break;

    case 12:
        Menu_Framework_SetBackgroundFade(0);
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if ((Menu_Framework_backgroundFadeStep == 0)
            && Menu_Framework_UnloadItemTextWhenReady()) {

            Menu_Framework_actionSelection.selectionKind = 8;
            temp_a1_2 = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 4;
            Menu_Framework_menuState.returnState = temp_a1_2;
            *state = Menu_Inventory_InventoryMenuState_None;

            return 1;
        }

        break;

    case 13:
        Menu_Inventory_restoreItemCursor = 1;
        *state = Menu_Inventory_InventoryMenuState_None;

        break;
    }

    if (Menu_Inventory_secondaryTitleEnabled != 0) {
        if (Menu_Inventory_secondaryTitleAnimationStep < 10) {
            ++Menu_Inventory_secondaryTitleAnimationStep;
        }
        row = Menu_Framework_rowAnimationSteps[10 - Menu_Inventory_secondaryTitleAnimationStep];
    } else {
        if (Menu_Inventory_secondaryTitleAnimationStep != 0) {
            --Menu_Inventory_secondaryTitleAnimationStep;
        }
        row = (10 - Menu_Inventory_secondaryTitleAnimationStep) << 5;
    }

    if ((Menu_Inventory_secondaryTitleAnimationStep != 0) && (Menu_Inventory_selectedItemCategoryIconOnTop == 0)) {

        int category = (Menu_Framework_menuMemory.opaque3A.currentCategory - 1) & 7;

        Menu_Framework_MenuRow* menuitem = Menu_Framework_SetMenuItem(
            31, row + 180, 34, 140, 8, (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[category]]);
        menuitem->selected = 1;

        if (category != 7) {
            menuitem->max = Items_Inventory_inventoryItemCapacities[category];
            menuitem->count = Items_Inventory_CountItems(category, 0);
        }
    }

    Menu_Framework_RenderAshley(0);

    if (Menu_Inventory_itemCategoryIconsEnabled) {
        if (Menu_Inventory_itemCategoryIconsAnimationStep < 4) {
            ++Menu_Inventory_itemCategoryIconsAnimationStep;
        }
    } else if (Menu_Inventory_itemCategoryIconsAnimationStep != 0) {
        --Menu_Inventory_itemCategoryIconsAnimationStep;
    }

    Menu_Inventory_DrawCategoryIcons(Menu_Inventory_itemCategoryIconsAnimationStep, Menu_Inventory_selectedItemCategoryIconOnTop);

    if (Menu_Inventory_statusCommandState) {
        if (Menu_Inventory_statusCommandAnimationStep < 10) {
            ++Menu_Inventory_statusCommandAnimationStep;
        }
        row = Menu_Framework_rowAnimationSteps[10 - Menu_Inventory_statusCommandAnimationStep];
    } else {
        if (Menu_Inventory_statusCommandAnimationStep != 0) {
            --Menu_Inventory_statusCommandAnimationStep;
        }
        row = (10 - Menu_Inventory_statusCommandAnimationStep) << 5;
    }

    if (Menu_Inventory_statusCommandAnimationStep != 0) {
        Menu_Framework_DrawButtonPromptBackground(16 - row, 38, 88, 10);
        Menu_Framework_DrawButtonIcon(Menu_Framework_ButtonId_Square, 8 - row, 36, NULL);
        Render_DrawCompactTextColor("STATUS", vs_getXY(28 - row, 38),
            vs_getRGB888(32, 32, 32) << Menu_Inventory_statusCommandState, NULL);
    }

    return 0;
}

// decomp: MENU3.PRG _loadObjectData (src/MENU/MENU3.PRG/68C4.c:12)
int Menu_Inventory_ReloadEquipmentModels(int category, int wepId, int material)
{
    int _[6] __attribute__((unused));
    int i;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[1];

    switch (Menu_Inventory_equipmentModelLoadStep) {
    case 0:
        if (category == 0) {

            Content_ReleaseWeaponModel(1);

            if ((i = wepId) && Menu_Inventory_IsTwoHandedWeapon(temp_s1->sequenceModelId)) {
                Content_ReleaseWeaponModel(0);
                Menu_Framework_QueueAshleyWeaponAnimations(10);
            }
        } else {
            if (category != temp_s1->sequenceModelId) {
                Menu_Framework_QueueAshleyWeaponAnimations(category);
            }

            Content_ReleaseWeaponModel(0);

            if (Battle_unusedGlobal2 && Menu_Inventory_IsTwoHandedWeapon(category)) {
                Content_ReleaseWeaponModel(1);
            }
        }

        Menu_Inventory_QueueEquipmentModel(wepId, material);
        Actors_SetActorWeaponDrawn(1, 0, 1);

        ++Menu_Inventory_equipmentModelLoadStep;
        break;

    case 1:
        if ((Content_UpdateActorAssetQueue() == 0) && (temp_s1->animationId == 1)) {
            Menu_Framework_ResetAshleyEquipmentPose();

            ++Menu_Inventory_equipmentModelLoadStep;
        }
        break;

    case 2:
        Menu_Framework_ReleaseActorPreview();
        Menu_Framework_CloneActorPreview(0);
        Menu_Framework_ResetActorPreviewPose();

        temp_s1->instance.weaponDrawn = 0;

        Actors_PlayActorAnimation(1, 1, 0, 0);
        Actors_AdvanceSkeletalAnimation(temp_s1, &temp_s1->pose, 1);
        Core_Memory_CopyAlignedWords(&temp_s1->blendPose, &temp_s1->pose, sizeof temp_s1->blendPose);

        for (i = 0; i < 2; ++i) {
            if (Actors_weaponModels[i + 2] != NULL) {
                Actors_weaponModels[i + 2]->reserved11 = 0;
            }
        }

        Actors_SetActorWeaponDrawn(1, 1, 1);

        ++Menu_Inventory_equipmentModelLoadStep;
        break;

    case 3:
        if (temp_s1->animationId == 101) {
            Menu_Inventory_equipmentModelLoadStep = 0;
            return 0;
        }

        break;
    }
    return 1;
}

// decomp: MENU3.PRG func_80109320 (src/MENU/MENU3.PRG/68C4.c:87)
int Menu_Inventory_IsTwoHandedWeapon(int category)
{
    if (category < 3) {
        return 0;
    }

    if (category == 8) {
        return 1;
    }

    return category & 1;
}

// decomp: MENU3.PRG _populateDataSlot (src/MENU/MENU3.PRG/68C4.c:100)
void Menu_Inventory_QueueEquipmentModel(int wepId, int material)
{
    Content_ActorAssetRequest objData;
    int index;
    int var_a3;

    if (wepId == 0) {
        return;
    }

    index = 0;

    if (wepId < 96) {
        var_a3 = 240;
    } else {
        index = 1;
        var_a3 = 241;
    }

    objData.assetKind = 3;
    objData.targetSlot = index;
    objData.modelId = wepId;
    objData.destination = Battle_characterState->equipmentModels[index];
    objData.subKind = 0;
    objData.textureSlot = var_a3;
    objData.material = material;

    Content_QueueActorAssetRequest(&objData);
}

// MAINMENU.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ResetMainMenuStatics(void)
{
    {
        static const char initial[] = "\x16\x12\x1C\x0C\xE7\x00\x00\xF8";
        memcpy((void*)&Menu_Inventory_miscItemShortcutMisc, &initial, sizeof Menu_Inventory_miscItemShortcutMisc);
    }
}

// MENU3.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ResetMenu3Statics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyRenderState, &initial, sizeof Menu_Inventory_ashleyRenderState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyYOffset, &initial, sizeof Menu_Inventory_ashleyYOffset);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_equipmentChoicesCursorAnimation, &initial, sizeof Menu_Inventory_equipmentChoicesCursorAnimation);
    }
    {
        static const char initial[] = { 0, 5, 5, 2, 3, 4, 6, 7 };
        memcpy((void*)&Menu_Inventory_armorMenuBodyPartCategories, &initial, sizeof Menu_Inventory_armorMenuBodyPartCategories);
    }
    {
        static const int (*initial[])(int) = { Menu_Inventory_UpdateArmorMenu, Menu_Inventory_UpdateWeaponMenu,
        Menu_Inventory_UpdateShieldMenu, Menu_Inventory_UpdateAccessoryMenu };
        memcpy((void*)&Menu_Inventory_dispatchEquipmentMenuEquipMenuHandlers, &initial, sizeof Menu_Inventory_dispatchEquipmentMenuEquipMenuHandlers);
    }
    {
        static const char initial[] = { 0x78, 0x79, 0x69, 0x6A, 0x68, 0x6D, 0x6B, 0x7A };
        memcpy((void*)&Menu_Inventory_equipMenuRowTypes, &initial, sizeof Menu_Inventory_equipMenuRowTypes);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Inventory_restoreItemCursor, &initial, sizeof Menu_Inventory_restoreItemCursor);
    }
}

// MENUB.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Inventory_ResetMenubStatics(void)
{
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyRenderState, &initial, sizeof Menu_Inventory_ashleyRenderState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Inventory_ashleyYOffset, &initial, sizeof Menu_Inventory_ashleyYOffset);
    }
}

