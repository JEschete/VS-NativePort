#pragma once

#include "Items/Inventory.hpp"

namespace VS::Menu::Loot {  // NOLINT(readability-identifier-naming)
[[nodiscard]] int StageBlade(const Items::InventoryBlade& blade,
                             int weapon_index,
                             Items::InventoryContents& inventory);
[[nodiscard]] int StageGrip(const Items::InventoryGrip& grip, int weapon_index,
                            Items::InventoryContents& inventory);
[[nodiscard]] int StageGem(const Items::InventoryGem& gem, int item_index,
                           Items::InventoryContents& inventory);
[[nodiscard]] int StageWeapon(const Items::LootedWeapon& weapon,
                              Items::InventoryContents& inventory);
[[nodiscard]] int StageArmor(const Items::InventoryArmor& armor,
                             Items::InventoryContents& inventory);
[[nodiscard]] int StageShield(const Items::LootedShield& shield,
                              Items::InventoryContents& inventory);
[[nodiscard]] int StageMiscItem(const Items::LootedItem& item,
                                Items::InventoryContents& inventory);
}  // namespace VS::Menu::Loot
