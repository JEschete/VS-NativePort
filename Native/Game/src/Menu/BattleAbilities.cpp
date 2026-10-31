// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Targeting.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/Heap.h"
#include "Core/Text/TextCode.h"
#include "DiscQueue.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/Menus.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Sounds.h"
#include "Menu/TextBox.h"
#include "Render/Font.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
#include "State/Settings.h"

// decomp: _abilityCount (_abilityCount@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_abilityCount = 0;

// decomp: _abilityMenuState (_abilityMenuState@src/MENU/MENU2.PRG/64.c)
u_int Menu_BattleAbilities_abilityMenuState = 0;

// decomp: _abilityText (_abilityText@src/MENU/MENU2.PRG/64.c)
u_short* Menu_BattleAbilities_abilityText = NULL;

// decomp: _battleAbilityMenuStrings (_battleAbilityMenuStrings@src/MENU/MENU2.PRG/143C.c)
u_short Menu_BattleAbilities_battleAbilityMenuStrings[292]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _battleAbilityStrings (_battleAbilityStrings@MENU/MENU2.PRG)
u_short Menu_BattleAbilities_battleAbilityStrings[1060]; /* contents: disc string table (ModuleData.cpp) */

// decomp: _firstAbility (_firstAbility@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_firstAbility = 0;

// decomp: _highlightedAbility (_highlightedAbility@src/MENU/MENU2.PRG/64.c)
int Menu_BattleAbilities_highlightedAbility = 0;

// decomp: _selectedAbilityType (_selectedAbilityType@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_selectedAbilityType = 0;

// decomp: _unlockableChainAbilities (_unlockableChainAbilities@src/MENU/MENU2.PRG/143C.c)
char Menu_BattleAbilities_unlockableChainAbilities[] = { 0x18, 0x19, 0x22, 0x25, 0x23, 0x1E, 0x1A,
    0x1B, 0x20, 0x1D, 0x1F };

// decomp: _unlockableDefenseAbilities (_unlockableDefenseAbilities@src/MENU/MENU2.PRG/143C.c)
char Menu_BattleAbilities_unlockableDefenseAbilities[] = { 0x2F, 0x30, 0x31, 0x32, 0x29, 0x2A, 0x33,
    0x34, 0x2C, 0x2D, 0x35 };

// decomp: _unlockedChainAbilities (_unlockedChainAbilities@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_unlockedChainAbilities[16];

// decomp: _unlockedDefenseAbilities (_unlockedDefenseAbilities@src/MENU/MENU2.PRG/64.c)
char Menu_BattleAbilities_unlockedDefenseAbilities[16];

// decomp: local static of _abilityMenu, at file scope so the module reset reaches it
static int Menu_BattleAbilities_assignmentsCursorAnimStep = 0;

// decomp: local static of _battleAbilityUnlocked, at file scope so the module reset reaches it
static u_char Menu_BattleAbilities_abilityUnlockRemainingChainAbilities[16];

// decomp: local static of _battleAbilityUnlocked, at file scope so the module reset reaches it
static u_char Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities[11];

// decomp: local static of _chainAbilityMenu, at file scope so the module reset reaches it
static int Menu_BattleAbilities_chainAbilityMenuState = 0;

// decomp: local static of _defenseAbilityMenu, at file scope so the module reset reaches it
static int Menu_BattleAbilities_defenseAbilityMenuState = 0;

// decomp: local static of _setRiskCost, at file scope so the module reset reaches it
static char Menu_BattleAbilities_setRiskCostStringBuffer[16];

// decomp: MENU2.PRG _breakArtUnlocked (src/MENU/MENU2.PRG/143C.c:27)
int Menu_BattleAbilities_UpdateBreakArtUnlock(int init)
{
    static char Menu_BattleAbilities_breakArtUnlockMessageTimeout;
    static char Menu_BattleAbilities_breakArtUnlockLayoutPadding[15] __attribute__((unused));

    if (init != 0) {
        int actionId;
        int weaponCategory = Battle_characterState->equippedWeaponCategory;
        int weaponCategoryMod = weaponCategory;
        u_char(*learned_arts_by_weapon_category)[12] = &Audio_Seq_unusedArtsStatus.artsLearned;
        weaponCategoryMod %= 10;
        actionId = (*learned_arts_by_weapon_category)[weaponCategoryMod];

        if (actionId == 4) {
            return 1;
        }

        if (Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[weaponCategoryMod]
            < Battle_breakArtPointRequirements[weaponCategoryMod][actionId]) {
            return 1;
        }

        Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(3));

        Audio_Seq_unusedArtsStatus.artsLearned[weaponCategoryMod] = actionId + 1;
        actionId = 184 + ((weaponCategory - 1) * 4) + actionId;
        Battle_stringContext.strings[0] =
            (char*)&Menu_BattleAbilities_battleAbilityMenuStrings[Menu_BattleAbilities_battleAbilityMenuStrings[weaponCategory]];
        Battle_stringContext.strings[1] = Battle_actionDefinitions[actionId].name;

        Menu_Framework_SetInformationMessage((char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                [Text_Menu2_battleAbilitiesMenu_Offset_breakArtUnlock]);

        Battle_actionDefinitions[actionId].unlocked = 1;
        Menu_BattleAbilities_breakArtUnlockMessageTimeout = 120;

    } else if (Menu_textBoxes[7].flags.done) {
        if ((Menu_BattleAbilities_breakArtUnlockMessageTimeout == 0) || (Input_buttonsPressed.ports[0].lowByte != 0)) {
            return 1;
        }
        --Menu_BattleAbilities_breakArtUnlockMessageTimeout;
    }

    return 0;
}

// decomp: MENU2.PRG _getAbilityDescription (src/MENU/MENU2.PRG/143C.c:81)
char* Menu_BattleAbilities_UnlockDescription(int abilityId)
{
    static char Menu_BattleAbilities_unlockDescriptionAbilityDescLookup[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, Text_Menu2_battleAbilities_Index_chainAbilityEffect2,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect3,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect4,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect5, 0,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect7,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect8,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect9,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect10, 0,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect11,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect12, 0,
        Text_Menu2_battleAbilities_Index_chainAbilityEffect13, 0, 0, 0,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect1,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect2, 0,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect4,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect5, 0,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect7,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect8,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect9,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect10,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect11,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect12,
        Text_Menu2_battleAbilities_Index_defenseAbilityEffect13 };

    u_char index = Menu_BattleAbilities_unlockDescriptionAbilityDescLookup[abilityId];

    if (Menu_BattleAbilities_unlockDescriptionAbilityDescLookup[abilityId] == 0) {
        return (char*)&Menu_BattleAbilities_battleAbilityMenuStrings[Text_Menu2_battleAbilitiesMenu_Offset_unknown1];
    }

    return (char*)&Menu_BattleAbilities_battleAbilityStrings[Menu_BattleAbilities_battleAbilityStrings[index]];
}

// decomp: MENU2.PRG _battleAbilityUnlocked (src/MENU/MENU2.PRG/143C.c:125)
int Menu_BattleAbilities_UpdateAbilityUnlock(int initialize)
{
    enum Menu_BattleAbilities_AbilityUnlockState {
        Menu_BattleAbilities_AbilityUnlockState_Init,
        Menu_BattleAbilities_AbilityUnlockState_BothRemaining,
        Menu_BattleAbilities_AbilityUnlockState_ChainOnly,
        Menu_BattleAbilities_AbilityUnlockState_DefenseOnly,
        Menu_BattleAbilities_AbilityUnlockState_UnlockChainAbility,
        Menu_BattleAbilities_AbilityUnlockState_UnlockDefenseAbility,
        Menu_BattleAbilities_AbilityUnlockState_ShowMessage,
        Menu_BattleAbilities_AbilityUnlockState_Reinit
    };

    static u_char Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount;
    static u_char Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount;
    static char Menu_BattleAbilities_abilityUnlockState;
    static char Menu_BattleAbilities_abilityUnlockTimeout;
    static char Menu_BattleAbilities_abilityUnlockMenuNotEntered;

    char* menuStrings[4];
    int rowTypes[2];
    char* chainAbilityMenuStrings[22];
    int chainAbilityRowTypes[11];
    char* defenseAbilityMenuStrings[22];
    int defenseAbilityRowTypes[11];
    int rowCount;
    int row;
    int i;
    int abilityState;
    Menu_Framework_MenuRow* menuItem;
    int action;

    if (initialize != 0) {

        i = Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked;

        if ((i == 22)
            || (Audio_Seq_unusedArtsStatus.kills.totalKills
                < Battle_battleAbilityPointRequirements[i])) {
            return 1;
        }

        ++Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked;

        memset(Menu_Framework_menuMemory.opaque34, 0, sizeof Menu_Framework_menuMemory.opaque34);

        Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount = 0;

        for (i = 0; i < 11; ++i) {
            int ability = Menu_BattleAbilities_unlockableChainAbilities[i];
            if (!Battle_actionDefinitions[ability].unlocked) {
                Menu_BattleAbilities_abilityUnlockRemainingChainAbilities[Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount++] = ability;
            }
        }

        Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount = 0;

        for (i = 0; i < 11; ++i) {
            int ability = Menu_BattleAbilities_unlockableDefenseAbilities[i];
            if (!Battle_actionDefinitions[ability].unlocked) {
                Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities[Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount++] = ability;
            }
        }

        Menu_BattleAbilities_abilityUnlockMenuNotEntered = 3;
        Menu_BattleAbilities_abilityUnlockTimeout = 120;
        Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_Init;

        return 0;
    }

    switch (Menu_BattleAbilities_abilityUnlockState) {
    case Menu_BattleAbilities_AbilityUnlockState_Init:
        abilityState = Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount != 0;

        if (Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount != 0) {
            abilityState += 2;
        }

        switch (abilityState) {

            enum Menu_BattleAbilities_RemainingAbilities {
                Menu_BattleAbilities_RemainingAbilities_None,
                Menu_BattleAbilities_RemainingAbilities_ChainOnly,
                Menu_BattleAbilities_RemainingAbilities_DefenseOnly,
                Menu_BattleAbilities_RemainingAbilities_Both
            };

        case Menu_BattleAbilities_RemainingAbilities_None:
            Menu_Framework_SetInformationMessage((char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                    [Text_Menu2_battleAbilitiesMenu_Offset_allAbilitiesUnlocked]);
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_ShowMessage;
            break;

        case Menu_BattleAbilities_RemainingAbilities_ChainOnly:
            menuItem = Menu_Framework_SetMenuItem(0, 320, 18, 140, 8,
                (char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                    [Text_Menu2_battleAbilitiesMenu_Offset_chainAbilities]);
            menuItem->state = 2;
            menuItem->targetPosition0 = 180;
            menuItem->selected = 1;
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_ChainOnly;
            break;

        case Menu_BattleAbilities_RemainingAbilities_DefenseOnly:
            menuItem = Menu_Framework_SetMenuItem(1, 320, 18, 140, 8,
                (char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                    [Text_Menu2_battleAbilitiesMenu_Offset_defenseAbilities]);
            menuItem->state = 2;
            menuItem->targetPosition0 = 180;
            menuItem->selected = 1;
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_DefenseOnly;
            break;

        case Menu_BattleAbilities_RemainingAbilities_Both:
            i = Input_settings.cursorMemory;
            menuStrings[0] = (char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                [Text_Menu2_battleAbilitiesMenu_Offset_chainAbilities];
            menuStrings[1] =
                (char*)(&Menu_BattleAbilities_battleAbilityMenuStrings[Text_Menu2_battleAbilitiesMenu_Offset_select]);
            menuStrings[2] = (char*)(&Menu_BattleAbilities_battleAbilityMenuStrings
                    [Text_Menu2_battleAbilitiesMenu_Offset_defenseAbilities]);
            menuStrings[3] = menuStrings[1];
            rowTypes[0] = 0;
            rowTypes[1] = 0;
            Input_settings.cursorMemory = 1;

            Menu_Framework_CreateMenuRows(2, menuRowInfo(10, 1, 0), menuStrings, rowTypes);

            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_BothRemaining;
            Input_settings.cursorMemory = i;

            break;
        }
        break;

    case Menu_BattleAbilities_AbilityUnlockState_BothRemaining:
        i = Menu_Framework_GetSelectedRow() + 1;

        if (i != Menu_Framework_MenuSelection_Confirm) {
            if (i < 0) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_Reinit;
            } else {
                Menu_Framework_FlyOutRowsAndHoistSelection(i - 1, 0);
                Menu_BattleAbilities_abilityUnlockState += i;
            }
        }

        break;

    case Menu_BattleAbilities_AbilityUnlockState_ChainOnly:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {

            for (i = 0; i < Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount; ++i) {
                chainAbilityMenuStrings[i * 2] =
                    Battle_actionDefinitions[Menu_BattleAbilities_abilityUnlockRemainingChainAbilities[i]].name;
                chainAbilityMenuStrings[i * 2 + 1] =
                    Menu_BattleAbilities_UnlockDescription(Menu_BattleAbilities_abilityUnlockRemainingChainAbilities[i]);
                chainAbilityRowTypes[i] = 0;
            }

            rowCount = 4;

            if (Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount == 1) {
                rowCount = 1;
            } else if (Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount < 5) {
                rowCount = Menu_BattleAbilities_abilityUnlockRemainingChainAbilityCount - 1;
            }

            i = Input_settings.cursorMemory;

            if (Menu_BattleAbilities_abilityUnlockMenuNotEntered & 1) {
                --Menu_BattleAbilities_abilityUnlockMenuNotEntered;
                Input_settings.cursorMemory = 1;
            }

            Menu_Framework_CreateMenuRows(rowCount, menuRowInfo(11, 1, 1),
                chainAbilityMenuStrings, chainAbilityRowTypes);

            Input_settings.cursorMemory = i;
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_UnlockChainAbility;
        }
        break;

    case Menu_BattleAbilities_AbilityUnlockState_DefenseOnly:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {

            for (i = 0; i < Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount; ++i) {
                defenseAbilityMenuStrings[i * 2] =
                    Battle_actionDefinitions[Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities[i]].name;
                defenseAbilityMenuStrings[i * 2 + 1] =
                    Menu_BattleAbilities_UnlockDescription(Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities[i]);
                defenseAbilityRowTypes[i] = 0;
            }

            rowCount = 4;

            if (Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount == 1) {
                rowCount = 1;
            } else if (Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount < 5) {
                rowCount = Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilityCount - 1;
            }

            i = Input_settings.cursorMemory;

            if (Menu_BattleAbilities_abilityUnlockMenuNotEntered & 2) {
                Menu_BattleAbilities_abilityUnlockMenuNotEntered -= 2;
                Input_settings.cursorMemory = 1;
            }

            Menu_Framework_CreateMenuRows(rowCount, menuRowInfo(12, 1, 1),
                defenseAbilityMenuStrings, defenseAbilityRowTypes);

            Input_settings.cursorMemory = i;
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_UnlockDefenseAbility;
        }
        break;

    case Menu_BattleAbilities_AbilityUnlockState_UnlockChainAbility:
        row = Menu_Framework_GetSelectedRow();
        action = row + 1;

        if (action != 0) {

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            if (action < 0) {
                Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_Reinit;
                break;
            }

            Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(3));

            action = Menu_BattleAbilities_abilityUnlockRemainingChainAbilities[row];
            Battle_stringContext.strings[1] = Battle_actionDefinitions[action].name;

            Menu_Framework_SetInformationMessage((char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                    [Text_Menu2_battleAbilitiesMenu_Offset_battleAbilityUnlock]);

            Battle_actionDefinitions[action].unlocked = 1;
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_ShowMessage;
        }
        break;

    case Menu_BattleAbilities_AbilityUnlockState_UnlockDefenseAbility:
        row = Menu_Framework_GetSelectedRow();
        action = row + 1;

        if (action != 0) {

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            if (action < 0) {
                Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_Reinit;
                break;
            }

            Menu_InitInformationTextBox((enum Menu_InformationBoxHeader)(3));

            action = Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities[row];
            Battle_stringContext.strings[1] = Battle_actionDefinitions[action].name;

            Menu_Framework_SetInformationMessage((char*)&Menu_BattleAbilities_battleAbilityMenuStrings
                    [Text_Menu2_battleAbilitiesMenu_Offset_battleAbilityUnlock]);

            Battle_actionDefinitions[action].unlocked = 1;
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_ShowMessage;
        }
        break;

    case Menu_BattleAbilities_AbilityUnlockState_ShowMessage:
        if (Menu_textBoxes[7].flags.done) {
            if ((Menu_BattleAbilities_abilityUnlockTimeout == 0) || (Input_buttonsPressed.ports[0].lowByte != 0)) {
                return 1;
            }
            --Menu_BattleAbilities_abilityUnlockTimeout;
        }
        break;

    case Menu_BattleAbilities_AbilityUnlockState_Reinit:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_BattleAbilities_abilityUnlockState = Menu_BattleAbilities_AbilityUnlockState_Init;
        }
        break;
    }

    return 0;
}

// decomp: MENU2.PRG vs_menu2_actionUnlock (src/MENU/MENU2.PRG/143C.c:422)
int Menu_BattleAbilities_UpdateUnlockNotifications(char* state)
{
    enum Menu_BattleAbilities_UnlockNotificationState { Menu_BattleAbilities_UnlockNotificationState_Init, Menu_BattleAbilities_UnlockNotificationState_BreakArtInit, Menu_BattleAbilities_UnlockNotificationState_BreakArtUnlock, Menu_BattleAbilities_UnlockNotificationState_AbilityInit, Menu_BattleAbilities_UnlockNotificationState_AbilityUnlock, Menu_BattleAbilities_UnlockNotificationState_Exit };

    switch (*state) {
    case Menu_BattleAbilities_UnlockNotificationState_Init:
        Menu_Framework_InitializeInformationBox();
        // Fallthrough
    case Menu_BattleAbilities_UnlockNotificationState_BreakArtInit:
        *state = Menu_BattleAbilities_UnlockNotificationState_AbilityInit;

        if ((Menu_Framework_unlockedBreakArt != 0) && (Menu_BattleAbilities_UpdateBreakArtUnlock(1) == 0)) {
            *state = Menu_BattleAbilities_UnlockNotificationState_BreakArtUnlock;
        }

        break;

    case Menu_BattleAbilities_UnlockNotificationState_BreakArtUnlock:
        if (Menu_BattleAbilities_UpdateBreakArtUnlock(0) != 0) {
            *state = Menu_BattleAbilities_UnlockNotificationState_BreakArtInit;
        }
        break;

    case Menu_BattleAbilities_UnlockNotificationState_AbilityInit:
        *state = Menu_BattleAbilities_UnlockNotificationState_Exit;

        if ((Menu_Framework_unlockedBattleAbility != 0) && (Menu_BattleAbilities_UpdateAbilityUnlock(1) == 0)) {
            *state = Menu_BattleAbilities_UnlockNotificationState_AbilityUnlock;
        }

        break;

    case Menu_BattleAbilities_UnlockNotificationState_AbilityUnlock:
        if (Menu_BattleAbilities_UpdateAbilityUnlock(0) != 0) {
            *state = Menu_BattleAbilities_UnlockNotificationState_AbilityInit;
        }

        break;

    case Menu_BattleAbilities_UnlockNotificationState_Exit:
        if (Menu_DismissTextBox(7) == 0) {
            *state = Menu_BattleAbilities_UnlockNotificationState_Init;
            return 1;
        }
        break;
    }

    return 0;
}

// decomp: MENU2.PRG _setRiskCost (src/MENU/MENU2.PRG/64.c:15)
void Menu_BattleAbilities_SetRiskCost(int id)
{

    int i;
    int cost;

    Menu_Framework_SetActionCostText(1, "RISK", 8, 0);

    cost = Battle_actionDefinitions[id].cost;
    Menu_BattleAbilities_setRiskCostStringBuffer[15] = 0;

    i = 15;

    do {
        --i;
        cost = Core_Math_SplitDecimalDigit(cost);
        Menu_BattleAbilities_setRiskCostStringBuffer[i] = (cost & 0xF) + '0';
        cost >>= 4;
    } while (cost != 0);

    --i;
    Menu_BattleAbilities_setRiskCostStringBuffer[i] = '#';

    Menu_Framework_SetActionCostText(0, &Menu_BattleAbilities_setRiskCostStringBuffer[i], 72, 0);
}

// decomp: MENU2.PRG _initAbilityMenu (src/MENU/MENU2.PRG/64.c:64)
int Menu_BattleAbilities_CreateAbilityRows(int abilityCount, int abilityType, u_short** menuText)
{
    int abilitiesOverflowCount;
    u_short* text;
    int i = 0;
    int j = 0;

    if (Menu_BattleAbilities_abilityText != NULL) {
        return 0;
    }

    Menu_BattleAbilities_selectedAbilityType = abilityType;

    if (Input_settings.cursorMemory != 0) {
        i = Menu_Framework_menuMemory.cursorMemories[abilityType * 2];
        j = Menu_Framework_menuMemory.cursorMemories[abilityType * 2 + 1];
    }

    abilitiesOverflowCount = abilityCount - 8;

    if (abilitiesOverflowCount < 0) {

        i += j;
        j = 0;

        if (i >= abilityCount) {
            i = 0;
        }
    } else {
        if (abilitiesOverflowCount < j) {
            if ((i + j) >= abilityCount) {
                i = 0;
                j = 0;
            } else {
                i += j - abilitiesOverflowCount;
                j = abilitiesOverflowCount;
            }
        }

        if ((j > 0) && (i == 0)) {
            i = 1;
            --j;
        }

        if (j < abilitiesOverflowCount) {
            if (i == 7) {
                i = 6;
                ++j;
            }
        }
    }

    Menu_BattleAbilities_highlightedAbility = i;
    Menu_BattleAbilities_firstAbility = j;

    if (abilityType == Menu_BattleAbilities_AbilityType_Chain) {
        Menu_BattleAbilities_SetRiskCost(Menu_BattleAbilities_unlockedChainAbilities[i + j]);
    } else {
        Menu_BattleAbilities_SetRiskCost(Menu_BattleAbilities_unlockedDefenseAbilities[i + j]);
    }

    Menu_BattleAbilities_abilityCount = abilityCount;
    Menu_BattleAbilities_abilityMenuState = 0;

    Menu_BattleAbilities_abilityText = (u_short *)(Core_Memory_AllocateFromHighEnd(abilityCount * 128));

    for (i = 0; i < abilityCount; ++i) {

        text = menuText[i * 2];

        if (text != NULL) {

            Menu_BattleAbilities_abilityText[i * 64] = Core_TextCode_Spacing | 16 << 8;

            for (j = 0; j < 14; ++j) {
                (i * 64 + j + Menu_BattleAbilities_abilityText)[1] = text[j];
            }

            Menu_BattleAbilities_abilityText[i * 64 + 15] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            Menu_BattleAbilities_abilityText[i * 64] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }

        text = menuText[i * 2 + 1];

        if (text != NULL) {
            Menu_BattleAbilities_abilityText[i * 64 + 16] = Core_TextCode_ChunkSize;
            for (j = 0; j < 46; ++j) {
                Menu_BattleAbilities_abilityText[i * 64 + j + 17] = text[j];
            }
            Menu_BattleAbilities_abilityText[i * 64 + 63] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        } else {
            Menu_BattleAbilities_abilityText[i * 64 + 16] = Core_TextCode_Terminator << 8 | Core_TextCode_Terminator;
        }
    }

    return 1;
}

// decomp: MENU2.PRG _mapAbility (src/MENU/MENU2.PRG/64.c:166)
void Menu_BattleAbilities_ToggleButtonAssignment(int abilityType, int button, int abilityId)
{
    int i;

    if (abilityType == Menu_BattleAbilities_AbilityType_Chain) {

        abilityId = Menu_BattleAbilities_unlockedChainAbilities[abilityId];

        if (Input_settings.mappedChainAbilities[button] == abilityId) {
            Menu_PlayLeaveSound();
            Input_settings.mappedChainAbilities[button] = 0;
            return;
        }

        Audio_PlaySfxCentered(0x7E, VS_SFX_SET);

        for (i = 0; i < 3; ++i) {
            if (Input_settings.mappedChainAbilities[i] == abilityId) {
                Input_settings.mappedChainAbilities[i] = 0;
            }
        }

        Input_settings.mappedChainAbilities[button] = abilityId;

    } else {

        abilityId = Menu_BattleAbilities_unlockedDefenseAbilities[abilityId];

        if (Input_settings.mappedDefenseAbilities[button] == abilityId) {
            Menu_PlayLeaveSound();
            Input_settings.mappedDefenseAbilities[button] = 0;
            return;
        }

        Audio_PlaySfxCentered(0x7E, VS_SFX_SET);

        for (i = 0; i < 3; ++i) {
            if (Input_settings.mappedDefenseAbilities[i] == abilityId) {
                Input_settings.mappedDefenseAbilities[i] = 0;
            }
        }

        Input_settings.mappedDefenseAbilities[button] = abilityId;
    }
}

// decomp: MENU2.PRG _abilityMenu (src/MENU/MENU2.PRG/64.c:217)
int Menu_BattleAbilities_UpdateAssignments(void)
{
    enum Menu_BattleAbilities_AssignmentState { Menu_BattleAbilities_AssignmentState_Init, Menu_BattleAbilities_AssignmentState_WaitReady = 16, Menu_BattleAbilities_AssignmentState_AbilityMenu };


    int selectedAbility;
    int i;
    int previousFirstAbility;
    int ability;
    Menu_Framework_MenuRow* menuItem;
    u_long* scratch = (u_long *)(((u_char*)(Render_frameScratch.overlayOrderingTable)) + 8);

    if (Menu_BattleAbilities_abilityMenuState < 10) {

        menuItem = Menu_Framework_SetMenuItem(Menu_BattleAbilities_abilityMenuState + 20, 320,
            (Menu_BattleAbilities_abilityMenuState * 16) + 50, 0x7E, 0,
            (char*)&Menu_BattleAbilities_abilityText[(Menu_BattleAbilities_firstAbility + Menu_BattleAbilities_abilityMenuState) * 64]);
        menuItem->state = 2;
        menuItem->targetPosition0 = 194;

        if ((Menu_BattleAbilities_abilityMenuState == Menu_BattleAbilities_AssignmentState_Init) && (Menu_BattleAbilities_firstAbility != 0)) {
            menuItem->isScrollable = 1;
        }

        if (++Menu_BattleAbilities_abilityMenuState == Menu_BattleAbilities_abilityCount) {
            Menu_BattleAbilities_abilityMenuState = Menu_BattleAbilities_AssignmentState_WaitReady;
        }

        if (Menu_BattleAbilities_abilityMenuState == 8) {
            if ((Menu_BattleAbilities_firstAbility + 8u) < Menu_BattleAbilities_abilityCount) {
                menuItem->isScrollable = 2;
            }

            Menu_BattleAbilities_abilityMenuState = Menu_BattleAbilities_AssignmentState_WaitReady;
        }

        return 0;
    }

    Menu_Framework_MenuItem(Menu_BattleAbilities_highlightedAbility + 20);
    Menu_Framework_SetInformationMessage(
        (char*)(Menu_BattleAbilities_abilityText + (((Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility) << 6) + 16)));

    switch (Menu_BattleAbilities_abilityMenuState) {
    case Menu_BattleAbilities_AssignmentState_WaitReady:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_BattleAbilities_abilityMenuState = Menu_BattleAbilities_AssignmentState_AbilityMenu;
        }
        break;

    case Menu_BattleAbilities_AssignmentState_AbilityMenu:
        Menu_Framework_displayActionCost = 0;
        selectedAbility = Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility;
        previousFirstAbility = Menu_BattleAbilities_firstAbility;

        if (Menu_BattleAbilities_abilityCount < 9) {
            for (i = 0; i < Menu_BattleAbilities_abilityCount; ++i) {
                if (Menu_BattleAbilities_selectedAbilityType == Menu_BattleAbilities_AbilityType_Chain) {

                    ability = Menu_BattleAbilities_unlockedChainAbilities[i];

                    if (Input_settings.mappedChainAbilities[0] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[1] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[2] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                } else {

                    ability = Menu_BattleAbilities_unlockedDefenseAbilities[i];

                    if (Input_settings.mappedDefenseAbilities[0] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[1] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[2] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                }
            }
        } else {
            for (i = 0; i < 8; ++i) {
                if (Menu_BattleAbilities_selectedAbilityType == Menu_BattleAbilities_AbilityType_Chain) {

                    ability = Menu_BattleAbilities_unlockedChainAbilities[i + Menu_BattleAbilities_firstAbility];

                    if (Input_settings.mappedChainAbilities[0] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[1] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[2] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                } else {

                    ability = Menu_BattleAbilities_unlockedDefenseAbilities[i + Menu_BattleAbilities_firstAbility];

                    if (Input_settings.mappedDefenseAbilities[0] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[1] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[2] == ability) {
                        Menu_Framework_DrawButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                }
            }
        }

        if (Input_buttonsPressed.allButtons & PADRright) {
            Menu_BattleAbilities_ToggleButtonAssignment(Menu_BattleAbilities_selectedAbilityType, 0, selectedAbility);
        } else if (Input_buttonsPressed.allButtons & PADRup) {
            Menu_BattleAbilities_ToggleButtonAssignment(Menu_BattleAbilities_selectedAbilityType, 1, selectedAbility);
        } else if (Input_buttonsPressed.allButtons & PADRleft) {
            Menu_BattleAbilities_ToggleButtonAssignment(Menu_BattleAbilities_selectedAbilityType, 2, selectedAbility);
        }

        Menu_Framework_MenuItem(Menu_BattleAbilities_highlightedAbility + 20)->selected = 0;

        if (Input_buttonsPressed.allButtons & PADRdown) {

            Menu_PlayLeaveSound();
            Core_Memory_FreeFromHighEnd(Menu_BattleAbilities_abilityText);

            Menu_BattleAbilities_abilityText = NULL;
            Menu_Framework_menuMemory.cursorMemories[Menu_BattleAbilities_selectedAbilityType * 2] = Menu_BattleAbilities_highlightedAbility;
            Menu_Framework_menuMemory.cursorMemories[Menu_BattleAbilities_selectedAbilityType * 2 + 1] = Menu_BattleAbilities_firstAbility;

            return 1;
        }

        if (Input_buttonRepeat & PADLup) {
            if (Menu_BattleAbilities_abilityCount < 9) {
                if (Menu_BattleAbilities_highlightedAbility == 0) {
                    Menu_BattleAbilities_highlightedAbility = Menu_BattleAbilities_abilityCount - 1;
                } else {
                    --Menu_BattleAbilities_highlightedAbility;
                }
            } else {
                if (Menu_BattleAbilities_firstAbility == 0) {
                    if (Menu_BattleAbilities_highlightedAbility == 0) {
                        if (Input_buttonsPressed.allButtons & PADLup) {
                            Menu_BattleAbilities_highlightedAbility = 7;
                            Menu_BattleAbilities_firstAbility = Menu_BattleAbilities_abilityCount - 8;
                        }
                    } else {
                        --Menu_BattleAbilities_highlightedAbility;
                    }
                } else {
                    if (Menu_BattleAbilities_highlightedAbility == 1) {
                        --Menu_BattleAbilities_firstAbility;
                    } else {
                        --Menu_BattleAbilities_highlightedAbility;
                    }
                }
            }
        }

        if (Input_buttonRepeat & PADLdown) {
            if (Menu_BattleAbilities_abilityCount < 9) {
                if (Menu_BattleAbilities_highlightedAbility == (Menu_BattleAbilities_abilityCount - 1)) {
                    Menu_BattleAbilities_highlightedAbility = 0;
                } else {
                    ++Menu_BattleAbilities_highlightedAbility;
                }
            } else {
                if (Menu_BattleAbilities_firstAbility == (Menu_BattleAbilities_abilityCount - 8)) {
                    if (Menu_BattleAbilities_highlightedAbility == 7) {
                        if (Input_buttonsPressed.allButtons & PADLdown) {
                            Menu_BattleAbilities_highlightedAbility = 0;
                            Menu_BattleAbilities_firstAbility = 0;
                        }
                    } else {
                        ++Menu_BattleAbilities_highlightedAbility;
                    }
                } else if (Menu_BattleAbilities_highlightedAbility == 6) {
                    ++Menu_BattleAbilities_firstAbility;
                } else {
                    ++Menu_BattleAbilities_highlightedAbility;
                }
            }
        }

        if (selectedAbility != (Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility)) {
            Menu_PlayChangeSound();
            if (Menu_BattleAbilities_firstAbility != previousFirstAbility) {
                char abilityBuf[Menu_BattleAbilities_abilityCount];

                for (i = 0; i < Menu_BattleAbilities_abilityCount; ++i) {
                    abilityBuf[i] = 0;
                }

                ability = Menu_BattleAbilities_abilityCount;
                if (Menu_BattleAbilities_abilityCount >= 9) {
                    ability = 8;
                }

                for (i = 0; i < ability; ++i) {
                    abilityBuf[i + previousFirstAbility] =
                        Menu_Framework_MenuItem(i + 20)->gradientState;
                }

                for (i = 0;;) {
                    menuItem = Menu_Framework_SetMenuItem(i + 20, 194, i * 16 + 50, 0x7E, 0,
                        (char*)(Menu_BattleAbilities_abilityText + ((Menu_BattleAbilities_firstAbility + i) << 6)));
                    menuItem->gradientState = abilityBuf[i + Menu_BattleAbilities_firstAbility];

                    if ((i == 0) && (Menu_BattleAbilities_firstAbility != 0)) {
                        menuItem->isScrollable = 1;
                    }

                    ++i;

                    if (i == Menu_BattleAbilities_abilityCount) {
                        break;
                    } else if (i == 8) {
                        if ((Menu_BattleAbilities_firstAbility + 8) < Menu_BattleAbilities_abilityCount) {
                            menuItem->isScrollable = 2;
                        }
                        break;
                    }
                }
            }
        }

        Menu_Framework_MenuItem(Menu_BattleAbilities_highlightedAbility + 20)->selected = 1;
        Menu_BattleAbilities_assignmentsCursorAnimStep = Menu_Framework_DrawMainMenuCursor(Menu_BattleAbilities_assignmentsCursorAnimStep, Menu_BattleAbilities_highlightedAbility + 2);

        if (Menu_BattleAbilities_selectedAbilityType == Menu_BattleAbilities_AbilityType_Chain) {
            Menu_BattleAbilities_SetRiskCost(Menu_BattleAbilities_unlockedChainAbilities[Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility]);
        } else {
            Menu_BattleAbilities_SetRiskCost(Menu_BattleAbilities_unlockedDefenseAbilities[Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility]);
        }

        break;
    }

    return 0;
}

// decomp: MENU2.PRG _chainAbilityMenu (src/MENU/MENU2.PRG/64.c:492)
int Menu_BattleAbilities_UpdateChainAbilityMenu(int initialize)
{
    enum Menu_BattleAbilities_AbilityListState { Menu_BattleAbilities_AbilityListState_Init, Menu_BattleAbilities_AbilityListState_AbilityMenu };

    static char Menu_BattleAbilities_chainAbilityMenuLayoutPadding[4] __attribute__((unused)) = { 0 };

    u_short* menuStrings[28];
    int i;
    int count;

    if (initialize != 0) {
        Menu_Framework_FlyOutRowsAndHoistSelection(0, 1);

        Menu_BattleAbilities_chainAbilityMenuState = Menu_BattleAbilities_AbilityListState_Init;
        return 0;
    }

    switch (Menu_BattleAbilities_chainAbilityMenuState) {
    case Menu_BattleAbilities_AbilityListState_Init:
        if ((Menu_Framework_shortcutInvoked == 0) && (Menu_Framework_AreMenuRowsIdle() == 0)) {
            break;
        }

        count = 0;

        for (i = 0; i < 14; ++i) {

            int action = Battle_chainAbilityActionIds[i];

            if (Battle_actionDefinitions[action].unlocked) {
                menuStrings[count * 2] = (u_short*)Battle_actionDefinitions[action].name;
                menuStrings[count * 2 + 1] = &Menu_BattleAbilities_battleAbilityStrings[Menu_BattleAbilities_battleAbilityStrings
                        [Text_Menu2_battleAbilities_Index_chainAbilityEffect0 + i]];
                Menu_BattleAbilities_unlockedChainAbilities[count] = action;
                ++count;
            }
        }

        Menu_BattleAbilities_CreateAbilityRows(count, Menu_BattleAbilities_AbilityType_Chain, menuStrings);

        Menu_BattleAbilities_chainAbilityMenuState = Menu_BattleAbilities_AbilityListState_AbilityMenu;
        break;

    case Menu_BattleAbilities_AbilityListState_AbilityMenu:
        if (Menu_BattleAbilities_UpdateAssignments() != 0) {
            Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(2));
            return 1;
        }
        break;
    }

    return 0;
}

// decomp: MENU2.PRG _defenseAbilityMenu (src/MENU/MENU2.PRG/64.c:553)
int Menu_BattleAbilities_UpdateDefenseAbilityMenu(int reset_menu)
{
    enum Menu_BattleAbilities_AbilityListState { Menu_BattleAbilities_AbilityListState_Init, Menu_BattleAbilities_AbilityListState_AbilityMenu };

    static char Menu_BattleAbilities_defenseAbilityMenuLayoutPadding[4] __attribute__((unused)) = { 0 };

    u_short* menuStrings[28];
    int i;
    int row;

    if (reset_menu != 0) {
        Menu_Framework_FlyOutRowsAndHoistSelection(1, 1);

        Menu_BattleAbilities_defenseAbilityMenuState = Menu_BattleAbilities_AbilityListState_Init;
        return 0;
    }

    switch (Menu_BattleAbilities_defenseAbilityMenuState) {
    case Menu_BattleAbilities_AbilityListState_Init:
        if ((Menu_Framework_shortcutInvoked == 0) && (Menu_Framework_AreMenuRowsIdle() == 0)) {
            break;
        }

        row = 0;

        for (i = 0; i < 14; ++i) {
            int action = Battle_defenseAbilityActionIds[i];
            if (Battle_actionDefinitions[action].unlocked) {
                menuStrings[row * 2] = (u_short*)Battle_actionDefinitions[action].name;
                menuStrings[row * 2 + 1] = &Menu_BattleAbilities_battleAbilityStrings[Menu_BattleAbilities_battleAbilityStrings
                        [Text_Menu2_battleAbilities_Index_defenseAbilityEffect0 + i]];
                Menu_BattleAbilities_unlockedDefenseAbilities[row] = action;
                ++row;
            }
        }

        Menu_BattleAbilities_CreateAbilityRows(row, Menu_BattleAbilities_AbilityType_Defense, menuStrings);

        Menu_BattleAbilities_defenseAbilityMenuState = Menu_BattleAbilities_AbilityListState_AbilityMenu;
        break;

    case Menu_BattleAbilities_AbilityListState_AbilityMenu:
        if (Menu_BattleAbilities_UpdateAssignments() != 0) {
            Menu_Framework_ClearMenuExcept((enum Menu_Framework_MenuItemId)(2));
            return 1;
        }

        break;
    }

    return 0;
}

// decomp: MENU2.PRG _setNextAbilityUnlockPointsRemaining (src/MENU/MENU2.PRG/64.c:612)
void Menu_BattleAbilities_DrawNextUnlockPoints(int row)
{
    char pointsBuf[16];
    int pos;
    char* pointsStr;

    int points = Audio_Seq_unusedArtsStatus.kills.totalKills;
    int i = Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked;

    if (i == 22) {
        return;
    }

    points = Battle_battleAbilityPointRequirements[i] - points;
    if (points < 0) {
        points = 0;
    }

    pos = (row + 206) | (66 << 16);

    pointsBuf[14] = 'T';
    pointsBuf[15] = 0;
    pointsBuf[13] = 'P';

    i = 12;
    do {
        points = Core_Math_SplitDecimalDigit(points);
        *(pointsBuf + i) = (points & 0xF) + 48;
        points >>= 4;
        --i;
    } while (points != 0);

    pointsStr = pointsBuf + i;
    *pointsStr = '#';

    Render_DrawCompactText("NEXT", pos, 0);
    Render_DrawCompactText(pointsStr, pos + 0x60, 0);
}

// decomp: MENU2.PRG vs_menu2_exec (src/MENU/MENU2.PRG/64.c:656)
int Menu_BattleAbilities_Update(u_char* state)
{
    enum Menu_BattleAbilities_BattleAbilitiesMenuState {
        Menu_BattleAbilities_BattleAbilitiesMenuState_None,
        Menu_BattleAbilities_BattleAbilitiesMenuState_Init = 3,
        Menu_BattleAbilities_BattleAbilitiesMenuState_SelectTypeInit,
        Menu_BattleAbilities_BattleAbilitiesMenuState_SelectType,
        Menu_BattleAbilities_BattleAbilitiesMenuState_Chain,
        Menu_BattleAbilities_BattleAbilitiesMenuState_Defense,
        Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToMainMenu,
        Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle
    };

    static char Menu_BattleAbilities_updateMenuAnimating;
    static u_char Menu_BattleAbilities_updatePointsAnimStep;
    static char Menu_BattleAbilities_updateLayoutPadding[14] __attribute__((unused));

    char* menuStrings[4];
    int rowTypes[2];
    int temp_s0;
    int val;

    switch (*state) {
    case Menu_BattleAbilities_BattleAbilitiesMenuState_Init:
        Menu_BattleAbilities_updateMenuAnimating = 0;
        Menu_BattleAbilities_updatePointsAnimStep = 10;

        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }

        Menu_Framework_InitializeInformationBox();

        if (Menu_Framework_shortcutInvoked != 0) {

            int type = Menu_Framework_shortcutInvoked - 7;

            Menu_Framework_SetMenuItem(Menu_Framework_shortcutInvoked + 3, 0x140, 0x22, 0x8C, 8,
                (char*)&Menu_BattleAbilities_battleAbilityStrings[Menu_BattleAbilities_battleAbilityStrings[type * 3]])
                ->selected = 1;

            if (type == 0) {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_Chain;
                Menu_BattleAbilities_UpdateChainAbilityMenu(1);
            } else {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_Defense;
                Menu_BattleAbilities_UpdateDefenseAbilityMenu(1);
            }

            break;
        }
        // Fallthrough

    case Menu_BattleAbilities_BattleAbilitiesMenuState_SelectTypeInit:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {

            menuStrings[0] =
                (char*)&Menu_BattleAbilities_battleAbilityStrings[Text_Menu2_battleAbilities_Offset_chainAbilities];
            menuStrings[1] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                [Text_Menu2_battleAbilities_Offset_chainAbilitiesDesc];
            rowTypes[0] = 0;
            rowTypes[1] = 0;

            if (Battle_HasUnlockedBattleAbility(0) == 0) {
                // BUG: Should be rowTypes, but flow doesn't seem to be used anyway
                menuStrings[0] = (char*)((intptr_t)menuStrings[0] | 1);
                menuStrings[1] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                    [Text_Menu2_battleAbilities_Offset_noChainAbilities];
            }

            menuStrings[2] =
                (char*)&Menu_BattleAbilities_battleAbilityStrings[Text_Menu2_battleAbilities_Offset_defenseAbilities];
            menuStrings[3] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                [Text_Menu2_battleAbilities_Offset_defenseAbilitiesDesc];

            if (Battle_HasUnlockedBattleAbility(1) == 0) {
                rowTypes[1] |= 1;
                menuStrings[3] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                    [Text_Menu2_battleAbilities_Offset_noDefenseAbilities];
            }

            val = Input_settings.cursorMemory;

            if (*state != 3) {
                Input_settings.cursorMemory = 1;
            }

            Menu_Framework_CreateMenuRows(2, menuRowInfo(7, 1, 1), menuStrings, rowTypes);

            Input_settings.cursorMemory = val;
            *state = Menu_BattleAbilities_BattleAbilitiesMenuState_SelectType;
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_SelectType:
        Menu_BattleAbilities_updateMenuAnimating = 1;
        val = Menu_Framework_GetSelectedRow() + 1;

        if (val != 0) {

            Menu_BattleAbilities_updateMenuAnimating = 0;

            if (val == 1) {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_Chain;
                Menu_BattleAbilities_UpdateChainAbilityMenu(1);
                break;
            }

            if (val == 2) {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_Defense;
                Menu_BattleAbilities_UpdateDefenseAbilityMenu(1);
                break;
            }

            Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

            if (val != -2) {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToMainMenu;
            } else {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle;
            }
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_Chain:
        if (Menu_BattleAbilities_UpdateChainAbilityMenu(0) != 0) {
            if (Menu_Framework_shortcutInvoked != 0) {

                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle;
            } else {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_SelectTypeInit;
            }
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_Defense:
        if (Menu_BattleAbilities_UpdateDefenseAbilityMenu(0) != 0) {
            if (Menu_Framework_shortcutInvoked != 0) {

                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);

                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle;
            } else {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_SelectTypeInit;
            }
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToMainMenu:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            *state = Menu_BattleAbilities_BattleAbilitiesMenuState_None;
            return 1;
        }

        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);

        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_menuState.currentMenu = 3;
            *state = Menu_BattleAbilities_BattleAbilitiesMenuState_None;
            return 1;
        }

        break;
    }

    if (Menu_BattleAbilities_updateMenuAnimating != 0) {
        Menu_BattleAbilities_DrawNextUnlockPoints(Menu_Framework_rowAnimationSteps[Menu_BattleAbilities_updatePointsAnimStep]);

        if (Menu_BattleAbilities_updatePointsAnimStep != 0) {
            --Menu_BattleAbilities_updatePointsAnimStep;
        }
    } else {
        if (Menu_BattleAbilities_updatePointsAnimStep < 10) {

            ++Menu_BattleAbilities_updatePointsAnimStep;

            Menu_BattleAbilities_DrawNextUnlockPoints(Menu_BattleAbilities_updatePointsAnimStep << 5);
        }
    }

    return 0;
}

// MENU2.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_BattleAbilities_ResetMenu2Statics(void)
{
    {
        static const char initial = 0;
        memcpy((void*)&Menu_BattleAbilities_abilityCount, &initial, sizeof Menu_BattleAbilities_abilityCount);
    }
    {
        static const u_int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_abilityMenuState, &initial, sizeof Menu_BattleAbilities_abilityMenuState);
    }
    {
        static const u_short* initial = NULL;
        memcpy((void*)&Menu_BattleAbilities_abilityText, &initial, sizeof Menu_BattleAbilities_abilityText);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_BattleAbilities_firstAbility, &initial, sizeof Menu_BattleAbilities_firstAbility);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_highlightedAbility, &initial, sizeof Menu_BattleAbilities_highlightedAbility);
    }
    {
        static const char initial = 0;
        memcpy((void*)&Menu_BattleAbilities_selectedAbilityType, &initial, sizeof Menu_BattleAbilities_selectedAbilityType);
    }
    {
        static const char initial[] = { 0x18, 0x19, 0x22, 0x25, 0x23, 0x1E, 0x1A,
    0x1B, 0x20, 0x1D, 0x1F };
        memcpy((void*)&Menu_BattleAbilities_unlockableChainAbilities, &initial, sizeof Menu_BattleAbilities_unlockableChainAbilities);
    }
    {
        static const char initial[] = { 0x2F, 0x30, 0x31, 0x32, 0x29, 0x2A, 0x33,
    0x34, 0x2C, 0x2D, 0x35 };
        memcpy((void*)&Menu_BattleAbilities_unlockableDefenseAbilities, &initial, sizeof Menu_BattleAbilities_unlockableDefenseAbilities);
    }
    {
        static const u_char initial[16] = {};
        memcpy((void*)&Menu_BattleAbilities_abilityUnlockRemainingChainAbilities, &initial, sizeof Menu_BattleAbilities_abilityUnlockRemainingChainAbilities);
    }
    {
        static const u_char initial[11] = {};
        memcpy((void*)&Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities, &initial, sizeof Menu_BattleAbilities_abilityUnlockRemainingDefenseAbilities);
    }
    {
        static const char initial[16] = {};
        memcpy((void*)&Menu_BattleAbilities_setRiskCostStringBuffer, &initial, sizeof Menu_BattleAbilities_setRiskCostStringBuffer);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_assignmentsCursorAnimStep, &initial, sizeof Menu_BattleAbilities_assignmentsCursorAnimStep);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_chainAbilityMenuState, &initial, sizeof Menu_BattleAbilities_chainAbilityMenuState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_BattleAbilities_defenseAbilityMenuState, &initial, sizeof Menu_BattleAbilities_defenseAbilityMenuState);
    }
}

}  // extern "C"
