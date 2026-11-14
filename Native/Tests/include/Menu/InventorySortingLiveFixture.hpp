#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Items/ItemRecordsLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Inventory/ItemList.h"
}

namespace VS::Menu::Inventory::sorting_test {
inline constexpr auto kOrderTables = Items::kItemCategoryCount - 1;

inline auto LiveOrders() {
  auto& order = Audio_Seq_unusedInventoryIndices;
  return std::array<std::span<std::uint8_t>, kOrderTables>{
      order.weapons, order.blades, order.grips, order.shields,
      order.armor,   order.gems,   order.misc};
}

struct LiveFixture {
  Items_Inventory saved_inventory = Audio_Seq_unusedInventory;
  Items_InventoryIndices saved_indices = Audio_Seq_unusedInventoryIndices;
  std::array<std::uint8_t*, kOrderTables> saved_routes{};

  LiveFixture() {
    std::copy_n(Menu_Inventory_inventoryIndices, saved_routes.size(),
                saved_routes.begin());
    Audio_Seq_unusedInventory = {};
    Audio_Seq_unusedInventoryIndices = {};
    const auto tables = LiveOrders();
    for (std::size_t row = 0; row < tables.size(); ++row) {
      Menu_Inventory_inventoryIndices[row] = tables[row].data();
    }
  }
  ~LiveFixture() {
    Audio_Seq_unusedInventory = saved_inventory;
    Audio_Seq_unusedInventoryIndices = saved_indices;
    std::copy(saved_routes.begin(), saved_routes.end(),
              Menu_Inventory_inventoryIndices);
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  LiveFixture(LiveFixture&&) = delete;
  LiveFixture& operator=(LiveFixture&&) = delete;
};
}  // namespace VS::Menu::Inventory::sorting_test
