#pragma once

extern "C" {
#include "Items/Inventory.h"
#include "Items/Loot.h"
}

namespace VS::Menu::Loot::Live {  // NOLINT(readability-identifier-naming)
[[nodiscard]] int StageBlade(Items_InventoryBlade* blade, int weapon_index);
[[nodiscard]] int StageGrip(Items_InventoryGrip* grip, int weapon_index);
[[nodiscard]] int StageGem(Items_InventoryGem* gem, int item_index);
[[nodiscard]] int StageWeapon(Items_LootedWeapon* weapon);
[[nodiscard]] int StageArmor(Items_InventoryArmor* armor);
[[nodiscard]] int StageShield(Items_LootedShield* shield);
[[nodiscard]] int StageMiscItem(Items_LootedItem* item);
}  // namespace VS::Menu::Loot::Live
