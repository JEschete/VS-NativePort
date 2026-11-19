#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Items/InventoryLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Inventory/ItemList.hpp"

namespace VS::Menu::Inventory::sorting_test {
inline constexpr auto kOrderTables = Items::kItemCategoryCount - 1;

inline auto LiveOrders() {
  auto& order = VS::Items::inventoryIndices;
  return std::array<std::span<std::uint8_t>, kOrderTables>{
      order.weapons, order.blades, order.grips, order.shields,
      order.armor,   order.gems,   order.misc};
}

struct LiveFixture {
  VS::Items::InventoryContents saved_inventory = VS::Items::inventory;
  VS::Items::InventoryIndices saved_indices = VS::Items::inventoryIndices;
  std::array<std::uint8_t*, kOrderTables> saved_routes{};

  LiveFixture() {
    saved_routes = VS::Menu::Inventory::categoryOrders;
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    const auto tables = LiveOrders();
    for (std::size_t row = 0; row < tables.size(); ++row) {
      VS::Menu::Inventory::categoryOrders[row] = tables[row].data();
    }
  }
  ~LiveFixture() {
    VS::Items::inventory = saved_inventory;
    VS::Items::inventoryIndices = saved_indices;
    VS::Menu::Inventory::categoryOrders = saved_routes;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  LiveFixture(LiveFixture&&) = delete;
  LiveFixture& operator=(LiveFixture&&) = delete;
};
}  // namespace VS::Menu::Inventory::sorting_test
