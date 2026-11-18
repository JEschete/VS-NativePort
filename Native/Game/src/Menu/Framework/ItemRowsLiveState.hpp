#pragma once
// Hand-maintained (tools/port/owned.csv).
// The item row texts in the shapes the copied menus, the battle flow and the
// diagnostics read: ITEMNAME.BIN, ITEMHELP.BIN and MENU12.BIN as one loaded
// block, and the blade categories' hand texts. ItemRowsLive.cpp defines them
// and hands them to the typed item rows (ItemRows.hpp).

#include "Legacy/Psx.h"
#include "Menu/Framework/ItemRows.hpp"

namespace VS::Menu::Framework {
// decomp: vs_mainMenu_itemHelp (MENU/MAINMENU.PRG 2D10.c): ITEMHELP.BIN in
// the loaded block, or stale after the block is freed.
extern u_short* itemHelp;
// decomp: vs_mainMenu_itemNames (MENU/MAINMENU.PRG 2D10.c): the loaded block
// (ITEMNAME.BIN first), or null when no block is loaded.
extern char (*itemNames)[kItemNameRowBytes];
// decomp: vs_mainMenu_menu12Text (MENU/MAINMENU.PRG 2D10.c): MENU12.BIN in
// the loaded block.
extern u_short* menu12Text;
// decomp: vs_mainMenu_weaponHands (MENU/MAINMENU.PRG 2D10.c): per blade
// category, 0 for one-handed and 1 for two-handed.
inline constexpr int kWeaponHandCategories = 10;
extern u_char weaponHands[kWeaponHandCategories];
}  // namespace VS::Menu::Framework
