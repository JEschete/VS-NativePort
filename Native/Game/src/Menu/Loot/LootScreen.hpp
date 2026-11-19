#pragma once
// MENUB.PRG's loot screen statics (src/MENU/MENUB.PRG/260.c:34-49): the staged
// loot inventory and its rows, the row cursor, and the screen's animation and
// input flags. The original compiled its char statics unsigned
// (-funsigned-char).
#include <array>
#include <cstddef>
#include <cstdint>

#include "Items/ItemRecords.hpp"
#include "Menu/Loot/LootRows.hpp"

namespace VS::Menu::Loot {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kCategoryHeaderCount = 7;

// decomp: gradientStateIndex (src/MENU/MENUB.PRG/260.c:34)
extern std::uint8_t gradientStateIndex;
// decomp: controlsEnabled (src/MENU/MENUB.PRG/260.c:35)
extern std::uint8_t controlsEnabled;
// decomp: lootList (src/MENU/MENUB.PRG/260.c:37)
extern LootRow* lootList;
// decomp: lootListWorkingCopy (src/MENU/MENUB.PRG/260.c:38)
extern LootRow* lootListWorkingCopy;
// decomp: inventory (src/MENU/MENUB.PRG/260.c:39): the working inventory the
// screen stages the loot into.
extern Items::InventoryContents* stagedInventory;
// decomp: inventoryBackup (src/MENU/MENUB.PRG/260.c:40): Ashley's inventory
// while a row's description swaps the staged one in.
extern Items::InventoryContents* inventoryBackup;
// decomp: showInventory (src/MENU/MENUB.PRG/260.c:41)
extern std::uint8_t showInventory;
// decomp: inventoryCapacityFlyinTimer (src/MENU/MENUB.PRG/260.c:42)
extern std::uint8_t inventoryCapacityFlyinTimer;
// decomp: showLoot (src/MENU/MENUB.PRG/260.c:43)
extern std::uint8_t showLoot;
// decomp: lootLeftEdge (src/MENU/MENUB.PRG/260.c:44)
extern std::uint8_t lootLeftEdge;
// decomp: selectedLoot (src/MENU/MENUB.PRG/260.c:45): 0xFF with no rows.
extern std::uint8_t selectedLoot;
// decomp: lootListOffset (src/MENU/MENUB.PRG/260.c:46)
extern std::uint8_t lootListOffset;
// decomp: lootListCount (src/MENU/MENUB.PRG/260.c:47)
extern std::uint8_t lootListCount;
// decomp: D_8010A6B7 (src/MENU/MENUB.PRG/260.c:48)
extern std::uint8_t previewedLoot;
// decomp: lootClaimed (src/MENU/MENUB.PRG/260.c:49)
extern std::uint8_t lootClaimed;
// decomp: itemCategoryHeaders (src/MENU/MENUB.PRG/260.c:961)
extern const std::array<const char*, kCategoryHeaderCount> kItemCategoryHeaders;

// MENUB.PRG reloads restore the initialized statics (PORTING_WATCHLIST.md §1).
void ResetMenubStatics();
}  // namespace VS::Menu::Loot
