#pragma once

#include "Items/ItemRecords.hpp"

namespace VS::Menu::Loot {  // NOLINT(readability-identifier-naming)

// Temporary row metadata, not additional player inventory slots.
inline constexpr int kRowCapacity = sizeof(VS::Items::InventoryIndices);

}  // namespace VS::Menu::Loot

namespace VS::Menu::Loot::Live {

// Native paging (LootRowsLive.cpp): stages the drops the last batch left
// behind; 0 when none were deferred or none remain.
int AdvanceStagedInventory();
// Forgets the pending batch before its loot records are released.
void ResetPaging();

}  // namespace VS::Menu::Loot::Live
