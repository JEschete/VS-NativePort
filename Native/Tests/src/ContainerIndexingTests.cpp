#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "Items/ContainerIndexing.hpp"

namespace {
namespace items = VS::Items;
using Category = items::ItemCategory;
using Storage = items::ContainerStorage;
constexpr auto kInvalidCategory = static_cast<Category>(255);
}  // namespace

TEST_CASE("F02927 category starts retain native display ownership",
          "[container-indexing][F02927]") {
  Storage storage;
  const std::array<std::size_t, 8> expected{0, 32, 96, 160, 192, 256, 448, 704};
  for (std::size_t category = 0; category < expected.size(); ++category) {
    CHECK(items::ContainerCategoryIndices(static_cast<Category>(category),
                                          storage) == expected[category]);
  }
  storage.capacities[0] = 0;
  storage.capacities[1] = 3;
  CHECK(items::ContainerCategoryIndices(Category::kGrip, storage) == 3);
}

TEST_CASE("F02927 validates only the used prefix",
          "[container-indexing][F02927]") {
  Storage storage;
  storage.capacities.fill(65535);
  CHECK(items::ContainerCategoryIndices(Category::kWeapon, storage) == 0);
  CHECK_THROWS_AS(items::ContainerCategoryIndices(Category::kBlade, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::ContainerCategoryIndices(kInvalidCategory, storage),
                  std::out_of_range);
}

TEST_CASE("F02928 row results are one based first matches",
          "[container-indexing][F02928]") {
  Storage storage;
  std::array<std::uint16_t, 32> rows{};
  rows[0] = 32;
  rows[3] = 7;
  rows[31] = 7;
  CHECK(items::FindContainerItemRow(Category::kWeapon, 31, rows, storage) == 1);
  CHECK(items::FindContainerItemRow(Category::kWeapon, 6, rows, storage) == 4);
  CHECK(items::FindContainerItemRow(Category::kWeapon, 5, rows, storage) == 0);
  CHECK(items::FindContainerItemRow(Category::kWeapon, -1, rows, storage) == 2);
}

TEST_CASE("F02928 compares a word token against unsigned halfwords",
          "[container-indexing][F02928]") {
  Storage storage;
  std::array<std::uint16_t, 32> rows{};
  rows.fill(65535);
  CHECK(items::FindContainerItemRow(Category::kWeapon, 65534, rows, storage) ==
        1);
  CHECK(items::FindContainerItemRow(Category::kWeapon, 65535, rows, storage) ==
        0);
  CHECK(items::FindContainerItemRow(Category::kWeapon,
                                    std::numeric_limits<std::int32_t>::max(),
                                    rows, storage) == 0);
  CHECK(items::FindContainerItemRow(Category::kWeapon,
                                    std::numeric_limits<std::int32_t>::min(),
                                    rows, storage) == 0);
}

TEST_CASE("F02928 order checks occur only when an element is read",
          "[container-indexing][F02928]") {
  Storage storage;
  const std::array<std::uint16_t, 1> rows{9};
  CHECK(items::FindContainerItemRow(Category::kWeapon, 8, rows, storage) == 1);
  CHECK_THROWS_AS(
      items::FindContainerItemRow(Category::kWeapon, 7, rows, storage),
      std::out_of_range);
  storage.capacities[0] = 0;
  CHECK(items::FindContainerItemRow(Category::kWeapon, 7, {}, storage) == 0);
  CHECK_THROWS_AS(items::FindContainerItemRow(kInvalidCategory, 0, {}, storage),
                  std::out_of_range);
}

TEST_CASE("F02929 appends at first vacancy and leaves full order unchanged",
          "[container-indexing][F02929]") {
  Storage storage;
  std::array<std::uint16_t, 32> rows{};
  rows.fill(8);
  rows[2] = 0;
  rows[31] = 0;
  items::AppendContainerItemIndex(Category::kWeapon, 255, rows, storage);
  CHECK(rows[2] == 256);
  CHECK(rows[31] == 0);
  items::AppendContainerItemIndex(Category::kWeapon, 256, rows, storage);
  CHECK(rows[31] == 257);
  const auto full = rows;
  items::AppendContainerItemIndex(Category::kWeapon, 0, rows, storage);
  CHECK(rows == full);
}

TEST_CASE("F02929 preserves low halfword stores including zero tokens",
          "[container-indexing][F02929]") {
  Storage storage;
  std::array<std::uint16_t, 1> rows{};
  items::AppendContainerItemIndex(Category::kWeapon, -1, rows, storage);
  CHECK(rows[0] == 0);
  items::AppendContainerItemIndex(Category::kWeapon, 65535, rows, storage);
  CHECK(rows[0] == 0);
  items::AppendContainerItemIndex(Category::kWeapon,
                                  std::numeric_limits<std::int32_t>::max(),
                                  rows, storage);
  CHECK(rows[0] == 0);
  items::AppendContainerItemIndex(Category::kWeapon, 65534, rows, storage);
  CHECK(rows[0] == 65535);
}

TEST_CASE("F02929 empty capacities and short spans do not invent entries",
          "[container-indexing][F02929]") {
  Storage storage;
  CHECK_THROWS_AS(
      items::AppendContainerItemIndex(Category::kBlade, 0, {}, storage),
      std::out_of_range);
  storage.capacities[1] = 0;
  CHECK_NOTHROW(
      items::AppendContainerItemIndex(Category::kBlade, 0, {}, storage));
  CHECK_THROWS_AS(
      items::AppendContainerItemIndex(kInvalidCategory, 0, {}, storage),
      std::out_of_range);
}

TEST_CASE("F02930 weapon parts clear in their actual tables",
          "[container-indexing][F02930]") {
  Storage storage;
  storage.items.weapons[31] = {
      .index = 8, .blade = 64, .grip = 64, .gems = {192, 192, 1, 2}};
  storage.items.blades[63].id = 255;
  storage.items.grips[63].id = 65535;
  storage.items.gems[191].id = 65535;
  storage.items.gems[0].id = 9;
  storage.items.gems[1].id = 7;
  items::ClearContainerItem(Category::kWeapon, 31, storage);
  CHECK(storage.items.weapons[31] == items::InventoryWeapon{.index = 32});
  CHECK(storage.items.blades[63] == items::InventoryBlade{.index = 64});
  CHECK(storage.items.grips[63] == items::InventoryGrip{.index = 64});
  CHECK(storage.items.gems[191] == items::InventoryGem{.index = 192});
  CHECK(storage.items.gems[0] == items::InventoryGem{.index = 1});
  CHECK(storage.items.gems[1].id == 7);
}

TEST_CASE("F02930 shield does not mask container gem references",
          "[container-indexing][F02930]") {
  Storage storage;
  storage.items.shields[0].base.id = 8;
  storage.items.shields[0].gems = {128, 192, 0, 1};
  storage.items.gems[127].id = 7;
  storage.items.gems[191].id = 7;
  storage.items.gems[0].id = 9;
  items::ClearContainerItem(Category::kShield, 0, storage);
  CHECK(storage.items.shields[0] == items::InventoryShield{.index = 1});
  CHECK(storage.items.gems[127] == items::InventoryGem{.index = 128});
  CHECK(storage.items.gems[191] == items::InventoryGem{.index = 192});
  CHECK(storage.items.gems[0].id == 9);
}

TEST_CASE("F02930 every simple category preserves its index width",
          "[container-indexing][F02930]") {
  Storage storage;
  storage.items.armor[63].id = 255;
  storage.items.misc[255] = {.id = 65535, .count = 100, .index = 255};
  items::ClearContainerItem(Category::kArmor, 63, storage);
  items::ClearContainerItem(Category::kMisc, 255, storage);
  CHECK(storage.items.armor[63] == items::InventoryArmor{.index = 64});
  CHECK(storage.items.misc[255] == items::InventoryMisc{});
  CHECK_NOTHROW(items::ClearContainerItem(Category::kManage, -1, storage));
  CHECK_THROWS_AS(items::ClearContainerItem(Category::kMisc, 256, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::ClearContainerItem(Category::kArmor, -1, storage),
                  std::out_of_range);
}

TEST_CASE("F02930 missing mandatory part is invalid not an empty part record",
          "[container-indexing][F02930]") {
  Storage storage;
  storage.items.weapons[0].grip = 1;
  storage.items.shields[31].base.id = 8;
  const auto before = storage.items;
  CHECK_THROWS_AS(items::ClearContainerItem(Category::kWeapon, 0, storage),
                  std::out_of_range);
  CHECK(storage.items == before);
  storage.items.weapons[0].blade = 1;
  storage.items.weapons[0].grip = 0;
  storage.items.blades[0].id = 5;
  CHECK_THROWS_AS(items::ClearContainerItem(Category::kWeapon, 0, storage),
                  std::out_of_range);
  CHECK(storage.items.blades[0] == items::InventoryBlade{.index = 1});
  CHECK(storage.items.weapons[0].blade == 1);
}

TEST_CASE("F02930 rejects out of range attachments at their first use",
          "[container-indexing][F02930]") {
  Storage storage;
  storage.items.weapons[0] = {.blade = 65, .grip = 1};
  CHECK_THROWS_AS(items::ClearContainerItem(Category::kWeapon, 0, storage),
                  std::out_of_range);
  storage.items.shields[0].gems[0] = 193;
  CHECK_THROWS_AS(items::ClearContainerItem(Category::kShield, 0, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::ClearContainerItem(kInvalidCategory, 0, storage),
                  std::out_of_range);
}

TEST_CASE(
    "F02931 occupancy identifiers preserve original unsigned field widths",
    "[container-indexing][F02931]") {
  Storage storage;
  storage.items.weapons[31].blade = 255;
  storage.items.blades[63].id = 255;
  storage.items.grips[63].id = 65535;
  storage.items.shields[31].base.id = 255;
  storage.items.armor[63].id = 255;
  storage.items.gems[191].id = 65535;
  storage.items.misc[255].id = 65535;
  CHECK(items::ContainerItemId(Category::kWeapon, 31, storage) == 255);
  CHECK(items::ContainerItemId(Category::kBlade, 63, storage) == 255);
  CHECK(items::ContainerItemId(Category::kGrip, 63, storage) == 65535);
  CHECK(items::ContainerItemId(Category::kShield, 31, storage) == 255);
  CHECK(items::ContainerItemId(Category::kArmor, 63, storage) == 255);
  CHECK(items::ContainerItemId(Category::kGem, 191, storage) == 65535);
  CHECK(items::ContainerItemId(Category::kMisc, 255, storage) == 65535);
}

TEST_CASE("F02931 empty records differ from refused lookups",
          "[container-indexing][F02931]") {
  Storage storage;
  storage.items.misc[0] = {.id = 0, .count = 100, .index = 1};
  CHECK(items::ContainerItemId(Category::kMisc, 0, storage) == 0);
  CHECK(items::ContainerItemId(Category::kManage, -1, storage) == 0);
  CHECK_THROWS_AS(items::ContainerItemId(Category::kMisc, 256, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::ContainerItemId(Category::kWeapon, -1, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::ContainerItemId(kInvalidCategory, 0, storage),
                  std::out_of_range);
}

TEST_CASE("F02932 counts ids not stack sizes or preserved indices",
          "[container-indexing][F02932]") {
  Storage storage;
  storage.items.misc.fill({.id = 65535, .count = 0, .index = 0});
  CHECK(items::CountContainerItems(Category::kMisc, storage) == 256);
  storage.items.misc[0] = {.id = 0, .count = 100, .index = 1};
  CHECK(items::CountContainerItems(Category::kMisc, storage) == 255);
  storage.capacities[6] = 1;
  CHECK(items::CountContainerItems(Category::kMisc, storage) == 0);
  CHECK(items::CountContainerItems(Category::kManage, storage) == 0);
}

TEST_CASE("F02932 respects table limits without clamping",
          "[container-indexing][F02932]") {
  Storage storage;
  storage.capacities[0] = 0;
  CHECK(items::CountContainerItems(Category::kWeapon, storage) == 0);
  storage.capacities[0] = 33;
  CHECK_THROWS_AS(items::CountContainerItems(Category::kWeapon, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::CountContainerItems(kInvalidCategory, storage),
                  std::out_of_range);
}

TEST_CASE("F02933 empty and full sentinels are zero based",
          "[container-indexing][F02933]") {
  Storage storage;
  storage.items.gems.fill({.id = 65535});
  CHECK(items::FindEmptyContainerSlot(Category::kGem, storage) == 192);
  storage.items.gems[191].id = 0;
  CHECK(items::FindEmptyContainerSlot(Category::kGem, storage) == 191);
  storage.items.gems[0].id = 0;
  storage.items.gems[0].index = 1;
  CHECK(items::FindEmptyContainerSlot(Category::kGem, storage) == 0);
  CHECK(items::FindEmptyContainerSlot(Category::kManage, storage) == 0);
}

TEST_CASE("F02933 capacity boundaries do not become fabricated slots",
          "[container-indexing][F02933]") {
  Storage storage;
  storage.capacities[6] = 0;
  CHECK(items::FindEmptyContainerSlot(Category::kMisc, storage) == 0);
  storage.capacities[6] = 257;
  CHECK_THROWS_AS(items::FindEmptyContainerSlot(Category::kMisc, storage),
                  std::out_of_range);
  CHECK_THROWS_AS(items::FindEmptyContainerSlot(kInvalidCategory, storage),
                  std::out_of_range);
}
