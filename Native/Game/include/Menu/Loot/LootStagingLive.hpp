#pragma once

#include "Items/ItemRecords.hpp"

namespace VS::Menu::Loot::Live {  // NOLINT(readability-identifier-naming)
[[nodiscard]] int StageBlade(VS::Items::InventoryBlade* blade,
                             int weapon_index);
[[nodiscard]] int StageGrip(VS::Items::InventoryGrip* grip, int weapon_index);
[[nodiscard]] int StageGem(VS::Items::InventoryGem* gem, int item_index);
[[nodiscard]] int StageWeapon(VS::Items::LootedWeapon* weapon);
[[nodiscard]] int StageArmor(VS::Items::InventoryArmor* armor);
[[nodiscard]] int StageShield(VS::Items::LootedShield* shield);
[[nodiscard]] int StageMiscItem(VS::Items::LootedItem* item);
}  // namespace VS::Menu::Loot::Live
