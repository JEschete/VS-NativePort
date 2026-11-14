#pragma once

extern "C" {
#include "Items/Loot.h"
#include "Menu/Loot/LootInventory.h"
}

namespace VS::Menu::Loot::Live {  // NOLINT(readability-identifier-naming)
void RefreshClaimableRows();
void AppendLootRow(int category, int slot);
[[nodiscard]] int CountContainedCategory(int counted_category,
                                         Menu_Loot_LootListItem* row);
[[nodiscard]] int IsDropAllowed(int difficulty_flags);
void StageDropRecord(Items_BattleLoot* record);
void BuildStagedInventory(Items_LootListNode* node);
void RecordClaimedKey(int item_id);
}  // namespace VS::Menu::Loot::Live
