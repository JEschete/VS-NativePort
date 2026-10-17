// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Audio/Seq/Channel.h"
#include "Audio/SoundBanks.h"
#include "Battle/TextBoxes.h"
#include "Content/StringTables.h"
#include "DiscQueue.h"
#include "Input/Controller.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/Options.h"
#include "Menu/TextBox.h"
#include "State/Settings.h"
#include "State/StateFlags.h"

// decomp: _2 (_2@src/MENU/MENU8.PRG/21A0.c)
char Menu_Options_layoutPadding1[2];

// decomp: _3 (_3@src/MENU/MENU8.PRG/21A0.c)
char Menu_Options_layoutPadding2[8];

// decomp: _menuStrings (_menuStrings@src/MENU/MENU8.PRG/88.c)
u_short Menu_Options_menuStrings[] = {
0 /* disc text table: loaded in Step 5 */
};

// decomp: _simpleMapValues (_simpleMapValues@src/MENU/MENU8.PRG/88.c)
char Menu_Options_simpleMapValues[] = { 3, 1, 0 };

// decomp: local static of _abilityTimingOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_abilityTimingChoiceSelectedRow = 0;

// decomp: local static of _armorStatusOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_armorStatusChoiceSelectedRow = 0;

// decomp: local static of _cursorMemoryOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_cursorMemoryChoiceSelectedRow = 0;

// decomp: local static of _informationOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_informationChoiceSelectedRow = 0;

// decomp: local static of _puzzleModeOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_puzzleModeChoiceSelectedRow = 0;

// decomp: local static of _simpleMapOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_simpleMapChoiceSelectedRow = 0;

// decomp: local static of _soundOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_soundChoiceSelectedRow = 0;

// decomp: local static of _vibrationOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_vibrationChoiceSelectedRow = 0;

// decomp: local static of _weaponStatusOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_weaponStatusChoiceSelectedRow = 0;

// decomp: local static of _abilityTimingOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_abilityTimingChoiceState = 0;

// decomp: local static of _armorStatusOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_armorStatusChoiceState = 0;

// decomp: local static of _cursorMemoryOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_cursorMemoryChoiceState = 0;

// decomp: local static of _informationOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_informationChoiceState = 0;

// decomp: local static of _puzzleModeOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_puzzleModeChoiceState = 0;

// decomp: local static of _simpleMapOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_simpleMapChoiceState = 0;

// decomp: local static of _soundOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_soundChoiceState = 0;

// decomp: local static of _vibrationOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_vibrationChoiceState = 0;

// decomp: local static of _weaponStatusOptionMenu, at file scope so the module reset reaches it
static int Menu_Options_weaponStatusChoiceState = 0;

// decomp: MENU8.PRG _simpleMapOptionMenu (src/MENU/MENU8.PRG/88.c:20)
int Menu_Options_UpdateSimpleMapChoice(int init)
{

    char* menuStrings[6];
    int rowTypes[3];
    int i;

    if (init != 0) {
        Menu_Options_simpleMapChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_simpleMapChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 3; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_frame]];
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_frameDesc]];
                rowTypes[i] = 0;
            }
            for (i = 0; i < 3; ++i) {
                if (Input_settings.simpleMap == Menu_Options_simpleMapValues[i]) {
                    rowTypes[i] |= 4;
                }
            }
            Menu_Framework_CreateMenuRows(3, 0x246, menuStrings, rowTypes);
            Menu_Options_simpleMapChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_simpleMapChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_simpleMapChoiceSelectedRow != 0) {
            if (Menu_Options_simpleMapChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_simpleMapChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_simpleMapChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _abilityTimingOptionMenu (src/MENU/MENU8.PRG/88.c:80)
int Menu_Options_UpdateAbilityTimingChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_abilityTimingChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_abilityTimingChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_abilityTimingOn]];
                menuStrings[i * 2 + 1] = (char*)&Menu_Options_menuStrings
                    [Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_abilityTimingOnDesc]];
                rowTypes[i] = 0;
            }
            i = 1 - ((*(u_int*)&Input_settings >> 5) & 1);
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x247, menuStrings, rowTypes);
            Menu_Options_abilityTimingChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_abilityTimingChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_abilityTimingChoiceSelectedRow != 0) {
            if (Menu_Options_abilityTimingChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_abilityTimingChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_abilityTimingChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _weaponStatusOptionMenu (src/MENU/MENU8.PRG/88.c:138)
int Menu_Options_UpdateWeaponStatusChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_weaponStatusChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_weaponStatusChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_weaponStatusOn]];
                menuStrings[i * 2 + 1] = (char*)&Menu_Options_menuStrings
                    [Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_weaponStatusOnDesc]];
                rowTypes[i] = 0;
            }
            i = 1 - Input_settings.weaponStatChange;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x248, menuStrings, rowTypes);
            Menu_Options_weaponStatusChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_weaponStatusChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_weaponStatusChoiceSelectedRow != 0) {
            if (Menu_Options_weaponStatusChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_weaponStatusChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_weaponStatusChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _armorStatusOptionMenu (src/MENU/MENU8.PRG/88.c:196)
int Menu_Options_UpdateArmorStatusChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_armorStatusChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_armorStatusChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_armorStatusOn]];
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_armorStatusOnDesc]];
                rowTypes[i] = 0;
            }
            i = 1 - Input_settings.armorStatChange;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x249, menuStrings, rowTypes);
            Menu_Options_armorStatusChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_armorStatusChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_armorStatusChoiceSelectedRow != 0) {
            if (Menu_Options_armorStatusChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_armorStatusChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_armorStatusChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _cursorMemoryOptionMenu (src/MENU/MENU8.PRG/88.c:255)
int Menu_Options_UpdateCursorMemoryChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_cursorMemoryChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_cursorMemoryChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_cursorMemoryOn]];
                menuStrings[i * 2 + 1] = (char*)&Menu_Options_menuStrings
                    [Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_cursorMemoryOnDesc]];
                rowTypes[i] = 0;
            }
            i = 1 - Input_settings.cursorMemory;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x24A, menuStrings, rowTypes);
            Menu_Options_cursorMemoryChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_cursorMemoryChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_cursorMemoryChoiceSelectedRow != 0) {
            if (Menu_Options_cursorMemoryChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_cursorMemoryChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_cursorMemoryChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _informationOptionMenu (src/MENU/MENU8.PRG/88.c:313)
int Menu_Options_UpdateInformationChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_informationChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_informationChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_infoOn]];
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_infoOnDesc]];
                rowTypes[i] = 0;
            }
            i = 1 - Input_settings.information;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x24B, menuStrings, rowTypes);
            Menu_Options_informationChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_informationChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_informationChoiceSelectedRow != 0) {
            if (Menu_Options_informationChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_informationChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_informationChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _puzzleModeOptionMenu (src/MENU/MENU8.PRG/88.c:370)
int Menu_Options_UpdatePuzzleModeChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_puzzleModeChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_puzzleModeChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_puzzleModeOn]];
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_puzzleModeOnDesc]];
                rowTypes[i] = 0;
            }
            i = ((*(u_int*)&Input_settings) >> 3) & 1;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x24C, menuStrings, rowTypes);
            Menu_Options_puzzleModeChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_puzzleModeChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_puzzleModeChoiceSelectedRow != 0) {
            if (Menu_Options_puzzleModeChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_puzzleModeChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_puzzleModeChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _soundOptionMenu (src/MENU/MENU8.PRG/88.c:429)
int Menu_Options_UpdateSoundChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;

    if (init != 0) {
        Menu_Options_soundChoiceState = 0;
        return 0;
    }
    switch (Menu_Options_soundChoiceState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_stereo]];
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_stereoDesc]];
                rowTypes[i] = 0;
            }
            i = Input_settings.monoSound;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x24D, menuStrings, rowTypes);
            Menu_Options_soundChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_soundChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_soundChoiceSelectedRow != 0) {
            if (Menu_Options_soundChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_soundChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_soundChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG _vibrationOptionMenu (src/MENU/MENU8.PRG/88.c:486)
int Menu_Options_UpdateVibrationChoice(int init)
{

    char* menuStrings[4];
    int rowTypes[2];
    int i;
    int currentState;

    if (init != 0) {
        Menu_Options_vibrationChoiceState = 0;
        return 0;
    }
    currentState = Menu_Options_vibrationChoiceState;
    switch (currentState) {
    case 0:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            for (i = 0; i < 2; ++i) {
                menuStrings[i * 2] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + Text_Menu8_menu_Index_vibrationOn]];
                menuStrings[i * 2 + 1] =
                    (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2
                                                      + Text_Menu8_menu_Index_vibrationOnDesc]];
                rowTypes[i] = 0;
            }
            i = 1 - Input_settings.vibrationOn;
            rowTypes[i] |= 4;
            Menu_Framework_CreateMenuRows(2, 0x24E, menuStrings, rowTypes);
            Menu_Options_vibrationChoiceState = 1;
        }
        break;
    case 1:
        Menu_Options_vibrationChoiceSelectedRow = Menu_Framework_GetSelectedRow() + 1;
        if (Menu_Options_vibrationChoiceSelectedRow != 0) {
            if (Menu_Options_vibrationChoiceSelectedRow == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                Menu_Framework_DismissInformationBox();
                Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
            } else {
                if (Menu_Options_vibrationChoiceSelectedRow == currentState) {
                    Input_PlayVibrationPreset(0);
                }
                Menu_Framework_ClearMenuExcept(7);
            }
            Menu_Options_vibrationChoiceState = 2;
        }
        break;
    case 2:
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            return Menu_Options_vibrationChoiceSelectedRow;
        }
        break;
    }
    return 0;
}

// decomp: MENU8.PRG vs_menu8_exec (src/MENU/MENU8.PRG/88.c:551)
int Menu_Options_Update(u_char* state)
{
    enum Menu_Options_OptionsMenuState {
        Menu_Options_OptionsMenuState_None,
        Menu_Options_OptionsMenuState_WaitReady = 3,
        Menu_Options_OptionsMenuState_Init,
        Menu_Options_OptionsMenuState_HandleInput,
        Menu_Options_OptionsMenuState_SimpleMap,
        Menu_Options_OptionsMenuState_AbilityTiming,
        Menu_Options_OptionsMenuState_WeaponStatus,
        Menu_Options_OptionsMenuState_ArmorStatus,
        Menu_Options_OptionsMenuState_CursorMemory,
        Menu_Options_OptionsMenuState_Information,
        Menu_Options_OptionsMenuState_PuzzleMode,
        Menu_Options_OptionsMenuState_Sound,
        Menu_Options_OptionsMenuState_Vibration,
        Menu_Options_OptionsMenuState_ExitToMenu,
        Menu_Options_OptionsMenuState_ExitToBattle
    };

    char* menuStrings[18];
    int rowTypes[9];
    int selectedRow;
    int i;
    int var_v1;
    u_int temp_v1_2;

    switch (*state) {
    case Menu_Options_OptionsMenuState_WaitReady:
        if (Menu_Framework_AreMenuRowsIdle() == 0) {
            break;
        }
        Menu_Framework_InitializeInformationBox();
        // Fallthrough
    case Menu_Options_OptionsMenuState_Init:
        for (i = 0; i < 9; ++i) {
            menuStrings[i * 2] = (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2]];
            menuStrings[i * 2 + 1] = (char*)&Menu_Options_menuStrings[Menu_Options_menuStrings[i * 2 + 1]];
            rowTypes[i] = 0;
        }

        if (Audio_Seq_unusedStateFlags.puzzleModeDisabled != 0) {
            menuStrings[13] =
                (char*)&Menu_Options_menuStrings[Text_Menu8_menu_Offset_puzzleModeSettingDisabled];
            rowTypes[6] = 1;
        }
        i = Input_settings.cursorMemory;
        if (*state != Menu_Options_OptionsMenuState_WaitReady) {
            Input_settings.cursorMemory = 1;
        }
        Menu_Framework_CreateMenuRows(9, 0x145, menuStrings, rowTypes);
        Input_settings.cursorMemory = i;
        *state = 5;
        break;
    case Menu_Options_OptionsMenuState_HandleInput:
        selectedRow = Menu_Framework_GetSelectedRow();
        i = selectedRow + 1;
        if (i != 0) {
            if (i > 0) {
                Menu_Framework_FlyOutRowsAndHoistSelection(Menu_Framework_menuMemory.opaque85[5], 1);
                switch (selectedRow) {
                case 0:
                    *state = Menu_Options_OptionsMenuState_SimpleMap;
                    Menu_Options_UpdateSimpleMapChoice(1);
                    return 0;
                case 1:
                    *state = Menu_Options_OptionsMenuState_AbilityTiming;
                    Menu_Options_UpdateAbilityTimingChoice(1);
                    return 0;
                case 2:
                    *state = Menu_Options_OptionsMenuState_WeaponStatus;
                    Menu_Options_UpdateWeaponStatusChoice(1);
                    return 0;
                case 3:
                    *state = Menu_Options_OptionsMenuState_ArmorStatus;
                    Menu_Options_UpdateArmorStatusChoice(1);
                    return 0;
                case 4:
                    *state = Menu_Options_OptionsMenuState_CursorMemory;
                    Menu_Options_UpdateCursorMemoryChoice(1);
                    return 0;
                case 5:
                    *state = Menu_Options_OptionsMenuState_Information;
                    Menu_Options_UpdateInformationChoice(1);
                    return 0;
                case 6:
                    *state = Menu_Options_OptionsMenuState_PuzzleMode;
                    Menu_Options_UpdatePuzzleModeChoice(1);
                    return 0;
                case 7:
                    *state = Menu_Options_OptionsMenuState_Sound;
                    Menu_Options_UpdateSoundChoice(1);
                    return 0;
                case 8:
                    *state = Menu_Options_OptionsMenuState_Vibration;
                    Menu_Options_UpdateVibrationChoice(1);
                    return 0;
                }
            } else if (i == -2) {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                *state = Menu_Options_OptionsMenuState_ExitToBattle;
            } else {
                Menu_Framework_ClearMenuExcept(Menu_Framework_MenuItemId_None);
                *state = Menu_Options_OptionsMenuState_ExitToMenu;
            }
        } else {
            if (Audio_Seq_unusedStateFlags.puzzleModeDisabled != 0) {
                if (Menu_Framework_HighlightedRow() == 6) {
                    Menu_InitInformationTextBox(1);
                    Menu_textBoxes[7].state = 0xB;
                } else if (Input_settings.information != 0) {
                    Menu_InitInformationTextBox(2);
                    Menu_textBoxes[7].state = 0xB;
                } else {
                    Menu_textBoxes[7].state = 0;
                }
            }
            break;
        }
        break;
    case Menu_Options_OptionsMenuState_SimpleMap:
        i = Menu_Options_UpdateSimpleMapChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            Input_settings.simpleMap = Menu_Options_simpleMapValues[i - 1];
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_AbilityTiming:
        i = Menu_Options_UpdateAbilityTimingChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            var_v1 = *(int*)&Input_settings & ~0x20;
            var_v1 |= ((2 - i) & 1) << 5;
            *(int*)&Input_settings = var_v1;
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_WeaponStatus:
        i = Menu_Options_UpdateWeaponStatusChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            var_v1 = *(int*)&Input_settings & ~0x40;
            var_v1 |= (((2 - i) & 1) << 6);
            *(int*)&Input_settings = var_v1;
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_ArmorStatus:
        i = Menu_Options_UpdateArmorStatusChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            var_v1 = *(int*)&Input_settings & ~0x80;
            var_v1 |= (((2 - i) & 1) << 7);
            *(int*)&Input_settings = var_v1;
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_CursorMemory:
        i = Menu_Options_UpdateCursorMemoryChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            Input_settings.cursorMemory = 2 - i;
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_Information:
        i = Menu_Options_UpdateInformationChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            Input_settings.information = 2 - i;
            Menu_Framework_InitializeInformationBox();
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_PuzzleMode:
        i = Menu_Options_UpdatePuzzleModeChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            temp_v1_2 = (*(int*)&Input_settings & ~8) | (((i - 1) & 1) * 8);
            *(int*)&Input_settings = temp_v1_2;
            Audio_Seq_unusedStateFlags.puzzleMode = ~(temp_v1_2 >> 3) & 1;
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_Sound:
        i = Menu_Options_UpdateSoundChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            Input_settings.monoSound = i - 1;
            Audio_SetMonoOutput(Input_settings.monoSound);
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_Vibration:
        i = Menu_Options_UpdateVibrationChoice(0);
        if (i == 0) {
            break;
        }
        if (i == -2) {
            *state = Menu_Options_OptionsMenuState_ExitToBattle;
            break;
        }
        if (i > 0) {
            Input_settings.vibrationOn = 2 - i;
        }
        *state = Menu_Options_OptionsMenuState_Init;
        break;
    case Menu_Options_OptionsMenuState_ExitToMenu:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            *state = Menu_Options_OptionsMenuState_None;
            return 1;
        }
        break;
    case Menu_Options_OptionsMenuState_ExitToBattle:
        Menu_Framework_DismissInformationBox();
        Menu_Framework_SetCommandLabel(Menu_Framework_MenuCommand_None);
        if (Menu_Framework_AreMenuRowsIdle() != 0) {
            Menu_Framework_menuState.currentMenu = 8;
            *state = Menu_Options_OptionsMenuState_None;
            return 1;
        }
        break;
    }
    return 0;
}

// MENU8.PRG reloads restore these statics' initial values (PORTING_WATCHLIST.md §1).
void Menu_Options_ResetMenu8Statics(void)
{
    {
        static const u_short initial[] = {
0 /* disc text table: loaded in Step 5 */
};
        memcpy((void*)&Menu_Options_menuStrings, &initial, sizeof Menu_Options_menuStrings);
    }
    {
        static const char initial[] = { 3, 1, 0 };
        memcpy((void*)&Menu_Options_simpleMapValues, &initial, sizeof Menu_Options_simpleMapValues);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_simpleMapChoiceSelectedRow, &initial, sizeof Menu_Options_simpleMapChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_simpleMapChoiceState, &initial, sizeof Menu_Options_simpleMapChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_abilityTimingChoiceSelectedRow, &initial, sizeof Menu_Options_abilityTimingChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_abilityTimingChoiceState, &initial, sizeof Menu_Options_abilityTimingChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_weaponStatusChoiceSelectedRow, &initial, sizeof Menu_Options_weaponStatusChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_weaponStatusChoiceState, &initial, sizeof Menu_Options_weaponStatusChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_armorStatusChoiceSelectedRow, &initial, sizeof Menu_Options_armorStatusChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_armorStatusChoiceState, &initial, sizeof Menu_Options_armorStatusChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_cursorMemoryChoiceSelectedRow, &initial, sizeof Menu_Options_cursorMemoryChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_cursorMemoryChoiceState, &initial, sizeof Menu_Options_cursorMemoryChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_informationChoiceSelectedRow, &initial, sizeof Menu_Options_informationChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_informationChoiceState, &initial, sizeof Menu_Options_informationChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_puzzleModeChoiceSelectedRow, &initial, sizeof Menu_Options_puzzleModeChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_puzzleModeChoiceState, &initial, sizeof Menu_Options_puzzleModeChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_soundChoiceSelectedRow, &initial, sizeof Menu_Options_soundChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_soundChoiceState, &initial, sizeof Menu_Options_soundChoiceState);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_vibrationChoiceSelectedRow, &initial, sizeof Menu_Options_vibrationChoiceSelectedRow);
    }
    {
        static const int initial = 0;
        memcpy((void*)&Menu_Options_vibrationChoiceState, &initial, sizeof Menu_Options_vibrationChoiceState);
    }
}

