#pragma once
// Hand-maintained (tools/port/owned.csv).
// The menu framework's variables in the shapes the copied menus, the battle
// flow and the diagnostics read and write. FrameworkLive.cpp defines them and
// mirrors the scalar ones with the typed framework State around each call
// (Framework.hpp); the menu table, the string buffer, the row types and the
// item lists stay where the workspace allocation put them.

#include "Items/Loot.h"
#include "Legacy/Psx.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/MenuMemory.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: _loadedSubMenu (BATTLE/BATTLE.PRG 5BF94.c): the module image byte
// is 0, so the zero-initialised definition stands for it.
extern signed char Menu_Framework_loadedSubMenu;
// decomp: vs_battle_menuItems (BATTLE/BATTLE.PRG 5BF94.c): the forty rows of
// the workspace block, or null with no workspace.
extern Menu_Framework_MenuRow* Menu_Framework_menuItems;
// decomp: D_800EB9C4 (BATTLE/BATTLE.PRG 5BF94.c)
extern Items_LootListNode* Menu_Framework_lootLists;
// decomp: D_800EB9C8 (BATTLE/BATTLE.PRG 5BF94.c)
extern Items_BattleLoot* Menu_Framework_lootEntry;
// decomp: D_800EB9CE (BATTLE/BATTLE.PRG 5BF94.c)
extern char Menu_textBoxBufferIndex;
// decomp: _menuBgUnpackedBuf (BATTLE/BATTLE.PRG 5BF94.c)
extern u_int* Menu_Framework_menuBackgroundBuffer;
// decomp: D_800F4CC0 (BATTLE/BATTLE.PRG 5BF94.c)
extern int Menu_Framework_mainMenuCursorAnimation;
// decomp: D_800F4CD0 (BATTLE/BATTLE.PRG 5BF94.c)
enum { kMenuTextBoxOrderingTableCount = 102 };
extern u_long Menu_textBoxOrderingTables[kMenuTextBoxOrderingTableCount];
// decomp: vs_battle_shortcutInvoked (BATTLE/BATTLE.PRG 5BF94.c)
extern char Menu_Framework_shortcutInvoked;
// decomp: D_800F4E6B (BATTLE/BATTLE.PRG 5BF94.c)
extern char Menu_saveLocationId;
// decomp: vs_battle_submenuStates (BATTLE/BATTLE.PRG 5BF94.c)
enum { kMenuSubmenuStateCount = 16 };
extern char Menu_Framework_submenuStates[kMenuSubmenuStateCount];
// decomp: vs_battle_rowTypeBuf (BATTLE/BATTLE.PRG 5BF94.c)
extern int* Menu_Framework_rowTypeBuf;
// decomp: vs_battle_unlockedBreakArt (BATTLE/BATTLE.PRG 5BF94.c)
extern char Menu_Framework_unlockedBreakArt;
// decomp: vs_battle_stringBuf (BATTLE/BATTLE.PRG 5BF94.c)
extern char* Menu_Framework_stringBuf;
// decomp: D_800F4E98 (BATTLE/BATTLE.PRG 5BF94.c)
extern Menu_Framework_ActionSelection Menu_Framework_actionSelection;
// decomp: D_800F4EA0 (BATTLE/BATTLE.PRG 5BF94.c)
extern int Menu_Framework_actionRestrictions;
// decomp: D_800F4EE8 (BATTLE/BATTLE.PRG 5BF94.c)
extern Menu_Framework_MenuMemory Menu_Framework_menuMemory;
// decomp: vs_battle_unlockedBattleAbility (BATTLE/BATTLE.PRG 5BF94.c)
extern char Menu_Framework_unlockedBattleAbility;
// decomp: D_800F4FDB (BATTLE/BATTLE.PRG 5BF94.c)
extern char Menu_Framework_openedWithoutMainMenu;
// decomp: vs_battle_menuState (BATTLE/BATTLE.PRG 5BF94.c)
extern Menu_Framework_MenuControlState Menu_Framework_menuState;
// decomp: _itemsText (MENU/MAINMENU.PRG 58EC.c): the list menu's entries.
extern u_short* Menu_Framework_itemsText;
// decomp: _menuRowYOffset (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_menuRowYOffset;
// decomp: _selectedRow (MENU/MAINMENU.PRG 58EC.c)
extern int Menu_Framework_selectedRow;
// decomp: _subMenuId (MENU/MAINMENU.PRG 58EC.c)
extern u_char Menu_Framework_subMenuId;
// decomp: _subMenuPage (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_subMenuPage;
// decomp: _subMenuRowCount (MENU/MAINMENU.PRG 58EC.c)
extern u_short Menu_Framework_subMenuRowCount;
// decomp: _subMenuRowInfo (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_subMenuRowInfo;
// decomp: _textHeaders (MENU/MAINMENU.PRG 58EC.c)
extern Menu_Framework_TextHeader Menu_Framework_textHeaders[2];
// decomp: vs_mainMenu_actionMenuState (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_actionMenuState;
// decomp: vs_mainMenu_backgroundFadeStep (MENU/MAINMENU.PRG 58EC.c)
extern int Menu_Framework_backgroundFadeStep;
// decomp: vs_mainMenu_containerEmptyBackup (MENU/MAINMENU.PRG): never read.
extern char Menu_Framework_unusedContainerEmptyBackup;
// decomp: vs_mainMenu_cursorColor (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_cursorColor;
// decomp: vs_mainMenu_displayActionCost (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_displayActionCost;
// decomp: vs_mainMenu_hideMenu (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_hideMenu;
// decomp: vs_mainMenu_itemsListRow (MENU/MAINMENU.PRG 58EC.c)
extern char Menu_Framework_itemsListRow;
// decomp: vs_menu_inventoryStorage (MENU/MAINMENU.PRG): never read.
extern Menu_Container_InventoryStorage* Menu_Framework_unusedInventoryStorage;

#ifdef __cplusplus
}
#endif
