#include "Items/ContainerTransfersLiveFixture.hpp"

#include <algorithm>
#include <array>

#include "Items/ContainerIndexing.hpp"

namespace container_transfers_tests {
LiveFixture::LiveFixture() {
  std::copy_n(Menu_Container_containerItemCapacities, saved_capacities.size(),
              saved_capacities.begin());
  std::copy(items::kContainerCapacities.begin(),
            items::kContainerCapacities.end(),
            Menu_Container_containerItemCapacities);
  std::copy_n(Menu_Inventory_inventoryIndices, saved_indices.size(),
              saved_indices.begin());
  const std::array<unsigned char*, items::kItemCategoryCount - 1> indices{
      order.weapons, order.blades, order.grips, order.shields,
      order.armor,   order.gems,   order.misc};
  std::copy(indices.begin(), indices.end(), Menu_Inventory_inventoryIndices);
  Menu_Framework_unusedInventoryStorage = storage.get();
  Menu_Container_containerData = &storage->container;
  Menu_Container_otherStorage = &storage->container;
  items::Live::Commit(Audio_Seq_unusedInventory, items::InventoryContents{});
}

LiveFixture::~LiveFixture() {
  std::copy(saved_capacities.begin(), saved_capacities.end(),
            Menu_Container_containerItemCapacities);
  std::copy(saved_indices.begin(), saved_indices.end(),
            Menu_Inventory_inventoryIndices);
  items::Live::Commit(Audio_Seq_unusedInventory, saved_inventory);
  Menu_Container_transferredMiscCount = saved_count;
  Menu_Framework_unusedInventoryStorage = saved_storage;
  Menu_Container_containerData = saved_owner;
  Menu_Container_otherStorage = saved_other;
}
}  // namespace container_transfers_tests
