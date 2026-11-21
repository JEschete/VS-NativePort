#pragma once
// Hand-maintained (tools/port/owned.csv).
// The menu framework's variables in the shapes the copied menus, the battle
// flow and the diagnostics read and write. FrameworkLive.cpp defines them and
// mirrors the scalar ones with the typed framework State around each call
// (Framework.hpp); the menu table, the string buffer, the row types and the
// item lists stay where the workspace allocation put them.

#include <array>

#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/Framework.hpp"

namespace VS::Content {
struct DiscQueueSlot;
}  // namespace VS::Content

namespace VS::Menu {
// decomp: D_800EB9CE (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/5BF94.c:361)
extern char textBoxBufferIndex;
// decomp: D_800F4CD0 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/5BF94.c:420)
inline constexpr int kTextBoxOrderingTableCount = 102;
extern std::uint32_t textBoxOrderingTables[kTextBoxOrderingTableCount];
// decomp: D_800F4E6B (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:293)
extern char saveLocationId;
}  // namespace VS::Menu

namespace VS::Menu::Framework {
// decomp: vs_MenuState (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:83):
// the framework's MenuControl and the menu overlay read it waits on.
struct LiveMenuControl {
  MenuControl control;
  VS::Content::DiscQueueSlot* cd_queue = nullptr;
};

// decomp: loadedSubMenu (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/5BF94.c:358;
// was _loadedSubMenu): the module image byte is 0, so the zero-initialised
// definition stands for it.
extern signed char loadedSubMenu;
// decomp: vs_battle_menuItems (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:277): the forty rows of the workspace block,
// or null with no workspace.
extern MenuRow* menuItems;
// decomp: D_800EB9C4 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:275)
extern VS::Items::LootListNode* lootLists;
// decomp: D_800EB9C8 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:276)
extern VS::Items::BattleLoot* lootEntry;
// decomp: menuBgUnpackedBuf (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/5BF94.c:363; was _menuBgUnpackedBuf)
extern std::uint32_t* menuBackgroundBuffer;
// decomp: D_800F4CC0 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/5BF94.c:419)
extern int mainMenuCursorAnimation;
// decomp: vs_battle_shortcutInvoked (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:292)
extern char shortcutInvoked;
// decomp: vs_battle_submenuStates (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:297)
extern char submenuStates[kSubmenuStateCount];
// decomp: vs_battle_rowTypeBuf (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:294)
extern int* rowTypeBuf;
// decomp: vs_battle_unlockedBreakArt (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:295)
extern char unlockedBreakArt;
// decomp: vs_battle_stringBuf (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:296)
extern char* stringBuf;
// decomp: D_800F4E98 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:299)
extern ActionSelection actionSelection;
// decomp: D_800F4EA0 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:300)
extern int actionRestrictions;
// decomp: D_800F4EE8 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:301)
extern MenuMemory menuMemory;
// decomp: vs_battle_unlockedBattleAbility (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:302)
extern char unlockedBattleAbility;
// decomp: D_800F4FDB (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/battleUi.h:303)
extern char openedWithoutMainMenu;
// decomp: vs_battle_menuState (BATTLE/BATTLE.PRG
// src/BATTLE/BATTLE.PRG/battleUi.h:307)
extern LiveMenuControl menuState;
// decomp: itemsText (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:801; was
// _itemsText): the list menu's entries.
extern std::uint16_t* itemsText;
// decomp: menuRowYOffset (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:806;
// was _menuRowYOffset)
extern char menuRowYOffset;
// decomp: selectedRow (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:802; was
// _selectedRow)
extern int selectedRow;
// decomp: currentSubMenuId (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:803;
// was _subMenuId)
extern std::uint8_t subMenuId;
// decomp: subMenuPage (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:805; was
// _subMenuPage)
extern char subMenuPage;
// decomp: subMenuRowCount (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:807;
// was _subMenuRowCount)
extern std::uint16_t subMenuRowCount;
// decomp: subMenuRowInfo (MENU/MAINMENU.PRG src/MENU/MAINMENU.PRG/58EC.c:804;
// was _subMenuRowInfo)
extern char subMenuRowInfo;
// decomp: _textHeaders (MENU/MAINMENU.PRG 58EC.c)
extern std::array<TextHeader, kTextHeaderCount> textHeaders;
// decomp: vs_mainMenu_actionMenuState (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:26)
extern char actionMenuState;
// decomp: vs_mainMenu_backgroundFadeStep (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:405)
extern int backgroundFadeStep;
// decomp: vs_mainMenu_containerEmptyBackup (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:59). The container save submenu keeps the
// "container empty" setting here for the session and restores it on exit
// (MENU7.PRG src/MENU/MENU7.PRG/260.c:710, 876).
extern char containerEmptyBackup;
// decomp: vs_mainMenu_cursorColor (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:403)
extern char cursorColor;
// decomp: vs_mainMenu_displayActionCost (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:402)
extern char displayActionCost;
// decomp: vs_mainMenu_hideMenu (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:404)
extern char hideMenu;
// decomp: vs_mainMenu_itemsListRow (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/58EC.c:800)
extern char itemsListRow;
// decomp: vs_menu_inventoryStorage (MENU/MAINMENU.PRG
// src/MENU/MAINMENU.PRG/C48.c:44). The container session's heap storage: the
// inventory snapshot restored on exit and the container copies the transfer
// screens edit (MENU7.PRG src/MENU/MENU7.PRG/260.c:711-901, MENUD.PRG
// 234.c:266-599).
extern VS::Menu::container::InventoryStorage* inventoryStorage;
}  // namespace VS::Menu::Framework
