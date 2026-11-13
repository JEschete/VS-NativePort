#pragma once

#include <algorithm>
#include <array>

#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ItemRecordsLive.hpp"

extern "C" {
#include "Menu/Container/ItemView.h"
#include "Menu/Container/Storage.h"
}

namespace container_indexing_tests {
namespace items = VS::Items;

struct LiveFixture {
  std::array<std::uint16_t, items::kItemCategoryCount> saved_capacities{};
  std::uint16_t saved_misc_count = Menu_Container_transferredMiscCount;
  Items_Container* saved_owner = Menu_Container_containerData;
  Items_Container owner{};
  Items_Container replacement{};

  LiveFixture() {
    std::copy_n(Menu_Container_containerItemCapacities, saved_capacities.size(),
                saved_capacities.begin());
    std::copy(items::kContainerCapacities.begin(),
              items::kContainerCapacities.end(),
              Menu_Container_containerItemCapacities);
  }
  ~LiveFixture() {
    std::copy(saved_capacities.begin(), saved_capacities.end(),
              Menu_Container_containerItemCapacities);
    Menu_Container_transferredMiscCount = saved_misc_count;
    Menu_Container_containerData = saved_owner;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  LiveFixture(LiveFixture&&) = delete;
  LiveFixture& operator=(LiveFixture&&) = delete;
};
}  // namespace container_indexing_tests
