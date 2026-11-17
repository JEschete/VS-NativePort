// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Audio/SfxLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionTable.h"
#include "Content/StringTables.h"
#include "Input/ControllerLiveState.h"
#include "Menu/BattleAbilities.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"

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

// decomp: local static of _chainAbilityMenu, at file scope so the module reset reaches it
static int Menu_BattleAbilities_chainAbilityMenuState = 0;

// decomp: local static of _defenseAbilityMenu, at file scope so the module reset reaches it
static int Menu_BattleAbilities_defenseAbilityMenuState = 0;

// decomp: MENU2.PRG _mapAbility (src/MENU/MENU2.PRG/64.c:166)
void Menu_BattleAbilities_ToggleButtonAssignment(int abilityType, int button, int abilityId)
{
    int i;

    if (abilityType == Menu_BattleAbilities_AbilityType_Chain) {

        abilityId = Menu_BattleAbilities_unlockedChainAbilities[abilityId];

        if (Input_settings.mappedChainAbilities[button] == abilityId) {
            VS::Menu::PlayLeaveSound();
            Input_settings.mappedChainAbilities[button] = 0;
            return;
        }

        VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_SET);

        for (i = 0; i < 3; ++i) {
            if (Input_settings.mappedChainAbilities[i] == abilityId) {
                Input_settings.mappedChainAbilities[i] = 0;
            }
        }

        Input_settings.mappedChainAbilities[button] = abilityId;

    } else {

        abilityId = Menu_BattleAbilities_unlockedDefenseAbilities[abilityId];

        if (Input_settings.mappedDefenseAbilities[button] == abilityId) {
            VS::Menu::PlayLeaveSound();
            Input_settings.mappedDefenseAbilities[button] = 0;
            return;
        }

        VS::Audio::LivePlaySfxCentered(0x7E, VS_SFX_SET);

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

        menuItem = VS::Menu::Framework::SetLiveMenuItem(Menu_BattleAbilities_abilityMenuState + 20, 320,
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

    VS::Menu::Framework::LiveMenuItem(Menu_BattleAbilities_highlightedAbility + 20);
    VS::Menu::Framework::SetLiveInformationMessage(
        (char*)(Menu_BattleAbilities_abilityText + (((Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility) << 6) + 16)));

    switch (Menu_BattleAbilities_abilityMenuState) {
    case Menu_BattleAbilities_AssignmentState_WaitReady:
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
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
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[1] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[2] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                } else {

                    ability = Menu_BattleAbilities_unlockedDefenseAbilities[i];

                    if (Input_settings.mappedDefenseAbilities[0] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[1] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[2] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                }
            }
        } else {
            for (i = 0; i < 8; ++i) {
                if (Menu_BattleAbilities_selectedAbilityType == Menu_BattleAbilities_AbilityType_Chain) {

                    ability = Menu_BattleAbilities_unlockedChainAbilities[i + Menu_BattleAbilities_firstAbility];

                    if (Input_settings.mappedChainAbilities[0] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[1] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedChainAbilities[2] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Square, 199, i * 16 + 48, scratch);
                    }
                } else {

                    ability = Menu_BattleAbilities_unlockedDefenseAbilities[i + Menu_BattleAbilities_firstAbility];

                    if (Input_settings.mappedDefenseAbilities[0] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Circle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[1] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
                            Menu_Framework_ButtonId_Triangle, 199, i * 16 + 48, scratch);
                    }

                    if (Input_settings.mappedDefenseAbilities[2] == ability) {
                        VS::Menu::Framework::DrawLiveButtonIcon(
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

        VS::Menu::Framework::LiveMenuItem(Menu_BattleAbilities_highlightedAbility + 20)->selected = 0;

        if (Input_buttonsPressed.allButtons & PADRdown) {

            VS::Menu::PlayLeaveSound();
            VS::Core::Memory::FreeLiveFromHighEnd(Menu_BattleAbilities_abilityText);

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
            VS::Menu::PlayChangeSound();
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
                        VS::Menu::Framework::LiveMenuItem(i + 20)->gradientState;
                }

                for (i = 0;;) {
                    menuItem = VS::Menu::Framework::SetLiveMenuItem(i + 20, 194, i * 16 + 50, 0x7E, 0,
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

        VS::Menu::Framework::LiveMenuItem(Menu_BattleAbilities_highlightedAbility + 20)->selected = 1;
        Menu_BattleAbilities_assignmentsCursorAnimStep = VS::Menu::Framework::DrawLiveMainMenuCursor(Menu_BattleAbilities_assignmentsCursorAnimStep, Menu_BattleAbilities_highlightedAbility + 2);

        if (Menu_BattleAbilities_selectedAbilityType == Menu_BattleAbilities_AbilityType_Chain) {
            VS::Menu::BattleAbilities::LiveSetRiskCost(Menu_BattleAbilities_unlockedChainAbilities[Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility]);
        } else {
            VS::Menu::BattleAbilities::LiveSetRiskCost(Menu_BattleAbilities_unlockedDefenseAbilities[Menu_BattleAbilities_highlightedAbility + Menu_BattleAbilities_firstAbility]);
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
        VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(0, 1);

        Menu_BattleAbilities_chainAbilityMenuState = Menu_BattleAbilities_AbilityListState_Init;
        return 0;
    }

    switch (Menu_BattleAbilities_chainAbilityMenuState) {
    case Menu_BattleAbilities_AbilityListState_Init:
        if ((Menu_Framework_shortcutInvoked == 0) && (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0)) {
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

        VS::Menu::BattleAbilities::LiveCreateAbilityRowsEntry(count, Menu_BattleAbilities_AbilityType_Chain, menuStrings);

        Menu_BattleAbilities_chainAbilityMenuState = Menu_BattleAbilities_AbilityListState_AbilityMenu;
        break;

    case Menu_BattleAbilities_AbilityListState_AbilityMenu:
        if (Menu_BattleAbilities_UpdateAssignments() != 0) {
            VS::Menu::Framework::ClearLiveMenuExcept(2);
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
        VS::Menu::Framework::FlyOutLiveRowsAndHoistSelection(1, 1);

        Menu_BattleAbilities_defenseAbilityMenuState = Menu_BattleAbilities_AbilityListState_Init;
        return 0;
    }

    switch (Menu_BattleAbilities_defenseAbilityMenuState) {
    case Menu_BattleAbilities_AbilityListState_Init:
        if ((Menu_Framework_shortcutInvoked == 0) && (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0)) {
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

        VS::Menu::BattleAbilities::LiveCreateAbilityRowsEntry(row, Menu_BattleAbilities_AbilityType_Defense, menuStrings);

        Menu_BattleAbilities_defenseAbilityMenuState = Menu_BattleAbilities_AbilityListState_AbilityMenu;
        break;

    case Menu_BattleAbilities_AbilityListState_AbilityMenu:
        if (Menu_BattleAbilities_UpdateAssignments() != 0) {
            VS::Menu::Framework::ClearLiveMenuExcept(2);
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
        points = VS::Core::Math::SplitDecimalDigit(points);
        *(pointsBuf + i) = (points & 0xF) + 48;
        points >>= 4;
        --i;
    } while (points != 0);

    pointsStr = pointsBuf + i;
    *pointsStr = '#';

    VS::Render::DrawLiveCompactText("NEXT", pos, 0);
    VS::Render::DrawLiveCompactText(pointsStr, pos + 0x60, 0);
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

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() == 0) {
            break;
        }

        VS::Menu::Framework::InitializeLiveInformationBox();

        if (Menu_Framework_shortcutInvoked != 0) {

            int type = Menu_Framework_shortcutInvoked - 7;

            VS::Menu::Framework::SetLiveMenuItem(Menu_Framework_shortcutInvoked + 3, 0x140, 0x22, 0x8C, 8,
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
        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {

            menuStrings[0] =
                (char*)&Menu_BattleAbilities_battleAbilityStrings[Text_Menu2_battleAbilities_Offset_chainAbilities];
            menuStrings[1] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                [Text_Menu2_battleAbilities_Offset_chainAbilitiesDesc];
            rowTypes[0] = 0;
            rowTypes[1] = 0;

            if (VS::Battle::LiveHasUnlockedBattleAbility(0) == 0) {
                // BUG: Should be rowTypes, but flow doesn't seem to be used anyway
                menuStrings[0] = (char*)((intptr_t)menuStrings[0] | 1);
                menuStrings[1] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                    [Text_Menu2_battleAbilities_Offset_noChainAbilities];
            }

            menuStrings[2] =
                (char*)&Menu_BattleAbilities_battleAbilityStrings[Text_Menu2_battleAbilities_Offset_defenseAbilities];
            menuStrings[3] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                [Text_Menu2_battleAbilities_Offset_defenseAbilitiesDesc];

            if (VS::Battle::LiveHasUnlockedBattleAbility(1) == 0) {
                rowTypes[1] |= 1;
                menuStrings[3] = (char*)&Menu_BattleAbilities_battleAbilityStrings
                    [Text_Menu2_battleAbilities_Offset_noDefenseAbilities];
            }

            val = Input_settings.cursorMemory;

            if (*state != 3) {
                Input_settings.cursorMemory = 1;
            }

            VS::Menu::Framework::CreateLiveMenuRows(2, menuRowInfo(7, 1, 1), menuStrings, rowTypes);

            Input_settings.cursorMemory = val;
            *state = Menu_BattleAbilities_BattleAbilitiesMenuState_SelectType;
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_SelectType:
        Menu_BattleAbilities_updateMenuAnimating = 1;
        val = VS::Menu::Framework::LiveSelectedRow() + 1;

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

            VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);

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

                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);

                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle;
            } else {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_SelectTypeInit;
            }
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_Defense:
        if (Menu_BattleAbilities_UpdateDefenseAbilityMenu(0) != 0) {
            if (Menu_Framework_shortcutInvoked != 0) {

                VS::Menu::Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);

                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle;
            } else {
                *state = Menu_BattleAbilities_BattleAbilitiesMenuState_SelectTypeInit;
            }
        }
        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToMainMenu:
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
            *state = Menu_BattleAbilities_BattleAbilitiesMenuState_None;
            return 1;
        }

        break;

    case Menu_BattleAbilities_BattleAbilitiesMenuState_ExitToBattle:
        VS::Menu::Framework::DismissLiveInformationBox();
        VS::Menu::Framework::SetLiveCommandLabel(Menu_Framework_MenuCommand_None);

        if (VS::Menu::Framework::AreLiveMenuRowsIdle() != 0) {
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
