#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include "Menu/InventorySortingWorld.hpp"

namespace {
namespace sorting = VS::Menu::Inventory;
namespace fixture = sorting::sorting_test;
namespace items = VS::Items;
constexpr auto kWeapon = items::ItemCategory::kWeapon;
constexpr auto kBlade = items::ItemCategory::kBlade;
constexpr auto kMisc = items::ItemCategory::kMisc;
constexpr auto kGem = items::ItemCategory::kGem;
constexpr int kBladeCategory = static_cast<int>(kBlade);
constexpr int kGemCategory = static_cast<int>(kGem);
constexpr int kStrength = 8;
constexpr int kMaterial = 1;
constexpr std::int32_t kLowest = std::numeric_limits<std::int32_t>::min();
constexpr std::int32_t kHighest = std::numeric_limits<std::int32_t>::max();
constexpr std::int32_t kNegative = -128;
constexpr std::uint8_t kOpaque = 0xA5;
constexpr std::uint8_t kThird = 3;
constexpr std::uint8_t kForeignToken = 255;
constexpr int kForeignCategory = 257;
constexpr std::size_t kRefusedCall = 5;

void SetOrder(fixture::World& world, items::ItemCategory category) {
  auto order = items::Inventory::DisplayOrder(world.indices, category);
  constexpr auto kTokens = std::to_array<std::uint8_t>({2, 0, 1, 2, kThird});
  std::ranges::copy(kTokens, order.begin());
  fixture::SetRecords(world);
  world.keys[1] = kNegative;
  world.keys[2] = kHighest;
  world.keys[kThird] = kHighest;
}
}  // namespace

TEST_CASE("F02583 inventory weapon ties duplicates and zero tails are stable",
          "[inventory-sorting][F02583]") {
  fixture::World world;
  SetOrder(world, kWeapon);
  const auto records = world.inventory;
  sorting::SortWeapons(world, kMaterial);
  const auto order = items::Inventory::DisplayOrder(world.indices, kWeapon);
  CHECK(std::array<std::uint8_t, 4>{order[0], order[1], order[2], order[3]} ==
        std::array<std::uint8_t, 4>{2, 2, 3, 1});
  CHECK(std::ranges::all_of(order.subspan(4),
                            [](auto value) { return value == 0; }));
  CHECK(world.inventory == records);
  CHECK(world.reads > order.size());
}

TEST_CASE("F02583 fixed weapon order ignores the equipment pointer routes",
          "[inventory-sorting][F02583]") {
  fixture::World world;
  SetOrder(world, kWeapon);
  world.routes[0] = kMisc;
  sorting::SortWeapons(world, kMaterial);
  CHECK(world.indices.weapons.front() == 2);
  CHECK(world.indices.misc.front() == 0);
  CHECK(world.events.front().operation == fixture::Operation::kFixedOrder);
}

TEST_CASE("F02583 inventory weapon order checks real source and part extents",
          "[inventory-sorting][F02583]") {
  fixture::World world;
  world.indices.weapons[0] = kForeignToken;
  CHECK_THROWS(sorting::SortWeapons(world, kMaterial));
  world.indices.weapons[0] = 1;
  world.inventory.weapons[0].blade = items::kBladeSlots + 1;
  CHECK_THROWS(sorting::SortWeapons(world, kMaterial));
  CHECK(world.indices.weapons[0] == 1);
}

TEST_CASE("F02585 shield duplicates and signed sentinel keys remain live",
          "[inventory-sorting][F02585]") {
  fixture::World world;
  fixture::SetRecords(world);
  world.indices.shields.fill(2);
  world.keys[2] = kNegative;
  sorting::SortShields(world, kStrength);
  CHECK(std::ranges::all_of(world.indices.shields,
                            [](auto value) { return value == 2; }));
  world.keys[2] = kLowest;
  sorting::SortShields(world, kStrength);
  CHECK(std::ranges::all_of(world.indices.shields,
                            [](auto value) { return value == 0; }));
  CHECK(world.inventory.shields[1].index == 2);
}

TEST_CASE("F02585 inventory shield sorting includes its last byte token",
          "[inventory-sorting][F02585]") {
  fixture::World world;
  fixture::SetRecords(world);
  world.indices.shields[0] = 1;
  world.indices.shields[1] = items::kShieldSlots;
  world.keys[1] = kNegative;
  world.keys[items::kShieldSlots] = kHighest;
  sorting::SortShields(world, kStrength);
  CHECK(world.indices.shields.front() == items::kShieldSlots);
  CHECK(world.indices.shields[1] == 1);
  CHECK(world.indices.weapons.front() == 0);
}

TEST_CASE("F02585 inventory shield keys do not run for empty invalid stats",
          "[inventory-sorting][F02585]") {
  fixture::World world;
  sorting::SortShields(world, kLowest);
  CHECK(world.reads == 0);
  CHECK(world.events.size() == 1);
  world.indices.shields[0] = kForeignToken;
  CHECK_THROWS(sorting::SortShields(world, kStrength));
}

TEST_CASE("F02587 inventory misc fills all sixty four byte-order slots",
          "[inventory-sorting][F02587]") {
  fixture::World world;
  fixture::SetRecords(world);
  for (std::size_t row = 0; row < items::kMiscSlots; ++row) {
    world.indices.misc[row] = static_cast<std::uint8_t>(row + 1);
  }
  world.keys.fill(kNegative);
  world.keys[items::kMiscSlots] = kHighest;
  sorting::SortMisc(world, kMaterial);
  CHECK(world.indices.misc.front() == items::kMiscSlots);
  CHECK(world.indices.misc[1] == 1);
  CHECK(world.indices.misc.back() == items::kMiscSlots - 1);
}

TEST_CASE("F02587 misc clears selected rows before the next refused maximum",
          "[inventory-sorting][F02587]") {
  fixture::World world;
  fixture::SetRecords(world);
  world.indices.misc[0] = 1;
  world.indices.misc[1] = 2;
  world.keys[1] = kHighest;
  world.keys[2] = kNegative;
  std::size_t calls = 0;
  world.hook = [&](auto&, auto operation) {
    if (operation == fixture::Operation::kMisc && ++calls == kRefusedCall) {
      throw std::runtime_error("next maximum refused");
    }
  };
  CHECK_THROWS_WITH(sorting::SortMisc(world, kMaterial),
                    "next maximum refused");
  CHECK(world.indices.misc[0] == 0);
  CHECK(world.indices.misc[1] == 2);
}

TEST_CASE("F02587 every misc key projects the current fixed inventory",
          "[inventory-sorting][F02587]") {
  fixture::World world;
  fixture::SetRecords(world);
  world.indices.misc[0] = 1;
  world.keys[1] = kHighest;
  world.hook = [](auto& state, auto operation) {
    if (operation == fixture::Operation::kMisc) {
      state.inventory.misc[0].id = kOpaque;
    }
  };
  sorting::SortMisc(world, kMaterial);
  CHECK(world.inventory.misc[0].id == kOpaque);
  CHECK(world.indices.misc[0] == 1);
  CHECK(world.reads == 2);
}

TEST_CASE("F02589 blade projection preserves fields its record does not own",
          "[inventory-sorting][F02589]") {
  fixture::World world;
  auto& blade = world.inventory.blades.back();
  blade.id = kOpaque;
  blade.sub_id = kOpaque;
  blade.strength = kNegative;
  blade.material = std::numeric_limits<std::uint16_t>::max();
  items::EquipmentPart target{};
  target.types.fill(kNegative);
  target.gem_slots = kOpaque;
  sorting::CopyEquipmentStats(target, kBladeCategory, items::kBladeSlots - 1,
                              world);
  CHECK(target.id == kOpaque);
  CHECK(target.sub_id == static_cast<std::int8_t>(kOpaque));
  CHECK(target.strength == kNegative);
  CHECK(target.material == kForeignToken);
  CHECK(target.types.front() == kNegative);
  CHECK(target.gem_slots == kOpaque);
}

TEST_CASE("F02589 current gem projection and unsupported categories stay lazy",
          "[inventory-sorting][F02589]") {
  fixture::World world;
  items::EquipmentPart target{};
  target.category = kOpaque;
  world.inventory.gems.back().strength = kNegative;
  world.inventory.gems.back().gem_effects = kOpaque;
  sorting::CopyEquipmentStats(target, kGemCategory, items::kGemSlots - 1,
                              world);
  CHECK(target.strength == kNegative);
  CHECK(target.gem_effects == kOpaque);
  CHECK(target.category == kOpaque);
  const auto before = target;
  const auto reads = world.reads;
  sorting::CopyEquipmentStats(target, kForeignCategory, kLowest, world);
  CHECK(target == before);
  CHECK(world.reads == reads);
  CHECK_THROWS(sorting::CopyEquipmentStats(target, kGemCategory, -1, world));
}

TEST_CASE("F02590 equipment captures capacity and route before key callbacks",
          "[inventory-sorting][F02590]") {
  fixture::World world;
  SetOrder(world, kBlade);
  world.indices.blades[0] = 1;
  world.indices.blades[1] = 2;
  world.indices.blades[2] = kThird;
  world.capacities[1] = 2;
  world.hook = [](auto& state, auto operation) {
    if (operation == fixture::Operation::kEquipmentOrder) {
      state.capacities[1] = 1;
      state.routes[1] = kMisc;
    }
  };
  sorting::SortEquipment(world, kBladeCategory, kStrength);
  CHECK(world.indices.blades[0] == 2);
  CHECK(world.indices.blades[1] == 1);
  CHECK(world.indices.blades[2] == kThird);
  CHECK(world.indices.misc[0] == 0);
  CHECK(world.events.front().operation == fixture::Operation::kCapacity);
}

TEST_CASE(
    "F02590 equipment accepts known order aliases without sorting them twice",
    "[inventory-sorting][F02590]") {
  fixture::World world;
  SetOrder(world, kMisc);
  world.routes[1] = kMisc;
  sorting::SortEquipment(world, kBladeCategory, kStrength);
  CHECK(world.indices.misc[0] == 2);
  CHECK(world.indices.misc[1] == 2);
  CHECK(world.indices.misc[2] == kThird);
  CHECK(world.indices.misc[3] == 1);
  CHECK(world.indices.blades[0] == 0);
}

TEST_CASE("F02590 empty unsupported projection categories are not eager reads",
          "[inventory-sorting][F02590]") {
  fixture::World world;
  sorting::SortEquipment(world, 0, kLowest);
  CHECK(world.reads == 0);
  world.capacities[1] = 0;
  sorting::SortEquipment(world, kBladeCategory, kStrength);
  CHECK(world.reads == 0);
  world.indices.weapons[0] = 1;
  CHECK_THROWS(sorting::SortEquipment(world, 0, kStrength));
  CHECK_THROWS(sorting::SortEquipment(world, -1, kStrength));
  CHECK_THROWS(sorting::SortEquipment(
      world, static_cast<int>(items::ItemCategory::kManage), kStrength));
}
