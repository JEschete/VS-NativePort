#pragma once

#include <algorithm>
#include <array>
#include <memory>

#include "Items/ContainerTransfersLive.hpp"
#include "Items/InventoryLive.hpp"
#include "Items/InventoryLiveState.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Inventory/ItemList.h"

namespace container_transfers_tests {
namespace items = VS::Items;

struct LiveFixture {
  std::array<std::uint16_t, items::kItemCategoryCount> saved_capacities{};
  std::array<unsigned char*, items::kItemCategoryCount - 1> saved_indices{};
  items::InventoryContents saved_inventory = VS::Items::inventory;
  std::uint16_t saved_count = Menu_Container_transferredMiscCount;
  Menu_Container_InventoryStorage* saved_storage =
      VS::Menu::Framework::inventoryStorage;
  VS::Items::Container* saved_owner = Menu_Container_containerData;
  VS::Items::Container* saved_other = Menu_Container_otherStorage;
  std::unique_ptr<Menu_Container_InventoryStorage> storage =
      std::make_unique<Menu_Container_InventoryStorage>();
  std::unique_ptr<Menu_Container_InventoryStorage> replacement =
      std::make_unique<Menu_Container_InventoryStorage>();
  VS::Items::InventoryIndices order{};

  LiveFixture();
  ~LiveFixture();
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  LiveFixture(LiveFixture&&) = delete;
  LiveFixture& operator=(LiveFixture&&) = delete;
};
}  // namespace container_transfers_tests
