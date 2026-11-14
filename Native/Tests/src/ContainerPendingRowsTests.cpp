#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

#include "Menu/ContainerPendingRowsWorld.hpp"

namespace {
using container_pending_rows_tests::Event;
using container_pending_rows_tests::World;
namespace tests = container_pending_rows_tests;
namespace rows = VS::Menu::container;
namespace items = VS::Items;
namespace framework = VS::Menu::Framework;
using Category = items::ItemCategory;
constexpr std::int32_t kLastPosition = rows::kPendingTransferEntries;
constexpr std::uint16_t kUnknownToken = 0xFF01;
constexpr std::uint32_t kInitialType = 0x12340000;
constexpr std::int32_t kHighCategory = 0x10000;
constexpr std::int32_t kWordWrappedCategory = 0x1000000;
constexpr std::int32_t kHighSlot = 0x10000;
constexpr std::size_t kParentOutsideWeaponTable = 33;
constexpr std::uint16_t kFirstToken = 1;
constexpr std::uint16_t kSecondToken = 2;
constexpr auto kGemCategory = static_cast<std::int32_t>(Category::kGem);
constexpr auto kMiscCategory = static_cast<std::int32_t>(Category::kMisc);
constexpr std::size_t kFourthPendingIndex = 4;
}  // namespace

TEST_CASE("F02979 searches through holes and returns the last one-based row",
          "[container-pending-rows][F02979]") {
  World world;
  world.first_pending.back() = tests::kLastGemPendingToken;
  CHECK(rows::FindPendingTransfer({kGemCategory, tests::kLastGemSlot}, world) ==
        kLastPosition);
  CHECK(rows::FindPendingTransfer({0, 0}, world) == 0);
  CHECK(world.first.reads == 0);
  CHECK(world.second.reads == 0);
}

TEST_CASE("F02979 compares wrapped full words rather than narrowed tokens",
          "[container-pending-rows][F02979]") {
  World world;
  CHECK(rows::FindPendingTransfer({0, -1}, world) == 1);
  CHECK(rows::FindPendingTransfer({kHighCategory, -1}, world) == 0);
  CHECK(rows::FindPendingTransfer({kWordWrappedCategory, -1}, world) == 1);
  CHECK(rows::FindPendingTransfer({0, kHighSlot - 1}, world) == 0);
  CHECK(rows::FindPendingTransfer({0, std::numeric_limits<std::int32_t>::max()},
                                  world) == 0);
}

TEST_CASE("F02979 and F02980 refuse a short list only at the reached row",
          "[container-pending-rows][F02979][F02980]") {
  World world;
  world.pending = std::span(world.first_pending).first(1);
  CHECK(rows::FindPendingTransfer({0, -1}, world) == 1);
  rows::AppendPendingTransfer({0, 0}, world);
  CHECK(world.first_pending.front() == 1);
  CHECK_THROWS_AS(rows::FindPendingTransfer({1, 0}, world), std::out_of_range);
  CHECK_THROWS_AS(rows::AppendPendingTransfer({1, 0}, world),
                  std::out_of_range);
}

TEST_CASE(
    "F02980 fills the first hole without deduplicating or changing neighbors",
    "[container-pending-rows][F02980]") {
  World world;
  world.first_pending[0] = tests::kBladePendingToken;
  world.first_pending[2] = tests::kMiscPendingToken;
  rows::AppendPendingTransfer({1, 0}, world);
  CHECK(world.first_pending[0] == tests::kBladePendingToken);
  CHECK(world.first_pending[1] == tests::kBladePendingToken);
  CHECK(world.first_pending[2] == tests::kMiscPendingToken);
  CHECK(world.first_pending.back() == 0);
}

TEST_CASE("F02980 truncates only the stored token and keeps the slot-256 carry",
          "[container-pending-rows][F02980]") {
  World world;
  rows::AppendPendingTransfer({kHighCategory, 0}, world);
  CHECK(world.first_pending.front() == 1);
  rows::AppendPendingTransfer({kMiscCategory, tests::kLastMiscSlot}, world);
  CHECK(world.first_pending[1] == tests::kLastMiscPendingToken);
  rows::AppendPendingTransfer({0, -1}, world);
  CHECK(world.first_pending[2] == 0);
  CHECK(rows::FindPendingTransfer({kMiscCategory, tests::kLastMiscSlot},
                                  world) == 2);
}

TEST_CASE("F02980 leaves a completely full list unchanged",
          "[container-pending-rows][F02980]") {
  World world;
  world.first_pending.fill(kFirstToken);
  const auto before = world.first_pending;
  rows::AppendPendingTransfer({kMiscCategory, 0}, world);
  CHECK(world.first_pending == before);
}

TEST_CASE(
    "F02981 prunes and stably compacts before appending missing categories",
    "[container-pending-rows][F02981]") {
  World world;
  tests::Populate(world.second);
  world.first_pending[0] = tests::kBladePendingToken + 1;
  world.first_pending[2] = tests::kMiscPendingToken;
  world.first_pending[kFourthPendingIndex] = tests::kBladePendingToken;
  world.first_pending.back() = tests::kMiscPendingToken;
  rows::RebuildPendingTransfers(world);
  constexpr auto kExpected = std::to_array<std::uint16_t>(
      {tests::kMiscPendingToken, tests::kBladePendingToken,
       tests::kMiscPendingToken, 1, 0x201, 0x301, 0x401, 0x501, 0});
  CHECK(std::ranges::equal(
      std::span(world.first_pending).first(kExpected.size()), kExpected));
}

TEST_CASE("F02981 retains duplicates and does not replace a full pending list",
          "[container-pending-rows][F02981]") {
  World world;
  tests::Populate(world.second);
  world.first_pending.fill(tests::kMiscPendingToken);
  rows::RebuildPendingTransfers(world);
  CHECK(std::ranges::count(world.first_pending, tests::kMiscPendingToken) ==
        kLastPosition);
  CHECK(world.first_pending.back() == tests::kMiscPendingToken);
}

TEST_CASE(
    "F02981 preserves misc slot 256 and repeated entry does not clamp its "
    "token",
    "[container-pending-rows][F02981][F02982]") {
  World world;
  world.second.storage.items.misc.back().id = tests::kMiscId;
  rows::RebuildPendingTransfers(world);
  CHECK(world.first_pending[0] == tests::kLastMiscPendingToken);
  CHECK(rows::BuildItemRows(Category::kManage, world) == 0);
  rows::RebuildPendingTransfers(world);
  CHECK(world.first_pending[0] == tests::kLastMiscPendingToken);
  CHECK(world.first_pending[1] == 0);
}

TEST_CASE(
    "F02981 captures the compacted list but reloads current staged and append "
    "owners",
    "[container-pending-rows][F02981]") {
  World world;
  world.second.storage.items.misc.front().id = tests::kMiscId;
  world.first.storage.items.blades.front().id = 1;
  world.first_pending[0] = tests::kBladePendingToken;
  world.second.on_read = [&] {
    world.staging = &world.first;
    world.pending = world.second_pending;
  };
  rows::RebuildPendingTransfers(world);
  CHECK(world.first_pending[0] == 0);
  CHECK(world.second_pending[0] == tests::kBladePendingToken);
  CHECK(world.second_pending[1] == 0);
  CHECK(world.second.reads == 1);
}

TEST_CASE(
    "F02981 retains completed pruning when a later record index is invalid",
    "[container-pending-rows][F02981]") {
  World world;
  world.first_pending[0] = tests::kBladePendingToken;
  world.first_pending[1] = tests::kBladePendingToken +
                           static_cast<std::uint16_t>(tests::kLastPartSlot) + 1;
  CHECK_THROWS_AS(rows::RebuildPendingTransfers(world), std::out_of_range);
  CHECK(world.first_pending[0] == 0);
  CHECK(world.first_pending[1] != 0);
}

TEST_CASE(
    "F02982 formats each category with real shared templates and equipment "
    "flags",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.first.storage.items.weapons.front().is_equipped = 1;
  world.first.storage.items.shields.front().is_equipped = 1;
  world.first.storage.items.armor.front().limb = 1;
  for (std::size_t value = 0; value < items::kItemCategoryCount - 1; ++value) {
    const auto category = static_cast<Category>(value);
    world.Select(category, std::array{kFirstToken});
    CHECK(rows::BuildItemRows(category, world) == 1);
    CHECK(world.output[0].stored);
    CHECK(world.output[0].text.description.has_value());
    CHECK(world.descriptions[0].front() != 0);
  }
  CHECK(world.types[0] == (1U << 9));
}

TEST_CASE(
    "F02982 chooses weapon and shield attachment restrictions from actual "
    "parents",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.first.storage.items.blades.front().assembled_weapon_index = 1;
  world.first.storage.items.gems.front().set_item_index =
      items::kShieldGemFlag | 1;
  world.Select(Category::kBlade, std::array{kFirstToken});
  CHECK(rows::BuildItemRows(Category::kBlade, world) == 1);
  CHECK((world.types[0] & tests::kRestrictionMask) == tests::kAttached);
  world.Select(Category::kGem, std::array{kFirstToken});
  world.first.storage.items.shields.front().is_equipped = 1;
  CHECK(rows::BuildItemRows(Category::kGem, world) == 1);
  CHECK((world.types[0] & tests::kRestrictionMask) == tests::kEquipped);
}

TEST_CASE(
    "F02982 pending rows retain their entry staging owner across workspace "
    "changes",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.second);
  world.first.storage.items.misc.front().id = tests::kMiscId + 1;
  world.first_pending[0] = tests::kMiscPendingToken;
  world.second_pending[0] = tests::kMiscPendingToken;
  world.callback = [&](Event event) {
    if (event == Event::kPending) {
      world.pending = world.second_pending;
      world.staging = &world.first;
    }
  };
  CHECK(rows::BuildItemRows(Category::kManage, world) == 1);
  const auto name = world.output[0].text.name;
  REQUIRE(name.has_value());
  if (!name) return;
  const auto offset = name->offset;
  CHECK(offset == tests::kMiscId * framework::kItemNameRowBytes);
  CHECK(world.output[0].records == &world.second);
  CHECK(world.staging_queries == 1);
}

TEST_CASE(
    "F02982 pending attachment rows and unknown categories preserve original "
    "output rules",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.second);
  world.second.storage.items.blades.front().assembled_weapon_index = 1;
  world.first_pending[0] = tests::kBladePendingToken;
  world.first_pending[1] = kUnknownToken;
  world.types[1] = kInitialType;
  CHECK(rows::BuildItemRows(Category::kManage, world) == 2);
  CHECK((world.types[0] & tests::kRestrictionMask) == tests::kPendingAttached);
  CHECK(world.types[1] == kInitialType);
  CHECK_FALSE(world.output[1].stored);
}

TEST_CASE(
    "F02982 refuses the first reached row outside its captured order span",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.Select(Category::kBlade, std::array{kFirstToken, kFirstToken});
  world.order_limit = items::kContainerCapacities.front() + 1;
  CHECK_THROWS_AS(rows::BuildItemRows(Category::kBlade, world),
                  std::out_of_range);
  CHECK(world.output[0].stored);
  CHECK_FALSE(world.output[1].stored);
}

TEST_CASE(
    "F02982 captures display order while each formatted active row uses the "
    "current owner",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.second.storage.items.misc[1].id = tests::kMiscId + 1;
  world.Select(Category::kMisc, std::array{kFirstToken, kSecondToken});
  world.on_store = [&](std::size_t) { world.active = &world.second; };
  CHECK(rows::BuildItemRows(Category::kMisc, world) == 2);
  CHECK(world.output[0].records == &world.first);
  CHECK(world.output[1].records == &world.second);
  CHECK(world.output[1].slot == 1);
  CHECK(world.order_queries == 1);
}

TEST_CASE(
    "F02982 reloads equipped state after formatting but keeps the borrowed "
    "weapon name",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.second.storage.items.weapons.front().is_equipped = 1;
  world.Select(Category::kWeapon, std::array{kFirstToken});
  world.on_store = [&](std::size_t) { world.active = &world.second; };
  CHECK(rows::BuildItemRows(Category::kWeapon, world) == 1);
  CHECK(world.output[0].records == &world.first);
  CHECK((world.types[0] & tests::kRestrictionMask) == tests::kEquipped);
}

TEST_CASE(
    "F02982 retains formatted output when a reached attachment parent is out "
    "of bounds",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.first.storage.items.blades.front().assembled_weapon_index =
      kParentOutsideWeaponTable;
  world.Select(Category::kBlade, std::array{kFirstToken});
  CHECK_THROWS_AS(rows::BuildItemRows(Category::kBlade, world),
                  std::out_of_range);
  CHECK(world.output[0].stored);
  CHECK(world.descriptions[0].front() != 0);
  CHECK((world.types[0] & tests::kRestrictionMask) == 0);
}

TEST_CASE(
    "F02982 builds all 256 misc rows even when the display capacity is zero",
    "[container-pending-rows][F02982]") {
  World world;
  world.limits[static_cast<std::size_t>(Category::kMisc)] = 0;
  std::array<std::uint16_t, tests::kRowCount> order{};
  for (std::size_t slot = 0; slot < order.size(); ++slot) {
    world.first.storage.items.misc[slot].id = tests::kMiscId;
    order[slot] = static_cast<std::uint16_t>(slot + 1);
  }
  world.Select(Category::kMisc, order);
  CHECK(rows::BuildItemRows(Category::kMisc, world) == tests::kRowCount);
  CHECK(world.output.back().stored);
  CHECK(world.output.back().slot == tests::kLastMiscSlot);
  CHECK(world.descriptions.back().front() != 0);
}

TEST_CASE(
    "F02982 does not roll back earlier rows after a short reached description",
    "[container-pending-rows][F02982]") {
  World world;
  tests::Populate(world.first);
  world.first.storage.items.misc[1].id = tests::kMiscId;
  world.short_description = 1;
  world.Select(Category::kMisc, std::array{kFirstToken, kSecondToken});
  CHECK_THROWS_AS(rows::BuildItemRows(Category::kMisc, world),
                  std::out_of_range);
  CHECK(world.output[0].stored);
  CHECK_FALSE(world.output[1].stored);
}
