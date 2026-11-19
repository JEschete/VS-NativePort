#pragma once

#include <array>

#include "Items/ContainerIndexing.hpp"
#include "Items/ContainerIndexingLive.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Container/ItemView.hpp"
#include "Menu/Container/Storage.hpp"

namespace container_indexing_tests {
namespace items = VS::Items;

struct LiveFixture {
  std::array<std::uint16_t, items::kItemCategoryCount> saved_capacities{};
  std::uint16_t saved_misc_count = VS::Menu::container::transferredMiscCount;
  VS::Items::Container* saved_owner = VS::Menu::container::containerData;
  VS::Items::Container owner{};
  VS::Items::Container replacement{};

  LiveFixture() {
    saved_capacities = VS::Menu::container::containerItemCapacities;
    VS::Menu::container::containerItemCapacities = items::kContainerCapacities;
  }
  ~LiveFixture() {
    VS::Menu::container::containerItemCapacities = saved_capacities;
    VS::Menu::container::transferredMiscCount = saved_misc_count;
    VS::Menu::container::containerData = saved_owner;
  }
  LiveFixture(const LiveFixture&) = delete;
  LiveFixture& operator=(const LiveFixture&) = delete;
  LiveFixture(LiveFixture&&) = delete;
  LiveFixture& operator=(LiveFixture&&) = delete;
};
}  // namespace container_indexing_tests
