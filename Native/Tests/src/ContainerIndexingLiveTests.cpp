#include <algorithm>
#include <array>
#include <iterator>

#include <catch2/catch_test_macros.hpp>

#include "Items/ContainerIndexingLiveFixture.hpp"

namespace {
namespace items = VS::Items;
using container_indexing_tests::LiveFixture;
}  // namespace

TEST_CASE("F02927 Live maps member arrays without flattening native pointers",
          "[container-indexing-live][F02927]") {
  LiveFixture fixture;
  const std::array<std::uint16_t*, 8> expected{
      fixture.owner.indices.weapons,
      fixture.owner.indices.blades,
      fixture.owner.indices.grips,
      fixture.owner.indices.shields,
      fixture.owner.indices.armor,
      fixture.owner.indices.gems,
      fixture.owner.indices.misc,
      fixture.owner.indices.misc + std::size(fixture.owner.indices.misc)};
  for (std::size_t category = 0; category < expected.size(); ++category) {
    CHECK(items::LiveContainerCategoryIndices(static_cast<int>(category),
                                              &fixture.owner) ==
          expected[category]);
  }
  Menu_Container_containerItemCapacities[0] = 1;
  CHECK(items::LiveContainerCategoryIndices(1, &fixture.owner) ==
        fixture.owner.indices.weapons + 1);
}

TEST_CASE("F02928 Live retains halfword lookup and empty token convention",
          "[container-indexing-live][F02928]") {
  LiveFixture fixture;
  auto* rows = fixture.owner.indices.weapons;
  rows[31] = 65535;
  CHECK(items::FindLiveContainerItemRow(0, 65534, rows) == 32);
  CHECK(items::FindLiveContainerItemRow(0, -1, rows) == 1);
  CHECK(items::FindLiveContainerItemRow(0, 65535, rows) == 0);
  Menu_Container_containerItemCapacities[0] = 0;
  CHECK(items::FindLiveContainerItemRow(0, 0, nullptr) == 0);
}

TEST_CASE("F02929 Live appends only the first free halfword",
          "[container-indexing-live][F02929]") {
  LiveFixture fixture;
  auto* rows = fixture.owner.indices.misc;
  rows[0] = 8;
  items::AppendLiveContainerItemIndex(6, 255, rows);
  CHECK(rows[0] == 8);
  CHECK(rows[1] == 256);
  CHECK(rows[2] == 0);
  Menu_Container_containerItemCapacities[6] = 0;
  CHECK_NOTHROW(items::AppendLiveContainerItemIndex(6, 0, nullptr));
}

TEST_CASE("F02930 Live uses production Typed and Commit for attached parts",
          "[container-indexing-live][F02930]") {
  LiveFixture fixture;
  auto& contents = fixture.owner.items;
  contents.weapons[0].blade = 64;
  contents.weapons[0].grip = 64;
  contents.weapons[0].gems[0] = 192;
  contents.weapons[0].gems[3] = 1;
  contents.blades[63].id = 255;
  contents.grips[63].id = 65535;
  contents.gems[191].id = 65535;
  contents.gems[0].id = 7;
  items::ClearLiveContainerItem(0, 0, &contents);
  CHECK(items::Live::Typed(contents.weapons[0]) ==
        items::InventoryWeapon{.index = 1});
  CHECK(items::Live::Typed(contents.blades[63]) ==
        items::InventoryBlade{.index = 64});
  CHECK(items::Live::Typed(contents.grips[63]) ==
        items::InventoryGrip{.index = 64});
  CHECK(items::Live::Typed(contents.gems[191]) ==
        items::InventoryGem{.index = 192});
  CHECK(contents.gems[0].id == 7);
}

TEST_CASE(
    "F02930 Live shield references are raw container slots not inventory flags",
    "[container-indexing-live][F02930]") {
  LiveFixture fixture;
  fixture.owner.items.shields[31].gems[0] = 128;
  fixture.owner.items.gems[127].id = 9;
  fixture.owner.items.gems[0].id = 8;
  items::ClearLiveContainerItem(3, 31, &fixture.owner.items);
  CHECK(items::Live::Typed(fixture.owner.items.shields[31]) ==
        items::InventoryShield{.index = 32});
  CHECK(items::Live::Typed(fixture.owner.items.gems[127]) ==
        items::InventoryGem{.index = 128});
  CHECK(fixture.owner.items.gems[0].id == 8);
  CHECK_NOTHROW(items::ClearLiveContainerItem(7, -1, nullptr));
}

TEST_CASE("F02931 Live looks up the explicit current owner not a cached global",
          "[container-indexing-live][F02931]") {
  LiveFixture fixture;
  fixture.owner.items.misc[255].id = 65535;
  fixture.replacement.items.misc[255].id = 32768;
  Menu_Container_containerData = &fixture.replacement;
  CHECK(items::LiveContainerItemId(6, 255, &fixture.owner.items) == 65535);
  CHECK(items::LiveContainerItemId(6, 255, &fixture.replacement.items) ==
        32768);
  CHECK(items::LiveContainerItemId(6, 0, &fixture.owner.items) == 0);
  CHECK(items::LiveContainerItemId(7, -1, nullptr) == 0);
}

TEST_CASE(
    "F02932 Live counts actual record occupancy and lazily skips unused owners",
    "[container-indexing-live][F02932]") {
  LiveFixture fixture;
  for (auto& item : fixture.owner.items.misc) item.id = 65535;
  CHECK(items::CountLiveContainerItems(6, &fixture.owner.items) == 256);
  fixture.owner.items.misc[0].id = 0;
  fixture.owner.items.misc[0].count = 100;
  CHECK(items::CountLiveContainerItems(6, &fixture.owner.items) == 255);
  Menu_Container_containerItemCapacities[6] = 0;
  CHECK(items::CountLiveContainerItems(6, nullptr) == 0);
  CHECK(items::CountLiveContainerItems(7, nullptr) == 0);
}

TEST_CASE("F02933 Live distinguishes full capacity from the first empty slot",
          "[container-indexing-live][F02933]") {
  LiveFixture fixture;
  for (auto& item : fixture.owner.items.misc) item.id = 1;
  CHECK(items::FindLiveEmptyContainerSlot(6, &fixture.owner.items) == 256);
  fixture.owner.items.misc[255].id = 0;
  CHECK(items::FindLiveEmptyContainerSlot(6, &fixture.owner.items) == 255);
  Menu_Container_containerItemCapacities[6] = 0;
  CHECK(items::FindLiveEmptyContainerSlot(6, nullptr) == 0);
  CHECK(items::FindLiveEmptyContainerSlot(7, nullptr) == 0);
}
