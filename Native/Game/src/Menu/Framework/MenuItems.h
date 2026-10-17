// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_MENU_FRAMEWORK_MENUITEMS_H_TYPES
#define VS_MENU_FRAMEWORK_MENUITEMS_H_TYPES
#include "Legacy/Psx.h"


// decomp: vs_battle_menuItemScrollable (src/BATTLE/BATTLE.PRG/5BF94.h:27)
enum Menu_Framework_MenuItemScroll {
    Menu_Framework_MenuItemScroll_None,
    Menu_Framework_MenuItemScroll_Up,
    Menu_Framework_MenuItemScroll_Down
};
typedef enum Menu_Framework_MenuItemScroll Menu_Framework_MenuItemScroll;

// decomp: vs_battle_menuItemState (src/BATTLE/BATTLE.PRG/5BF94.h:33)
enum Menu_Framework_MenuItemState {
    Menu_Framework_MenuItemState_Inactive,
    Menu_Framework_MenuItemState_Idle,
    Menu_Framework_MenuItemState_SlideX,
    Menu_Framework_MenuItemState_SlideY,
    Menu_Framework_MenuItemState_SlideXY,
    Menu_Framework_MenuItemState_Dismiss = 5
};
typedef enum Menu_Framework_MenuItemState Menu_Framework_MenuItemState;

// decomp: vs_battle_menuStates (src/BATTLE/BATTLE.PRG/5BF94.h:68)
enum Menu_Framework_MenuState {
    Menu_Framework_MenuState_MainMenu,
    Menu_Framework_MenuState_NoneClosing = 0x1F,
    Menu_Framework_MenuState_NoneClosed = 0x3F,
    Menu_Framework_MenuState_OpenedFromMainMenu = 0x40
};
typedef enum Menu_Framework_MenuState Menu_Framework_MenuState;

#endif // VS_MENU_FRAMEWORK_MENUITEMS_H_TYPES
