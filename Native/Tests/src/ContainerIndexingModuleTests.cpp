#include <string_view>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Items/ContainerIndexingLiveFixture.hpp"

extern "C" {
#include "Boot/OverlayReset.h"
}

namespace {
namespace items = VS::Items;
using container_indexing_tests::LiveFixture;

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  const auto* resolved = VS::Content::ResolveModuleAddress(
      {Content_moduleSymbols, Content_moduleSymbolCount}, "MENU/MENUD.PRG",
      address);
  return resolved == reinterpret_cast<const void*>(entry);
}
}  // namespace

TEST_CASE("F02927 binds its actual MENUD pointer two-argument entry",
          "[container-indexing-module][F02927]") {
  static_assert(std::is_same_v<decltype(&items::LiveContainerCategoryIndices),
                               std::uint16_t* (*)(int, Items_Container*)>);
  CHECK(IsBound(0x80102C94, &items::LiveContainerCategoryIndices));
}

TEST_CASE("F02928 binds its actual MENUD int three-argument entry",
          "[container-indexing-module][F02928]") {
  static_assert(std::is_same_v<decltype(&items::FindLiveContainerItemRow),
                               int (*)(int, int, std::uint16_t*)>);
  CHECK(IsBound(0x80102CD0, &items::FindLiveContainerItemRow));
}

TEST_CASE("F02929 binds its actual MENUD void three-argument entry",
          "[container-indexing-module][F02929]") {
  static_assert(std::is_same_v<decltype(&items::AppendLiveContainerItemIndex),
                               void (*)(int, int, std::uint16_t*)>);
  CHECK(IsBound(0x80102D28, &items::AppendLiveContainerItemIndex));
}

TEST_CASE("F02930 binds its actual MENUD void record-clear entry",
          "[container-indexing-module][F02930]") {
  static_assert(std::is_same_v<decltype(&items::ClearLiveContainerItem),
                               void (*)(int, int, Items_ContainerData*)>);
  CHECK(IsBound(0x80102D80, &items::ClearLiveContainerItem));
}

TEST_CASE("F02931 binds its actual MENUD occupancy entry",
          "[container-indexing-module][F02931]") {
  static_assert(std::is_same_v<decltype(&items::LiveContainerItemId),
                               int (*)(int, int, Items_ContainerData*)>);
  CHECK(IsBound(0x80102F88, &items::LiveContainerItemId));
}

TEST_CASE("F02932 binds its actual MENUD int two-argument count entry",
          "[container-indexing-module][F02932]") {
  static_assert(std::is_same_v<decltype(&items::CountLiveContainerItems),
                               int (*)(int, Items_ContainerData*)>);
  CHECK(IsBound(0x80103070, &items::CountLiveContainerItems));
}

TEST_CASE("F02933 binds its actual MENUD int two-argument empty-slot entry",
          "[container-indexing-module][F02933]") {
  static_assert(std::is_same_v<decltype(&items::FindLiveEmptyContainerSlot),
                               int (*)(int, Items_ContainerData*)>);
  CHECK(IsBound(0x80103110, &items::FindLiveEmptyContainerSlot));
}

TEST_CASE(
    "Container policies preserve ordinary MENUD table reload and retained BSS",
    "[container-indexing-module][F02927][F02930][F02932][F02933]") {
  LiveFixture fixture;
  Menu_Container_containerItemCapacities[0] = 0;
  Menu_Container_containerItemCapacities[6] = 0;
  Menu_Container_transferredMiscCount = 91;
  Menu_Container_containerData = &fixture.owner;
  fixture.owner.items.misc[0].id = 65535;
  fixture.owner.indices.misc[0] = 256;
  CHECK(items::CountLiveContainerItems(6, &fixture.owner.items) == 0);
  Boot_ResetMenudStatics();
  CHECK(items::CountLiveContainerItems(6, &fixture.owner.items) == 1);
  CHECK(items::FindLiveEmptyContainerSlot(6, &fixture.owner.items) == 1);
  CHECK(Menu_Container_transferredMiscCount == 91);
  CHECK(Menu_Container_containerData == &fixture.owner);
  CHECK(fixture.owner.indices.misc[0] == 256);
  CHECK(items::LiveContainerCategoryIndices(6, &fixture.owner) ==
        fixture.owner.indices.misc);
}
