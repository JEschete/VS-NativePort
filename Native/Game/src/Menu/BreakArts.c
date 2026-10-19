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
#include "Content/StringTables.h"
#include "Core/Math/FixedPoint.h"
#include "DiscQueue.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/BreakArts.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Menus.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "State/Progress.h"
#include "State/Settings.h"

// decomp: _ (_@src/MENU/MENU1.PRG/30.c)
char Menu_BreakArts_layoutPadding1;

// decomp: local static of _setHPCost, at file scope so the module reset reaches it
static char Menu_BreakArts_setHpCostDigitBuffer[16];

// decomp: _strings (_strings@src/MENU/MENU1.PRG/30.c)
u_short Menu_BreakArts_strings[2151]; /* contents: disc string table (ModuleData.c) */

// decomp: local static of _weaponArtsMenu, at file scope so the module reset reaches it
static u_short Menu_BreakArts_artListActionIds[4];

// decomp: local static of _weaponArtsMenu, at file scope so the module reset reaches it
static int Menu_BreakArts_artListForceCursorMemory = 0;

// decomp: local static of _weaponArtsMenu, at file scope so the module reset reaches it
static int Menu_BreakArts_artListSelectedRow = 0;

// decomp: local static of _weaponCategoriesMenu, at file scope so the module reset reaches it
static int Menu_BreakArts_weaponCategoriesSelectedRow = 0;

// decomp: local static of _weaponArtsMenu, at file scope so the module reset reaches it
static int Menu_BreakArts_artListState = 0;

// decomp: local static of _weaponArtsMenu, at file scope so the module reset reaches it
static int Menu_BreakArts_artListWeaponType = 0;

// decomp: local static of vs_menu1_exec, at file scope so the module reset reaches it
static int Menu_BreakArts_updateWeaponType = 0;

// decomp: MENU1.PRG _setHPCost (src/MENU/MENU1.PRG/30.c:13)
void Menu_BreakArts_SetHpCost(int id)
{

    int flags;
    int i;
    int cost;

    flags = Battle_ActorActionRestrictionFlags(0, id);
    Menu_Framework_SetActionCostText(1, "HP", 8, (flags >> 1) & 1);

    cost = Battle_actionDefinitions[id].cost;
    Menu_BreakArts_setHpCostDigitBuffer[15] = 0;

    i = 15;
    do {
        --i;
        cost = Core_Math_SplitDecimalDigit(cost);
        Menu_BreakArts_setHpCostDigitBuffer[i] = (cost & 0xF) + '0';
        cost >>= 4;
    } while (cost != 0);

    --i;
    Menu_BreakArts_setHpCostDigitBuffer[i] = '#';

    Menu_Framework_SetActionCostText(0, &Menu_BreakArts_setHpCostDigitBuffer[i], 72, (flags >> 1) & 1);
}

// decomp: MENU1.PRG _setNextArtUnlockPointsRemaining (src/MENU/MENU1.PRG/30.c:45)
void Menu_BreakArts_DrawNextUnlockPoints(int xOffset, int weaponCategory, int row)
{
    char pointsBuf[16];
    int pos;
    int points;
    int i;
    char* pointsStr;

    int category = weaponCategory;
    category %= 10;

    i = Audio_Seq_unusedArtsStatus.artsLearned[category];

    if (i == 4) {
        return;
    }

    points = Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[category];
    points = Battle_breakArtPointRequirements[category][i] - points;

    if (points < 0) {
        points = 0;
    }

    pos = (xOffset + 206) | (((row * 16) + 50) << 16);
    pointsBuf[14] = 'T';
    pointsBuf[15] = 0;
    pointsBuf[13] = 'P';

    i = 12;

    do {
        points = Core_Math_SplitDecimalDigit(points);
        pointsBuf[i] = (points & 0xF) + '0';
        points = points >> 4;
        --i;
    } while (points != 0);

    pointsStr = pointsBuf + i;
    pointsStr[0] = '#';

    Render_DrawCompactText("NEXT", pos, NULL);
    Render_DrawCompactText(pointsStr, pos + 96, NULL);
}

// decomp: MENU1.PRG _weaponArtsMenu (src/MENU/MENU1.PRG/30.c:104)
int Menu_BreakArts_UpdateArtList(int typeCursorMem)
{
    enum Menu_BreakArts_ArtSelectionState { Menu_BreakArts_ArtSelectionState_Init, Menu_BreakArts_ArtSelectionState_WaitForSelection, Menu_BreakArts_ArtSelectionState_ReturnSelection };

    static char Menu_BreakArts_artListIsLastRow;
    static u_char Menu_BreakArts_artListAnimationIndex;
    static char Menu_BreakArts_artListArtsLearned;
    static char Menu_BreakArts_artListRemainingRows;
    static char Menu_BreakArts_artListLayoutPadding[12];

    int i;

    if (typeCursorMem != 0) {
        Menu_Framework_MenuRow* menuItem;
        Menu_BreakArts_artListIsLastRow = 0;
        Menu_BreakArts_artListAnimationIndex = 10;
        Menu_BreakArts_artListForceCursorMemory = typeCursorMem >> 4;
        typeCursorMem &= 0xF;
        Menu_BreakArts_artListWeaponType = typeCursorMem;
        menuItem = Menu_Framework_SetMenuItem(
            10, 320, 34, 0x7E, 8, (char*)&Menu_BreakArts_strings[Menu_BreakArts_strings[(Menu_BreakArts_artListWeaponType - 1) * 3]]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        menuItem->rowIcon = typeCursorMem;
        Menu_BreakArts_artListState = Menu_BreakArts_ArtSelectionState_Init;
        return 0;
    }

    switch (Menu_BreakArts_artListState) {
    case Menu_BreakArts_ArtSelectionState_Init: {
        char* menuStrings[10];
        int rowTypes[5];
        int rowCount;
        int cursorMemory;

        if ((Menu_Framework_shortcutInvoked == 0) && (Menu_Framework_AreMenuRowsIdle() == 0)) {
            break;
        }

        rowCount = 0;

        for (i = 0; i < 4; ++i) {

            int actionId = Battle_ActionId_DaggerArtOne + (Menu_BreakArts_artListWeaponType - 1) * 4 + i;

            if (Battle_actionDefinitions[actionId].unlocked) {
                menuStrings[rowCount * 2] = (char*)Battle_actionDefinitions[actionId].name;
                menuStrings[rowCount * 2 + 1] =
                    (char*)&Menu_BreakArts_strings[Menu_BreakArts_strings[Text_Menu1_strings_Index_daggerArt1 + i
                                              + ((Menu_BreakArts_artListWeaponType - 1) * 4)]];
                rowTypes[rowCount] = 0;

                if ((Menu_BreakArts_artListWeaponType != Battle_characterState->equippedWeaponCategory)
                    || (Battle_ActorActionRestrictionFlags(0, actionId) != 0)) {
                    rowTypes[rowCount] = 1;
                }

                Menu_BreakArts_artListActionIds[rowCount++] = actionId;
            }
        }

        for (i = 0; i < 4; ++i) {
            if (Menu_Framework_actionRestrictions & 0x15F) {
                rowTypes[i] |= 1;
            }
        }

        if (Menu_Framework_shortcutInvoked == 0) {
            menuStrings[rowCount * 2] = (char*)&Menu_BreakArts_strings[Text_Menu1_strings_Offset_viewArts];
            menuStrings[rowCount * 2 + 1] =
                (char*)&Menu_BreakArts_strings[Text_Menu1_strings_Offset_viewArtsDesc];
            rowTypes[rowCount] = 2;
            Menu_BreakArts_artListActionIds[rowCount] = 0xFFFF;
            ++rowCount;
        }

        cursorMemory = Input_settings.cursorMemory;

        if (Menu_BreakArts_artListForceCursorMemory != 0) {
            Input_settings.cursorMemory = 1;
            Menu_BreakArts_artListForceCursorMemory = 0;
        }

        Menu_Framework_CreateMenuRows(
            rowCount, (Menu_BreakArts_artListWeaponType + 11) | 0x200, menuStrings, rowTypes);

        Input_settings.cursorMemory = cursorMemory;
        Menu_BreakArts_artListState = Menu_BreakArts_ArtSelectionState_WaitForSelection;
        Menu_BreakArts_artListArtsLearned = rowCount;
        Menu_BreakArts_artListRemainingRows = rowCount;

        break;
    }

    case Menu_BreakArts_ArtSelectionState_WaitForSelection:
        if (Menu_BreakArts_artListRemainingRows != 0) {
            --Menu_BreakArts_artListRemainingRows;
        }

        Menu_BreakArts_artListIsLastRow = Menu_BreakArts_artListRemainingRows == 0;
        Menu_BreakArts_artListSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        Menu_Framework_displayActionCost = 0;

        if (Menu_BreakArts_artListSelectedRow != Menu_Framework_MenuSelection_Confirm) {

            Menu_BreakArts_artListIsLastRow = 0;

            if (Menu_BreakArts_artListSelectedRow > 0) {
                Menu_BreakArts_artListSelectedRow = Menu_BreakArts_artListActionIds[Menu_BreakArts_artListSelectedRow - 1];
            } else if (Menu_Framework_shortcutInvoked != 0) {
                Menu_BreakArts_artListSelectedRow = Menu_Framework_MenuSelection_Quit;
            }

            if (Menu_BreakArts_artListSelectedRow == 0xFFFF) {
                Menu_Framework_ClearMenuExcept(1);
            } else {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            }

            Menu_BreakArts_artListState = Menu_BreakArts_ArtSelectionState_ReturnSelection;
        } else {
            i = Menu_BreakArts_artListActionIds[Menu_Framework_HighlightedRow()];
            if (i != 0xFFFF) {
                Menu_BreakArts_SetHpCost(i);
            }
        }
        break;

    case Menu_BreakArts_ArtSelectionState_ReturnSelection:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_BreakArts_artListSelectedRow;
        }
        break;
    }

    if (Menu_BreakArts_artListIsLastRow != 0) {
        if (Menu_BreakArts_artListAnimationIndex != 0) {
            --Menu_BreakArts_artListAnimationIndex;
        }
    } else {
        Menu_BreakArts_artListAnimationIndex = Menu_Framework_animationIndices[Menu_BreakArts_artListAnimationIndex];
    }

    Menu_BreakArts_DrawNextUnlockPoints(
        Menu_Framework_rowAnimationSteps[Menu_BreakArts_artListAnimationIndex], Menu_BreakArts_artListWeaponType, Menu_BreakArts_artListArtsLearned);

    return 0;
}

// decomp: MENU1.PRG _weaponCategoriesMenu (src/MENU/MENU1.PRG/30.c:266)
int Menu_BreakArts_UpdateWeaponCategories(int initialize)
{
    enum Menu_BreakArts_ArtSelectionState { Menu_BreakArts_ArtSelectionState_Init, Menu_BreakArts_ArtSelectionState_WaitForSelection, Menu_BreakArts_ArtSelectionState_ReturnSelection };

    static int Menu_BreakArts_weaponCategoriesState = Menu_BreakArts_ArtSelectionState_Init;

    if (initialize != 0) {
        Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(
            10, 320, 34, 0x7E, 8, (char*)&Menu_BreakArts_strings[Text_Menu1_strings_Offset_viewArts]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 180;
        menuItem->selected = 1;
        Menu_BreakArts_weaponCategoriesState = Menu_BreakArts_ArtSelectionState_Init;
        return 0;
    }

    switch (Menu_BreakArts_weaponCategoriesState) {
    case Menu_BreakArts_ArtSelectionState_Init: {
        char* menuStrings[20];
        int rowTypes[10];
        int i;
        int j;

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        for (i = 0; i < 10; ++i) {
            menuStrings[i * 2] = (char*)&Menu_BreakArts_strings[Menu_BreakArts_strings[i * 3]];
            menuStrings[i * 2 + 1] = (char*)&Menu_BreakArts_strings[Menu_BreakArts_strings[i * 3 + 1]];
            rowTypes[i] = 0x04000000 * (i + 1);
            for (j = 0; j < 4; ++j) {
                if (Battle_actionDefinitions[i * 4 + Battle_ActionId_DaggerArtOne + j].unlocked) {
                    break;
                }
            }
            if (j == 4) {
                menuStrings[i * 2 + 1] = (char*)&Menu_BreakArts_strings[Menu_BreakArts_strings[i * 3 + 2]];
                rowTypes[i] |= 1;
            } else if (i == (Battle_characterState->equippedWeaponCategory - 1)) {
                rowTypes[i] |= 4;
            }
        }

        Menu_Framework_CreateMenuRows(10, (2 << 8) | 22, menuStrings, rowTypes);

        Menu_BreakArts_weaponCategoriesState = Menu_BreakArts_ArtSelectionState_WaitForSelection;
        break;
    }

    case Menu_BreakArts_ArtSelectionState_WaitForSelection:
        Menu_BreakArts_weaponCategoriesSelectedRow = Menu_Framework_GetSelectedRow() + 1;

        if (Menu_BreakArts_weaponCategoriesSelectedRow != Menu_Framework_MenuSelection_Confirm) {
            if (Menu_BreakArts_weaponCategoriesSelectedRow == Menu_Framework_MenuSelection_Quit) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(1);
            }
            Menu_BreakArts_weaponCategoriesState = Menu_BreakArts_ArtSelectionState_ReturnSelection;
        }
        break;

    case Menu_BreakArts_ArtSelectionState_ReturnSelection:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_BreakArts_weaponCategoriesSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU1.PRG _setMenuHeader (src/MENU/MENU1.PRG/30.c:344)
void Menu_BreakArts_CreateHeader(void)
{
    Menu_Framework_MenuRow* menuItem = Menu_Framework_SetMenuItem(1, 320, 18, 0x8C, 8,
        (char*)&Menu_menuStrings
            [Menu_menuStrings[Text_Battle_menuStrings_Index_breakArts]]);

    menuItem->state = 2;
    menuItem->targetPosition0 = 180;
    menuItem->selected = 1;

    Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_Menu);
    Menu_Framework_InitializeInformationBox();
}

// decomp: MENU1.PRG vs_menu1_exec (src/MENU/MENU1.PRG/30.c:358)
int Menu_BreakArts_Update(u_char* state)
{
    enum Menu_BreakArts_BreakArtsMenuState {
        Menu_BreakArts_BreakArtsMenuState_None,
        Menu_BreakArts_BreakArtsMenuState_Init = 3,
        Menu_BreakArts_BreakArtsMenuState_ArtsInit,
        Menu_BreakArts_BreakArtsMenuState_ArtsInitWithTitle,
        Menu_BreakArts_BreakArtsMenuState_ArtsMenu,
        Menu_BreakArts_BreakArtsMenuState_WeaponCategoryMenu,
        Menu_BreakArts_BreakArtsMenuState_ExitToMainMenu,
        Menu_BreakArts_BreakArtsMenuState_ExecuteArt,
        Menu_BreakArts_BreakArtsMenuState_Exit
    };


    switch (*state) {
    case Menu_BreakArts_BreakArtsMenuState_Init:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Framework_InitializeInformationBox();

        Menu_BreakArts_updateWeaponType = Battle_characterState->equippedWeaponCategory;
        // Fallthrough

    case Menu_BreakArts_BreakArtsMenuState_ArtsInit:
        Menu_BreakArts_UpdateArtList(Menu_BreakArts_updateWeaponType);
        *state = Menu_BreakArts_BreakArtsMenuState_ArtsMenu;
        break;

    case Menu_BreakArts_BreakArtsMenuState_ArtsInitWithTitle:
        Menu_BreakArts_CreateHeader();
        Menu_BreakArts_UpdateArtList(Menu_BreakArts_updateWeaponType | Menu_BreakArts_ArtListOption_OptForceCursorMemory);
        *state = Menu_BreakArts_BreakArtsMenuState_ArtsMenu;
        break;

    case Menu_BreakArts_BreakArtsMenuState_ArtsMenu: {
        int selectedRow = Menu_BreakArts_UpdateArtList(0);

        if (selectedRow == Menu_Framework_MenuSelection_Confirm) {
            break;
        }

        if (selectedRow > 0) {
            if (selectedRow == Menu_Framework_MenuSelection_Back) {
                *state = Menu_BreakArts_BreakArtsMenuState_WeaponCategoryMenu;
                Menu_BreakArts_UpdateWeaponCategories(1);
            } else {
                Menu_Framework_actionSelection.executeAbility.word = selectedRow;
                Menu_Framework_menuState.executeAbilityType = 5;
                *state = Menu_BreakArts_BreakArtsMenuState_ExecuteArt;
            }
        } else if (selectedRow != Menu_Framework_MenuSelection_Quit) {
            *state = Menu_BreakArts_BreakArtsMenuState_ExitToMainMenu;
        } else {
            *state = Menu_BreakArts_BreakArtsMenuState_Exit;
        }

        break;
    }

    case Menu_BreakArts_BreakArtsMenuState_WeaponCategoryMenu: {
        int row = Menu_BreakArts_UpdateWeaponCategories(0);

        if (row == 0) {
            break;
        }

        if (row == Menu_Framework_MenuSelection_Quit) {
            *state = Menu_BreakArts_BreakArtsMenuState_Exit;
        } else {
            if (row > 0) {
                Menu_BreakArts_updateWeaponType = row;
            }

            *state = Menu_BreakArts_BreakArtsMenuState_ArtsInit;
        }

        break;
    }

    case Menu_BreakArts_BreakArtsMenuState_ExitToMainMenu:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            *state = Menu_BreakArts_BreakArtsMenuState_None;
            return 1;
        }

        break;

    case Menu_BreakArts_BreakArtsMenuState_ExecuteArt:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_actionSelection.selectionKind = 6;
            Menu_Framework_menuState.returnState = Menu_Framework_menuState.currentMenu;
            Menu_Framework_menuState.currentMenu = 2;
            *state = Menu_BreakArts_BreakArtsMenuState_None;
            return 1;
        }

        break;

    case Menu_BreakArts_BreakArtsMenuState_Exit:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_menuState.currentMenu = 2;
            *state = Menu_BreakArts_BreakArtsMenuState_None;
            return 1;
        }

        break;
    }

    return 0;
}

// MENU1.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_BreakArts_ResetMenu1Statics(void)
{
    {
        static const char initial[16];
        memcpy((void*)&Menu_BreakArts_setHpCostDigitBuffer, &initial, sizeof Menu_BreakArts_setHpCostDigitBuffer);
    }
    {
        static const u_short initial[4];
        memcpy((void*)&Menu_BreakArts_artListActionIds, &initial, sizeof Menu_BreakArts_artListActionIds);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BreakArts_artListForceCursorMemory, &initial, sizeof Menu_BreakArts_artListForceCursorMemory);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BreakArts_artListSelectedRow, &initial, sizeof Menu_BreakArts_artListSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BreakArts_artListState, &initial, sizeof Menu_BreakArts_artListState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BreakArts_artListWeaponType, &initial, sizeof Menu_BreakArts_artListWeaponType);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BreakArts_weaponCategoriesSelectedRow, &initial, sizeof Menu_BreakArts_weaponCategoriesSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BreakArts_updateWeaponType, &initial, sizeof Menu_BreakArts_updateWeaponType);
    }
}

