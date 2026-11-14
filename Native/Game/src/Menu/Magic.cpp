// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/AbilitiesLive.hpp"
#include "Battle/ActionCostsLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/RoomStateLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/MagicMenusLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Content/StringTables.h"
#include "Field/ChestInteractionsLive.h"
#include "Input/ControllerLiveState.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"
#include "Render/Palette.h"
#include "State/StateFlags.h"

// decomp: vs_battle_warlockSpellIds (vs_battle_warlockSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_warlockSpellIds[20]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_shamanSpellIds (vs_battle_shamanSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_shamanSpellIds[8]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_sorcererSpellIds (vs_battle_sorcererSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_sorcererSpellIds[20]; /* contents: module image (ModuleData.cpp) */

// decomp: vs_battle_enchanterSpellIds (vs_battle_enchanterSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_enchanterSpellIds[8]; /* contents: module image (ModuleData.cpp) */

// decomp: D_8010694A (D_8010694A@src/MENU/MENU0.PRG/84.c)
u_char Menu_Magic_warlockLevelCursorBlink;

// decomp: _0 (_0@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_layoutPadding1[5];

// decomp: _1 (_1@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_layoutPadding2[2];

// decomp: _3 (_3@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_layoutPadding3[6];

// decomp: _ (_@src/MENU/MENU0.PRG/84.c)
const int Menu_Magic_layoutPadding4 __attribute__((unused)) = 0;

// decomp: _MPCostDirectTextBuffer (_MPCostDirectTextBuffer@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_mpCostDirectTextBuffer[16];

// decomp: _MPCostTextBuffer (_MPCostTextBuffer@src/MENU/MENU0.PRG/84.c)
signed char Menu_Magic_mpCostTextBuffer[16];

// decomp: _availableEnchanterSpells (_availableEnchanterSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableEnchanterSpells[8];

// decomp: _availableShamanSpells (_availableShamanSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableShamanSpells[6];

// decomp: _availableSorcererSpells (_availableSorcererSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableSorcererSpells[18];

// decomp: _availableWarlockSpells (_availableWarlockSpells@src/MENU/MENU0.PRG/84.c)
char Menu_Magic_availableWarlockSpells[18];

// decomp: _magicStrings (_magicStrings@MENU/MENU0.PRG)
u_short Menu_Magic_magicStrings[2462]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _savePointData (_savePointData@MENU/MENU0.PRG)
int Menu_Magic_savePointData[] = { 0x0305001B, 0x0801000E, 0x04030199, 0x0C0C001C, 0x070C0028,
    0x040200CD, 0x0F070035, 0x08060093, 0x060000CE, 0x09080106, 0x09080111, 0x0C030098,
    0x090800E0, 0x060900C8, 0x050800CA, 0x050F00CC, 0x0109009B, 0x0A07007C, 0x000400CF,
    0x080800EC, 0x020400F1, 0x0705011D, 0x06080120, 0x04080139, 0x04080147, 0x0904014F,
    0x010300A2, 0x150C00A3, 0x030200D0, 0x060600D1, 0x1007008B, 0x010400AB, 0x05020055,
    0x07060063, 0x0A0400D2, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000 };

// decomp: _teleportationStrings (_teleportationStrings@MENU/MENU0.PRG)
u_short Menu_Magic_teleportationStrings[1599]; /* contents: disc string table (ModuleData.cpp) */

// decomp: MENU0.PRG _teleportMenu (src/MENU/MENU0.PRG/15AC.c:22)
int Menu_Magic_UpdateTeleportMenu(int init)
{
    enum Menu_Magic_TeleportMenuState { Menu_Magic_TeleportMenuState_InitTeleport, Menu_Magic_TeleportMenuState_HandleInput, Menu_Magic_TeleportMenuState_ReturnIfReady };

    static u_char Menu_Magic_teleportMenuAvailableSavePoints[48];
    static u_short Menu_Magic_teleportCosts[48];
    static int Menu_Magic_teleportMenuSelectedRow;
    static int Menu_Magic_teleportMenuCurrentMp;
    static char Menu_Magic_teleportMenuState;

    char* menuStrings[96];
    int rowTypes[48];
    int nearestSavePoint;
    int i;
    int rowCount;
    int savePointState;

    if (init != 0) {
        VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(4, 1);
        Menu_Magic_teleportMenuState = Menu_Magic_TeleportMenuState_InitTeleport;
        Menu_Magic_teleportMenuCurrentMp = Battle_characterState->stats->currentMp;
        return 0;
    }

    switch (Menu_Magic_teleportMenuState) {
    case Menu_Magic_TeleportMenuState_InitTeleport:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }
        rowCount = 0;
        for (i = 0; i < 48; ++i) {
            savePointState = Audio_Seq_unusedStateFlags.savePointStates[i];
            if (savePointState == 0) {
                continue;
            }
            menuStrings[2 * rowCount] = (char*)(&Menu_Magic_teleportationStrings
                    [Menu_Magic_teleportationStrings[i * 2 + Text_Menu0_teleportation_Index_locations]]);
            menuStrings[(rowCount * 2) + 1] =
                (char*)(&Menu_Magic_teleportationStrings[Menu_Magic_teleportationStrings
                        [i * 2 + Text_Menu0_teleportation_Index_locations + 1]]);
            Menu_Magic_teleportCosts[rowCount] = Menu_Magic_TeleportMpCost(i);
            rowTypes[rowCount] =
                (savePointState == 2) | (Menu_Magic_teleportMenuCurrentMp < Menu_Magic_teleportCosts[rowCount]);
            if (Menu_Magic_teleportCosts[rowCount] == 15) {
                rowTypes[rowCount] |= 4;
                nearestSavePoint = rowCount;
            }
            if (savePointState == 2) {
                menuStrings[(rowCount * 2) + 1] =
                    (char*)(Menu_Magic_teleportationStrings
                            + Text_Menu0_teleportation_Offset_teleportationDisabled);
            }
            Menu_Magic_teleportMenuAvailableSavePoints[rowCount] = i;
            ++rowCount;
        }

        if (Menu_Framework_actionRestrictions & 0xB7) {
            for (i = 0; i < 48; ++i) {
                rowTypes[i] |= 1;
            }
        }
        if ((rowCount < 9) || (nearestSavePoint < 8)) {
            Menu_Framework_menuMemory.cursorMemories[22] = nearestSavePoint;
            Menu_Framework_menuMemory.cursorMemories[23] = 0;
        } else if (nearestSavePoint >= (rowCount - 7)) {
            Menu_Framework_menuMemory.cursorMemories[22] = nearestSavePoint - (rowCount - 8);
            Menu_Framework_menuMemory.cursorMemories[23] = rowCount - 8;
        } else {
            Menu_Framework_menuMemory.cursorMemories[22] = 3;
            Menu_Framework_menuMemory.cursorMemories[23] = nearestSavePoint - 3;
        }
        savePointState = Input_settings.cursorMemory;
        Input_settings.cursorMemory = 1;
        VS::Menu::Framework::CreateLiveMenuRows(rowCount, (2 << 8) | 11, menuStrings, rowTypes);
        Menu_Magic_teleportMenuState = Menu_Magic_TeleportMenuState_HandleInput;
        Input_settings.cursorMemory = savePointState;
        break;

    case Menu_Magic_TeleportMenuState_HandleInput:
        Menu_Magic_teleportMenuSelectedRow = VS::Menu::Framework::LiveSelectedRow() + 1;
        if (Menu_Magic_teleportMenuSelectedRow != 0) {
            Menu_Framework_displayActionCost = 0;
            if (Menu_Magic_teleportMenuSelectedRow == (-1)) {
                VS::Menu::Framework::ClearLiveMenuExcept(0);
            } else {
                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                VS::Menu::Framework::DismissLiveInformationBox();
                VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);
            }
            Menu_Magic_teleportMenuState = Menu_Magic_TeleportMenuState_ReturnIfReady;
        } else {
            i = Menu_Magic_teleportCosts[VS::Menu::Framework::LiveHighlightedRow()];
            Menu_Magic_SetTeleportMpCost(i, Menu_Magic_teleportMenuCurrentMp < i);
        }
        break;

    case Menu_Magic_TeleportMenuState_ReturnIfReady:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            if (Menu_Magic_teleportMenuSelectedRow > 0) {
                int v1;
                int packed_location_and_tile_x;
                int packed_destination_tile_z;
                int selected_save_point_index = Menu_Magic_teleportMenuAvailableSavePoints[Menu_Magic_teleportMenuSelectedRow - 1];
                i = Menu_Magic_savePointData[selected_save_point_index] + 1;
                v1 = i & 0x1FF;
                init = ~0x3E00;
                packed_location_and_tile_x = (rowCount & ~0x1FF) | v1;
                v1 = (i >> 7) & 0x3E00;
                packed_location_and_tile_x &= init;
                packed_location_and_tile_x |= v1;
                packed_destination_tile_z = (i >> 0x18) & 0x1F;
                packed_destination_tile_z <<= 0xE;
                v1 = Menu_Magic_teleportCosts[Menu_Magic_teleportMenuSelectedRow - 1] << 0x13;
                packed_location_and_tile_x &= 0x3FFF;
                return packed_location_and_tile_x | packed_destination_tile_z | v1;
            }
            return Menu_Magic_teleportMenuSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU0.PRG _setMenuHeader (src/MENU/MENU0.PRG/15AC.c:148)
void Menu_Magic_CreateHeader(void)
{
    Menu_Framework_MenuRow* menuItem;

    menuItem = VS::Menu::Framework::SetLiveMenuItem(0, 320, 18, 0x8C, 8,
        (char*)&Menu_menuStrings[Menu_menuStrings[Text_Battle_menuStrings_Index_magic]]);
    menuItem->state = 2;
    menuItem->targetPosition0 = 180;
    menuItem->selected = 1;
    VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_Menu);
    VS::Menu::Framework::InitializeLiveInformationBox();
}

// decomp: MENU0.PRG vs_menu0_exec (src/MENU/MENU0.PRG/15AC.c:166)
int Menu_Magic_Update(u_char* state)
{
    enum Menu_Magic_MagicMenuState {
        Menu_Magic_MagicMenuState_None,
        Menu_Magic_MagicMenuState_HandleShortcut = 3,
        Menu_Magic_MagicMenuState_InitSubmenu,
        Menu_Magic_MagicMenuState_HandleSubmenu,
        Menu_Magic_MagicMenuState_InitWarlockMenu,
        Menu_Magic_MagicMenuState_WarlockMenu,
        Menu_Magic_MagicMenuState_InitShamanMenu,
        Menu_Magic_MagicMenuState_ShamanMenu,
        Menu_Magic_MagicMenuState_InitSorcererMenu,
        Menu_Magic_MagicMenuState_SorcererMenu,
        Menu_Magic_MagicMenuState_InitEnchanterMenu,
        Menu_Magic_MagicMenuState_EnchanterMenu,
        Menu_Magic_MagicMenuState_TeleportMenu,
        Menu_Magic_MagicMenuState_ExitToMainMenu,
        Menu_Magic_MagicMenuState_ExecuteMagic,
        Menu_Magic_MagicMenuState_Exit
    };

    char* menuStrings[10];
    int rowTypes[5];
    int selectedRow;
    int j;
    int i;

    switch (*state) {
    case Menu_Magic_MagicMenuState_HandleShortcut:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }

        VS::Menu::Framework::InitializeLiveInformationBox();

        i = Menu_Framework_shortcutInvoked;

        if (i != 0) {
            enum Menu_Magic_ShortcutMagicMenu { Menu_Magic_ShortcutMagicMenu_Warlock = 1, Menu_Magic_ShortcutMagicMenu_Shaman, Menu_Magic_ShortcutMagicMenu_Sorcerer, Menu_Magic_ShortcutMagicMenu_Enchanter };
            VS::Menu::Framework::SetLiveMenuItem(i + 9, 0x140, 0x22, 0x8C, 8,
                (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[(i * 3) - 3]])
                ->selected = 1;
            switch (i) {
            case Menu_Magic_ShortcutMagicMenu_Warlock:
                *state = Menu_Magic_MagicMenuState_WarlockMenu;
                VS::Menu::Magic::UpdateWarlockMenuLive(1);
                break;
            case Menu_Magic_ShortcutMagicMenu_Shaman:
                *state = Menu_Magic_MagicMenuState_ShamanMenu;
                VS::Menu::Magic::UpdateShamanMenuLive(1);
                break;
            case Menu_Magic_ShortcutMagicMenu_Sorcerer:
                *state = Menu_Magic_MagicMenuState_SorcererMenu;
                VS::Menu::Magic::UpdateSorcererMenuLive(1);
                break;
            case Menu_Magic_ShortcutMagicMenu_Enchanter:
                *state = Menu_Magic_MagicMenuState_EnchanterMenu;
                VS::Menu::Magic::UpdateEnchanterMenuLive(1);
                break;
            }
            break;
        }
        // Fallthrough

    case Menu_Magic_MagicMenuState_InitSubmenu:
        for (i = 0; i < 4; ++i) {
            menuStrings[i * 2] =
                (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[i * 3 + Text_Menu0_magic_Index_warlock]];
            menuStrings[i * 2 + 1] =
                (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[i * 3 + Text_Menu0_magic_Index_warlockDesc]];
            rowTypes[i] = 0;

            if (VS::Battle::LiveHasUnlockedSpellClass(i) == 0) {
                rowTypes[i] |= 1;
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[i * 3
                                                        + Text_Menu0_magic_Index_warlockLocked]];
            }
        }

        if (Audio_Seq_unusedStateFlags.opaqueB5 != 0) {
            for (j = 0; j < 48; ++j) {
                if (Audio_Seq_unusedStateFlags.savePointStates[j] != 0) {
                    break;
                }
            }

            menuStrings[i * 2] =
                (char*)(Menu_Magic_teleportationStrings + Text_Menu0_teleportation_Offset_teleportation);

            menuStrings[i * 2 + 1] =
                (Field_CurrentSavePointIdEntry() != 0) && (Audio_Seq_unusedStateFlags.opaqueB5 == 2)
                    ? (char*)(Menu_Magic_teleportationStrings
                              + Text_Menu0_teleportation_Offset_teleportationDisabled)
                    : (char*)(Menu_Magic_teleportationStrings
                              + Text_Menu0_teleportation_Offset_teleportationDesc);

            if ((Field_CurrentSavePointIdEntry() != 0) && (Audio_Seq_unusedStateFlags.opaqueB5 == 2)) {
                menuStrings[i * 2 + 1] =
                    (char*)(Menu_Magic_teleportationStrings
                            + Text_Menu0_teleportation_Offset_teleportationDisabled);
            }

            rowTypes[i++] =
                ((j == 48) || (Audio_Seq_unusedStateFlags.opaqueB5 == 2) || (Field_CurrentSavePointIdEntry() == 0));
        }

        j = Input_settings.cursorMemory;

        if (*state != Menu_Magic_MagicMenuState_HandleShortcut) {
            Input_settings.cursorMemory = 1;
        }

        VS::Menu::Framework::CreateLiveMenuRows(i, (1 << 8) | 5, menuStrings, rowTypes);
        Input_settings.cursorMemory = j;
        *state = Menu_Magic_MagicMenuState_HandleSubmenu;

        break;

    case Menu_Magic_MagicMenuState_HandleSubmenu:
        selectedRow = VS::Menu::Framework::LiveSelectedRow();
        i = selectedRow + 1;

        if (i == 0) {
            break;
        }

        if (i > 0) {
            enum Menu_Magic_MagicSubmenu { Menu_Magic_MagicSubmenu_Warlock, Menu_Magic_MagicSubmenu_Shaman, Menu_Magic_MagicSubmenu_Sorcerer, Menu_Magic_MagicSubmenu_Enchanter, Menu_Magic_MagicSubmenu_Teleport };
            switch (selectedRow) {
            case Menu_Magic_MagicSubmenu_Warlock:
                *state = Menu_Magic_MagicMenuState_WarlockMenu;
                VS::Menu::Magic::UpdateWarlockMenuLive(1);
                break;
            case Menu_Magic_MagicSubmenu_Shaman:
                *state = Menu_Magic_MagicMenuState_ShamanMenu;
                VS::Menu::Magic::UpdateShamanMenuLive(1);
                break;
            case Menu_Magic_MagicSubmenu_Sorcerer:
                *state = Menu_Magic_MagicMenuState_SorcererMenu;
                VS::Menu::Magic::UpdateSorcererMenuLive(1);
                break;
            case Menu_Magic_MagicSubmenu_Enchanter:
                *state = Menu_Magic_MagicMenuState_EnchanterMenu;
                VS::Menu::Magic::UpdateEnchanterMenuLive(1);
                break;
            case Menu_Magic_MagicSubmenu_Teleport:
                *state = Menu_Magic_MagicMenuState_TeleportMenu;
                Menu_Magic_UpdateTeleportMenu(1);
                break;
            }
        } else {
            if (i == Menu_Framework_MenuSelection_Quit) {
                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
                *state = Menu_Magic_MagicMenuState_ExitToMainMenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_InitWarlockMenu:
        Menu_Magic_CreateHeader();
        VS::Menu::Framework::SetLiveMenuItem(
            0xA, 0x140, 0x22, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_warlock))
            ->selected = 1;
        VS::Menu::Magic::UpdateWarlockMenuLive(2);

        *state = Menu_Magic_MagicMenuState_WarlockMenu;
        break;

    case Menu_Magic_MagicMenuState_WarlockMenu:
        i = VS::Menu::Magic::UpdateWarlockMenuLive(0);

        if (i == 0) {
            break;
        }

        if (i > 0) {
            Menu_Framework_actionSelection.executeAbility.word = i;
            Menu_Framework_menuState.executeAbilityType = 6;
            *state = Menu_Magic_MagicMenuState_ExecuteMagic;
        } else {
            if (i == -2) {
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                *state = Menu_Magic_MagicMenuState_InitSubmenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_InitShamanMenu:
        Menu_Magic_CreateHeader();
        VS::Menu::Framework::SetLiveMenuItem(
            11, 320, 34, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_shaman))
            ->selected = 1;
        VS::Menu::Magic::UpdateShamanMenuLive(2);

        *state = Menu_Magic_MagicMenuState_ShamanMenu;
        break;

    case Menu_Magic_MagicMenuState_ShamanMenu:
        i = VS::Menu::Magic::UpdateShamanMenuLive(0);

        if (i == 0) {
            break;
        }

        if (i > 0) {
            Menu_Framework_actionSelection.executeAbility.word = i;
            Menu_Framework_menuState.executeAbilityType = 8;
            *state = Menu_Magic_MagicMenuState_ExecuteMagic;
        } else {
            if (i == -2) {
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                *state = Menu_Magic_MagicMenuState_InitSubmenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_InitSorcererMenu:
        Menu_Magic_CreateHeader();
        VS::Menu::Framework::SetLiveMenuItem(
            12, 320, 34, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_sorcerer))
            ->selected = 1;
        VS::Menu::Magic::UpdateSorcererMenuLive(2);

        *state = Menu_Magic_MagicMenuState_SorcererMenu;
        break;

    case Menu_Magic_MagicMenuState_SorcererMenu:
        i = VS::Menu::Magic::UpdateSorcererMenuLive(0);

        if (i == 0) {
            break;
        }

        if (i > 0) {
            Menu_Framework_actionSelection.executeAbility.word = i;
            Menu_Framework_menuState.executeAbilityType = 10;
            *state = Menu_Magic_MagicMenuState_ExecuteMagic;
        } else {
            if (i == -2) {
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                *state = Menu_Magic_MagicMenuState_InitSubmenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_InitEnchanterMenu:
        Menu_Magic_CreateHeader();
        VS::Menu::Framework::SetLiveMenuItem(
            13, 320, 34, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_enchanter))
            ->selected = 1;
        VS::Menu::Magic::UpdateEnchanterMenuLive(2);

        *state = Menu_Magic_MagicMenuState_EnchanterMenu;
        break;

    case Menu_Magic_MagicMenuState_EnchanterMenu:
        i = VS::Menu::Magic::UpdateEnchanterMenuLive(0);

        if (i == 0) {
            break;
        }

        if (i > 0) {
            Menu_Framework_actionSelection.executeAbility.word = i;
            Menu_Framework_menuState.executeAbilityType = 12;
            *state = Menu_Magic_MagicMenuState_ExecuteMagic;
        } else {
            if (i == -2) {
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                *state = Menu_Magic_MagicMenuState_InitSubmenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_TeleportMenu:
        i = Menu_Magic_UpdateTeleportMenu(0);

        if (i == 0) {
            break;
        }

        if (i > 0) {
            Menu_Framework_actionSelection.selectionKind = 0xA;
            Menu_Framework_actionSelection.executeAbility.word = i - 1;
            *state = Menu_Magic_MagicMenuState_Exit;
        } else {
            if (i == -2) {
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                *state = Menu_Magic_MagicMenuState_InitSubmenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_ExitToMainMenu:
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            *state = Menu_Magic_MagicMenuState_None;
            return 1;
        }

        break;

    case Menu_Magic_MagicMenuState_ExecuteMagic:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            Menu_Framework_actionSelection.selectionKind = 7;
            Menu_Framework_menuState.returnState = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 1;
            *state = Menu_Magic_MagicMenuState_None;
            return 1;
        }

        break;

    case Menu_Magic_MagicMenuState_Exit:
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            Menu_Framework_menuState.currentMenu = 1;
            *state = Menu_Magic_MagicMenuState_None;
            return 1;
        }

        break;
    }
    return 0;
}

// decomp: MENU0.PRG _setMPCost (src/MENU/MENU0.PRG/84.c:18)
void Menu_Magic_SetSpellMpCost(int id)
{
    int flags;
    int i;
    int cost;
    int variableCost = 0;

    if (id < 0) {
        variableCost = 1;
        id = -id;
    }

    flags = VS::Battle::LiveActorActionRestrictionFlags(0, id);
    VS::Menu::Framework::SetLiveActionCostText(1, "MP", 8, (flags >> 1) & 1);

    cost = Battle_actionDefinitions[id].cost;
    Menu_Magic_mpCostTextBuffer[15] = NULL;

    i = 15;

    if (variableCost != 0) {
        i = 13;
        Menu_Magic_mpCostTextBuffer[14] = '-';
        Menu_Magic_mpCostTextBuffer[13] = '[';
    }

    do {
        cost = VS::Core::Math::SplitDecimalDigit(cost);
        Menu_Magic_mpCostTextBuffer[--i] = (cost & 0xF) + '0';
        cost >>= 4;
        if (variableCost != 0) {
            Menu_Magic_mpCostTextBuffer[--i] = '[';
        }
    } while (cost != 0);

    Menu_Magic_mpCostTextBuffer[--i] = '#';

    VS::Menu::Framework::SetLiveActionCostText(
        0, (const char *)(&Menu_Magic_mpCostTextBuffer[i]), variableCost * 4 + 72, (flags >> 1) & 1);
}

// decomp: MENU0.PRG _setMPCostDirect (src/MENU/MENU0.PRG/84.c:64)
void Menu_Magic_SetTeleportMpCost(int costDecimal, int disabled)
{
    int cost;
    int i;

    VS::Menu::Framework::SetLiveActionCostText(1, "MP", 8, disabled);

    cost = costDecimal;
    i = 15;
    Menu_Magic_mpCostDirectTextBuffer[15] = 0;

    do {
        cost = VS::Core::Math::SplitDecimalDigit(cost);
        Menu_Magic_mpCostDirectTextBuffer[--i] = (cost & 0xF) + '0';
        cost >>= 4;
    } while (cost != 0);

    Menu_Magic_mpCostDirectTextBuffer[--i] = '#';

    VS::Menu::Framework::SetLiveActionCostText(0, &Menu_Magic_mpCostDirectTextBuffer[i], 72, disabled);
}

// decomp: MENU0.PRG _getTeleportCost (src/MENU/MENU0.PRG/84.c:117)
int Menu_Magic_TeleportMpCost(int targetSavePointId)
{
    int i;
    int j;
    int cost;
    int closestSavePointId;
    int roomId = VS::Field::LiveCurrentRoomId();

    do {
        for (i = 0; i < 48; ++i) {
            if ((Menu_Magic_savePointData[i] & 0xFFFF) == roomId) {
                break;
            }
        }
    } while (0);

    closestSavePointId = i;
    if (targetSavePointId < i) {
        i = targetSavePointId;
        targetSavePointId = closestSavePointId;
    }

    cost = 15;

    for (j = i; j < targetSavePointId; ++j) {
        static char Menu_Magic_teleportMpCostSavePointCosts[] = { 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            4, 4, 4, 4, 4, 0 };
        cost += Menu_Magic_teleportMpCostSavePointCosts[j];
    }

    return cost;
}

}  // extern "C"
