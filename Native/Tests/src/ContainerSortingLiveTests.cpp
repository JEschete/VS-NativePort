#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "Items/ContainerIndexingLiveFixture.hpp"
#include "Menu/Container/ContainerSortingLive.hpp"
#include "Menu/SortingKeysFixture.hpp"

namespace {
namespace sorting = VS::Menu::container;
namespace items = VS::Items;
using container_indexing_tests::LiveFixture;
constexpr int kMaterial = 1;
constexpr int kStrength = 8;
constexpr int kWeapon = static_cast<int>(items::ItemCategory::kWeapon);
constexpr int kShield = static_cast<int>(items::ItemCategory::kShield);
constexpr int kMisc = static_cast<int>(items::ItemCategory::kMisc);
constexpr int kBlade = static_cast<int>(items::ItemCategory::kBlade);
constexpr int kGrip = static_cast<int>(items::ItemCategory::kGrip);
constexpr int kArmor = static_cast<int>(items::ItemCategory::kArmor);
constexpr int kGem = static_cast<int>(items::ItemCategory::kGem);
constexpr std::int8_t kNegative = -128;
constexpr std::int8_t kPositive = 127;
constexpr std::int8_t kReserved = 91;
constexpr std::uint8_t kFlaggedGem = 255;
constexpr char kRawFlaggedByte = std::bit_cast<char>(kFlaggedGem);
constexpr std::uint16_t kWideMaterial = 65535;
constexpr std::uint8_t kLowMaterial = 2;
constexpr std::uint8_t kHighMaterial = 9;
constexpr std::size_t kLastBlade = items::kContainerCapacities[kBlade] - 1;
constexpr std::size_t kLastGem = items::kContainerCapacities[kGem] - 1;
constexpr auto kWeaponCount = items::kContainerCapacities[kWeapon];
constexpr auto kShieldCount = items::kContainerCapacities[kShield];
constexpr auto kMiscCount = items::kContainerCapacities[kMisc];
}  // namespace

TEST_CASE(
    "F02961 checked Live uses real weapon stats and stable halfword orders",
    "[container-sorting-live][F02961]") {
  LiveFixture fixture;
  auto& owner = fixture.owner;
  Menu_Container_containerData = &owner;
  owner.items.weapons[0].blade = 1;
  std::span(owner.items.weapons).back().blade = kLastBlade + 1;
  owner.items.blades[0].material = kLowMaterial;
  owner.items.blades[kLastBlade].material = kHighMaterial;
  owner.indices.weapons[0] = kWeaponCount;
  owner.indices.weapons[1] = 1;
  owner.indices.weapons[2] = kWeaponCount;
  const auto before =
      std::bit_cast<std::array<std::byte, sizeof(Items_ContainerData)>>(
          owner.items);
  sorting::SortLiveWeapons(kMaterial);
  CHECK(owner.indices.weapons[0] == 1);
  CHECK(owner.indices.weapons[1] == 32);
  CHECK(owner.indices.weapons[2] == 32);
  CHECK(owner.indices.weapons[3] == 0);
  CHECK((std::bit_cast<std::array<std::byte, sizeof(Items_ContainerData)>>(
             owner.items) == before));
}

TEST_CASE("F02963 checked Live folds flagged gems and signed shield attributes",
          "[container-sorting-live][F02963]") {
  LiveFixture fixture;
  auto& owner = fixture.owner;
  Menu_Container_containerData = &owner;
  owner.items.shields[0].base.strength = kPositive;
  auto& last = std::span(owner.items.shields).back();
  last.base.strength = kPositive;
  last.gems[0] = kFlaggedGem;
  owner.items.gems[kPositive - 1].strength = kNegative;
  owner.indices.shields[0] = kShieldCount;
  owner.indices.shields[1] = 1;
  owner.indices.shields[2] = kShieldCount;
  sorting::SortLiveShields(kStrength);
  CHECK(owner.indices.shields[0] == 1);
  CHECK(owner.indices.shields[1] == 32);
  CHECK(owner.indices.shields[2] == 32);
  CHECK(last.gems[0] == kFlaggedGem);
}

TEST_CASE(
    "F02965 checked Live reaches misc slot 256 and retains equal-count order",
    "[container-sorting-live][F02965]") {
  LiveFixture fixture;
  auto& owner = fixture.owner;
  Menu_Container_containerData = &owner;
  owner.items.misc[0].id = 1;
  owner.items.misc[0].count = kRawFlaggedByte;
  auto& last = std::span(owner.items.misc).back();
  last.id = kWideMaterial;
  last.count = kRawFlaggedByte;
  owner.indices.misc[0] = kMiscCount;
  owner.indices.misc[1] = 1;
  owner.indices.misc[2] = kMiscCount;
  sorting::SortLiveMisc(kMaterial);
  CHECK(owner.indices.misc[0] == 256);
  CHECK(owner.indices.misc[1] == 1);
  sorting::SortLiveMisc(0);
  CHECK(owner.indices.misc[0] == 1);
  CHECK(owner.indices.misc[1] == 256);
  CHECK(owner.indices.misc[2] == 256);
  CHECK(static_cast<std::uint8_t>(last.count) == kFlaggedGem);
}

TEST_CASE(
    "F02967 checked Live preserves opaque blade fields and current owners",
    "[container-sorting-live][F02967]") {
  LiveFixture fixture;
  Menu_Container_containerData = &fixture.owner;
  auto& blade = fixture.owner.items.blades[kLastBlade];
  blade.id = kFlaggedGem;
  blade.strength = kNegative;
  blade.material = kWideMaterial;
  items::EquipmentPart initial{};
  initial.types.fill(kReserved);
  initial.classes.fill(kReserved);
  initial.affinities.fill(kReserved);
  Items_EquipmentPartStats target{};
  items::Live::Commit(target, initial);
  sorting::CopyLiveEquipmentStats(&target, kBlade, kLastBlade);
  const auto projected = items::Live::Typed(target);
  CHECK(projected.id == kFlaggedGem);
  CHECK(projected.material == kFlaggedGem);
  CHECK(projected.strength == kNegative);
  CHECK(projected.types == initial.types);
  CHECK(projected.classes.back() == kReserved);
  CHECK(projected.affinities.back() == kReserved);
  Menu_Container_containerData = &fixture.replacement;
  fixture.replacement.items.blades[kLastBlade].id = kLowMaterial;
  sorting::CopyLiveEquipmentStats(&target, kBlade, kLastBlade);
  CHECK(items::Live::Typed(target).id == kLowMaterial);
}

TEST_CASE(
    "F02967 Live projects grip armor gem without clearing unrelated fields",
    "[container-sorting-live][F02967]") {
  LiveFixture fixture;
  Menu_Container_containerData = &fixture.owner;
  fixture.owner.items.grips[0].strength = kNegative;
  fixture.owner.items.armor[0].strength = kPositive;
  fixture.owner.items.gems[kLastGem].strength = kNegative;
  fixture.owner.items.gems[kLastGem].gemEffects = kRawFlaggedByte;
  Items_EquipmentPartStats target{};
  target.currentDp = kWideMaterial;
  sorting::CopyLiveEquipmentStats(&target, kGrip, 0);
  CHECK(items::Live::Typed(target).strength == kNegative);
  CHECK(target.currentDp == kWideMaterial);
  sorting::CopyLiveEquipmentStats(&target, kArmor, 0);
  CHECK(items::Live::Typed(target).strength == kPositive);
  target.category = kHighMaterial;
  sorting::CopyLiveEquipmentStats(&target, kGem, kLastGem);
  CHECK(items::Live::Typed(target).strength == kNegative);
  CHECK(items::Live::Typed(target).gem_effects == kFlaggedGem);
  CHECK(items::Live::Typed(target).category == kHighMaterial);
}

TEST_CASE(
    "F02967 unsupported categories are lazy before owner and output access",
    "[container-sorting-live][F02967]") {
  LiveFixture fixture;
  Menu_Container_containerData = nullptr;
  for (const int category :
       VS::Menu::sorting_test::kUnsupportedEquipmentCategories) {
    CHECK_NOTHROW(sorting::CopyLiveEquipmentStats(
        nullptr, category, std::numeric_limits<int>::min()));
  }
}

TEST_CASE(
    "F02968 checked Live handles empty missing owners without stale caches",
    "[container-sorting-live][F02968]") {
  LiveFixture fixture;
  Menu_Container_containerData = nullptr;
  Menu_Container_containerItemCapacities[kBlade] = 0;
  CHECK_NOTHROW(sorting::SortLiveEquipment(kBlade, kStrength));
  Menu_Container_containerData = &fixture.owner;
  Menu_Container_containerItemCapacities[kBlade] =
      items::kContainerCapacities[kBlade];
  fixture.owner.items.blades[kLastBlade].strength = kPositive;
  fixture.owner.items.blades[0].strength = kNegative;
  fixture.owner.indices.blades[0] = 1;
  fixture.owner.indices.blades[1] = kLastBlade + 1;
  sorting::SortLiveEquipment(kBlade, kStrength);
  CHECK(fixture.owner.indices.blades[0] == kLastBlade + 1);
  CHECK(fixture.owner.indices.blades[1] == 1);
  fixture.owner.items.blades[0].strength = kPositive;
  fixture.owner.items.blades[kLastBlade].strength = kNegative;
  sorting::SortLiveEquipment(kBlade, kStrength);
  CHECK(fixture.owner.indices.blades[0] == 1);
  CHECK(fixture.owner.indices.blades[1] == kLastBlade + 1);
}
