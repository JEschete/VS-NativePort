#include "Items/ContainerTransfersLiveFixture.hpp"

#include <array>

#include "Items/ContainerIndexing.hpp"
#include "Items/InventoryLive.hpp"

namespace container_transfers_tests {
LiveFixture::LiveFixture() {
  saved_capacities = VS::Menu::container::containerItemCapacities;
  VS::Menu::container::containerItemCapacities = items::kContainerCapacities;
  saved_indices = VS::Menu::Inventory::categoryOrders;
  const std::array<unsigned char*, items::kItemCategoryCount - 1> indices{
      order.weapons.data(), order.blades.data(), order.grips.data(),
      order.shields.data(), order.armor.data(),  order.gems.data(),
      order.misc.data()};
  VS::Menu::Inventory::categoryOrders = indices;
  VS::Menu::Framework::inventoryStorage = storage.get();
  VS::Menu::container::containerData = &storage->container;
  VS::Menu::container::otherStorage = &storage->container;
  VS::Items::inventory = items::InventoryContents{};
}

LiveFixture::~LiveFixture() {
  VS::Menu::container::containerItemCapacities = saved_capacities;
  VS::Menu::Inventory::categoryOrders = saved_indices;
  VS::Items::inventory = saved_inventory;
  VS::Menu::container::transferredMiscCount = saved_count;
  VS::Menu::Framework::inventoryStorage = saved_storage;
  VS::Menu::container::containerData = saved_owner;
  VS::Menu::container::otherStorage = saved_other;
}
}  // namespace container_transfers_tests
