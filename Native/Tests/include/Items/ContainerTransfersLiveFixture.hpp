#pragma once

#include <algorithm>
#include <array>
#include <memory>

#include "Items/ContainerTransfersLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/InventoryLiveState.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/ItemList.hpp"

namespace container_transfers_tests {
namespace items = VS::Items;

struct LiveFixture {
  std::array<std::uint16_t, items::kItemCategoryCount> saved_capacities{};
  std::array<unsigned char*, items::kItemCategoryCount - 1> saved_indices{};
  items::InventoryContents saved_inventory = VS::Items::inventory;
  std::uint16_t saved_count = VS::Menu::container::transferredMiscCount;
  VS::Menu::container::InventoryStorage* saved_storage =
      VS::Menu::Framework::inventoryStorage;
  VS::Items::Container* saved_owner = VS::Menu::container::containerData;
  VS::Items::Container* saved_other = VS::Menu::container::otherStorage;
  std::unique_ptr<VS::Menu::container::InventoryStorage> storage =
      std::make_unique<VS::Menu::container::InventoryStorage>();
  std::unique_ptr<VS::Menu::container::InventoryStorage> replacement =
      std::make_unique<VS::Menu::container::InventoryStorage>();
  VS::Items::InventoryIndices order{};

  LiveFixture();
  ~LiveFixture();
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  LiveFixture(LiveFixture&&) = delete;
  LiveFixture& operator=(LiveFixture&&) = delete;
};
}  // namespace container_transfers_tests
