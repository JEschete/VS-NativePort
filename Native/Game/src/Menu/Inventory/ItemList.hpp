#pragma once
// The MAINMENU.PRG item list statics (src/MENU/MAINMENU.PRG); MAINMENU.PRG
// reloads restore their initial values (PORTING_WATCHLIST.md §1).
#include <array>
#include <cstddef>
#include <cstdint>

#include "Items/ItemRecords.hpp"

namespace VS::Menu::Inventory {
// One display order per listed item category, weapons through misc items.
inline constexpr std::size_t kListedCategories = Items::kItemCategoryCount - 1;
using CategoryOrderTable = std::array<std::uint8_t*, kListedCategories>;

// decomp: vs_mainMenu_freezeTabArrows
// (vs_mainMenu_freezeTabArrows@MENU/MAINMENU.PRG): nonzero while the item
// screens hold the tab navigation arrows still.
extern std::uint8_t freezeTabArrows;
// decomp: vs_mainMenu_inventoryIndices
// (vs_mainMenu_inventoryIndices@MENU/MAINMENU.PRG): the display order each
// listed category reads, pointing into Items::inventoryIndices by default.
extern CategoryOrderTable categoryOrders;

// Restores freezeTabArrows and categoryOrders to their initial values.
void ResetItemListStatics();
}  // namespace VS::Menu::Inventory
