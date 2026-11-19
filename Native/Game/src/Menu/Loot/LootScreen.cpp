// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Menu/Loot/LootScreen.hpp"

#include <cstdint>

#include "Items/ItemRecords.hpp"
#include "Menu/Loot/LootRows.hpp"

// decomp: _gradientStateIndex (_gradientStateIndex@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::gradientStateIndex = 0;

// decomp: _controlsEnabled (_controlsEnabled@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::controlsEnabled = 0;

// decomp: _lootList (_lootList@src/MENU/MENUB.PRG/260.c)
VS::Menu::Loot::LootRow* VS::Menu::Loot::lootList;

// decomp: _lootListWorkingCopy (_lootListWorkingCopy@src/MENU/MENUB.PRG/260.c)
VS::Menu::Loot::LootRow* VS::Menu::Loot::lootListWorkingCopy;

// decomp: _inventory (_inventory@src/MENU/MENUB.PRG/260.c)
VS::Items::InventoryContents* VS::Menu::Loot::stagedInventory;

// decomp: _inventoryBackup (_inventoryBackup@src/MENU/MENUB.PRG/260.c)
VS::Items::InventoryContents* VS::Menu::Loot::inventoryBackup;

// decomp: _showInventory (_showInventory@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::showInventory;

// decomp: _inventoryCapacityFlyinTimer
// (_inventoryCapacityFlyinTimer@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::inventoryCapacityFlyinTimer;

// decomp: _showLoot (_showLoot@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::showLoot;

// decomp: _lootLeftEdge (_lootLeftEdge@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::lootLeftEdge;

// decomp: _selectedLoot (_selectedLoot@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::selectedLoot;

// decomp: _lootListOffset (_lootListOffset@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::lootListOffset;

// decomp: _lootListCount (_lootListCount@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::lootListCount;

// decomp: D_8010A6B7 (D_8010A6B7@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::previewedLoot;

// decomp: _lootClaimed (_lootClaimed@src/MENU/MENUB.PRG/260.c)
std::uint8_t VS::Menu::Loot::lootClaimed;

// decomp: _itemCategoryHeaders (_itemCategoryHeaders@src/MENU/MENUB.PRG/260.c)
const std::array<const char*, VS::Menu::Loot::kCategoryHeaderCount>
    VS::Menu::Loot::kItemCategoryHeaders = {"WEAPON", "BLADE", "GRIP", "SHIELD",
                                            "ARMOR",  "GEM",   "MISC"};

void VS::Menu::Loot::ResetMenubStatics() {
  gradientStateIndex = 0;
  controlsEnabled = 0;
}
