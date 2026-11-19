#include "Items/ContainerTransfersLiveFixture.hpp"

#include <algorithm>
#include <array>

#include "Items/ContainerIndexing.hpp"
#include "Items/InventoryLive.hpp"

namespace container_transfers_tests {
LiveFixture::LiveFixture() {
  std::copy_n(Menu_Container_containerItemCapacities, saved_capacities.size(),
              saved_capacities.begin());
  std::copy(items::kContainerCapacities.begin(),
            items::kContainerCapacities.end(),
            Menu_Container_containerItemCapacities);
  saved_indices = VS::Menu::Inventory::categoryOrders;
  const std::array<unsigned char*, items::kItemCategoryCount - 1> indices{
      order.weapons.data(), order.blades.data(), order.grips.data(),
      order.shields.data(), order.armor.data(),  order.gems.data(),
      order.misc.data()};
  VS::Menu::Inventory::categoryOrders = indices;
  VS::Menu::Framework::inventoryStorage = storage.get();
  Menu_Container_containerData = &storage->container;
  Menu_Container_otherStorage = &storage->container;
  VS::Items::inventory = items::InventoryContents{};
}

LiveFixture::~LiveFixture() {
  std::copy(saved_capacities.begin(), saved_capacities.end(),
            Menu_Container_containerItemCapacities);
  VS::Menu::Inventory::categoryOrders = saved_indices;
  VS::Items::inventory = saved_inventory;
  Menu_Container_transferredMiscCount = saved_count;
  VS::Menu::Framework::inventoryStorage = saved_storage;
  Menu_Container_containerData = saved_owner;
  Menu_Container_otherStorage = saved_other;
}
}  // namespace container_transfers_tests
