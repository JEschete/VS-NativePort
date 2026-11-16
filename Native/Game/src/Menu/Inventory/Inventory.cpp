// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/ActionCostsLive.hpp"
#include "Battle/HudStateLive.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Menu/Framework/EquipmentDetailsLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/DetailsPoliciesLive.hpp"
#include "Menu/Inventory/EquipmentMenusLive.hpp"
#include "Menu/Inventory/ItemDetailDispatchLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Hud.h"
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/InventoryLiveState.h"
#include "Items/KeyItems.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Framework/MenuItems.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Inventory/Inventory.h"
#include "Menu/Inventory/ItemList.h"
#include "Menu/Status/ActorStatus.h"

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
char Menu_Inventory_equipmentModelLoadStep; /* contents: module image (ModuleData.cpp) */

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
u_short Menu_Inventory_menuText[472]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _miscActions (_miscActions@src/MENU/MENU3.PRG/16C.c)
char Menu_Inventory_unusedMiscActions[64];

// decomp: _selectedItemRow (_selectedItemRow@src/MENU/MENU3.PRG/16C.c)
int Menu_Inventory_selectedItemRow;

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
        VS::Menu::Framework::InitializeLiveInformationBox();

        if (initialize & 1) {
            Menu_Inventory_miscItemShortcutNoForceCursorMemory = 1;
        }

        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_Init;
        return;
    }

    switch (Menu_Inventory_miscItemShortcutState) {
    case Menu_Inventory_MiscItemShortcutState_Init:
        menuItem = VS::Menu::Framework::SetLiveMenuItem(0, 320, 18, 140, 8,
            (char*)&Menu_menuStrings
                [Menu_menuStrings[Text_Battle_menuStrings_Index_items]]);
        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        menuItem->selected = 1;
        menuItem->targetPosition0 = 180;
        menuItem->unselectable = (Menu_Framework_actionRestrictions & 7) != 0;
        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_WaitTitle;

        VS::Menu::Framework::LoadLiveItemText(1);

        break;

    case Menu_Inventory_MiscItemShortcutState_WaitTitle:
        if (!VS::Menu::Framework::AreLiveMenuRowsIdle() || !VS::Menu::Framework::LoadLiveItemText(0)) {
            break;
        }

        menuItem = VS::Menu::Framework::SetLiveMenuItem(10, 320, 34, 140, 8, Menu_Inventory_miscItemShortcutMisc);
        menuItem->state = Menu_Framework_MenuItemState_SlideX;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_InitItemRows;
        break;

    case Menu_Inventory_MiscItemShortcutState_InitItemRows:
        buf = VS::Core::Memory::AllocateLiveFromHighEnd(0x1900 + 64 * 2 * sizeof(*menuText));
        rowCount = 0;

        VS::Items::Inventory::RebuildLiveDisplayIndices(Items_ItemCategory_Misc);

        rowTypes = (int *)(((u_char*)(buf)) + 0x1800);
        menuText = (char **)(((u_char*)(buf)) + 0x1900);

        for (i = 0; i < 64; ++i, ++rowCount) {

            Items_InventoryMisc* misc;
            int index = Audio_Seq_unusedInventoryIndices.misc[i];

            if (index == 0) {
                break;
            }

            misc = &Audio_Seq_unusedInventory.misc[index - 1];

            VS::Menu::Framework::FormatLiveMiscItemRow(misc, &menuText[i * 2], rowTypes - -i, ((u_char*)(buf)) + i * 96);

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
                || (VS::Battle::LiveActorActionRestrictionFlags(0, Menu_Inventory_miscItemShortcutActions[i]) & 2)) {
                rowTypes[i] |= Menu_Framework_RowTypeFlag_Disabled;
            }
        }

        Menu_Inventory_miscItemShortcutNoItems = rowCount == 0;

        if (Menu_Inventory_miscItemShortcutNoItems != 0) {
            VS::Menu::Framework::SetLiveInformationMessage(
                (char*)&Menu_Framework_itemHelp[Text_Menu_ITEMHELP_BIN_Offset_noItemsInCategory]);
        } else {

            i = Input_settings.cursorMemory;

            if (Menu_Inventory_miscItemShortcutNoForceCursorMemory != 0) {
                Menu_Inventory_miscItemShortcutNoForceCursorMemory = 0;
            } else {
                Input_settings.cursorMemory = 1;
            }

            VS::Menu::Framework::CreateLiveMenuRows(
                rowCount, (25 << 12) | (2 << 8) | 36, menuText, rowTypes);

            Input_settings.cursorMemory = i;
        }

        VS::Core::Memory::FreeLiveFromHighEnd(buf);

        Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_WaitForInput;
        break;

    case Menu_Inventory_MiscItemShortcutState_WaitForInput:
        if (Menu_Inventory_miscItemShortcutNoItems) {
            if (Input_buttonsPressed.allButtons & (PADRup | PADRright | PADRdown)) {
                if (Input_buttonsPressed.allButtons & PADRright) {

                    VS::Menu::PlayInvalidSound();

                } else {

                    VS::Menu::PlayLeaveSound();
                    VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                    VS::Menu::Framework::DismissLiveInformationBox();

                    Menu_Inventory_miscItemShortcutSelectedRow = -1;
                    Menu_Inventory_miscItemShortcutState = Menu_Inventory_MiscItemShortcutState_Quit;
                }
            }
        } else {
            Menu_Inventory_miscItemShortcutSelectedRow = VS::Menu::Framework::LiveSelectedRow() + 1;

            if (Menu_Inventory_miscItemShortcutSelectedRow != 0) {

                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                VS::Menu::Framework::DismissLiveInformationBox();

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
        if (!VS::Menu::Framework::AreLiveMenuRowsIdle() || !VS::Menu::Framework::UnloadLiveItemTextWhenReady()) {
            break;
        }

        if (Menu_Inventory_miscItemShortcutSelectedRow > 0) {
            Menu_Framework_actionSelection.selectionKind = 8;
        }

        Menu_Framework_menuState.currentMenu = Menu_Framework_MenuState_NoneClosed;
        Menu_Framework_actionSelection.menuStatus = 2;
        return;
    }

    VS::Menu::Framework::UpdateLiveScreen();

    return;
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
    char item_use_return_menu;
    char equipment_return_menu;

    switch (*state) {
    case Menu_Inventory_InventoryMenuState_Init:
        VS::Menu::Inventory::SetLivePanelVisibility(0);
        VS::Menu::Framework::SetLiveActorComparisonStats(16);

        Menu_Inventory_secondaryTitleAnimationStep = 0;
        Menu_Inventory_itemCategoryIconsAnimationStep = 0;
        Menu_Inventory_statusCommandAnimationStep = 0;
        Menu_Inventory_selectedItemCategoryIconOnTop = 0;

        if (Menu_Framework_itemNames == NULL) {
            VS::Menu::Framework::LoadLiveItemText(1);
        }

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }

        VS::Field::LiveSetFieldRenderingEnabled(0);
        VS::Menu::Framework::InitializeLiveInformationBox();
        VS::Battle::SetLiveHudHidden(1);

        Battle_hudSlideOffset = 0x200000;

        VS::Menu::Framework::RenderLiveAshley(1);
        Menu_Inventory_UpdateItemList(Menu_Inventory_restoreItemCursor + 1);

        Menu_Inventory_restoreItemCursor = 0;
        *state = Menu_Inventory_InventoryMenuState_LoadItemNamesWait;
        // Fallthrough

    case Menu_Inventory_InventoryMenuState_LoadItemNamesWait:
        if (VS::Menu::Framework::LoadLiveItemText(0) == 0) {
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
                VS::Menu::Inventory::UpdateLiveEquipmentMenu(1);

                Menu_Inventory_freezeTabArrows = 0;
                *state = Menu_Inventory_InventoryMenuState_Equip;

                break;

            case 2:
                Menu_Inventory_ashleyRenderState = 3;

                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);

                Menu_Inventory_secondaryTitleEnabled = 0;
                Menu_Inventory_itemCategoryIconsEnabled = 0;
                Menu_Inventory_statusCommandState = 0;
                *state = 7;

                break;

            case 3:
                VS::Menu::Inventory::SetLivePanelVisibility(0);
                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                VS::Field::LiveSetFieldRenderingEnabled(1);

                Menu_Framework_menuState.executeAbilityType = 13;
                *state = 12;

                VS::Battle::SetLiveHudHidden(0);

                Battle_hudSlideOffset = 0;

                VS::Menu::Framework::RenderLiveAshley(2);

                break;
            }
        } else {
            if (row == Menu_Framework_MenuSelection_Quit) {
                *state = Menu_Inventory_InventoryMenuState_QuitToBattle;
            } else {
                *state = Menu_Inventory_InventoryMenuState_Quit;

                VS::Menu::Inventory::SetLivePanelVisibility(0);
                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                VS::Field::LiveSetFieldRenderingEnabled(1);
                VS::Battle::SetLiveHudHidden(0);

                Battle_hudSlideOffset = 0;

                VS::Menu::Framework::RenderLiveAshley(2);
            }
        }
        break;

    case Menu_Inventory_InventoryMenuState_Equip:
        row = VS::Menu::Inventory::UpdateLiveEquipmentMenu(0);

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
        if (VS::Menu::Framework::AreLiveMenuRowsIdle()) {
            VS::Menu::Framework::RenderLiveAshley(2);

            *state = 8;
            item_use_return_menu = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 13;
            Menu_Framework_menuState.returnState = item_use_return_menu;
            Menu_Framework_actionRestrictions |= 0x200;
        }
        break;

    case 8:
        if (Menu_Framework_menuState.returnState != 0) {
            VS::Menu::Framework::SetLiveBackgroundPriority(-4, 128);
            VS::Field::LiveSetFieldRenderingEnabled(1);
            VS::Menu::Inventory::SetLivePanelVisibility(0);
            VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
            VS::Battle::SetLiveHudHidden(0);

            Battle_hudSlideOffset = 0;
            Menu_Inventory_closeDelay = 10;

            *state = 11;
        } else if (VS::Menu::Framework::AreLiveMenuRowsIdle()) {
            VS::Menu::Inventory::SetLivePanelVisibility(0);

            Menu_Inventory_secondaryTitleAnimationStep = 0;
            Menu_Inventory_itemCategoryIconsAnimationStep = 0;
            Menu_Inventory_statusCommandAnimationStep = 0;
            Menu_Inventory_selectedItemCategoryIconOnTop = 0;

            VS::Menu::Framework::RenderLiveAshley(1);
            Menu_Inventory_UpdateItemList(1);

            *state = Menu_Inventory_InventoryMenuState_ItemsTop;
        }
        break;

    case Menu_Inventory_InventoryMenuState_Quit:
        VS::Menu::Framework::SetLiveBackgroundFade(0);
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() && VS::Menu::Framework::UnloadLiveItemTextWhenReady()) {
            *state = Menu_Inventory_InventoryMenuState_None;
            return 1;
        }

        break;

    case Menu_Inventory_InventoryMenuState_QuitToBattle:
        VS::Field::LiveSetFieldRenderingEnabled(1);
        VS::Menu::Inventory::SetLivePanelVisibility(0);
        VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
        VS::Battle::SetLiveHudHidden(0);

        Battle_hudSlideOffset = 0;

        VS::Menu::Framework::RenderLiveAshley(2);

        Menu_Inventory_closeDelay = 10;
        *state = 11;

        break;

    case 11:
        VS::Menu::Framework::SetLiveBackgroundFade(0);
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Inventory_closeDelay != 0) {
            --Menu_Inventory_closeDelay;
            break;
        }

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() && VS::Menu::Framework::UnloadLiveItemTextWhenReady()) {
            Menu_Framework_menuState.currentMenu = 4;
            *state = Menu_Inventory_InventoryMenuState_None;
            return 1;
        }

        break;

    case 12:
        VS::Menu::Framework::SetLiveBackgroundFade(0);
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if ((Menu_Framework_backgroundFadeStep == 0)
            && VS::Menu::Framework::UnloadLiveItemTextWhenReady()) {

            Menu_Framework_actionSelection.selectionKind = 8;
            equipment_return_menu = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 4;
            Menu_Framework_menuState.returnState = equipment_return_menu;
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

        Menu_Framework_MenuRow* menuitem = VS::Menu::Framework::SetLiveMenuItem(
            31, row + 180, 34, 140, 8, (char*)&Menu_Inventory_menuText[Menu_Inventory_menuText[category]]);
        menuitem->selected = 1;

        if (category != 7) {
            menuitem->max = Items_Inventory_inventoryItemCapacities[category];
            menuitem->count = VS::Items::Inventory::CountLiveItems(category, 0);
        }
    }

    VS::Menu::Framework::RenderLiveAshley(0);

    if (Menu_Inventory_itemCategoryIconsEnabled) {
        if (Menu_Inventory_itemCategoryIconsAnimationStep < 4) {
            ++Menu_Inventory_itemCategoryIconsAnimationStep;
        }
    } else if (Menu_Inventory_itemCategoryIconsAnimationStep != 0) {
        --Menu_Inventory_itemCategoryIconsAnimationStep;
    }

    VS::Menu::Inventory::DrawLiveItemCategoryIcons(Menu_Inventory_itemCategoryIconsAnimationStep, Menu_Inventory_selectedItemCategoryIconOnTop);

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
        VS::Menu::Framework::DrawLiveButtonPromptBackground(16 - row, 38, 88, 10);
        VS::Menu::Framework::DrawLiveButtonIcon(Menu_Framework_ButtonId_Square, 8 - row, 36, NULL);
        VS::Render::DrawLiveCompactTextColor("STATUS", vs_getXY(28 - row, 38),
            vs_getRGB888(32, 32, 32) << Menu_Inventory_statusCommandState, NULL);
    }

    return 0;
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

}  // extern "C"
