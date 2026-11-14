#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include "Items/EquipmentStats.hpp"

namespace VS::Menu::Framework::sorting {
// Both inventory and container use two live key scans, not cached sort keys.
template <typename World, typename Token, typename Key>
void StableOrder(World& world, std::span<Token> order, std::size_t count,
                 std::string_view bounds_reason, Key key) {
  const auto index = [&](std::size_t row) -> Token& {
    if (row >= order.size()) {
      world.Refuse(bounds_reason);
    }
    return order[row];
  };
  std::vector<Token> sorted(count);
  std::size_t selected = 0;
  for (;;) {
    auto highest = std::numeric_limits<std::int32_t>::min();
    for (std::size_t row = 0; row < count; ++row) {
      const auto token = index(row);
      if (token != 0) highest = std::max(highest, key(token));
    }
    if (highest == std::numeric_limits<std::int32_t>::min()) break;
    for (std::size_t row = 0; row < count; ++row) {
      const auto token = index(row);
      if (token != 0 && key(token) == highest) {
        if (selected >= sorted.size()) {
          world.Refuse("sort selected more entries than its captured capacity");
        }
        sorted[selected++] = token;
        index(row) = 0;
      }
    }
  }
  std::copy(sorted.begin(), sorted.end(), order.begin());
}

[[nodiscard]] inline bool HasEquipmentProjection(std::int32_t category) {
  return category == static_cast<std::int32_t>(Items::ItemCategory::kBlade) ||
         category == static_cast<std::int32_t>(Items::ItemCategory::kGrip) ||
         category == static_cast<std::int32_t>(Items::ItemCategory::kArmor) ||
         category == static_cast<std::int32_t>(Items::ItemCategory::kGem);
}

// Readers resolve their own inventory/container source at the call point.
template <typename Reader>
void ProjectEquipmentStats(Items::EquipmentPart& target, std::int32_t category,
                           Reader read) {
  switch (static_cast<Items::ItemCategory>(category)) {
    case Items::ItemCategory::kBlade:
      Items::ReadInventoryBladeStats(
          target, read.template operator()<Items::InventoryBlade>());
      return;
    case Items::ItemCategory::kGrip:
      Items::ReadInventoryGripStats(
          target, read.template operator()<Items::InventoryGrip>());
      return;
    case Items::ItemCategory::kArmor:
      Items::ReadInventoryArmorStats(
          target, read.template operator()<Items::InventoryArmor>());
      return;
    case Items::ItemCategory::kGem:
      Items::ReadInventoryGemStats(
          target, read.template operator()<Items::InventoryGem>());
      return;
    default:
      return;
  }
}
}  // namespace VS::Menu::Framework::sorting
