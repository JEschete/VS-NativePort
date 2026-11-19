#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.hpp"
#include "Items/Inventory.h"
#include "Items/ItemRecords.hpp"
#include "Items/ItemRecordsLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Loot/LootRows.hpp"
#include "Menu/Loot/LootRowsLive.hpp"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.hpp"
#include "State/ScriptFlagsLive.hpp"
#include "State/StateFlags.hpp"

namespace {
namespace items = VS::Items;
namespace loot = VS::Menu::Loot;
namespace live = VS::Menu::Loot::Live;
using items::ItemCategory;
using items::ItemsStatus;
using loot::DropSlot;

class FakeCheck final : public loot::ClaimCheck {
 public:
  bool Fits(const loot::LootRow& row,
            const items::InventoryContents& /*staged*/) override {
    seen.push_back(row);
    return fits(row);
  }

  std::function<bool(const loot::LootRow&)> fits = [](const loot::LootRow&) {
    return true;
  };
  std::vector<loot::LootRow> seen;
};

loot::LootRow Row(ItemCategory category, std::uint8_t index) {
  return {.category = static_cast<std::uint8_t>(category), .item_index = index};
}

items::LootedWeapon Weapon() {
  items::LootedWeapon weapon{};
  weapon.header.difficulty_flags = 1;
  weapon.blade.id = 0x31;
  weapon.grip.id = 0x12;
  return weapon;
}

items::LootedArmor Armor(std::uint8_t id, std::uint8_t flags) {
  items::LootedArmor armor{};
  armor.header.difficulty_flags = flags;
  armor.armor.id = id;
  return armor;
}

struct ExpectedCount {
  ItemCategory counted;
  loot::LootRow row;
  std::int32_t count;
};

// A weapon, a refused armor and three misc stacks, of which only one counts.
loot::LootStaging StageMixedRecord() {
  items::BattleLoot record{};
  record.weapon = Weapon();
  record.blade.header.difficulty_flags = 2;
  record.blade.blade.id = 0x40;
  record.armor[1] = Armor(7, 3);
  record.misc[0] = {.item_id = 0, .difficulty_flags = 3, .count = 1};
  record.misc[1] = {.item_id = 323, .difficulty_flags = 1, .count = 2};
  record.misc[2] = {.item_id = 324, .difficulty_flags = 3, .count = 0};
  loot::LootStaging staging{};
  FakeCheck check;
  check.fits = [](const loot::LootRow& row) {
    return row.category != static_cast<std::uint8_t>(ItemCategory::kArmor);
  };
  loot::StageDropRecord(record, 7, staging, check);
  return staging;
}

items::BattleLoot ArmorAndMiscRecord() {
  items::BattleLoot record{};
  record.armor[0] = Armor(7, 1);
  record.misc[0] = {.item_id = 323, .difficulty_flags = 1, .count = 2};
  return record;
}

// Eighteen armors: the first batch fills the sixteen armor slots.
std::array<items::BattleLoot, 9> ArmorRecords() {
  std::array<items::BattleLoot, 9> records{};
  for (std::size_t i = 0; i < records.size(); ++i) {
    for (std::size_t armor = 0; armor < 2; ++armor) {
      records[i].armor[armor] =
          Armor(static_cast<std::uint8_t>(1 + i * 2 + armor), 1);
    }
  }
  return records;
}

template <typename Entry>
bool IsBound(unsigned address, Entry entry) {
  return VS::Content::ResolveModuleAddress(VS::Content::ModuleSymbols(),
                                           "MENU/MENUB.PRG", address) ==
         reinterpret_cast<const void*>(entry);
}

struct LiveRowsFixture {
  Items_Inventory staged{};
  std::array<Menu_Loot_LootListItem, VS::Menu::Loot::kRowCapacity> rows{};
  Items_Inventory* previous_inventory{Menu_Loot_inventory};
  Menu_Loot_LootListItem* previous_rows{Menu_Loot_lootList};
  Items_Inventory previous_ashley{VS::Items::inventory};
  Items_InventoryIndices previous_indices{VS::Items::inventoryIndices};
  Items_BattleLoot* previous_entry{VS::Menu::Framework::lootEntry};
  std::uint8_t previous_difficulty{VS::State::stateFlags.difficulty};

  LiveRowsFixture() {
    VS::Menu::Loot::Live::ResetPaging();
    Menu_Loot_inventory = &staged;
    Menu_Loot_lootList = rows.data();
    Menu_Loot_lootListCount = 0;
    Menu_Loot_lootListOffset = 0;
    Menu_Loot_selectedLoot = 0;
    VS::Items::inventory = {};
    VS::Items::inventoryIndices = {};
    VS::State::stateFlags.difficulty = 0;
  }

  ~LiveRowsFixture() {
    VS::Menu::Loot::Live::ResetPaging();
    Menu_Loot_inventory = previous_inventory;
    Menu_Loot_lootList = previous_rows;
    Menu_Loot_lootListCount = 0;
    VS::Items::inventory = previous_ashley;
    VS::Items::inventoryIndices = previous_indices;
    VS::Menu::Framework::lootEntry = previous_entry;
    VS::State::stateFlags.difficulty = previous_difficulty;
  }

  LiveRowsFixture(const LiveRowsFixture&) = delete;
  LiveRowsFixture& operator=(const LiveRowsFixture&) = delete;
};
}  // namespace

TEST_CASE("F02853 refresh marks each listed row with the claim check",
          "[loot-rows][F02853]") {
  loot::LootStaging staging{};
  staging.cursor.count = 3;
  for (std::uint8_t i = 0; i < 4; ++i) {
    staging.rows[i] = Row(static_cast<ItemCategory>(i), i);
    staging.rows[i].state = 9;
  }
  FakeCheck check;
  check.fits = [](const loot::LootRow& row) {
    return row.category != static_cast<std::uint8_t>(ItemCategory::kBlade);
  };
  loot::RefreshClaimableRows(staging, check);
  CHECK(check.seen.size() == 3);
  CHECK(staging.rows[0].state == 1);
  CHECK(staging.rows[1].state == 0);
  CHECK(staging.rows[2].state == 1);
  CHECK(staging.rows[3].state == 9);
}

TEST_CASE("F02853 refresh keeps the selection inside the visible rows",
          "[loot-rows][F02853]") {
  struct Case {
    loot::LootCursor before;
    loot::LootCursor after;
  };
  const auto [before, after] =
      GENERATE(Case{{0, 0, 0}, {0, 0, 0xFF}}, Case{{5, 0, 6}, {5, 0, 4}},
               Case{{5, 3, 1}, {5, 0, 4}}, Case{{8, 0, 7}, {8, 0, 7}},
               Case{{9, 0, 7}, {9, 1, 6}}, Case{{20, 15, 3}, {20, 12, 6}},
               Case{{20, 4, 7}, {20, 5, 6}}, Case{{20, 12, 7}, {20, 12, 7}},
               Case{{20, 30, 0}, {20, 12, 7}});
  loot::LootStaging staging{};
  staging.cursor = before;
  FakeCheck check;
  loot::RefreshClaimableRows(staging, check);
  CHECK(staging.cursor == after);
}

TEST_CASE("F02854 append adds a zero-based row and refuses a full list",
          "[loot-rows][F02854]") {
  loot::LootStaging staging{};
  staging.rows[0].slide_step = 5;
  staging.rows[0].state = 1;
  REQUIRE(loot::AppendLootRow(staging, ItemCategory::kArmor, 3));
  CHECK(staging.rows[0] == loot::LootRow{4, 2, 0, 1});
  REQUIRE(loot::AppendLootRow(staging, ItemCategory::kMisc, 0));
  CHECK(staging.rows[1].item_index == 0xFF);
  CHECK(staging.cursor.count == 2);

  staging.cursor.count = loot::kLootRowCapacity;
  const auto rows = staging.rows;
  CHECK_FALSE(loot::AppendLootRow(staging, ItemCategory::kGem, 1));
  CHECK(staging.rows == rows);
  CHECK(staging.cursor.count == loot::kLootRowCapacity);
}

TEST_CASE("F02855 counts a weapon's parts and its socketed gems",
          "[loot-rows][F02855]") {
  items::InventoryContents inventory{};
  inventory.weapons[2].blade = 1;
  inventory.weapons[2].grip = 3;
  inventory.weapons[2].gems = {5, 0, 7, 8};
  inventory.grips[2].gem_slots = 3;
  inventory.shields[1].base.gem_slots = 2;
  inventory.shields[1].gems = {1, 2, 3, 0};
  const auto weapon = Row(ItemCategory::kWeapon, 2);
  const auto blade = Row(ItemCategory::kBlade, 0);
  const std::array<ExpectedCount, 10> expected{{
      {ItemCategory::kWeapon, weapon, 1},
      {ItemCategory::kBlade, weapon, 1},
      {ItemCategory::kGrip, weapon, 1},
      {ItemCategory::kGem, weapon, 2},
      {ItemCategory::kShield, weapon, 0},
      {ItemCategory::kManage, weapon, 0},
      {ItemCategory::kBlade, blade, 1},
      {ItemCategory::kGrip, blade, 0},
      {ItemCategory::kGem, Row(ItemCategory::kGem, 4), 1},
      {ItemCategory::kGem, Row(ItemCategory::kShield, 1), 2},
  }};
  for (const auto& [counted, row, count] : expected) {
    CAPTURE(static_cast<int>(counted), static_cast<int>(row.category),
            static_cast<int>(row.item_index));
    CHECK(loot::CountContainedCategory(counted, row, inventory) ==
          loot::CategoryCount{count});
  }
}

TEST_CASE("F02855 refuses part and socket reads outside the inventory",
          "[loot-rows][F02855]") {
  items::InventoryContents inventory{};
  const auto weapon = Row(ItemCategory::kWeapon, 0);
  CHECK(loot::CountContainedCategory(ItemCategory::kGem, weapon, inventory) ==
        loot::CategoryCount{0, ItemsStatus::kPartReferenceOutOfRange});
  inventory.weapons[0].grip = items::kGripSlots + 1;
  CHECK(loot::CountContainedCategory(ItemCategory::kGem, weapon, inventory) ==
        loot::CategoryCount{0, ItemsStatus::kPartReferenceOutOfRange});
  inventory.weapons[0].grip = 1;
  inventory.weapons[0].gems = {1, 1, 1, 1};
  inventory.grips[0].gem_slots = 5;
  CHECK(loot::CountContainedCategory(ItemCategory::kGem, weapon, inventory) ==
        loot::CategoryCount{4, ItemsStatus::kSlotOutOfRange});
  CHECK(loot::CountContainedCategory(
            ItemCategory::kGem, Row(ItemCategory::kWeapon, items::kWeaponSlots),
            inventory) == loot::CategoryCount{0, ItemsStatus::kSlotOutOfRange});
  CHECK(loot::CountContainedCategory(
            ItemCategory::kGem, Row(ItemCategory::kShield, items::kShieldSlots),
            inventory) == loot::CategoryCount{0, ItemsStatus::kSlotOutOfRange});
}

TEST_CASE("F02857 a drop needs the bit of the current difficulty",
          "[loot-rows][F02857]") {
  CHECK(loot::DropDifficultyMatch(3, 0) == 1);
  CHECK(loot::DropDifficultyMatch(2, 0) == 0);
  CHECK(loot::DropDifficultyMatch(2, 1) == 2);
  CHECK(loot::DropDifficultyMatch(4, 1) == 0);
  CHECK(loot::DropDifficultyMatch(0xFF, 0xFF) == 0);
  CHECK(loot::DropDifficultyMatch(-1, 0xFF) == 0x100);
}

TEST_CASE("F02858 lists eligible drops in record order",
          "[loot-rows][F02858]") {
  const auto staging = StageMixedRecord();
  REQUIRE(staging.cursor == loot::LootCursor{3, 0, 0});
  const std::array<loot::LootRow, 3> rows{
      {{0, 0, 0, 1}, {4, 0, 0, 0}, {6, 0, 0, 1}}};
  CHECK(std::ranges::equal(std::span(staging.rows).first(rows.size()), rows));
  const std::array<loot::StagedDrop, 3> drops{
      {{7, DropSlot::kWeapon}, {7, DropSlot::kArmor1}, {7, DropSlot::kMisc1}}};
  REQUIRE(staging.batch.count == drops.size());
  CHECK(std::ranges::equal(std::span(staging.batch.drops).first(drops.size()),
                           drops));
  CHECK_FALSE(staging.batch.deferred);
}

TEST_CASE("F02858 stages eligible drops in the working inventory",
          "[loot-rows][F02858]") {
  const auto staging = StageMixedRecord();
  CHECK(staging.inventory.blades[0].id == 0x31);
  CHECK(staging.inventory.blades[1].id == 0);
  CHECK(staging.inventory.armor[0].id == 7);
  CHECK(staging.inventory.misc[0].id == 323);
  CHECK(staging.inventory.misc[0].count == 2);
}

TEST_CASE("F02858 defers a drop with no bag room", "[loot-rows][F02858]") {
  loot::LootStaging staging{};
  for (std::size_t i = 0; i < items::kArmorSlots; ++i) {
    staging.inventory.armor[i].id = static_cast<std::uint8_t>(i + 1);
  }
  FakeCheck check;
  loot::StageDropRecord(ArmorAndMiscRecord(), 0, staging, check);
  CHECK(staging.cursor.count == 1);
  CHECK(staging.rows[0].category ==
        static_cast<std::uint8_t>(ItemCategory::kMisc));
  CHECK(staging.batch.count == 1);
  CHECK(staging.batch.deferred);
}

TEST_CASE("F02858 defers eligible drops when no row is free",
          "[loot-rows][F02858]") {
  loot::LootStaging full{};
  full.cursor.count = loot::kLootRowCapacity;
  full.cursor.offset = loot::kLootRowCapacity - loot::kVisibleLootRows;
  FakeCheck check;
  loot::StageDropRecord(ArmorAndMiscRecord(), 0, full, check);
  CHECK(full.inventory == items::InventoryContents{});
  CHECK(full.batch.count == 0);
  CHECK(full.batch.deferred);

  auto ineligible = ArmorAndMiscRecord();
  ineligible.armor[0].header.difficulty_flags = 2;
  ineligible.misc[0].difficulty_flags = 2;
  loot::LootStaging untouched{};
  untouched.cursor.count = loot::kLootRowCapacity;
  loot::StageDropRecord(ineligible, 0, untouched, check);
  CHECK_FALSE(untouched.batch.deferred);
}

TEST_CASE("F02859 rebuilds the working inventory from every record",
          "[loot-rows][F02859]") {
  std::array<items::BattleLoot, 2> records{};
  records[0].armor[0] = Armor(10, 1);
  records[1].misc[0] = {.item_id = 323, .difficulty_flags = 1, .count = 1};
  loot::LootStaging staging{};
  staging.inventory.misc[5].id = 99;
  staging.cursor = {5, 2, 3};
  staging.batch.count = 1;
  staging.batch.deferred = true;
  FakeCheck check;
  loot::BuildStagedInventory(records, staging, check);

  CHECK(staging.inventory.armor[0].id == 10);
  CHECK(staging.inventory.misc[0].id == 323);
  CHECK(staging.inventory.misc[5].id == 0);
  CHECK(staging.cursor == loot::LootCursor{2, 0, 0});
  const std::array<loot::StagedDrop, 2> drops{
      {{0, DropSlot::kArmor0}, {1, DropSlot::kMisc0}}};
  REQUIRE(staging.batch.count == drops.size());
  CHECK(std::ranges::equal(std::span(staging.batch.drops).first(drops.size()),
                           drops));
  CHECK_FALSE(staging.batch.deferred);
}

TEST_CASE("F02859 advancing without a deferred batch leaves records alone",
          "[loot-rows][F02859]") {
  std::array<items::BattleLoot, 1> records{};
  records[0].armor[0] = Armor(10, 1);
  loot::LootStaging staging{};
  FakeCheck check;
  loot::BuildStagedInventory(records, staging, check);
  const auto before = records;
  CHECK_FALSE(loot::AdvanceStagedInventory(records, staging, check));
  CHECK(records == before);
}

TEST_CASE("F02859 advancing clears the claimed batch", "[loot-rows][F02859]") {
  auto records = ArmorRecords();
  loot::LootStaging staging{};
  FakeCheck check;
  loot::BuildStagedInventory(records, staging, check);
  REQUIRE(staging.cursor.count == items::kArmorSlots);
  REQUIRE(staging.batch.deferred);

  REQUIRE(loot::AdvanceStagedInventory(records, staging, check));
  const auto cleared = [](const items::BattleLoot& record) {
    return record.armor[0].header.difficulty_flags == 0 &&
           record.armor[1].header.difficulty_flags == 0;
  };
  CHECK(std::ranges::all_of(std::span(records).first(8), cleared));
  CHECK(records[8].armor[0].header.difficulty_flags == 1);
}

TEST_CASE("F02859 advancing stages the deferred drops", "[loot-rows][F02859]") {
  auto records = ArmorRecords();
  loot::LootStaging staging{};
  FakeCheck check;
  loot::BuildStagedInventory(records, staging, check);
  staging.cursor.selected = 4;
  REQUIRE(loot::AdvanceStagedInventory(records, staging, check));
  CHECK(staging.cursor == loot::LootCursor{2, 0, 0});
  CHECK(staging.inventory.armor[0].id == 17);
  CHECK(staging.inventory.armor[1].id == 18);
  CHECK_FALSE(staging.batch.deferred);
  CHECK_FALSE(loot::AdvanceStagedInventory(records, staging, check));
}

TEST_CASE("F02866 key items from 458 raise their script flag",
          "[loot-rows][F02866]") {
  CHECK_FALSE(loot::ClaimedKeyFlag(457).has_value());
  CHECK(loot::ClaimedKeyFlag(458) == std::optional<std::int16_t>(896));
  CHECK(loot::ClaimedKeyFlag(467) == std::optional<std::int16_t>(905));
  CHECK(loot::ClaimedKeyFlag(32400) == std::optional<std::int16_t>(-32698));
}

TEST_CASE("Loot rows are bound at the original MENUB addresses",
          "[loot-rows][F02853][F02854][F02855][F02857][F02858][F02859]"
          "[F02866]") {
  CHECK(IsBound(0x801080F0U, &live::RefreshClaimableRows));
  CHECK(IsBound(0x8010822CU, &live::AppendLootRow));
  CHECK(IsBound(0x80108264U, &live::CountContainedCategory));
  CHECK(IsBound(0x801086DCU, &live::IsDropAllowed));
  CHECK(IsBound(0x801086F4U, &live::StageDropRecord));
  CHECK(IsBound(0x801088D4U, &live::BuildStagedInventory));
  CHECK(IsBound(0x8010A24CU, &live::RecordClaimedKey));
}

TEST_CASE("Live loot rows use the screen's rows, cursor and difficulty",
          "[loot-rows][F02854][F02855][F02857]") {
  LiveRowsFixture fixture;
  VS::State::stateFlags.difficulty = 1;
  CHECK(live::IsDropAllowed(2) == 2);
  CHECK(live::IsDropAllowed(1) == 0);

  live::AppendLootRow(Items_ItemCategory_Armor, 3);
  CHECK(Menu_Loot_lootListCount == 1);
  CHECK(fixture.rows[0].itemCategory == Items_ItemCategory_Armor);
  CHECK(fixture.rows[0].itemIndex == 2);
  live::AppendLootRow(static_cast<int>(ItemCategory::kManage), 1);
  CHECK(Menu_Loot_lootListCount == 1);

  CHECK(live::CountContainedCategory(Items_ItemCategory_Armor,
                                     &fixture.rows[0]) == 1);
  CHECK(live::CountContainedCategory(8, &fixture.rows[0]) == 0);
  CHECK(live::CountContainedCategory(Items_ItemCategory_Armor, nullptr) == 0);
}

TEST_CASE("Live chest staging pages the single loot entry",
          "[loot-rows][F02853][F02858][F02859]") {
  LiveRowsFixture fixture;
  Items_BattleLoot chest{};
  items::BattleLoot typed{};
  typed.misc[0] = {.item_id = 323, .difficulty_flags = 3, .count = 2};
  typed.misc[1] = {.item_id = 467, .difficulty_flags = 3, .count = 1};
  items::Live::Commit(chest, typed);
  VS::Menu::Framework::lootEntry = &chest;
  live::BuildStagedInventory(nullptr);
  REQUIRE(Menu_Loot_lootListCount == 2);
  CHECK(fixture.rows[0].state == 1);
  CHECK(fixture.rows[1].state == 1);
  CHECK(fixture.staged.misc[0].id == 323);
  CHECK(VS::Menu::Loot::Live::AdvanceStagedInventory() == 0);
  CHECK(items::Live::Typed(chest) == typed);
}

TEST_CASE("Live key claims set the key item's script flag",
          "[loot-rows][F02866]") {
  constexpr int kFlag = 458 + loot::kKeyItemFlagBase;
  const auto previous = VS::State::LiveScriptFlag(kFlag);
  VS::State::SetLiveScriptFlag(kFlag, 0);
  live::RecordClaimedKey(457);
  CHECK(VS::State::LiveScriptFlag(kFlag) == 0);
  live::RecordClaimedKey(458);
  CHECK(VS::State::LiveScriptFlag(kFlag) == 1);
  VS::State::SetLiveScriptFlag(kFlag, previous);
}
