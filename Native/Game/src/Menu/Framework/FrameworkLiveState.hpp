#pragma once
// Hand-maintained (tools/port/owned.csv).
// The menu framework's variables in the shapes the copied menus, the battle
// flow and the diagnostics read and write. FrameworkLive.cpp defines them and
// mirrors the scalar ones with the typed framework State around each call
// (Framework.hpp); the menu table, the string buffer, the row types and the
// item lists stay where the workspace allocation put them.

#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/MenuMemory.h"

namespace VS::Menu {
// decomp: D_800EB9CE (BATTLE/BATTLE.PRG 5BF94.c)
extern char textBoxBufferIndex;
// decomp: D_800F4CD0 (BATTLE/BATTLE.PRG 5BF94.c)
inline constexpr int kTextBoxOrderingTableCount = 102;
extern u_long textBoxOrderingTables[kTextBoxOrderingTableCount];
// decomp: D_800F4E6B (BATTLE/BATTLE.PRG 5BF94.c)
extern char saveLocationId;
}  // namespace VS::Menu

namespace VS::Menu::Framework {
// decomp: _loadedSubMenu (BATTLE/BATTLE.PRG 5BF94.c): the module image byte
// is 0, so the zero-initialised definition stands for it.
extern signed char loadedSubMenu;
// decomp: vs_battle_menuItems (BATTLE/BATTLE.PRG 5BF94.c): the forty rows of
// the workspace block, or null with no workspace.
extern MenuRow* menuItems;
// decomp: D_800EB9C4 (BATTLE/BATTLE.PRG 5BF94.c)
extern VS::Items::LootListNode* lootLists;
// decomp: D_800EB9C8 (BATTLE/BATTLE.PRG 5BF94.c)
extern VS::Items::BattleLoot* lootEntry;
// decomp: _menuBgUnpackedBuf (BATTLE/BATTLE.PRG 5BF94.c)
extern u_int* menuBackgroundBuffer;
// decomp: D_800F4CC0 (BATTLE/BATTLE.PRG 5BF94.c)
extern int mainMenuCursorAnimation;
// decomp: vs_battle_shortcutInvoked (BATTLE/BATTLE.PRG 5BF94.c)
extern char shortcutInvoked;
// decomp: vs_battle_submenuStates (BATTLE/BATTLE.PRG 5BF94.c)
extern char submenuStates[kSubmenuStateCount];
// decomp: vs_battle_rowTypeBuf (BATTLE/BATTLE.PRG 5BF94.c)
extern int* rowTypeBuf;
// decomp: vs_battle_unlockedBreakArt (BATTLE/BATTLE.PRG 5BF94.c)
extern char unlockedBreakArt;
// decomp: vs_battle_stringBuf (BATTLE/BATTLE.PRG 5BF94.c)
extern char* stringBuf;
// decomp: D_800F4E98 (BATTLE/BATTLE.PRG 5BF94.c)
extern ActionSelection actionSelection;
// decomp: D_800F4EA0 (BATTLE/BATTLE.PRG 5BF94.c)
extern int actionRestrictions;
// decomp: D_800F4EE8 (BATTLE/BATTLE.PRG 5BF94.c)
extern Menu_Framework_MenuMemory menuMemory;
// decomp: vs_battle_unlockedBattleAbility (BATTLE/BATTLE.PRG 5BF94.c)
extern char unlockedBattleAbility;
// decomp: D_800F4FDB (BATTLE/BATTLE.PRG 5BF94.c)
extern char openedWithoutMainMenu;
// decomp: vs_battle_menuState (BATTLE/BATTLE.PRG 5BF94.c)
extern Menu_Framework_MenuControlState menuState;
// decomp: _itemsText (MENU/MAINMENU.PRG 58EC.c): the list menu's entries.
extern u_short* itemsText;
// decomp: _menuRowYOffset (MENU/MAINMENU.PRG 58EC.c)
extern char menuRowYOffset;
// decomp: _selectedRow (MENU/MAINMENU.PRG 58EC.c)
extern int selectedRow;
// decomp: _subMenuId (MENU/MAINMENU.PRG 58EC.c)
extern u_char subMenuId;
// decomp: _subMenuPage (MENU/MAINMENU.PRG 58EC.c)
extern char subMenuPage;
// decomp: _subMenuRowCount (MENU/MAINMENU.PRG 58EC.c)
extern u_short subMenuRowCount;
// decomp: _subMenuRowInfo (MENU/MAINMENU.PRG 58EC.c)
extern char subMenuRowInfo;
// decomp: _textHeaders (MENU/MAINMENU.PRG 58EC.c)
extern Menu_Framework_TextHeader textHeaders[2];
// decomp: vs_mainMenu_actionMenuState (MENU/MAINMENU.PRG 58EC.c)
extern char actionMenuState;
// decomp: vs_mainMenu_backgroundFadeStep (MENU/MAINMENU.PRG 58EC.c)
extern int backgroundFadeStep;
// decomp: vs_mainMenu_containerEmptyBackup (MENU/MAINMENU.PRG 58EC.c). The
// container save submenu keeps the "container empty" setting here for the
// session and restores it on exit (MENU7.PRG 260.c:710, 876).
extern char containerEmptyBackup;
// decomp: vs_mainMenu_cursorColor (MENU/MAINMENU.PRG 58EC.c)
extern char cursorColor;
// decomp: vs_mainMenu_displayActionCost (MENU/MAINMENU.PRG 58EC.c)
extern char displayActionCost;
// decomp: vs_mainMenu_hideMenu (MENU/MAINMENU.PRG 58EC.c)
extern char hideMenu;
// decomp: vs_mainMenu_itemsListRow (MENU/MAINMENU.PRG 58EC.c)
extern char itemsListRow;
// decomp: vs_menu_inventoryStorage (MENU/MAINMENU.PRG C48.c). The container
// session's heap storage: the inventory snapshot restored on exit and the
// container copies the transfer screens edit (MENU7.PRG 260.c:711-901,
// MENUD.PRG 234.c:266-599).
extern Menu_Container_InventoryStorage* inventoryStorage;
}  // namespace VS::Menu::Framework
