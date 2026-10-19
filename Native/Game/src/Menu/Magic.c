// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Sphere.h"
#include "Content/StringTables.h"
#include "Core/Math/FixedPoint.h"
#include "DiscQueue.h"
#include "Field/Gimmicks.h"
#include "Field/RoomState.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Magic.h"
#include "Menu/Sounds.h"
#include "Render/Font.h"
#include "Render/Images.h"
#include "Render/LegacyPackets.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Sprites.h"
#include "Render/TextureAnimation.h"
#include "Render/UiPrimitives.h"
#include "Save/MemoryCard/GameFiles.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: vs_battle_warlockSpellIds (vs_battle_warlockSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_warlockSpellIds[20]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_shamanSpellIds (vs_battle_shamanSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_shamanSpellIds[8]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_sorcererSpellIds (vs_battle_sorcererSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_sorcererSpellIds[20]; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_enchanterSpellIds (vs_battle_enchanterSpellIds@BATTLE/BATTLE.PRG)
char Menu_Magic_enchanterSpellIds[8]; /* contents: module image (ModuleData.c) */

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
u_short Menu_Magic_magicStrings[2462]; /* contents: disc string table (ModuleData.c) */

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
u_short Menu_Magic_teleportationStrings[1599]; /* contents: disc string table (ModuleData.c) */

// decomp: local static of _warlockMagicMenu, at file scope so the module reset reaches it
static char Menu_Magic_warlockMenuLevelledSpells[] = { Battle_WarlockSpell_Explosion,
        Battle_WarlockSpell_Thunderburst, Battle_WarlockSpell_FlameSphere,
        Battle_WarlockSpell_GaeaStrike, Battle_WarlockSpell_Avalanche,
        Battle_WarlockSpell_RadialSurge, Battle_WarlockSpell_Meteor };

// decomp: local static of _warlockMagicMenu, at file scope so the module reset reaches it
static char* Menu_Magic_warlockMenuSpellLevels[] = { "LV1", "LV2", "LV3", "LV4", "--" };

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
        Menu_Framework_FlyOutRowsAndHoistSelection(4, 1);
        Menu_Magic_teleportMenuState = Menu_Magic_TeleportMenuState_InitTeleport;
        Menu_Magic_teleportMenuCurrentMp = Battle_characterState->stats->currentMp;
        return 0;
    }

    switch (Menu_Magic_teleportMenuState) {
    case Menu_Magic_TeleportMenuState_InitTeleport:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
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
        Menu_Framework_CreateMenuRows(rowCount, (2 << 8) | 11, menuStrings, rowTypes);
        Menu_Magic_teleportMenuState = Menu_Magic_TeleportMenuState_HandleInput;
        Input_settings.cursorMemory = savePointState;
        break;

    case Menu_Magic_TeleportMenuState_HandleInput:
        Menu_Magic_teleportMenuSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Magic_teleportMenuSelectedRow != 0) {
            Menu_Framework_displayActionCost = 0;
            if (Menu_Magic_teleportMenuSelectedRow == (-1)) {
                Menu_Framework_ClearMenuExcept(0);
            } else {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            }
            Menu_Magic_teleportMenuState = Menu_Magic_TeleportMenuState_ReturnIfReady;
        } else {
            i = Menu_Magic_teleportCosts[Menu_Framework_HighlightedRow()];
            Menu_Magic_SetTeleportMpCost(i, Menu_Magic_teleportMenuCurrentMp < i);
        }
        break;

    case Menu_Magic_TeleportMenuState_ReturnIfReady:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            if (Menu_Magic_teleportMenuSelectedRow > 0) {
                int v1;
                int v0;
                int a0;
                int a2 = Menu_Magic_teleportMenuAvailableSavePoints[Menu_Magic_teleportMenuSelectedRow - 1];
                i = Menu_Magic_savePointData[a2] + 1;
                v1 = i & 0x1FF;
                init = ~0x3E00;
                v0 = (rowCount & ~0x1FF) | v1;
                v1 = (i >> 7) & 0x3E00;
                v0 &= init;
                v0 |= v1;
                a0 = (i >> 0x18) & 0x1F;
                a0 <<= 0xE;
                v1 = Menu_Magic_teleportCosts[Menu_Magic_teleportMenuSelectedRow - 1] << 0x13;
                v0 &= 0x3FFF;
                return v0 | a0 | v1;
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

    menuItem = Menu_Framework_SetMenuItem(0, 320, 18, 0x8C, 8,
        (char*)&Menu_menuStrings[Menu_menuStrings[Text_Battle_menuStrings_Index_magic]]);
    menuItem->state = 2;
    menuItem->targetPosition0 = 180;
    menuItem->selected = 1;
    Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
    Menu_Framework_InitializeInformationBox();
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
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Framework_InitializeInformationBox();

        i = Menu_Framework_shortcutInvoked;

        if (i != 0) {
            enum Menu_Magic_ShortcutMagicMenu { Menu_Magic_ShortcutMagicMenu_Warlock = 1, Menu_Magic_ShortcutMagicMenu_Shaman, Menu_Magic_ShortcutMagicMenu_Sorcerer, Menu_Magic_ShortcutMagicMenu_Enchanter };
            Menu_Framework_SetMenuItem(i + 9, 0x140, 0x22, 0x8C, 8,
                (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[(i * 3) - 3]])
                ->selected = 1;
            switch (i) {
            case Menu_Magic_ShortcutMagicMenu_Warlock:
                *state = Menu_Magic_MagicMenuState_WarlockMenu;
                Menu_Magic_UpdateWarlockMenu(1);
                break;
            case Menu_Magic_ShortcutMagicMenu_Shaman:
                *state = Menu_Magic_MagicMenuState_ShamanMenu;
                Menu_Magic_UpdateShamanMenu(1);
                break;
            case Menu_Magic_ShortcutMagicMenu_Sorcerer:
                *state = Menu_Magic_MagicMenuState_SorcererMenu;
                Menu_Magic_UpdateSorcererMenu(1);
                break;
            case Menu_Magic_ShortcutMagicMenu_Enchanter:
                *state = Menu_Magic_MagicMenuState_EnchanterMenu;
                Menu_Magic_UpdateEnchanterMenu(1);
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

            if (Battle_HasUnlockedSpellClass(i) == 0) {
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
                (Field_CurrentSavePointId() != 0) && (Audio_Seq_unusedStateFlags.opaqueB5 == 2)
                    ? (char*)(Menu_Magic_teleportationStrings
                              + Text_Menu0_teleportation_Offset_teleportationDisabled)
                    : (char*)(Menu_Magic_teleportationStrings
                              + Text_Menu0_teleportation_Offset_teleportationDesc);

            if ((Field_CurrentSavePointId() != 0) && (Audio_Seq_unusedStateFlags.opaqueB5 == 2)) {
                menuStrings[i * 2 + 1] =
                    (char*)(Menu_Magic_teleportationStrings
                            + Text_Menu0_teleportation_Offset_teleportationDisabled);
            }

            rowTypes[i++] =
                ((j == 48) || (Audio_Seq_unusedStateFlags.opaqueB5 == 2) || (Field_CurrentSavePointId() == 0));
        }

        j = Input_settings.cursorMemory;

        if (*state != Menu_Magic_MagicMenuState_HandleShortcut) {
            Input_settings.cursorMemory = 1;
        }

        Menu_Framework_CreateMenuRows(i, (1 << 8) | 5, menuStrings, rowTypes);
        Input_settings.cursorMemory = j;
        *state = Menu_Magic_MagicMenuState_HandleSubmenu;

        break;

    case Menu_Magic_MagicMenuState_HandleSubmenu:
        selectedRow = Menu_Framework_GetSelectedRow();
        i = selectedRow + 1;

        if (i == 0) {
            break;
        }

        if (i > 0) {
            enum Menu_Magic_MagicSubmenu { Menu_Magic_MagicSubmenu_Warlock, Menu_Magic_MagicSubmenu_Shaman, Menu_Magic_MagicSubmenu_Sorcerer, Menu_Magic_MagicSubmenu_Enchanter, Menu_Magic_MagicSubmenu_Teleport };
            switch (selectedRow) {
            case Menu_Magic_MagicSubmenu_Warlock:
                *state = Menu_Magic_MagicMenuState_WarlockMenu;
                Menu_Magic_UpdateWarlockMenu(1);
                break;
            case Menu_Magic_MagicSubmenu_Shaman:
                *state = Menu_Magic_MagicMenuState_ShamanMenu;
                Menu_Magic_UpdateShamanMenu(1);
                break;
            case Menu_Magic_MagicSubmenu_Sorcerer:
                *state = Menu_Magic_MagicMenuState_SorcererMenu;
                Menu_Magic_UpdateSorcererMenu(1);
                break;
            case Menu_Magic_MagicSubmenu_Enchanter:
                *state = Menu_Magic_MagicMenuState_EnchanterMenu;
                Menu_Magic_UpdateEnchanterMenu(1);
                break;
            case Menu_Magic_MagicSubmenu_Teleport:
                *state = Menu_Magic_MagicMenuState_TeleportMenu;
                Menu_Magic_UpdateTeleportMenu(1);
                break;
            }
        } else {
            if (i == Menu_Framework_MenuSelection_Quit) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                *state = Menu_Magic_MagicMenuState_Exit;
            } else {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                *state = Menu_Magic_MagicMenuState_ExitToMainMenu;
            }
        }

        break;

    case Menu_Magic_MagicMenuState_InitWarlockMenu:
        Menu_Magic_CreateHeader();
        Menu_Framework_SetMenuItem(
            0xA, 0x140, 0x22, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_warlock))
            ->selected = 1;
        Menu_Magic_UpdateWarlockMenu(2);

        *state = Menu_Magic_MagicMenuState_WarlockMenu;
        break;

    case Menu_Magic_MagicMenuState_WarlockMenu:
        i = Menu_Magic_UpdateWarlockMenu(0);

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
        Menu_Framework_SetMenuItem(
            11, 320, 34, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_shaman))
            ->selected = 1;
        Menu_Magic_UpdateShamanMenu(2);

        *state = Menu_Magic_MagicMenuState_ShamanMenu;
        break;

    case Menu_Magic_MagicMenuState_ShamanMenu:
        i = Menu_Magic_UpdateShamanMenu(0);

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
        Menu_Framework_SetMenuItem(
            12, 320, 34, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_sorcerer))
            ->selected = 1;
        Menu_Magic_UpdateSorcererMenu(2);

        *state = Menu_Magic_MagicMenuState_SorcererMenu;
        break;

    case Menu_Magic_MagicMenuState_SorcererMenu:
        i = Menu_Magic_UpdateSorcererMenu(0);

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
        Menu_Framework_SetMenuItem(
            13, 320, 34, 0x7E, 8, (char*)(Menu_Magic_magicStrings + Text_Menu0_magic_Offset_enchanter))
            ->selected = 1;
        Menu_Magic_UpdateEnchanterMenu(2);

        *state = Menu_Magic_MagicMenuState_EnchanterMenu;
        break;

    case Menu_Magic_MagicMenuState_EnchanterMenu:
        i = Menu_Magic_UpdateEnchanterMenu(0);

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
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            *state = Menu_Magic_MagicMenuState_None;
            return 1;
        }

        break;

    case Menu_Magic_MagicMenuState_ExecuteMagic:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_actionSelection.selectionKind = 7;
            Menu_Framework_menuState.returnState = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 1;
            *state = Menu_Magic_MagicMenuState_None;
            return 1;
        }

        break;

    case Menu_Magic_MagicMenuState_Exit:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
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

    flags = Battle_ActorActionRestrictionFlags(0, id);
    Menu_Framework_SetActionCostText(1, "MP", 8, (flags >> 1) & 1);

    cost = Battle_actionDefinitions[id].cost;
    Menu_Magic_mpCostTextBuffer[15] = NULL;

    i = 15;

    if (variableCost != 0) {
        i = 13;
        Menu_Magic_mpCostTextBuffer[14] = '-';
        Menu_Magic_mpCostTextBuffer[13] = '[';
    }

    do {
        cost = Core_Math_SplitDecimalDigit(cost);
        Menu_Magic_mpCostTextBuffer[--i] = (cost & 0xF) + '0';
        cost >>= 4;
        if (variableCost != 0) {
            Menu_Magic_mpCostTextBuffer[--i] = '[';
        }
    } while (cost != 0);

    Menu_Magic_mpCostTextBuffer[--i] = '#';

    Menu_Framework_SetActionCostText(
        0, &Menu_Magic_mpCostTextBuffer[i], variableCost * 4 + 72, (flags >> 1) & 1);
}

// decomp: MENU0.PRG _setMPCostDirect (src/MENU/MENU0.PRG/84.c:64)
void Menu_Magic_SetTeleportMpCost(int costDecimal, int disabled)
{
    int cost;
    int i;

    Menu_Framework_SetActionCostText(1, "MP", 8, disabled);

    cost = costDecimal;
    i = 15;
    Menu_Magic_mpCostDirectTextBuffer[15] = 0;

    do {
        cost = Core_Math_SplitDecimalDigit(cost);
        Menu_Magic_mpCostDirectTextBuffer[--i] = (cost & 0xF) + '0';
        cost >>= 4;
    } while (cost != 0);

    Menu_Magic_mpCostDirectTextBuffer[--i] = '#';

    Menu_Framework_SetActionCostText(0, &Menu_Magic_mpCostDirectTextBuffer[i], 72, disabled);
}

// decomp: MENU0.PRG _getTeleportCost (src/MENU/MENU0.PRG/84.c:117)
int Menu_Magic_TeleportMpCost(int targetSavePointId)
{
    int i;
    int j;
    int cost;
    int closestSavePointId;
    int roomId = Field_CurrentRoomId();

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

// decomp: MENU0.PRG _warlockMagicMenu (src/MENU/MENU0.PRG/84.c:164)
int Menu_Magic_UpdateWarlockMenu(u_int initShortcutInvoked)
{
    enum Menu_Magic_WarlockMenuState { Menu_Magic_WarlockMenuState_Init, Menu_Magic_WarlockMenuState_HandleInput, Menu_Magic_WarlockMenuState_ReturnIfReady, Menu_Magic_WarlockMenuState_LevelledSpellInit, Menu_Magic_WarlockMenuState_LevelledSpell };


    static u_int Menu_Magic_warlockMenuState;
    static int Menu_Magic_warlockMenuSelectedRow;
    static int Menu_Magic_warlockMenuShortcutInvoked;

    int rowCount;
    int i;

    if (initShortcutInvoked) {

        Menu_Magic_warlockMenuShortcutInvoked = (initShortcutInvoked ^ 2) < 1;

        Menu_Framework_FlyOutRowsAndHoistSelection(0, 1);

        Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_Init;
        return 0;
    }

    switch (Menu_Magic_warlockMenuState) {
    case Menu_Magic_WarlockMenuState_Init: {
        char* menuStrings[36];
        int rowTypes[18];

        if (!Menu_Framework_shortcutInvoked && !Menu_Framework_AreMenuRowsIdle()) {
            break;
        }

        rowCount = 0;

        for (i = 0; i < 18; ++i) {
            int actionId = Menu_Magic_warlockSpellIds[i];

            if (Battle_actionDefinitions[actionId].unlocked) {
                menuStrings[rowCount * 2] =
                    (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[i
                                                        + Text_Menu0_magic_Index_warlockSpells]];
                menuStrings[rowCount * 2 + 1] = (char*)&Menu_Magic_magicStrings
                    [Menu_Magic_magicStrings[i + Text_Menu0_magic_Index_warlockSpellDescs]];
                rowTypes[rowCount] = 0;

                if (Battle_ActorActionRestrictionFlags(0, actionId) != 0) {
                    rowTypes[rowCount] |= 1;
                }

                Menu_Magic_availableWarlockSpells[rowCount] = actionId;
                ++rowCount;
            }
        }

        if (Menu_Framework_actionRestrictions & 0xB7) {
            for (i = 0; i < 18; ++i) {
                rowTypes[i] |= 1;
            }
        }

        i = Input_settings.cursorMemory;

        if (Menu_Magic_warlockMenuShortcutInvoked != 0) {
            Input_settings.cursorMemory = 1;
        }

        Menu_Framework_CreateMenuRows(rowCount, (2 << 8) | 7, menuStrings, rowTypes);

        Input_settings.cursorMemory = i;
        Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_HandleInput;
        break;
    }
    case Menu_Magic_WarlockMenuState_HandleInput:

        Menu_Magic_warlockMenuSelectedRow = Menu_Framework_GetSelectedRow() + 1;

        if (Menu_Magic_warlockMenuSelectedRow != 0) {

            Menu_Framework_displayActionCost = 0;

            if (Menu_Framework_shortcutInvoked && (Menu_Magic_warlockMenuSelectedRow == -1)) {
                Menu_Magic_warlockMenuSelectedRow = -2;
            }

            if (Menu_Magic_warlockMenuSelectedRow == -1) {
                Menu_Framework_ClearMenuExcept(0);

                Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_ReturnIfReady;
                break;
            }

            if (Menu_Magic_warlockMenuSelectedRow > 0) {

                Menu_Magic_warlockMenuSelectedRow = Menu_Magic_availableWarlockSpells[Menu_Magic_warlockMenuSelectedRow - 1];

                for (rowCount = 0; rowCount < 7; ++rowCount) {
                    if ((Menu_Magic_warlockMenuSelectedRow == Menu_Magic_warlockMenuLevelledSpells[rowCount])
                        && (Battle_actionDefinitions[Menu_Magic_warlockMenuSelectedRow + 1].unlocked)) {

                        Menu_Framework_FlyOutRowsAndHoistSelection(
                            Menu_Framework_menuMemory.cursorMemories[14], 2);

                        Menu_Framework_displayActionCost = 1;
                        Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_LevelledSpellInit;
                    }
                }

                if (Menu_Magic_warlockMenuState == Menu_Magic_WarlockMenuState_LevelledSpellInit) {
                    break;
                }
            }

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Framework_DismissInformationBox();
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

            Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_ReturnIfReady;

        } else {

            i = Menu_Magic_availableWarlockSpells[Menu_Framework_HighlightedRow()];

            for (rowCount = 0; rowCount < 7; ++rowCount) {
                if (i == Menu_Magic_warlockMenuLevelledSpells[rowCount]) {
                    if (Battle_actionDefinitions[i + 1].unlocked) {
                        i = -i;
                    }
                    break;
                }
            }

            Menu_Magic_SetSpellMpCost(i);
        }

        break;

    case Menu_Magic_WarlockMenuState_ReturnIfReady:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Magic_warlockMenuSelectedRow;
        }
        break;

    case Menu_Magic_WarlockMenuState_LevelledSpellInit:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        i = 2;

        if (Battle_actionDefinitions[Menu_Magic_warlockMenuSelectedRow + 2].unlocked) {
            i = 3;
            if (Battle_actionDefinitions[Menu_Magic_warlockMenuSelectedRow + 3].unlocked) {
                i = 4;
            }
        }

        Menu_Framework_menuMemory.cursorMemories[13] = i;

        if (Menu_Framework_menuMemory.cursorMemories[12] >= i) {
            Menu_Framework_menuMemory.cursorMemories[12] = i - 1;
        }

        Menu_Magic_warlockLevelCursorBlink = 0;
        Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_LevelledSpell;
        break;

    case Menu_Magic_WarlockMenuState_LevelledSpell: {
        u_long* temp_s6;
        int level;
        int var_s7_2;
        u_long* prim;

        i = Menu_Framework_menuMemory.cursorMemories[12];
        rowCount = Menu_Framework_menuMemory.cursorMemories[13];

        if (Input_buttonsPressed.allButtons & PADRup) {

            Menu_PlayLeaveSound();

            Menu_Framework_displayActionCost = 0;

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
            Menu_Framework_DismissInformationBox();
            Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

            Menu_Magic_warlockMenuSelectedRow = -2;
            Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_ReturnIfReady;
            break;
        }

        if (Input_buttonsPressed.allButtons & PADRdown) {

            Menu_PlayLeaveSound();

            for (i = 20; i < 30; ++i) {
                Menu_Framework_FlyOutRowRight(i);
            }

            Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_Init;
            break;
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            if (Battle_ActorActionRestrictionFlags(0, Menu_Magic_warlockMenuSelectedRow + i) == 0) {

                Menu_PlaySelectSound();

                Menu_Framework_displayActionCost = 0;

                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

                Menu_Magic_warlockMenuState = Menu_Magic_WarlockMenuState_ReturnIfReady;
                Menu_Magic_warlockMenuSelectedRow += i;
                break;

            } else {
                Menu_PlayInvalidSound();
            }
        }

        if (Input_buttonRepeat & PADLright) {
            if (i < (rowCount - 1)) {
                ++i;
            }
        }

        if (Input_buttonRepeat & PADLleft) {
            if (i > 0) {
                --i;
            }
        }

        if (i != Menu_Framework_menuMemory.cursorMemories[0xC]) {
            Menu_PlayChangeSound();
            Menu_Framework_menuMemory.cursorMemories[0xC] = i;
        }

        Menu_Magic_SetSpellMpCost(Menu_Magic_warlockMenuSelectedRow + i);

        temp_s6 = Render_frameScratch.orderingTable - 4;

        for (level = 0; level < 4; ++level) {

            var_s7_2 = 0;

            if (i >= level) {
                int v;

                Render_DrawCompactText(
                    Menu_Magic_warlockMenuSpellLevels[level], (99 + level * 32) | vs_getXY(0, 103), NULL);

                prim = Render_QueueSpriteWithTexturePage(
                    Menu_Framework_cursorBrightnessAnimation[Menu_Magic_warlockLevelCursorBlink],
                    (96 + level * 32) | vs_getXY(0, 100), vs_getWH(32, 16), temp_s6);
                prim[4] = vs_getUV0Clut(192, 112, 976, 223);

                v = Menu_Magic_warlockLevelCursorBlink;

                if (level == i) {
                    ++Menu_Magic_warlockLevelCursorBlink;
                    Menu_Magic_warlockLevelCursorBlink &= 0xF;
                } else {
                    Menu_Magic_warlockLevelCursorBlink = v & 0xF;
                }

            } else if (level < rowCount) {

                Render_DrawCompactText(
                    Menu_Magic_warlockMenuSpellLevels[level], (99 + level * 32) | vs_getXY(0, 103), NULL);

                prim = Render_QueueSpriteWithTexturePage(
                    64, (96 + level * 32) | vs_getXY(0, 100), vs_getWH(32, 16), temp_s6);
                prim[4] = vs_getUV0Clut(192, 112, 992, 223);

            } else {

                var_s7_2 = 1;

                Render_DrawCompactText(
                    Menu_Magic_warlockMenuSpellLevels[4], (102 + level * 32) | vs_getXY(0, 103), NULL);

                prim = Render_QueueSpriteWithTexturePage(
                    0, (96 + level * 32) | vs_getXY(0, 100), vs_getWH(32, 16), temp_s6);
                prim[1] = 0x64602000;
                prim[4] = vs_getUV0Clut(192, 112, 992, 223);
            }

            prim[-3] = (u_short)prim[-3] | (getClut(848, 223) << 16);
            prim[-5] = prim[-5] | 0x02000000;
            prim[-8] = (u_short)prim[-8] | (getClut(848, 223) << 16);
            prim[-10] |= 0x02000000;

            if (var_s7_2 != 0) {
                prim[-12] =
                    vs_getTpage(768, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);
            } else {
                prim[-17] =
                    vs_getTpage(768, 0, Render_TextureFormat_Palette4Bit, Render_BlendMode_Add, Render_Dithering_Off);
                prim[-13] = (u_short)prim[-13] | (getClut(848, 223) << 16);
                prim[-15] |= 0x02000000;
            }
        }

        prim = Render_QueueSpriteWithTexturePage(
            128, vs_getXY(80, 100), vs_getWH(16, 16), temp_s6);
        prim[4] = vs_getUV0Clut(80, 48, 976, 223);

        prim += 6;

        prim[0] = vs_getTag(u_long[9], *temp_s6);
        prim[1] = vs_getRGB0(Render_LegacyPrimitiveCode_PolyFt4, 128, 128, 128);
        prim[2] = vs_getXY(224, 100);
        prim[3] = vs_getUV0Clut(95, 48, 976, 223);
        prim[4] = vs_getXY(240, 100);
        prim[5] =
            vs_getUV1Tpage(79, 48, 768, Render_TextureFormat_Palette4Bit, Render_BlendMode_Average, Render_Dithering_Off);
        prim[6] = vs_getXY(224, 116);
        prim[7] = vs_getUV(95, 64);
        prim[8] = vs_getXY(240, 116);
        prim[9] = vs_getUV(79, 64);

        *temp_s6 = ((u_int)prim << 8) >> 8;
        Render_frameScratch.primitiveCursor = prim + 10;

        break;
    }
    }
    return 0;
}

// decomp: MENU0.PRG _shamanMagicMenu (src/MENU/MENU0.PRG/84.c:512)
int Menu_Magic_UpdateShamanMenu(u_int initShortcutInvoked)
{
    enum Menu_Magic_ShamanMenuState { Menu_Magic_ShamanMenuState_Init, Menu_Magic_ShamanMenuState_HandleInput, Menu_Magic_ShamanMenuState_ReturnIfReady };

    static int Menu_Magic_shamanMenuState;
    static int Menu_Magic_shamanMenuSelectedRow;
    static int Menu_Magic_shamanMenuShortcutInvoked;

    char* menuStrings[12];
    int rowTypes[6];
    int i;
    int rowCount;
    int actionId;

    if (initShortcutInvoked != 0) {
        Menu_Magic_shamanMenuShortcutInvoked = (initShortcutInvoked ^ 2) < 1;
        Menu_Framework_FlyOutRowsAndHoistSelection(1, 1);
        Menu_Magic_shamanMenuState = Menu_Magic_ShamanMenuState_Init;
        return 0;
    }

    switch (Menu_Magic_shamanMenuState) {
    case Menu_Magic_ShamanMenuState_Init:
        if ((Menu_Framework_shortcutInvoked == 0) && (Menu_Framework_AreMenuRowsIdle() == 0)) {
            break;
        }

        rowCount = 0;
        for (i = 0; i < 6; ++i) {
            actionId = Menu_Magic_shamanSpellIds[i];
            if (!Battle_actionDefinitions[actionId].unlocked) {
                continue;
            }
            menuStrings[rowCount * 2] = Battle_actionDefinitions[actionId].name;
            menuStrings[rowCount * 2 + 1] =
                (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[i + Text_Menu0_magic_Index_shamanSpellDescs]];
            rowTypes[rowCount] = 0;
            if (Battle_ActorActionRestrictionFlags(0, actionId) != 0) {
                rowTypes[rowCount] |= 1;
            }
            Menu_Magic_availableShamanSpells[rowCount] = actionId;
            ++rowCount;
        }

        if (Menu_Framework_actionRestrictions & 0xB7) {
            for (i = 0; i < 6; ++i) {
                rowTypes[i] |= 1;
            }
        }

        i = Input_settings.cursorMemory;
        if (Menu_Magic_shamanMenuShortcutInvoked != 0) {
            Input_settings.cursorMemory = 1;
        }
        Menu_Framework_CreateMenuRows(rowCount, (2 << 8) | 8, menuStrings, rowTypes);
        Input_settings.cursorMemory = i;
        Menu_Magic_shamanMenuState = Menu_Magic_ShamanMenuState_HandleInput;
        break;
    case Menu_Magic_ShamanMenuState_HandleInput:
        Menu_Magic_shamanMenuSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Magic_shamanMenuSelectedRow != 0) {
            Menu_Framework_displayActionCost = 0;
            if ((Menu_Framework_shortcutInvoked != 0) && (Menu_Magic_shamanMenuSelectedRow == -1)) {
                Menu_Magic_shamanMenuSelectedRow = -2;
            }
            if (Menu_Magic_shamanMenuSelectedRow == -1) {
                Menu_Framework_ClearMenuExcept(0);
            } else {
                if (Menu_Magic_shamanMenuSelectedRow > 0) {
                    Menu_Magic_shamanMenuSelectedRow = Menu_Magic_availableShamanSpells[Menu_Magic_shamanMenuSelectedRow - 1];
                }
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            }
            Menu_Magic_shamanMenuState = Menu_Magic_ShamanMenuState_ReturnIfReady;
        } else {
            Menu_Magic_SetSpellMpCost(Menu_Magic_availableShamanSpells[Menu_Framework_HighlightedRow()]);
        }
        break;
    case Menu_Magic_ShamanMenuState_ReturnIfReady:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Magic_shamanMenuSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU0.PRG _sorcererMagicMenu (src/MENU/MENU0.PRG/84.c:613)
int Menu_Magic_UpdateSorcererMenu(u_int initShortcutInvoked)
{
    enum Menu_Magic_SpellListMenuState { Menu_Magic_SpellListMenuState_Init, Menu_Magic_SpellListMenuState_HandleInput, Menu_Magic_SpellListMenuState_ReturnIfready };

    static int Menu_Magic_sorcererMenuState;
    static int Menu_Magic_sorcererMenuSelectedRow;
    static int Menu_Magic_sorcererMenuShortcutInvoked;

    char* menuStrings[36];
    int rowTypes[18];
    int i;
    int rowCount;
    int actionId;

    if (initShortcutInvoked != 0) {
        Menu_Magic_sorcererMenuShortcutInvoked = (initShortcutInvoked ^ 2) < 1;
        Menu_Framework_FlyOutRowsAndHoistSelection(2, 1);
        Menu_Magic_sorcererMenuState = Menu_Magic_SpellListMenuState_Init;
        return 0;
    }

    switch (Menu_Magic_sorcererMenuState) {
    case Menu_Magic_SpellListMenuState_Init:
        if (!Menu_Framework_shortcutInvoked && !Menu_Framework_AreMenuRowsIdle()) {
            break;
        }
        rowCount = 0;
        for (i = 0; i < 18; ++i) {
            actionId = Menu_Magic_sorcererSpellIds[i];
            if (!Battle_actionDefinitions[actionId].unlocked) {
                continue;
            }
            menuStrings[rowCount * 2] = Battle_actionDefinitions[actionId].name;
            menuStrings[rowCount * 2 + 1] =
                (char*)&Menu_Magic_magicStrings[Menu_Magic_magicStrings[i
                                                    + Text_Menu0_magic_Index_sorcererSpellDescs]];
            rowTypes[rowCount] = 0;
            if (Battle_ActorActionRestrictionFlags(0, actionId) != 0) {
                rowTypes[rowCount] |= 1;
            }
            Menu_Magic_availableSorcererSpells[rowCount] = actionId;
            ++rowCount;
        }

        if (Menu_Framework_actionRestrictions & 0xB7) {
            for (i = 0; i < 18; ++i) {
                rowTypes[i] |= 1;
            }
        }

        i = Input_settings.cursorMemory;
        if (Menu_Magic_sorcererMenuShortcutInvoked != 0) {
            Input_settings.cursorMemory = 1;
        }
        Menu_Framework_CreateMenuRows(rowCount, (2 << 8) | 9, menuStrings, rowTypes);
        Input_settings.cursorMemory = i;
        Menu_Magic_sorcererMenuState = Menu_Magic_SpellListMenuState_HandleInput;
        break;
    case Menu_Magic_SpellListMenuState_HandleInput:
        Menu_Magic_sorcererMenuSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Magic_sorcererMenuSelectedRow != 0) {
            Menu_Framework_displayActionCost = 0;
            if (Menu_Framework_shortcutInvoked && (Menu_Magic_sorcererMenuSelectedRow == -1)) {
                Menu_Magic_sorcererMenuSelectedRow = -2;
            }
            if (Menu_Magic_sorcererMenuSelectedRow == -1) {
                Menu_Framework_ClearMenuExcept(0);
            } else {
                if (Menu_Magic_sorcererMenuSelectedRow > 0) {
                    Menu_Magic_sorcererMenuSelectedRow = Menu_Magic_availableSorcererSpells[Menu_Magic_sorcererMenuSelectedRow - 1];
                }
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            }
            Menu_Magic_sorcererMenuState = Menu_Magic_SpellListMenuState_ReturnIfready;
        } else {
            Menu_Magic_SetSpellMpCost(Menu_Magic_availableSorcererSpells[Menu_Framework_HighlightedRow()]);
        }
        break;
    case Menu_Magic_SpellListMenuState_ReturnIfready:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Magic_sorcererMenuSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU0.PRG _enchanterMagicMenu (src/MENU/MENU0.PRG/84.c:713)
int Menu_Magic_UpdateEnchanterMenu(u_int initShortcutInvoked)
{
    enum Menu_Magic_SpellListMenuState { Menu_Magic_SpellListMenuState_Init, Menu_Magic_SpellListMenuState_HandleInput, Menu_Magic_SpellListMenuState_ReturnIfready };
    static int Menu_Magic_enchanterMenuState;
    static int Menu_Magic_enchanterMenuSelectedRow;
    static int Menu_Magic_enchanterMenuShortcutInvoked;

    char* menuStrings[16];
    int rowTypes[8];
    int i;
    int rowCount;
    int actionId;

    if (initShortcutInvoked != 0) {
        Menu_Magic_enchanterMenuShortcutInvoked = (initShortcutInvoked ^ 2) < 1;
        Menu_Framework_FlyOutRowsAndHoistSelection(3, 1);
        Menu_Magic_enchanterMenuState = Menu_Magic_SpellListMenuState_Init;
        return 0;
    }

    switch (Menu_Magic_enchanterMenuState) {
    case Menu_Magic_SpellListMenuState_Init:
        if ((Menu_Framework_shortcutInvoked == 0) && (Menu_Framework_AreMenuRowsIdle() == 0)) {
            break;
        }
        rowCount = 0;
        for (i = 0; i < 8; ++i) {
            actionId = Menu_Magic_enchanterSpellIds[i];
            if (!Battle_actionDefinitions[actionId].unlocked) {
                continue;
            }
            menuStrings[rowCount * 2] = Battle_actionDefinitions[actionId].name;
            menuStrings[rowCount * 2 + 1] = (char*)&Menu_Magic_magicStrings
                [Menu_Magic_magicStrings[i + Text_Menu0_magic_Index_enchanterSpellDescs]];
            rowTypes[rowCount] = 0;
            if (Battle_ActorActionRestrictionFlags(0, actionId) != 0) {
                rowTypes[rowCount] |= 1;
            }
            Menu_Magic_availableEnchanterSpells[rowCount] = actionId;
            ++rowCount;
        }

        if (Menu_Framework_actionRestrictions & 0xB7) {
            for (i = 0; i < 8; ++i) {
                rowTypes[i] |= 1;
            }
        }

        i = Input_settings.cursorMemory;
        if (Menu_Magic_enchanterMenuShortcutInvoked != 0) {
            Input_settings.cursorMemory = 1;
        }
        Menu_Framework_CreateMenuRows(rowCount, (2 << 8) | 10, menuStrings, rowTypes);
        Input_settings.cursorMemory = i;
        Menu_Magic_enchanterMenuState = Menu_Magic_SpellListMenuState_HandleInput;
        break;
    case Menu_Magic_SpellListMenuState_HandleInput:
        Menu_Magic_enchanterMenuSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Magic_enchanterMenuSelectedRow != 0) {
            Menu_Framework_displayActionCost = 0;
            if ((Menu_Framework_shortcutInvoked != 0) && (Menu_Magic_enchanterMenuSelectedRow == -1)) {
                Menu_Magic_enchanterMenuSelectedRow = -2;
            }
            if (Menu_Magic_enchanterMenuSelectedRow == -1) {
                Menu_Framework_ClearMenuExcept(0);
            } else {
                if (Menu_Magic_enchanterMenuSelectedRow > 0) {
                    Menu_Magic_enchanterMenuSelectedRow = Menu_Magic_availableEnchanterSpells[Menu_Magic_enchanterMenuSelectedRow - 1];
                }
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            }
            Menu_Magic_enchanterMenuState = Menu_Magic_SpellListMenuState_ReturnIfready;
        } else {
            Menu_Magic_SetSpellMpCost(Menu_Magic_availableEnchanterSpells[Menu_Framework_HighlightedRow()]);
        }
        break;
    case Menu_Magic_SpellListMenuState_ReturnIfready:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Magic_enchanterMenuSelectedRow;
        }
        break;
    }
    return 0;
}

// MENU0.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Magic_ResetMenu0Statics(void)
{
    {
        static const char initial[] = { Battle_WarlockSpell_Explosion,
        Battle_WarlockSpell_Thunderburst, Battle_WarlockSpell_FlameSphere,
        Battle_WarlockSpell_GaeaStrike, Battle_WarlockSpell_Avalanche,
        Battle_WarlockSpell_RadialSurge, Battle_WarlockSpell_Meteor };
        memcpy((void*)&Menu_Magic_warlockMenuLevelledSpells, &initial, sizeof Menu_Magic_warlockMenuLevelledSpells);
    }
    {
        static const char* initial[] = { "LV1", "LV2", "LV3", "LV4", "--" };
        memcpy((void*)&Menu_Magic_warlockMenuSpellLevels, &initial, sizeof Menu_Magic_warlockMenuSpellLevels);
    }
}

