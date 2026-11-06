// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_FRAMEWORK_H_TYPES
#define VS_MENU_FRAMEWORK_FRAMEWORK_H_TYPES
#include "Legacy/Psx.h"

typedef struct Content_DiscQueueSlot Content_DiscQueueSlot;
typedef struct Menu_Framework_MenuControlState Menu_Framework_MenuControlState;
typedef struct Menu_Framework_MenuRow Menu_Framework_MenuRow;
typedef struct Menu_Framework_TextHeader Menu_Framework_TextHeader;

// decomp: vs_mainMenu_actionsMenuActions (src/MENU/MAINMENU.PRG/58EC.h:7)
enum Menu_Framework_ActionsMenuAction { Menu_Framework_ActionsMenuAction_Discard = 4, Menu_Framework_ActionsMenuAction_Sort = 45 };
typedef enum Menu_Framework_ActionsMenuAction Menu_Framework_ActionsMenuAction;

// decomp: vs_mainMenu_buttonIDs (src/MENU/MAINMENU.PRG/58EC.h:89)
enum Menu_Framework_ButtonId {
    Menu_Framework_ButtonId_Triangle,
    Menu_Framework_ButtonId_Square,
    Menu_Framework_ButtonId_Circle,
    Menu_Framework_ButtonId_Cross,
    Menu_Framework_ButtonId_L1,
    Menu_Framework_ButtonId_R1,
    Menu_Framework_ButtonId_L2,
    Menu_Framework_ButtonId_R2
};
typedef enum Menu_Framework_ButtonId Menu_Framework_ButtonId;

// decomp: vs_mainMenu_copyItem_flags (src/MENU/MAINMENU.PRG/2D10.h:200)
enum Menu_Framework_CopyItemFlag { Menu_Framework_CopyItemFlag_Write = 0x10 };
typedef enum Menu_Framework_CopyItemFlag Menu_Framework_CopyItemFlag;

// decomp: vs_mainMenu_icons (src/MENU/MAINMENU.PRG/58EC.h:134)
enum Menu_Framework_MainMenuIcon { Menu_Framework_MainMenuIcon_Gem = 22 };
typedef enum Menu_Framework_MainMenuIcon Menu_Framework_MainMenuIcon;

// decomp: vs_mainMenu_menuCommands (src/MENU/MAINMENU.PRG/58EC.h:45)
enum Menu_Framework_MenuCommand {
    Menu_Framework_MenuCommand_None,
    Menu_Framework_MenuCommand_Command,
    Menu_Framework_MenuCommand_Menu,
    Menu_Framework_MenuCommand_Num,
    Menu_Framework_MenuCommand_Warning,
    Menu_Framework_MenuCommand_Help,
    Menu_Framework_MenuCommand_Info
};
typedef enum Menu_Framework_MenuCommand Menu_Framework_MenuCommand;

// decomp: vs_mainMenu_menuItemIds (src/MENU/MAINMENU.PRG/C48.h:6)
enum Menu_Framework_MenuItemId { Menu_Framework_MenuItemId_None = 40 };
typedef enum Menu_Framework_MenuItemId Menu_Framework_MenuItemId;

// decomp: vs_mainMenu_menuSelection (src/MENU/MAINMENU.PRG/58EC.h:211)
enum Menu_Framework_MenuSelection {
    Menu_Framework_MenuSelection_Confirm,
    Menu_Framework_MenuSelection_Back = 0xFFFF,
    Menu_Framework_MenuSelection_Quit = -2
};
typedef enum Menu_Framework_MenuSelection Menu_Framework_MenuSelection;

// decomp: vs_mainMenu_renderDpPpBarsState (src/MENU/MAINMENU.PRG/C48.h:104)
enum Menu_Framework_DpPpBarsState {
    Menu_Framework_DpPpBarsState_SlideIn = 0 << 2,
    Menu_Framework_DpPpBarsState_SlideOut = 1 << 2,
    Menu_Framework_DpPpBarsState_Static = 2 << 2
};
typedef enum Menu_Framework_DpPpBarsState Menu_Framework_DpPpBarsState;

// decomp: vs_mainMenu_renderDpPpBarsTarget (src/MENU/MAINMENU.PRG/C48.h:97)
enum Menu_Framework_DpPpBarsTarget {
    Menu_Framework_DpPpBarsTarget_None,
    Menu_Framework_DpPpBarsTarget_Dp,
    Menu_Framework_DpPpBarsTarget_Pp,
    Menu_Framework_DpPpBarsTarget_Both
};
typedef enum Menu_Framework_DpPpBarsTarget Menu_Framework_DpPpBarsTarget;

// decomp: vs_mainMenu_rowTypeFlags (src/MENU/MAINMENU.PRG/58EC.h:178)
enum Menu_Framework_RowTypeFlag {
    Menu_Framework_RowTypeFlag_Disabled = 1,
};
typedef enum Menu_Framework_RowTypeFlag Menu_Framework_RowTypeFlag;

// decomp: vs_mainMenu_statPage (src/MENU/MAINMENU.PRG/58EC.h:337)
enum Menu_Framework_StatPage {
    Menu_Framework_StatPage_Class,
    Menu_Framework_StatPage_Affinity,
    Menu_Framework_StatPage_Type,
    Menu_Framework_StatPage_None,
};
typedef enum Menu_Framework_StatPage Menu_Framework_StatPage;

// decomp: vs_mainMenu_statPageEnabled (src/MENU/MAINMENU.PRG/C48.h:77)
enum Menu_Framework_StatPageEnabled {
    Menu_Framework_StatPageEnabled_Class = 1 << 0,
    Menu_Framework_StatPageEnabled_Affinity = 1 << 1,
    Menu_Framework_StatPageEnabled_Type = 1 << 2,
    Menu_Framework_StatPageEnabled_All =
        Menu_Framework_StatPageEnabled_Class | Menu_Framework_StatPageEnabled_Affinity | Menu_Framework_StatPageEnabled_Type
};
typedef enum Menu_Framework_StatPageEnabled Menu_Framework_StatPageEnabled;

// decomp: vs_mainMenu_statusUiIds (src/MENU/MAINMENU.PRG/58EC.h:319)
enum Menu_Framework_StatusUiId {
    Menu_Framework_StatusUiId_UiPp,
    Menu_Framework_StatusUiId_UiDp,
    Menu_Framework_StatusUiId_UistatPageFirstElement,
    Menu_Framework_StatusUiId_UistatPageLastElement = 8,
    Menu_Framework_StatusUiId_UiPrincipleItem,
    Menu_Framework_StatusUiId_UiRange = 16,
    Menu_Framework_StatusUiId_UiRisk,
    Menu_Framework_StatusUiId_UiStr,
    Menu_Framework_StatusUiId_UiInt,
    Menu_Framework_StatusUiId_UiAgi,
};
typedef enum Menu_Framework_StatusUiId Menu_Framework_StatusUiId;

// decomp: textHeader_t (src/MENU/MAINMENU.PRG/58EC.c:21)
typedef struct Menu_Framework_TextHeader {
    char text[14];
    char disabled;
    char x;
} Menu_Framework_TextHeader;
_Static_assert(sizeof(Menu_Framework_TextHeader) == 0x10, "Menu_Framework_TextHeader must keep its PS1 layout");

// decomp: vs_battle_menuItem_t (src/BATTLE/BATTLE.PRG/5BF94.h:66)
typedef struct Menu_Framework_MenuRow {
    u_char state;
    u_char w;
    u_char backgroundWidth;
    u_char animationStep;
    u_char gradientState;
    u_char isScrollable;
    u_char selected;
    u_char unselectable;
    u_char buttonId;
    u_char rowIcon;
    u_char fontColor;
    u_char invertGradient;
    u_char material;
    u_char outsetIcon;
    u_short itemPage;
    u_short count;
    u_short max;
    short x;
    short y;
    short targetPosition0;
    short targetPosition1;
    char text[32];
    char* subText;
} Menu_Framework_MenuRow;

// decomp: vs_battle_menuState_t (src/BATTLE/BATTLE.PRG/5BF94.h:81)
typedef struct Menu_Framework_MenuControlState {
    char currentMenu;
    char returnState;
    char executeAbilityType;
    char loading;
    Content_DiscQueueSlot* cdQueue;
} Menu_Framework_MenuControlState;

#endif // VS_MENU_FRAMEWORK_FRAMEWORK_H_TYPES
