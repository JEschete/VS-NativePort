#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "Items/Inventory.hpp"
#include "Menu/Inventory/InventorySorting.hpp"
#include "Menu/SortingKeysFixture.hpp"

namespace VS::Menu::Inventory::sorting_test {
enum class Operation : std::uint8_t {
  kInventory,
  kFixedOrder,
  kEquipmentOrder,
  kCapacity,
  kWeapon,
  kShield,
  kMisc,
  kEquipment
};
struct Event {
  Operation operation{};
  std::int32_t stat{};
  std::int32_t value{};
  std::uint32_t hash{};
  std::uint32_t version{};
};
inline constexpr auto kListed = Items::kItemCategoryCount - 1;
inline constexpr std::size_t kKeySlots = Items::kMiscSlots + 1;

class World : public VS::Menu::sorting_test::KeyedWorld<SortingWorld, World,
                                                        Operation, kKeySlots> {
 public:
  Items::InventoryContents inventory{};
  Items::InventoryIndices indices{};
  Items::CategoryCounts capacities = Items::kCategoryCapacities;
  std::array<Items::ItemCategory, kListed> routes{
      Items::ItemCategory::kWeapon, Items::ItemCategory::kBlade,
      Items::ItemCategory::kGrip,   Items::ItemCategory::kShield,
      Items::ItemCategory::kArmor,  Items::ItemCategory::kGem,
      Items::ItemCategory::kMisc};
  std::span<std::uint8_t> captured;
  std::size_t trace_count = 0;
  std::size_t reads = 0;
  std::uint32_t version = 0;
  std::function<void(World&, Operation)> hook;
  std::vector<Event> events;

  Items::InventoryContents CurrentInventory() override {
    ++reads;
    return inventory;
  }
  std::span<std::uint8_t> FixedOrder(Items::ItemCategory category) override {
    captured = Items::Inventory::DisplayOrder(indices, category);
    auto result = captured;
    Observe(Operation::kFixedOrder);
    return result;
  }
  std::span<std::uint8_t> EquipmentOrder(
      Items::ItemCategory category) override {
    captured = Items::Inventory::DisplayOrder(
        indices, routes[static_cast<std::size_t>(category)]);
    auto result = captured;
    Observe(Operation::kEquipmentOrder);
    return result;
  }
  std::uint8_t Capacity(Items::ItemCategory category) override {
    const auto result = capacities[static_cast<std::size_t>(category)];
    Observe(Operation::kCapacity);
    return result;
  }
  std::int32_t Observe(Operation operation, std::int32_t stat = 0,
                       std::int32_t value = 0) {
    std::uint32_t hash = 0;
    const auto count = std::min(trace_count, captured.size());
    for (std::size_t row = 0; row < count; ++row) {
      hash += static_cast<std::uint32_t>(row + 1) * captured[row];
    }
    events.push_back({operation, stat, value, hash, version});
    if (hook) hook(*this, operation);
    return value;
  }
};

inline void SetRecords(World& world) {
  for (std::size_t row = 0; row < world.inventory.weapons.size(); ++row) {
    world.inventory.weapons[row].index = static_cast<std::uint8_t>(row + 1);
    world.inventory.shields[row].index = static_cast<std::uint8_t>(row + 1);
  }
  for (std::size_t row = 0; row < world.inventory.blades.size(); ++row) {
    world.inventory.blades[row].index = static_cast<std::uint8_t>(row + 1);
    world.inventory.grips[row].index = static_cast<std::uint16_t>(row + 1);
    world.inventory.armor[row].index = static_cast<std::uint8_t>(row + 1);
  }
  for (std::size_t row = 0; row < world.inventory.gems.size(); ++row) {
    world.inventory.gems[row].index = static_cast<std::int16_t>(row + 1);
  }
  for (std::size_t row = 0; row < world.inventory.misc.size(); ++row) {
    world.inventory.misc[row].index = static_cast<std::uint8_t>(row + 1);
  }
}
}  // namespace VS::Menu::Inventory::sorting_test
