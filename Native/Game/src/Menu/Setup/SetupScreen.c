// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Feedback.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "DiscQueue.h"
#include "Field/RoomLoading.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/EquipmentStats.h"
#include "Items/Inventory.h"
#include "Items/Weapons.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Framework/EquipmentDetails.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/ItemRows.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Setup/Assembly.h"
#include "Menu/Setup/Combination.h"
#include "Menu/Setup/SetupScreen.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Settings.h"

// decomp: _itemCount (_itemCount@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_itemCount;

// decomp: _itemsList (_itemsList@src/MENU/MENUC.PRG/168.c)
u_short* Menu_Setup_itemsList;

// decomp: _itemsListSelection (_itemsListSelection@src/MENU/MENUC.PRG/168.c)
int Menu_Setup_itemsListSelection;

// decomp: _itemsListState (_itemsListState@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_itemsListState;

// decomp: _itemsListWindow (_itemsListWindow@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_itemsListWindow;

// decomp: _rowInfoUnused (_rowInfoUnused@src/MENU/MENUC.PRG/168.c)
char Menu_Setup_rowInfoUnused;

// decomp: local static of _navigateItemsList, at file scope so the module reset reaches it
static char Menu_Setup_itemListCursorAnimState = 0;

// decomp: local static of _confirmCombine, at file scope so the module reset reaches it
static char Menu_Setup_threeChoicePromptCursorState = 0;

// decomp: local static of _confirmationPrompt, at file scope so the module reset reaches it
static char Menu_Setup_confirmationPromptCursorState = 0;

// decomp: MENUC.PRG _drawOk (src/MENU/MENUC.PRG/168.c:19)
void Menu_Setup_DrawConfirmationLabel(int row, int brightness)
{
    Render_DrawCompactTextColor("OK", vs_getXY(175, row * 16 + 32),
        ((Core_Gte_Sin(brightness * 8) >> 5) + 64) * vs_getRGB888(1, 1, 1),
        Render_frameScratch.orderingTable - 0xC);
}

// decomp: MENUC.PRG _setMenuTitle (src/MENU/MENUC.PRG/168.c:29)
void Menu_Setup_SetTitleRow(int id, int textTableOffset)
{
    Menu_Framework_MenuRow* menuItem;

    int y = 18;

    if (id != 0) {
        y = 34;
    }

    menuItem = Menu_Framework_SetMenuItem(
        id, 320, y, 140, 8, (char*)&Menu_Framework_menu12Text[textTableOffset]);
    menuItem->state = Menu_Framework_MenuItemState_SlideX;
    menuItem->targetPosition0 = 180;
    menuItem->selected = 1;
}

// decomp: MENUC.PRG _initMenuItem (src/MENU/MENUC.PRG/168.c:110)
void Menu_Setup_InitializeEquipmentHeader(int textOffset, int icon)
{
    int a0;
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(
        32, -164, 18, 164, 8, (char*)&Menu_Framework_menu12Text[textOffset]);
    menuItem->state = 5;
    menuItem->targetPosition0 = 16;
    menuItem->selected = 1;
    menuItem->rowIcon = icon;

    Menu_Framework_ResetEquipmentStats();
    a0 = 7;
    if (icon == 24) {
        Menu_Framework_SetRangeAndRisk(0, 0, 0, 1);
        a0 = 3;
    }

    Menu_Framework_UpdateEquipmentStatPages(a0);

    a0 = 3;
    if (icon == 28) {
        a0 = 1;
    }

    Menu_Framework_UpdateDpPpBars(a0);
    Menu_Framework_UpdatePrimaryStatPanel(1);
}

// decomp: MENUC.PRG _copyMenuItem (src/MENU/MENUC.PRG/168.c:141)
void Menu_Setup_MoveMenuRow(int source, int target)
{
    if (source != target) {
        Core_Memory_CopyHalfwords(&Menu_Framework_menuItems[target], &Menu_Framework_menuItems[source],
            sizeof Menu_Framework_menuItems[target]);
        Menu_Framework_DeactivateRow(source);
    }
}

// decomp: MENUC.PRG _confirmationPrompt (src/MENU/MENUC.PRG/168.c:155)
int Menu_Setup_UpdateConfirmationPrompt(int initialize_confirmation)
{
    static char Menu_Setup_confirmationPromptState;
    static char Menu_Setup_confirmationPromptSelectedOption;

    int i;
    Menu_Framework_MenuRow* menuItem;

    int action = 0;

    if (initialize_confirmation != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Setup_confirmationPromptSelectedOption = 0;
        Menu_Setup_confirmationPromptState = 0;
        return 0;
    }

    switch (Menu_Setup_confirmationPromptState) {
    case 0:
    case 1:
        menuItem =
            Menu_Framework_SetMenuItem(Menu_Setup_confirmationPromptState + 29, 320, (Menu_Setup_confirmationPromptState * 16) + 146, 320 - 194, 0,
                (char*)&Menu_Framework_menu12Text
                    [Menu_Framework_menu12Text[Menu_Setup_confirmationPromptState + Text_Menu_MENU12_BIN_Index_optionYes]]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        ++Menu_Setup_confirmationPromptState;
        break;
    case 2:
        Menu_Setup_confirmationPromptState += Menu_Framework_AreMenuRowsIdle();
        break;
    case 3:
        for (i = 0; i < 2; ++i) {
            Menu_Framework_MenuItem(i + 29)->selected = i == Menu_Setup_confirmationPromptSelectedOption;
        }
        if (Input_buttonsPressed.allButtons & PADRup) {
            action = 3;
        } else if (Input_buttonsPressed.allButtons & PADRdown) {
            action = 2;
        } else if (Input_buttonsPressed.allButtons & PADRright) {
            action = Menu_Setup_confirmationPromptSelectedOption + 1;
        } else {
            if (Input_buttonRepeat & (PADLup | PADLdown)) {
                Menu_PlayChangeSound();
                Menu_Setup_confirmationPromptSelectedOption = 1 - Menu_Setup_confirmationPromptSelectedOption;
            }
            Menu_Setup_confirmationPromptCursorState = Menu_Framework_DrawMainMenuCursor(Menu_Setup_confirmationPromptCursorState, Menu_Setup_confirmationPromptSelectedOption + 8);
        }
        break;
    }

    if (action != 0) {
        if (action == 2) {
            Menu_PlayLeaveSound();
        }
        for (i = 29; i < 31; ++i) {
            Menu_Framework_FlyOutRowRight(i);
        }
    }

    return action;
}

// decomp: MENUC.PRG _confirmCombine (src/MENU/MENUC.PRG/168.c:225)
int Menu_Setup_UpdateThreeChoicePrompt(int cancel_combine_row)
{
    static char Menu_Setup_threeChoicePromptState;
    static char Menu_Setup_threeChoicePromptSelectedOption;
    static char Menu_Setup_threeChoicePromptCancelCombinePos;
    static char Menu_Setup_threeChoicePromptLayoutPadding[3] __attribute__((unused));

    int i;
    u_short* text;
    Menu_Framework_MenuRow* menuItem;

    int action = 0;

    if (cancel_combine_row != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Setup_threeChoicePromptCancelCombinePos = cancel_combine_row;
        Menu_Setup_threeChoicePromptSelectedOption = 0;
        Menu_Setup_threeChoicePromptState = 0;
        return 0;
    }

    switch (Menu_Setup_threeChoicePromptState) {
    case 0:
    case 1:
    case 2:
        text = &Menu_Framework_menu12Text
                   [Menu_Framework_menu12Text[Menu_Setup_threeChoicePromptState + Text_Menu_MENU12_BIN_Index_optionYes]];
        if (Menu_Setup_threeChoicePromptState == Menu_Setup_threeChoicePromptCancelCombinePos) {
            text = &Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_cancelCombine];
        }
        menuItem = Menu_Framework_SetMenuItem(
            Menu_Setup_threeChoicePromptState + 29, 320, (Menu_Setup_threeChoicePromptState * 16) + 130, 320 - 194, 0, (char*)text);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;
        ++Menu_Setup_threeChoicePromptState;
        break;
    case 3:
        Menu_Setup_threeChoicePromptState += Menu_Framework_AreMenuRowsIdle();
        break;
    case 4:
        for (i = 0; i < 3; ++i) {
            Menu_Framework_MenuItem(i + 29)->selected = i == Menu_Setup_threeChoicePromptSelectedOption;
        }
        if (Input_buttonsPressed.allButtons & PADRup) {
            action = 3;
        } else if (Input_buttonsPressed.allButtons & PADRdown) {
            action = 2;
        } else if (Input_buttonsPressed.allButtons & PADRright) {
            action = Menu_Setup_threeChoicePromptSelectedOption + 1;
        } else {
            i = Menu_Setup_threeChoicePromptSelectedOption;
            if (Input_buttonRepeat & PADLup) {
                i += 2;
            }
            if (Input_buttonRepeat & PADLdown) {
                ++i;
            }
            if (i >= 3) {
                i -= 3;
            }
            if (i != Menu_Setup_threeChoicePromptSelectedOption) {
                Menu_PlayChangeSound();
                Menu_Setup_threeChoicePromptSelectedOption = i;
            }
            Menu_Setup_threeChoicePromptCursorState = Menu_Framework_DrawMainMenuCursor(Menu_Setup_threeChoicePromptCursorState, Menu_Setup_threeChoicePromptSelectedOption + 7);
        }
        break;
    }

    if (action != 0) {
        if (action == 2) {
            Menu_PlayLeaveSound();
        }
        for (i = 29; i < 32; ++i) {
            Menu_Framework_FlyOutRowRight(i);
        }
    }
    return action;
}

// decomp: MENUC.PRG _leaveItemSlotSelection (src/MENU/MENUC.PRG/168.c:419)
int Menu_Setup_LeaveItemSelection(int hide_equipment_panels)
{

    Menu_PlayLeaveSound();
    Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
    Menu_Framework_FlyOutRowLeft(0);

    if (hide_equipment_panels != 0) {
        Menu_Framework_UpdateEquipmentStatPages(-1);
        Menu_Framework_UpdatePrimaryStatPanel(2);
        Menu_Framework_UpdateDpPpBars(4);
    }

    Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);

    return Input_buttonsPressed.allButtons & PADRup ? -2 : -1;
}

// decomp: MENUC.PRG _populateItemsList (src/MENU/MENUC.PRG/168.c:450)
void Menu_Setup_BuildItemList(int count, char** text, int* rowTypes)
{
    int j;
    int i;
    u_short* encoded_row_text;
    Menu_Framework_MenuRow* menuItem;

    Menu_Setup_itemCount = count;
    Menu_Setup_itemsListWindow = 0;
    Menu_Setup_itemsListSelection = 0;
    Menu_Setup_itemsListState = 1;
    /* Actually something along the lines of an array of
        struct _itemInfo{
            u_short title[14];
            int     rowType;
            u_short description[48];
        };
    but this doesn't match */
    Menu_Setup_itemsList = Core_Memory_AllocateFromHighEnd(count << 7);

    for (i = 0; i < count; ++i) {

        *((int*)(&(&Menu_Setup_itemsList[i * 0x40])[14])) = rowTypes[i];

        encoded_row_text = (u_short*)text[i * 2];

        if (encoded_row_text != NULL) {
            for (j = 0; j < 13; ++j) {
                Menu_Setup_itemsList[(i * 0x40) + j] = encoded_row_text[j];
            }
            Menu_Setup_itemsList[(i * 0x40) + 13] = 0xE7E7;
        } else {
            Menu_Setup_itemsList[i * 0x40] = 0xE7E7;
        }

        encoded_row_text = (u_short*)text[i * 2 + 1];

        if (encoded_row_text != NULL) {
            Menu_Setup_itemsList[(i * 0x40) + 16] = 0xF8;
            for (j = 0; j < 46; ++j) {
                Menu_Setup_itemsList[(i * 0x40) + j + 17] = encoded_row_text[j];
            }
            Menu_Setup_itemsList[(i * 0x40) + 63] = 0xE7E7;
        } else {
            Menu_Setup_itemsList[(i * 0x40) + 16] = 0xE7E7;
        }
    }

    j = rowTypes[0];
    menuItem = Menu_Framework_SetMenuItem(20, 155, 18, 165, 0, (char*)Menu_Setup_itemsList);
    menuItem->unselectable = j & 1;
    menuItem->rowIcon = j >> 26;
    menuItem->material = (j >> 16) & 7;
    menuItem->selected = 1;
    Menu_Setup_rowInfoUnused = (j >> 19) & 0x7F;
}

// decomp: MENUC.PRG _getSelectedRow (src/MENU/MENUC.PRG/168.c:511)
int Menu_Setup_GetSelectedItemRow(void) { return Menu_Setup_itemsList == NULL ? Menu_Setup_itemsListSelection : -1; }

// decomp: MENUC.PRG _navigateItemsList (src/MENU/MENUC.PRG/168.c:518)
void Menu_Setup_UpdateItemList(int list_update_flags)
{

    int itemInfo;
    int previousSelection;
    int v0;
    int outset_icon_code;
    int i;
    int previousWindow;
    Menu_Framework_MenuRow* menuItem;

    if (Menu_Setup_itemsListState < 6) {
        if (Menu_Setup_itemsListState == Menu_Setup_itemCount) {
            Menu_Setup_itemsListState = 16;
        } else {
            menuItem = Menu_Framework_SetMenuItem(Menu_Setup_itemsListState + 20, 320,
                (Menu_Setup_itemsListState * 16) + 18, 151, 0,
                (char*)&Menu_Setup_itemsList[Menu_Setup_itemsListState * 64]);
            menuItem->state = 2;
            menuItem->targetPosition0 = 169;
            itemInfo = *(int*)&Menu_Setup_itemsList[Menu_Setup_itemsListState * 64 + 14];
            menuItem->unselectable = itemInfo & 1;
            menuItem->rowIcon = (itemInfo >> 0x1A);
            v0 = (itemInfo >> 9) & 0x7F;
            if (v0 != 0) {
                menuItem->outsetIcon = v0 - 100;
            }
            menuItem->material = (itemInfo >> 0x10) & 7;
            ++Menu_Setup_itemsListState;
            if (Menu_Setup_itemsListState == 6) {
                if (Menu_Setup_itemCount >= 7) {
                    menuItem->isScrollable = 2;
                }
                Menu_Setup_itemsListState = 16;
            }
        }
    } else {
        previousWindow = Menu_Setup_itemsListWindow;
        previousSelection = Menu_Setup_itemsListSelection + previousWindow;

        menuItem = Menu_Framework_MenuItem(Menu_Setup_itemsListSelection + 20);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Setup_itemsList[previousSelection * 64 + 16]);

        switch (Menu_Setup_itemsListState) {
        case 16:
            if (Menu_Framework_AreMenuRowsIdle() != 0) {
                Menu_Setup_itemsListState = 17;
                return;
            }
            return;

        case 17:
            if (Input_buttonsPressed.allButtons & PADRup) {
                if (!(list_update_flags & 2)) {
                    Menu_PlayLeaveSound();
                }

                Core_Memory_FreeFromHighEnd(Menu_Setup_itemsList);

                Menu_Setup_itemsList = NULL;
                Menu_Setup_itemsListSelection = -3;
                return;
            }

            if (Input_buttonsPressed.allButtons & PADRright) {
                if (menuItem->unselectable != 0) {
                    Menu_PlayInvalidSound();
                } else {
                    Core_Memory_FreeFromHighEnd(Menu_Setup_itemsList);

                    Menu_Setup_itemsList = NULL;
                    Menu_Setup_itemsListSelection = previousSelection;

                    if (list_update_flags != 0) {
                        if (previousSelection == 0) {
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
                Core_Memory_FreeFromHighEnd(Menu_Setup_itemsList);

                Menu_Setup_itemsList = NULL;
                Menu_Setup_itemsListSelection = -2;
                return;
            }

            if (Input_buttonRepeat & PADLup) {
                if (Menu_Setup_itemCount < 7 || Menu_Setup_itemsListWindow == 0) {
                    if (Menu_Setup_itemsListSelection != 0) {
                        --Menu_Setup_itemsListSelection;
                    }
                } else {
                    if (Menu_Setup_itemsListSelection == 2) {
                        --Menu_Setup_itemsListWindow;
                    } else {
                        --Menu_Setup_itemsListSelection;
                    }
                }
            }

            if (Input_buttonRepeat & PADLdown) {
                if (Menu_Setup_itemCount < 7) {
                    if (Menu_Setup_itemsListSelection < (Menu_Setup_itemCount - 1)) {
                        ++Menu_Setup_itemsListSelection;
                    }
                } else {
                    if (Menu_Setup_itemsListWindow == (Menu_Setup_itemCount - 6)) {
                        if (Menu_Setup_itemsListSelection < 5) {
                            ++Menu_Setup_itemsListSelection;
                        }
                    } else if (Menu_Setup_itemsListSelection == 4) {
                        ++Menu_Setup_itemsListWindow;
                    } else {
                        ++Menu_Setup_itemsListSelection;
                    }
                }
            }

            if (previousSelection != (Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow)) {

                Menu_PlayChangeSound();

                if (Menu_Setup_itemsListWindow != previousWindow) {

                    char saved_row_gradient_states[Menu_Setup_itemCount];

                    for (i = 0; i < Menu_Setup_itemCount; ++i) {
                        saved_row_gradient_states[i] = 0;
                    }

                    itemInfo = Menu_Setup_itemCount;

                    if (itemInfo >= 7u) {
                        itemInfo = 6;
                    }

                    for (i = 1; i < itemInfo; ++i) {
                        menuItem = Menu_Framework_MenuItem(i + 20);
                        saved_row_gradient_states[i + previousWindow] = menuItem->gradientState;
                    }

                    for (i = 1;;) {
                        int v0;
                        menuItem = Menu_Framework_SetMenuItem(i + 20, 169, i * 16 + 18, 151, 0,
                            (char*)&Menu_Setup_itemsList[(i + Menu_Setup_itemsListWindow) * 64]);
                        menuItem->gradientState = *(saved_row_gradient_states + (i + Menu_Setup_itemsListWindow));
                        v0 = Menu_Setup_itemsListWindow;
                        v0 = i + Menu_Setup_itemsListWindow;
                        itemInfo = *(int*)&Menu_Setup_itemsList[v0 * 64 + 14];
                        menuItem->unselectable = itemInfo & 1;
                        menuItem->rowIcon = (itemInfo >> 0x1A);
                        outset_icon_code = (itemInfo >> 9) & 0x7F;

                        if (outset_icon_code != 0) {
                            menuItem->outsetIcon = outset_icon_code - 100;
                        }

                        menuItem->material = (itemInfo >> 0x10) & 7;
                        ++i;

                        if (i == Menu_Setup_itemCount) {
                            break;
                        }

                        if (i == 6) {
                            if ((Menu_Setup_itemsListWindow + 6) < Menu_Setup_itemCount) {
                                menuItem->isScrollable = 2;
                            }
                            break;
                        }
                    }

                    if (Menu_Setup_itemsListWindow != 0) {
                        Menu_Framework_MenuItem(21)->isScrollable = 1;
                    }
                }

                itemInfo = *(
                    int*)&Menu_Setup_itemsList[(Menu_Setup_itemsListSelection + Menu_Setup_itemsListWindow) * 64 + 14];
                Menu_Setup_rowInfoUnused = (itemInfo >> 0x13) & 0x7F;
            }

            Menu_Framework_MenuItem(Menu_Setup_itemsListSelection + 20)->selected = 1;

            i = (((Menu_Setup_itemsListSelection * 16) + 10) << 0x10) | 155;

            if (Menu_Setup_itemsListSelection == 0) {
                i -= 14;
            }

            Menu_Setup_itemListCursorAnimState = Menu_Framework_DrawCursor(Menu_Setup_itemListCursorAnimState, i);
            break;
        }
    }
}

// decomp: MENUC.PRG _initMenuHeader (src/MENU/MENUC.PRG/168.c:784)
void Menu_Setup_AnimateItemHeader(int menu_row_id)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_MenuItem(menu_row_id);
    menuItem->state = 4;
    menuItem->targetPosition0 = 155;
    menuItem->targetPosition1 = 18;
}

// decomp: MENUC.PRG _disassembleMenu (src/MENU/MENUC.PRG/168.c:2100)
int Menu_Setup_UpdateDisassembly(int equipment_category_selection)
{
    static int Menu_Setup_disassemblySelectedRow;
    static char Menu_Setup_disassemblyState;
    static char Menu_Setup_disassemblyAppearStep;
    static char Menu_Setup_disassemblyIsShield;

    char* menuText[16];
    int rowTypes[8];
    int confirmation;
    int count;
    int i;
    int j;
    int selectedItem;
    int itemInfo;
    Items_InventoryShield* shield;
    Menu_Framework_MenuRow* menuItem;

    if (equipment_category_selection != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Setup_disassemblyIsShield = equipment_category_selection - 1;
        Menu_Setup_disassemblyState = 0;
        return 0;
    }

    switch (Menu_Setup_disassemblyState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Setup_SetTitleRow(0,
                Menu_Framework_menu12Text[Menu_Setup_disassemblyIsShield + Text_Menu_MENU12_BIN_Index_disassembleWeapon]);
            Menu_Setup_disassemblyState = 1;
        }
        break;
    case 1:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            count = 0;
            for (i = 0; i < 8; i++) {
                if (Menu_Setup_disassemblyIsShield != 0) {
                    selectedItem = Audio_Seq_unusedInventoryIndices.shields[i];
                    if (selectedItem != 0) {
                        shield = &Audio_Seq_unusedInventory.shields[selectedItem - 1];
                        Menu_Framework_FormatInventoryShieldRow(shield,
                            &menuText[count * 2], &rowTypes[count], Menu_Framework_stringBuf);
                        menuText[count * 2 + 1] = (char*)&Menu_Framework_menu12Text
                            [Text_Menu_MENU12_BIN_Offset_chooseShieldDisassemble];
                        for (j = 0; j < shield->base.gemSlots; ++j) {
                            if (shield->gems[j] != 0) {
                                break;
                            }
                        }
                        if (j == shield->base.gemSlots) {
                            menuText[count * 2 + 1] = (char*)&Menu_Framework_menu12Text
                                [Text_Menu_MENU12_BIN_Offset_noGems];
                            rowTypes[count] |= 1;
                        }
                        ++count;
                        Menu_Setup_availableItems[count] = selectedItem;
                    }
                } else {
                    selectedItem = Audio_Seq_unusedInventoryIndices.weapons[i];
                    if (selectedItem != 0) {
                        Menu_Framework_FormatInventoryWeaponRow(
                            &Audio_Seq_unusedInventory.weapons[selectedItem - 1],
                            &menuText[count * 2], &rowTypes[count], Menu_Framework_stringBuf);
                        menuText[count * 2 + 1] = (char*)&Menu_Framework_menu12Text
                            [Text_Menu_MENU12_BIN_Offset_chooseWeaponDisassemble];
                        ++count;
                        Menu_Setup_availableItems[count] = selectedItem;
                    }
                }
            }
            Menu_Framework_CreateMenuRows(count, (Menu_Setup_disassemblyIsShield + 58) | 0x19100, menuText, rowTypes);
            Menu_Setup_disassemblyState = 2;
        }
        break;
    case 2:
        Menu_Setup_disassemblySelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Setup_disassemblySelectedRow == 0) {
            break;
        }
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        if (Menu_Setup_disassemblySelectedRow < 0) {
            return Menu_Setup_disassemblySelectedRow;
        }
        menuItem = Menu_Framework_MenuItem(0);
        menuItem->state = 2;
        menuItem->w = 164;
        menuItem->targetPosition0 = 16;
        menuItem = Menu_Framework_MenuItem(Menu_Setup_disassemblySelectedRow + 9);
        menuItem->state = 4;
        menuItem->targetPosition0 = 155;
        menuItem->targetPosition1 = 18;
        Menu_Setup_disassemblyState = 3;
        break;
    case 3:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }
        Menu_Framework_MenuItem(0)->rowIcon = Menu_Setup_disassemblyIsShield != 0 ? 27 : 24;
        Menu_Setup_MoveMenuRow(0, 0x20);
        Menu_Setup_MoveMenuRow(Menu_Setup_disassemblySelectedRow + 9, 10);
        selectedItem = Menu_Setup_availableItems[Menu_Setup_disassemblySelectedRow];
        Menu_Setup_combiningItem = selectedItem - 1;
        if (Menu_Setup_disassemblyIsShield != 0) {
            Menu_Framework_SetShieldStats(selectedItem);
        } else {
            Menu_Framework_SetWeaponStats(selectedItem);
        }
        Menu_Framework_UpdateEquipmentStatPages(7);
        Menu_Framework_UpdateDpPpBars(3);
        Menu_Setup_disassemblyAppearStep = 0;
        Menu_Setup_disassemblyState = 4;
        break;
    case 4:
        if (Menu_Setup_disassemblyAppearStep < 5) {
            ++Menu_Setup_disassemblyAppearStep;
            if (Menu_Setup_disassemblyIsShield != 0) {
                Menu_Framework_CreateShieldGemRow(Menu_Setup_disassemblyAppearStep, Menu_Setup_combiningItem + 1, 1);
            } else {
                Menu_Framework_CreateWeaponComponentRow(Menu_Setup_disassemblyAppearStep, Menu_Setup_combiningItem + 1, 1);
            }
            break;
        }
        if (Menu_Setup_disassemblyIsShield != 0) {
            itemInfo = Audio_Seq_unusedInventory.shields[Menu_Setup_combiningItem].isEquipped;
        } else {
            itemInfo = Audio_Seq_unusedInventory.weapons[Menu_Setup_combiningItem].isEquipped;
        }
        Menu_Setup_UpdateConfirmationPrompt(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text
                    [Menu_Setup_disassemblyIsShield * 4 + Text_Menu_MENU12_BIN_Index_disassembleWeaponConfirm
                        + (itemInfo != 0)]]);
        Menu_Setup_disassemblyState = itemInfo + 5;
        break;
    case 5:
    case 6:
        confirmation = Menu_Setup_UpdateConfirmationPrompt(0);
        if (confirmation == 0) {
            break;
        }
        Menu_Framework_UpdateEquipmentStatPages(-1);
        Menu_Framework_UpdateDpPpBars(4);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Framework_FlyOutRowLeft(0);

        switch (confirmation) {
        case 1:
            Audio_PlaySfxCentered(0x7E, 0x19);
            if (Menu_Setup_disassemblyIsShield != 0) {
                Menu_Setup_DetachShieldGems(Menu_Setup_combiningItem);
            } else {
                Menu_Setup_DisassembleWeapon(Menu_Setup_combiningItem);
            }
            if (Menu_Setup_disassemblyState == 5) {
                return -1;
            }
            if (Menu_Setup_disassemblyIsShield != 0) {
                Items_Inventory_UnequipShield();
            } else {
                Items_Inventory_UnequipWeapon();
            }
            Menu_Setup_disassemblyState = 7;
            break;
        case 2:
            Menu_Setup_disassemblyState = 0;
            break;
        case 3:
            return Menu_Setup_LeaveItemSelection(0);
        }
        break;
    case 7:
        if (Menu_Setup_ReloadAshleyEquipment(Menu_Setup_disassemblyIsShield) == 0) {
            return -1;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _disassembleTopMenu (src/MENU/MENUC.PRG/168.c:2285)
int Menu_Setup_UpdateDisassemblyCategory(int initialize_disassembly_category)
{
    static char Menu_Setup_disassemblyCategoryState;
    static char Menu_Setup_disassemblyCategoryLayoutPadding __attribute__((unused));
    static char Menu_Setup_disassemblyCategoryForceNoCursorMemory;
    static char Menu_Setup_disassemblyCategoryLayoutPadding2[2] __attribute__((unused));

    char* text[4];
    int rowTypes[2];
    int selection;
    int itemCount;
    int i;

    if (initialize_disassembly_category != 0) {
        Menu_Setup_disassemblyCategoryForceNoCursorMemory = 1;
        Menu_Framework_FlyOutRowsAndHoistSelection(2, 1);
        Menu_Setup_disassemblyCategoryState = 0;
        return 0;
    }

    switch (Menu_Setup_disassemblyCategoryState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                text[i * 2] = (char*)&Menu_Framework_menu12Text
                    [Menu_Framework_menu12Text[i * 2 + Text_Menu_MENU12_BIN_Index_weapons]];
                text[i * 2 + 1] = (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text
                        [i + Text_Menu_MENU12_BIN_Index_disassembleWeaponDesc]];

                itemCount = Items_Inventory_CountItems((i != 0) * 3, NULL);
                rowTypes[i] = (itemCount << 9) + 0x400000;

                if (itemCount == 0) {
                    text[i * 2 + 1] =
                        (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_noItems];
                    rowTypes[i] |= 1;
                }
            }
            selection = Input_settings.cursorMemory;
            if (Menu_Setup_disassemblyCategoryForceNoCursorMemory == 0) {
                Input_settings.cursorMemory = 1;
            }
            Menu_Setup_disassemblyCategoryForceNoCursorMemory = 0;
            Menu_Framework_CreateMenuRows(2, 0x239, text, rowTypes);
            Input_settings.cursorMemory = selection;
            Menu_Setup_disassemblyCategoryState = 1;
        }
        break;
    case 1:
        selection = Menu_Framework_GetSelectedRow() + 1;
        if (selection == 0) {
            break;
        }
        if (selection < 0) {
            Menu_Framework_ClearMenuExcept(0);
            return selection;
        }
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Setup_UpdateDisassembly(selection);
        Menu_Setup_disassemblyCategoryState = 2;
        break;
    case 2:
        selection = Menu_Setup_UpdateDisassembly(0);
        if (selection == 0) {
            break;
        }
        if (selection != -1) {
            return selection;
        }
        Menu_Setup_disassemblyCategoryState = 3;
        break;
    case 3:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_InitializeInformationBox();
            Menu_Setup_SetTitleRow(0, Text_Menu_MENU12_BIN_Offset_setup);
            Menu_Setup_SetTitleRow(12, Text_Menu_MENU12_BIN_Offset_disassemble);
            Menu_Setup_disassemblyCategoryState = 0;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _renameWeaponMenu (src/MENU/MENUC.PRG/168.c:2374)
int Menu_Setup_UpdateWeaponRename(int initialize_weapon_rename)
{
    static int Menu_Setup_weaponRenameSelectedRow;
    static char Menu_Setup_weaponRenameState;
    static char Menu_Setup_weaponRenameAppearStep;
    static char Menu_Setup_weaponRenameLayoutPadding[2] __attribute__((unused));

    char* menuText[16];
    int rowTypes[8];
    int i;
    int itemId;
    Menu_Framework_MenuRow* menuItem;

    if (initialize_weapon_rename != 0) {
        Menu_InitInformationTextBox(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_chooseWeaponRename]);
        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
        Menu_Setup_weaponRenameState = 0;
        return 0;
    }

    switch (Menu_Setup_weaponRenameState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Setup_SetTitleRow(0, Text_Menu_MENU12_BIN_Offset_rename);
            Menu_Setup_weaponRenameState = 1;
        }
        break;
    case 1:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            int count = 0;
            for (i = 0; i < 8; ++i) {
                itemId = Audio_Seq_unusedInventoryIndices.weapons[i];
                if (itemId != 0) {
                    Menu_Framework_FormatInventoryWeaponRow(
                        &Audio_Seq_unusedInventory.weapons[itemId - 1], &menuText[count * 2],
                        &rowTypes[count], Menu_Framework_stringBuf);
                    menuText[(count * 2) + 1] = (char*)&Menu_Framework_menu12Text
                        [Text_Menu_MENU12_BIN_Offset_chooseWeaponRename];
                    ++count;
                    Menu_Setup_availableItems[count] = itemId;
                }
            }
            Menu_Framework_CreateMenuRows(count, 0x1913C, menuText, rowTypes);
            Menu_Setup_weaponRenameState = 2;
        }
        break;
    case 2:
        Menu_Setup_weaponRenameSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Setup_weaponRenameSelectedRow == 0) {
            break;
        }

        Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

        if (Menu_Setup_weaponRenameSelectedRow < 0) {
            return Menu_Setup_weaponRenameSelectedRow;
        }
        menuItem = Menu_Framework_MenuItem(0);
        menuItem->state = 2;
        menuItem->w = 164;
        menuItem->targetPosition0 = 16;
        menuItem = Menu_Framework_MenuItem(Menu_Setup_weaponRenameSelectedRow + 9);
        menuItem->state = 4;
        menuItem->targetPosition0 = 155;
        menuItem->targetPosition1 = 18;
        Menu_Setup_weaponRenameState = 3;
        break;
    case 3:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_MenuItem(0)->rowIcon = 24;
            Menu_Setup_MoveMenuRow(0, 32);
            Menu_Setup_MoveMenuRow(Menu_Setup_weaponRenameSelectedRow + 9, 10);
            itemId = Menu_Setup_availableItems[Menu_Setup_weaponRenameSelectedRow];
            Menu_Setup_combiningItem = itemId - 1;
            Menu_Framework_SetWeaponStats(itemId);
            Menu_Framework_UpdateEquipmentStatPages(7);
            Menu_Framework_UpdateDpPpBars(3);
            Menu_Setup_weaponRenameAppearStep = 0;
            Menu_Setup_weaponRenameState = 4;
        }
        break;
    case 4:
        if (Menu_Setup_weaponRenameAppearStep < 5) {
            ++Menu_Setup_weaponRenameAppearStep;
            Menu_Framework_CreateWeaponComponentRow(Menu_Setup_weaponRenameAppearStep, Menu_Setup_combiningItem + 1, 1);
            return 0;
        }
        Menu_Setup_UpdateConfirmationPrompt(1);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_renamePrompt]);
        Menu_Setup_weaponRenameState = 5;
        break;
    case 5:
        i = Menu_Setup_UpdateConfirmationPrompt(0);
        if (i != 0) {
            Menu_Framework_UpdateEquipmentStatPages(-1);
            Menu_Framework_UpdateDpPpBars(4);
            switch (i) {
            case 1:
                Menu_PlaySelectSound();
                Menu_Framework_ClearMenuExcept(10);
                Menu_Framework_MenuItem(10)->backgroundWidth = i;
                Menu_Setup_weaponRenameState = 6;
                break;
            case 2:
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_FlyOutRowLeft(0);
                Menu_Setup_weaponRenameState = 0;
                break;
            case 3:
                return Menu_Setup_LeaveItemSelection(0);
            }
        }
        break;
    case 6:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_cursorColor = 0;
            Menu_Framework_stringBuf[0] = 1;
            Menu_Framework_stringBuf[1] = Menu_Setup_combiningItem;
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
            return 1;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _repairMenu (src/MENU/MENUC.PRG/168.c:2509)
int Menu_Setup_UpdateRepair(int initialize_repair)
{
    static int Menu_Setup_repairReturnedFromPrompt;
    static char Menu_Setup_repairState;
    static char Menu_Setup_repairLayoutPadding[3] __attribute__((unused));

    int ppCost;
    int i;
    int repair_item_id;

    if (initialize_repair != 0) {
        Menu_Framework_FlyOutRowsAndHoistSelection(4, 1);
        Menu_Setup_repairState = 0;
        return 0;
    }

    switch (Menu_Setup_repairState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Setup_UpdateConfirmationPrompt(1);
            Menu_Framework_SetInformationMessage(
                (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_repairConfirm]);
            Menu_Setup_repairState = 1;
        }
        break;
    case 1:
        i = Menu_Setup_UpdateConfirmationPrompt(0);
        if (i == 0) {
            break;
        }
        Menu_Setup_repairReturnedFromPrompt = -1;
        if (i == 1) {
            Audio_PlaySfxCentered(0x7E, 0xD);
            Menu_Setup_repairState = 2;
        } else {
            if (i == 3) {
                Menu_PlayLeaveSound();
                return -2;
            }
            Menu_Framework_InitializeInformationBox();
            Menu_Framework_FlyOutRowRight(14);
            Menu_Setup_repairState = 4;
        }
        break;
    case 2:
        Menu_InitInformationTextBox(3);
        Menu_Framework_SetInformationMessage(
            (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_repairComplete]);

        for (i = 0; i < 16; ++i) {
            repair_item_id = Audio_Seq_unusedInventoryIndices.blades[i];
            if (repair_item_id != 0) {
                Items_InventoryBlade* blade = &Audio_Seq_unusedInventory.blades[repair_item_id - 1];

                ppCost = blade->maxDp - blade->currentDp;
                ppCost = blade->currentPp - (ppCost / 100);
                blade->currentDp = blade->maxDp;

                if (ppCost >= 0) {
                    blade->currentPp = ppCost;
                } else {
                    blade->currentPp = 0;
                }
            }
        }

        for (i = 0; i < 8; ++i) {
            repair_item_id = Audio_Seq_unusedInventoryIndices.weapons[i];
            if (repair_item_id != 0) {
                Items_InventoryWeapon* weapon = &Audio_Seq_unusedInventory.weapons[repair_item_id - 1];
                if (weapon->isEquipped != 0) {
                    Items_EquipAshleyWeapon(weapon);
                }
            }
        }

        for (i = 0; i < 8; ++i) {
            repair_item_id = Audio_Seq_unusedInventoryIndices.shields[i];
            if (repair_item_id != 0) {
                Items_InventoryShield* shield = &Audio_Seq_unusedInventory.shields[repair_item_id - 1];

                ppCost = shield->base.maxDp - shield->base.currentDp;
                ppCost = shield->base.currentPp - (ppCost / 100);
                shield->base.currentDp = shield->base.maxDp;

                if (ppCost >= 0) {
                    shield->base.currentPp = ppCost;
                } else {
                    shield->base.currentPp = 0;
                }

                if (shield->isEquipped != 0) {
                    Items_EquipAshleyShield(shield);
                }
            }
        }

        for (i = 0; i < 16; ++i) {
            repair_item_id = Audio_Seq_unusedInventoryIndices.armor[i];
            if (repair_item_id != 0) {
                Items_InventoryArmor* armor = &Audio_Seq_unusedInventory.armor[repair_item_id - 1];
                if (armor->category != 7) {

                    ppCost = armor->maxDp - armor->currentDp;
                    ppCost = armor->currentPp - (ppCost / 100);
                    armor->currentDp = armor->maxDp;

                    if (ppCost >= 0) {
                        armor->currentPp = ppCost;
                    } else {
                        armor->currentPp = 0;
                    }
                    if (armor->limb != 0) {
                        Items_EquipAshleyArmor(armor->limb - 1, armor);
                    }
                }
            }
        }

        Menu_Setup_repairState = 3;
        break;
    case 3:
        if (Input_buttonsPressed.ports[0].lowByte != 0) {
            Menu_PlayLeaveSound();
            Menu_Framework_FlyOutRowRight(0xE);
            if (Input_buttonsPressed.allButtons & PADRup) {
                return -2;
            }
            Menu_Framework_InitializeInformationBox();
            Menu_Setup_repairState = 4;
        }
        break;
    case 4:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Setup_repairReturnedFromPrompt;
        }
        break;
    }
    return 0;
}

// decomp: MENUC.PRG vs_menuC_exec (src/MENU/MENUC.PRG/168.c:4550)
int Menu_Setup_Update(u_char* state)
{
    static char Menu_Setup_firstEntry;

    char* menuText[12];
    int i;
    int selectedRow;
    Menu_Framework_MenuRow* menuItem;

    switch (*state) {
    case 0:
        Menu_Setup_firstEntry = 1;
        // fallthrough
    case 1:

        for (i = 0; i < 7; ++i) {
            Items_Inventory_RebuildDisplayIndices(i);
        }
        Menu_Framework_SetBackgroundPriority(2046, 128);
        menuItem = Menu_Framework_MenuItem(0);
        if ((menuItem->state != 1) || (menuItem->x != 180)) {
            Menu_Setup_SetTitleRow(0, Text_Menu_MENU12_BIN_Offset_setup);
        }
        *state = 2;
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 6; ++i) {
                menuText[i * 2] = (char*)&Menu_Framework_menu12Text
                    [Menu_Framework_menu12Text[i * 2 + Text_Menu_MENU12_BIN_Index_assembleWeapons]];
                menuText[i * 2 + 1] =
                    (char*)&Menu_Framework_menu12Text[Menu_Framework_menu12Text
                            [i * 2 + Text_Menu_MENU12_BIN_Index_assembleWeaponsDesc]];
                Menu_Framework_rowTypeBuf[i] = 0;
            }

            i = Items_Inventory_CountItems(0, 0);
            if (i == 8) {
                menuText[1] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_weaponsFull];
                Menu_Framework_rowTypeBuf[0] = 1;
            }
            if (i == 0) {
                menuText[7] =
                    (char*)&Menu_Framework_menu12Text[Text_Menu_MENU12_BIN_Offset_noWeapons];
                Menu_Framework_rowTypeBuf[3] = 1;
            }
            Menu_Setup_currentWorkshop = Field_IsCurrentRoomWorkshop();
            if (!Menu_Setup_currentWorkshop) {
                for (i = 3; i != 0;) {
                    if (++i == 6) {
                        i = 0;
                    }
                    menuText[i * 2 + 1] = (char*)&Menu_Framework_menu12Text
                        [Text_Menu_MENU12_BIN_Offset_notInWorkshop];
                    Menu_Framework_rowTypeBuf[i] = 1;
                }
            }
            i = Input_settings.cursorMemory;
            if (Menu_Setup_firstEntry == 0) {
                Input_settings.cursorMemory = 1;
            }
            Menu_Setup_firstEntry = 0;
            Menu_Framework_CreateMenuRows(6, 0x135, menuText, Menu_Framework_rowTypeBuf);
            Input_settings.cursorMemory = i;
            *state = 3;
        }
        break;
    case 3:
        Menu_Framework_UpdateInformationVisibility(
            Menu_Framework_HighlightedRow(), Menu_Framework_itemsListRow);
        selectedRow = Menu_Framework_GetSelectedRow();
        i = selectedRow + 1;
        if (i != 0) {
            if (i < 0) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                if (i == -2) {
                    Menu_textBoxes[7].state = 0;
                    *state = 0xB;
                } else {
                    Menu_Framework_menuState.currentMenu = Menu_Framework_menuState.returnState;
                    Menu_Framework_menuState.returnState = 0;
                    Menu_Framework_actionRestrictions &= ~0x200;
                    *state = 0;
                }
            } else {
                switch (selectedRow) {
                case 0:
                    Menu_Setup_UpdateAssembly(1);
                    *state = 4;
                    break;
                case 1:
                    Menu_Setup_UpdateGemCategoryMenu(1);
                    *state = 5;
                    break;
                case 2:
                    Menu_Setup_UpdateDisassemblyCategory(1);
                    *state = 6;
                    break;
                case 3:
                    Menu_Setup_UpdateWeaponRename(1);
                    *state = 7;
                    break;
                case 4:
                    Menu_Setup_UpdateRepair(1);
                    *state = 8;
                    break;
                case 5:
                    Menu_Setup_UpdateCombinationCategory(1);
                    *state = 9;
                    break;
                }
            }
        }
        break;
    case 4:
        i = Menu_Setup_UpdateAssembly(0);
        if (i != 0) {
            if (i != -2) {
                *state = 0xA;
                if (i == 1) {
                    Menu_Framework_menuState.currentMenu = 8;
                    Menu_Framework_DismissInformationBox();
                } else {
                    Menu_Framework_InitializeInformationBox();
                }
            } else {
                *state = 0xB;
            }
        }
        break;
    case 5:
        i = Menu_Setup_UpdateGemCategoryMenu(0);
        if (i != 0) {
            if (i == -2) {
                *state = 0xB;
            } else {
                *state = 0xA;
            }
        }
        break;
    case 6:
        i = Menu_Setup_UpdateDisassemblyCategory(0);
        if (i != 0) {
            if (i == -2) {
                *state = 0xB;
            } else {
                *state = 0xA;
            }
        }
        break;
    case 7:
        i = Menu_Setup_UpdateWeaponRename(0);
        if (i != 0) {
            if (i != -2) {
                *state = 0xA;
                if (i == 1) {
                    Menu_Framework_menuState.currentMenu = 8;
                }
            } else {
                *state = 0xB;
            }
        }
        break;
    case 8:
        i = Menu_Setup_UpdateRepair(0);
        if (i != 0) {
            if (i == -2) {
                *state = 0xB;
            } else {
                *state = 0xA;
            }
        }
        break;
    case 9:
        i = Menu_Setup_UpdateCombinationCategory(0);
        if (i != 0) {
            if (i == -2) {
                *state = 0xB;
            } else {
                *state = 0xA;
            }
        }
        break;
    case 10:
        Menu_Framework_InitializeInformationBox();
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            *state = 1;
        }
        break;
    case 11:
        Menu_Framework_menuState.currentMenu = Menu_Framework_menuState.returnState;
        Menu_Framework_menuState.returnState = 1;
        Menu_Framework_actionRestrictions &= ~0x200;
        *state = 0;
        break;
    }
    return 0;
}

// decomp: MENUC.PRG _loadObject (src/MENU/MENUC.PRG/9258.c:7)
int Menu_Setup_ReloadAshleyEquipment(int equipment_reload_mode)
{
    int _[6] __attribute__((unused));
    Actors_ActorRecord* ashley_actor = Actors_actorRecords[0];

    if (equipment_reload_mode == 1) {
        Content_ReleaseWeaponModel(1);
        Menu_Framework_ResetAshleyEquipmentPose();
        return 0;
    }

    switch (Menu_Inventory_equipmentModelLoadStep) {
    case 0:
        if (ashley_actor->sequenceModelId == 10) {
            return 0;
        }

        Content_ReleaseWeaponModel(0);
        Menu_Framework_QueueAshleyWeaponAnimations(10);

        ++Menu_Inventory_equipmentModelLoadStep;
        break;

    case 1:
        if (Content_UpdateActorAssetQueue() == 0) {

            Menu_Framework_ResetAshleyEquipmentPose();

            Menu_Inventory_equipmentModelLoadStep = 0;
            return 0;
        }

        break;
    }

    return 1;
}

// MENUC.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Setup_SetupScreen_ResetMenucStatics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_confirmationPromptCursorState, &initial, sizeof Menu_Setup_confirmationPromptCursorState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_threeChoicePromptCursorState, &initial, sizeof Menu_Setup_threeChoicePromptCursorState);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_Setup_itemListCursorAnimState, &initial, sizeof Menu_Setup_itemListCursorAnimState);
    }
}

