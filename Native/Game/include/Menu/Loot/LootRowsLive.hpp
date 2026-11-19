#pragma once

#include "Items/ItemRecords.hpp"
#include "Menu/Loot/LootRows.hpp"

namespace VS::Menu::Loot::Live {  // NOLINT(readability-identifier-naming)
void RefreshClaimableRows();
void AppendLootRow(int category, int slot);
[[nodiscard]] int CountContainedCategory(int counted_category, LootRow* row);
[[nodiscard]] int IsDropAllowed(int difficulty_flags);
void StageDropRecord(VS::Items::BattleLoot* record);
void BuildStagedInventory(VS::Items::LootListNode* node);
void RecordClaimedKey(int item_id);
}  // namespace VS::Menu::Loot::Live
