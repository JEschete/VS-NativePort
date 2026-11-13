#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Items/ContainerGemCounts.hpp"
#include "Items/ContainerTransfers.hpp"
#include "Menu/Container/ContainerCapacity.hpp"
#include "Menu/ContainerCapacityWorld.hpp"
#include "Menu/Framework/Framework.hpp"

namespace {
namespace items = VS::Items;
namespace capacity = VS::Menu::container;
namespace framework = VS::Menu::Framework;
namespace samples = container_capacity_tests;
using samples::World;
constexpr std::int32_t kLastWeapon = 31;
constexpr std::int32_t kLastShield = 31;
constexpr std::int32_t kLastGem = 191;
constexpr std::uint32_t kGray = 0x808080;
constexpr std::uint32_t kBelowHalf = 0x804020;
}  // namespace

TEST_CASE("F02971 highlights strictly below half rather than equality",
          "[container-capacity][F02971]") {
  World world;
  constexpr int kLimit = 8;
  constexpr int kHalf = 4;
  constexpr auto kPosition = framework::PackXy(112, 100);
  capacity::DrawCapacityCount(
      world, {.limit = kLimit, .position = kPosition, .occupied = kHalf - 1});
  CHECK(world.draws.at(0).value.value == kLimit);
  CHECK(world.draws.at(1).text == "/");
  CHECK(world.draws.at(2).value.position == kPosition - 32U);
  CHECK(world.draws.at(2).value.color == kBelowHalf);
  world.draws.clear();
  capacity::DrawCapacityCount(
      world, {.limit = kLimit, .position = kPosition, .occupied = kHalf});
  CHECK(world.draws.at(2).value.color == kGray);
}

TEST_CASE("F02971 keeps packed subtraction and signed doubling wrap",
          "[container-capacity][F02971]") {
  World world;
  constexpr std::uint32_t kBorrowed = 0x0063FFE0;
  constexpr int kRow = 100;
  capacity::DrawCapacityCount(
      world, {.limit = 1,
              .position = framework::PackXy(0, kRow),
              .occupied = std::numeric_limits<std::int32_t>::max()});
  CHECK(world.draws.at(1).value.position == kBorrowed);
  CHECK(world.draws.at(2).value.color == kBelowHalf);
  CHECK(world.draws.at(2).value.value ==
        std::numeric_limits<std::int32_t>::max());
}

TEST_CASE("F02972 renders seven real category counts and original labels",
          "[container-capacity][F02972]") {
  World world;
  constexpr std::size_t kLastRecordCategory = 6;
  constexpr std::size_t kEvents = 51;
  for (std::size_t value = 0; value <= kLastRecordCategory; ++value) {
    const auto category = static_cast<items::ItemCategory>(value);
    samples::Fill(world.bag, category, value + 1);
    samples::Fill(world.destination, category, value + 2);
  }
  capacity::DrawStorageCapacities(world);
  CHECK(world.draws.size() == kEvents);
  CHECK(world.draws.at(0).text == "GADGET BAG");
  CHECK(world.draws.at(1).text == "CONTAINER");
  CHECK(world.draws.at(5).text == "$WEAPON");
  CHECK(world.draws.at(kEvents - 4).text == "$MISC");
  CHECK(world.draws.back().value.value == kLastRecordCategory + 2);
  CHECK(world.draws.back().value.position == framework::PackXy(216, 172));
}

TEST_CASE("F02972 observes later capacities after real draw callbacks",
          "[container-capacity][F02972]") {
  World world;
  constexpr std::uint16_t kChangedLimit = 201;
  constexpr auto kMisc = static_cast<std::size_t>(items::ItemCategory::kMisc);
  world.number_callback = [&] { world.limits.at(kMisc) = kChangedLimit; };
  capacity::DrawStorageCapacities(world);
  constexpr std::size_t kLastCapacityEvent = 48;
  CHECK(world.draws.at(kLastCapacityEvent).value.value == kChangedLimit);
  CHECK(world.draws.at(kLastCapacityEvent).value.position ==
        framework::PackXy(248, 172));
}

TEST_CASE("F02973 counts last grip and unsigned references through slot four",
          "[container-capacity][F02973]") {
  items::ContainerStorage source;
  samples::ArmedWeapon(source);
  CHECK(items::CountContainerWeaponGems(kLastWeapon, source) == 2);
  source.items.grips.back().gem_slots = 0;
  CHECK(items::CountContainerWeaponGems(kLastWeapon, source) == 0);
  constexpr std::uint8_t kFourSlots = 4;
  constexpr std::uint8_t kHighReference = 255;
  source.items.grips.back().gem_slots = kFourSlots;
  source.items.weapons.back().gems.back() = kHighReference;
  CHECK(items::CountContainerWeaponGems(kLastWeapon, source) == kFourSlots - 1);
}

TEST_CASE("F02973 diagnoses required grip and reached reference extent",
          "[container-capacity][F02973]") {
  items::ContainerStorage source;
  CHECK_THROWS_AS(items::CountContainerWeaponGems(0, source),
                  std::out_of_range);
  samples::ArmedWeapon(source);
  constexpr std::uint8_t kBeyondReferences = 5;
  source.items.grips.back().gem_slots = kBeyondReferences;
  CHECK_THROWS_AS(items::CountContainerWeaponGems(kLastWeapon, source),
                  std::out_of_range);
}

TEST_CASE("F02974 counts the fourth shield reference without a weapon mask",
          "[container-capacity][F02974]") {
  items::ContainerStorage source;
  constexpr std::uint8_t kFourSlots = 4;
  constexpr std::uint8_t kFlagOnly = 128;
  source.items.shields.back().base.gem_slots = kFourSlots;
  source.items.shields.back().gems = {0, 1, kFlagOnly, 0};
  CHECK(items::CountContainerShieldGems(kLastShield, source) == 2);
  source.items.shields.back().gems.back() = kFlagOnly;
  CHECK(items::CountContainerShieldGems(kLastShield, source) == kFourSlots - 1);
  source.items.shields.back().base.gem_slots = 0;
  CHECK(items::CountContainerShieldGems(kLastShield, source) == 0);
}

TEST_CASE("F02974 diagnoses only a reached out-of-record slot",
          "[container-capacity][F02974]") {
  items::ContainerStorage source;
  constexpr std::uint8_t kBeyondReferences = 5;
  source.items.shields.back().base.gem_slots = kBeyondReferences;
  CHECK_THROWS_AS(items::CountContainerShieldGems(kLastShield, source),
                  std::out_of_range);
  CHECK_THROWS_AS(items::CountContainerShieldGems(-1, source),
                  std::out_of_range);
}

TEST_CASE("F02975 measures equality and one beyond for every real category",
          "[container-capacity][F02975]") {
  World world;
  constexpr std::size_t kRecordCategories = 7;
  for (std::size_t value = 0; value < kRecordCategories; ++value) {
    const auto category = static_cast<items::ItemCategory>(value);
    const auto limit = world.limits.at(value);
    samples::Fill(world.destination, category, limit - 1);
    CHECK_FALSE(capacity::WouldExceedCapacity(category, 1, world));
    CHECK(capacity::WouldExceedCapacity(category, 2, world));
  }
}

TEST_CASE("F02975 includes staging, aliases and late receiving-limit changes",
          "[container-capacity][F02975]") {
  World world;
  samples::Fill(world.destination, items::ItemCategory::kGem, 1);
  world.staging = &world.destination;
  constexpr auto kGem = static_cast<std::size_t>(items::ItemCategory::kGem);
  world.limit_callback = [&] { world.limits.at(kGem) = 2; };
  CHECK_FALSE(
      capacity::WouldExceedCapacity(items::ItemCategory::kGem, 0, world));
  CHECK(capacity::WouldExceedCapacity(items::ItemCategory::kGem, 1, world));
  CHECK_FALSE(capacity::WouldExceedCapacity(
      items::ItemCategory::kGem, std::numeric_limits<std::int32_t>::max(),
      world));
}

TEST_CASE("F02976 reserves parent, blade, grip and every counted gem",
          "[container-capacity][F02976]") {
  World world;
  samples::ArmedWeapon(world.source);
  constexpr auto kFits = capacity::TransferError::kFits;
  constexpr auto kFull = capacity::TransferError::kPendingListFull;
  constexpr std::size_t kFirstRefused = 60;
  std::fill_n(world.pending.begin(), kFirstRefused - 1, 1);
  CHECK(capacity::TransferCapacityError(
            {items::ItemCategory::kWeapon, kLastWeapon}, world) == kFits);
  world.pending.at(kFirstRefused - 1) = 1;
  CHECK(capacity::TransferCapacityError(
            {items::ItemCategory::kWeapon, kLastWeapon}, world) == kFull);
  constexpr unsigned kWeaponChecks = 4;
  CHECK(world.receiving_queries == kWeaponChecks);
}

TEST_CASE("F02976 checks each weapon component capacity in original order",
          "[container-capacity][F02976]") {
  constexpr std::array kCategories{
      items::ItemCategory::kWeapon, items::ItemCategory::kBlade,
      items::ItemCategory::kGrip, items::ItemCategory::kGem};
  for (const auto blocked : kCategories) {
    World world;
    samples::ArmedWeapon(world.source);
    samples::Fill(
        world.destination, blocked,
        items::kContainerCapacities[static_cast<std::size_t>(blocked)]);
    CHECK(capacity::TransferCapacityError(
              {items::ItemCategory::kWeapon, kLastWeapon}, world) ==
          capacity::TransferError::kDestinationFull);
    CHECK(world.checked_limits.back() == blocked);
  }
}

TEST_CASE("F02976 reads all 64 entries but nothing after the first empty",
          "[container-capacity][F02976]") {
  World world;
  world.pending.fill(1);
  CHECK(
      capacity::TransferCapacityError({items::ItemCategory::kGem, -1}, world) ==
      capacity::TransferError::kPendingListFull);
  CHECK(world.receiving_queries == 0);
  world.pending.front() = 0;
  world.pending_extent = 1;
  CHECK(
      capacity::TransferCapacityError({items::ItemCategory::kGem, -1}, world) ==
      capacity::TransferError::kFits);
  CHECK(world.active_queries == 0);
}

TEST_CASE("F02976 still reads mandatory weapon grip before a full queue result",
          "[container-capacity][F02976]") {
  World world;
  world.pending.fill(1);
  CHECK_THROWS_AS(
      capacity::TransferCapacityError({items::ItemCategory::kWeapon, 0}, world),
      std::out_of_range);
  CHECK(world.receiving_queries == 0);
}

TEST_CASE("F02976 shields reserve gems and test receiving gem equality",
          "[container-capacity][F02976]") {
  World world;
  constexpr std::uint8_t kThreeSlots = 3;
  auto& shield = world.source.items.shields.back();
  shield.base.gem_slots = kThreeSlots;
  shield.gems = {1, 0, 1, 0};
  constexpr auto kGem = static_cast<std::size_t>(items::ItemCategory::kGem);
  samples::Fill(world.destination, items::ItemCategory::kGem,
                world.limits.at(kGem) - 2);
  CHECK(capacity::TransferCapacityError(
            {items::ItemCategory::kShield, kLastShield}, world) ==
        capacity::TransferError::kFits);
  samples::Fill(world.staged, items::ItemCategory::kGem, 1);
  CHECK(capacity::TransferCapacityError(
            {items::ItemCategory::kShield, kLastShield}, world) ==
        capacity::TransferError::kDestinationFull);
}

TEST_CASE(
    "F02977 returns a last gem and leaves its selected queue token intact",
    "[container-capacity][F02977]") {
  World world;
  constexpr std::uint16_t kLastGemToken = 0x5C0;
  constexpr std::int32_t kLastPendingRow = 63;
  constexpr std::uint16_t kGemId = 513;
  world.pending.back() = kLastGemToken;
  world.staged.items.gems.back().id = kGemId;
  capacity::UndoPendingTransfer(kLastPendingRow, world);
  CHECK(world.source.items.gems.front().id == kGemId);
  CHECK(world.staged.items.gems.back().id == 0);
  CHECK(world.staged.items.gems.back().index == kLastGem + 1);
  CHECK(world.pending.back() == kLastGemToken);
}

TEST_CASE("F02977 clears staging even when the receiving table is full",
          "[container-capacity][F02977]") {
  World world;
  constexpr std::uint16_t kLastGemToken = 0x5C0;
  samples::Fill(world.source, items::ItemCategory::kGem,
                world.source.items.gems.size());
  world.pending.front() = kLastGemToken;
  world.staged.items.gems.back().id = 2;
  const auto before = world.source.items;
  capacity::UndoPendingTransfer(0, world);
  CHECK(world.source.items == before);
  CHECK(world.staged.items.gems.back().id == 0);
}

TEST_CASE("F02977 keeps the selected token while reloading staging after copy",
          "[container-capacity][F02977]") {
  World world;
  items::ContainerStorage replacement;
  constexpr std::uint16_t kLastGemToken = 0x5C0;
  world.pending.front() = kLastGemToken;
  world.staged.items.gems.back().id = 2;
  replacement.items.gems.back().id = 1;
  world.staging_callback = [&] {
    if (world.staging_queries == 2) {
      world.pending.front() = 1;
      world.staging = &replacement;
    }
  };
  capacity::UndoPendingTransfer(0, world);
  CHECK(world.source.items.gems.front().id == 2);
  CHECK(world.staged.items.gems.back().id == 2);
  CHECK(replacement.items.gems.back().id == 0);
}

TEST_CASE("F02978 stages equipment parts and clears the actual source tree",
          "[container-capacity][F02978]") {
  World world;
  samples::ArmedWeapon(world.source);
  capacity::StageItemTransfer({items::ItemCategory::kWeapon, kLastWeapon},
                              world);
  CHECK(world.staged.items.weapons.front().blade == 1);
  CHECK(world.staged.items.weapons.front().grip == 1);
  CHECK(world.staged.items.weapons.front().gems.at(2) == 2);
  CHECK(world.staged.items.gems.front().set_item_index == 1);
  CHECK(world.source.items.weapons.back().blade == 0);
  CHECK(world.source.items.grips.back().id == 0);
  CHECK(world.source.items.gems.back().id == 0);
  CHECK(world.pending.front() == 0);
}

TEST_CASE(
    "F02978 does not roll back a failed copy or a later mandatory refusal",
    "[container-capacity][F02978]") {
  World world;
  samples::Fill(world.staged, items::ItemCategory::kGem,
                world.staged.items.gems.size());
  world.source.items.gems.back().id = 2;
  capacity::StageItemTransfer({items::ItemCategory::kGem, kLastGem}, world);
  CHECK(world.source.items.gems.back().id == 0);
  world.staged.items.gems = {};
  samples::ArmedWeapon(world.source);
  world.source.items.weapons.back().grip = 0;
  CHECK_THROWS_AS(capacity::StageItemTransfer(
                      {items::ItemCategory::kWeapon, kLastWeapon}, world),
                  std::out_of_range);
  CHECK(world.staged.items.weapons.front().index == 1);
  CHECK(world.source.items.weapons.back().blade != 0);
}

TEST_CASE("F02978 uses original alias mutation and reloads active before clear",
          "[container-capacity][F02978]") {
  World world;
  world.staging = &world.source;
  world.source.items.gems.back().id = 2;
  capacity::StageItemTransfer({items::ItemCategory::kGem, kLastGem}, world);
  CHECK(world.source.items.gems.front().id == 2);
  CHECK(world.source.items.gems.back().id == 0);
  items::ContainerStorage replacement;
  world.source.items.gems.back().id = 2;
  replacement.items.gems.back().id = 1;
  world.active_queries = 0;
  world.active_callback = [&] {
    if (world.active_queries == 2) world.active = &replacement;
  };
  capacity::StageItemTransfer({items::ItemCategory::kGem, kLastGem}, world);
  CHECK(replacement.items.gems.back().id == 0);
  CHECK(world.source.items.gems.back().id == 2);
}
