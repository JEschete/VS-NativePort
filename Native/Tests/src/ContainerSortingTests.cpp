#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "Menu/ContainerSortingWorld.hpp"

namespace {
namespace sorting = VS::Menu::container;
namespace fixture = sorting::sort_test;
namespace items = VS::Items;
constexpr auto kWeapon = items::ItemCategory::kWeapon;
constexpr auto kShield = items::ItemCategory::kShield;
constexpr auto kBlade = items::ItemCategory::kBlade;
constexpr auto kMisc = items::ItemCategory::kMisc;
constexpr auto kGem = items::ItemCategory::kGem;
constexpr int kBladeCategory = static_cast<int>(kBlade);
constexpr int kGemCategory = static_cast<int>(kGem);
constexpr int kMaterial = 1;
constexpr int kStrength = 8;
constexpr std::int32_t kLowest = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t kHighest = std::numeric_limits<std::int32_t>::max();
constexpr std::uint16_t kInvalidToken = 65535;
constexpr std::uint8_t kOpaque = 0xA5;
constexpr std::int8_t kNegative = -128;
constexpr std::int32_t kNegativeKey = -128;
constexpr std::uint16_t kThirdToken = 3;
constexpr int kRefusedCall = 5;
constexpr int kForeignCategory = 257;
}  // namespace

TEST_CASE("Container weapon sort retains ties duplicates and zero tail",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  fixture::SetOrder(world, kWeapon, {2, 0, 1, 2, kThirdToken});
  world.keys[1] = kNegativeKey;
  world.keys[2] = kHighest;
  world.keys[kThirdToken] = kHighest;
  const auto records = world.owners[0].items;
  sorting::SortWeapons(world, kMaterial);
  const auto result = world.Order(kWeapon);
  CHECK(std::array<std::uint16_t, 4>{result[0], result[1], result[2],
                                     result[3]} ==
        std::array<std::uint16_t, 4>{2, 2, 3, 1});
  CHECK(std::ranges::all_of(result.subspan(4, 28),
                            [](auto token) { return token == 0; }));
  CHECK(world.owners[0].items == records);
}

TEST_CASE("Container weapon source is captured but part owner is current",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  fixture::SetOrder(world, kWeapon, {1});
  world.owners[0].items.weapons[0].blade = 1;
  world.owners[1].items.blades[0].material = kOpaque;
  world.hook = [](auto& state, auto operation) {
    if (operation == fixture::Operation::kOrder) state.owner = 1;
  };
  sorting::SortWeapons(world, kMaterial);
  CHECK(world.owners[0].indices[0] == 1);
  CHECK(world.owners[1].indices[0] == 0);
  CHECK(world.owner == 1);
  CHECK(world.observed_material == kOpaque);
  CHECK(world.events.front().operation == fixture::Operation::kSource);
}

TEST_CASE("Container shield sort handles full duplicate and sentinel tables",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  auto order = world.Order(kShield).first(
      items::kContainerCapacities[static_cast<std::size_t>(kShield)]);
  std::ranges::fill(order, 2);
  world.keys[2] = kNegativeKey;
  sorting::SortShields(world, kMaterial);
  CHECK(std::ranges::all_of(order, [](auto token) { return token == 2; }));
  world.keys[2] = kLowest;
  sorting::SortShields(world, kMaterial);
  CHECK(std::ranges::all_of(order, [](auto token) { return token == 0; }));
  CHECK(world.owners[0].items.shields[1].index == 2);
}

TEST_CASE("Container shield sorting reads first and last actual records",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  constexpr auto kLast = items::kContainerCapacities[3];
  fixture::SetOrder(world, kShield, {1, kLast, 1});
  world.keys[kLast] = kHighest;
  world.keys[1] = kNegativeKey;
  sorting::SortShields(world, kStrength);
  const auto order = world.Order(kShield);
  CHECK(order[0] == kLast);
  CHECK(order[1] == 1);
  CHECK(order[2] == 1);
}

TEST_CASE("Container misc supports all 256 tokens and byte provenance wrap",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  constexpr auto kCount = items::kContainerCapacities[6];
  auto order = world.Order(kMisc).first(kCount);
  for (std::uint16_t row = 0; row < kCount; ++row) order[row] = row + 1;
  world.keys.fill(kNegative);
  world.keys[0] = kHighest;
  sorting::SortMisc(world, kMaterial);
  CHECK(order[0] == 1);
  CHECK(order[1] == 2);
  CHECK(order.back() == kCount);
  CHECK(world.owners[0].items.misc.back().index == kCount - 1);
}

TEST_CASE("Container misc clears selected rows before a later failed key",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  fixture::SetOrder(world, kMisc, {1, 2});
  world.keys[0] = kHighest;
  world.keys[1] = kNegativeKey;
  int calls = 0;
  world.hook = [&](auto&, auto operation) {
    if (operation == fixture::Operation::kMisc && ++calls == kRefusedCall) {
      throw std::runtime_error("next maximum refused");
    }
  };
  CHECK_THROWS_WITH(sorting::SortMisc(world, kMaterial),
                    "next maximum refused");
  world.hook = {};
  const auto order = world.Order(kMisc);
  CHECK(order[0] == 0);
  CHECK(order[1] == 2);
}

TEST_CASE("Container projection preserves fields absent from a blade record",
          "[container-sorting]") {
  items::ContainerStorage records;
  items::EquipmentPart target{};
  target.types.fill(kNegative);
  target.gem_slots = kOpaque;
  target.gem_effects = kOpaque;
  items::InventoryBlade blade{};
  blade.id = kOpaque;
  blade.sub_id = kOpaque;
  blade.strength = kNegative;
  blade.material = kInvalidToken;
  const items::ContainerRecord source = blade;
  sorting::CopyEquipmentStats(target, kBladeCategory, &source, records);
  CHECK(target.id == kOpaque);
  CHECK(target.sub_id == static_cast<std::int8_t>(kOpaque));
  CHECK(target.strength == kNegative);
  CHECK(target.material == std::numeric_limits<std::uint8_t>::max());
  CHECK(target.types.front() == kNegative);
  CHECK(target.gem_slots == kOpaque);
  CHECK(target.gem_effects == kOpaque);
}

TEST_CASE(
    "Container projection routes grip armor gem and lazy unsupported types",
    "[container-sorting]") {
  items::ContainerStorage records;
  items::EquipmentPart target{};
  target.category = kOpaque;
  items::InventoryGem gem{};
  gem.strength = kNegative;
  gem.gem_effects = kOpaque;
  const items::ContainerRecord source = gem;
  sorting::CopyEquipmentStats(target, kGemCategory, &source, records);
  CHECK(target.strength == kNegative);
  CHECK(target.gem_effects == kOpaque);
  CHECK(target.category == kOpaque);
  const auto before = target;
  sorting::CopyEquipmentStats(target, kForeignCategory, nullptr, records);
  CHECK(target == before);
  CHECK_THROWS(
      sorting::CopyEquipmentStats(target, kBladeCategory, &source, records));
  CHECK_THROWS(
      sorting::CopyEquipmentStats(target, kGemCategory, nullptr, records));
}

TEST_CASE("Container equipment sort captures capacity before order callbacks",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  world.owners[0].capacities[1] = 2;
  fixture::SetOrder(world, kBlade, {1, 2, kThirdToken});
  world.keys[1] = kNegativeKey;
  world.keys[2] = kHighest;
  world.hook = [](auto& state, auto operation) {
    if (operation == fixture::Operation::kOrder) {
      state.owners[0].capacities[1] = 1;
    }
  };
  sorting::SortEquipment(world, kBladeCategory, kStrength);
  world.hook = {};
  const auto order = world.Order(kBlade);
  CHECK(order[0] == 2);
  CHECK(order[1] == 1);
  CHECK(order[2] == 3);
  CHECK(world.events.front().operation == fixture::Operation::kCapacity);
}

TEST_CASE("Container equipment sort reuses current owner at each projection",
          "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  fixture::SetOrder(world, kGem, {1, 2});
  world.keys[1] = kNegativeKey;
  world.keys[2] = kHighest;
  world.hook = [](auto& state, auto operation) {
    if (operation == fixture::Operation::kEquipment) state.owner = 1;
  };
  sorting::SortEquipment(world, kGemCategory, kStrength);
  CHECK(world.owners[0].indices[256] == 2);
  CHECK(world.owners[0].indices[257] == 1);
  CHECK(world.owners[1].indices[256] == 0);
  CHECK(world.owner == 1);
}

TEST_CASE(
    "Container sorting keeps empty orders lazy and checks real read extents",
    "[container-sorting]") {
  fixture::World world;
  fixture::SetRecords(world);
  sorting::SortWeapons(world, std::numeric_limits<int>::min());
  sorting::SortShields(world, std::numeric_limits<int>::min());
  sorting::SortMisc(world, std::numeric_limits<int>::min());
  world.owners[0].capacities[1] = 0;
  world.order_extent = 0;
  CHECK_NOTHROW(sorting::SortEquipment(world, kBladeCategory, kMaterial));
  CHECK_THROWS(sorting::SortWeapons(world, kMaterial));
  world.order_extent.reset();
  fixture::SetOrder(world, kWeapon, {kInvalidToken});
  CHECK_THROWS(sorting::SortWeapons(world, kMaterial));
  CHECK_THROWS(sorting::SortEquipment(world, -1, kMaterial));
}
