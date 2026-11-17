#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Core/Memory/HeapLive.hpp"
#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/ItemRows.hpp"
#include "Menu/Framework/ItemRowsLive.hpp"
#include "Menu/Inventory/ItemListRows.hpp"
#include "Menu/Inventory/ItemListRowsLive.hpp"
#include "Menu/InventorySortingLiveFixture.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Inventory/Inventory.h"
}

namespace {
namespace inventory = VS::Menu::Inventory;
namespace framework = VS::Menu::Framework;
using inventory::ItemRowsContext;
using inventory::ItemRowsRefusal;
using VS::Items::InventoryContents;
using VS::Items::ItemCategory;

constexpr int kWeapon = 0;
constexpr int kBlade = 1;
constexpr int kGrip = 2;
constexpr int kShield = 3;
constexpr int kArmor = 4;
constexpr int kGem = 5;
constexpr int kMisc = 6;
constexpr int kManage = 7;
constexpr std::uint16_t kShieldThreeGem = 0x183;
constexpr std::uint16_t kWideWeaponLink = 0x105;
constexpr std::uint8_t kAllShieldBits = 0xFF;
constexpr std::uint8_t kSlotMask = 0x7F;
constexpr std::uint8_t kMiscAction = 0xE0;
constexpr std::uint8_t kLowAction = 105;
constexpr std::uint8_t kUpgradable = 106;
constexpr std::uint8_t kLastUpgradable = 133;
constexpr std::uint8_t kAboveUpgradable = 134;
constexpr std::uint16_t kFirstMisc = inventory::kFirstMiscActionItem;
constexpr std::uint16_t kLastMisc = kFirstMisc + 133;
constexpr std::int32_t kRememberedRow = 3;
constexpr std::int32_t kSlidingRow = 22;
constexpr std::int16_t kTargetX = 77;
constexpr std::int16_t kSlidingX = 300;
constexpr std::uint32_t kFormattedType = 0x11;
constexpr std::uint8_t kOutsideWeapon = 9;
constexpr std::int32_t kWrappedCursor = 58;

struct FormatCall {
  ItemCategory category;
  std::size_t slot;
  std::size_t row;
  friend bool operator==(const FormatCall&, const FormatCall&) = default;
};

class FakeRowsWorld final : public inventory::ItemRowsWorld {
 public:
  std::vector<FormatCall> formats;
  std::vector<std::uint32_t> types = std::vector<std::uint32_t>(256);
  std::vector<std::pair<std::size_t, std::uint8_t>> actions;
  std::vector<std::uint32_t> unlocked;
  std::size_t writes = 0;

  bool ActionUnlocked(std::uint32_t action) override {
    return std::ranges::find(unlocked, action) != unlocked.end();
  }
  void FormatRow(ItemCategory category, std::size_t slot,
                 std::size_t row) override {
    formats.push_back({category, slot, row});
    types[row] = kFormattedType;
    ++writes;
  }
  void AddRowFlags(std::size_t row, std::uint32_t flags) override {
    types[row] |= flags;
    ++writes;
  }
  void StoreMiscAction(std::size_t row, std::uint8_t action) override {
    actions.emplace_back(row, action);
    ++writes;
  }
};

std::array<std::uint8_t, inventory::kMiscActionMapEntries> MiscMap() {
  std::array<std::uint8_t, inventory::kMiscActionMapEntries> map{};
  map.fill(kMiscAction);
  return map;
}

inventory::ItemRowsInput Input(
    ItemCategory category, std::span<const std::uint8_t> order,
    std::span<const std::uint8_t> map = {},
    ItemRowsContext context = ItemRowsContext::kItemMenu) {
  return {.category = category,
          .context = context,
          .order = order,
          .capacity = static_cast<std::uint8_t>(order.size()),
          .misc_action_map = map,
          .row_capacity = 256};
}

class FakeRestoreWorld final : public inventory::ItemRowsRestoreWorld {
 public:
  std::array<framework::MenuRow, framework::kRowCount> rows{};
  std::vector<std::uint8_t> memory = std::vector<std::uint8_t>(0xF2);
  std::uint8_t list_row = 16;
  std::uint8_t step = 1;
  int advances = 0;
  int row_calls = 0;
  int missing_row = -1;
  // The main pass call that rewrites the cursor, or -1.
  int rewrite_call = -1;
  std::size_t rewrite_index = 0;
  std::uint8_t rewrite_value = 0;

  std::uint8_t ListAnimationRow() override { return list_row; }
  void AdvanceListRows() override {
    ++advances;
    list_row = static_cast<std::uint8_t>(list_row + step);
  }
  framework::MenuRow* Row(std::int32_t index) override {
    ++row_calls;
    if (row_calls == rewrite_call) memory[rewrite_index] = rewrite_value;
    if (index == missing_row) return nullptr;
    return &rows[static_cast<std::size_t>(index)];
  }
  std::span<const std::uint8_t> CursorMemory() override { return memory; }
};

std::size_t CursorIndex(int category) {
  return static_cast<std::size_t>(category + inventory::kItemListCursorBase) *
         2;
}

// Weapon 1 and shield 3 are equipped; blades, gems and armor link to them.
InventoryContents AssembledItems() {
  InventoryContents items{};
  items.weapons[0].is_equipped = 1;
  items.shields[2].is_equipped = 1;
  items.blades[0].assembled_weapon_index = 1;
  items.blades[1].assembled_weapon_index = 2;
  items.gems[0].set_item_index = kShieldThreeGem;
  items.gems[1].set_item_index = VS::Items::kShieldGemFlag | 1U;
  items.gems[2].set_item_index = 1;
  items.armor[0].limb = 2;
  return items;
}

// The refusal of a list, which must leave the world untouched.
std::optional<ItemRowsRefusal> Refusal(const inventory::ItemRowsInput& input,
                                       const InventoryContents& items) {
  FakeRowsWorld world;
  const auto rows = inventory::BuildItemRows(input, items, world);
  CHECK(world.writes == 0);
  if (rows) return std::nullopt;
  return rows.error();
}
}  // namespace

TEST_CASE("Inventory ParentWeaponIndex reads blade grip and weapon gem links",
          "[inventory-rows][F02593]") {
  InventoryContents items{};
  items.blades[3].assembled_weapon_index = 3;
  items.grips[15].assembled_weapon_index = kWideWeaponLink;
  items.gems[0].set_item_index = 2;
  items.gems[47].set_item_index = kShieldThreeGem;
  CHECK(inventory::ParentWeaponIndex(kBlade, 3, items) == 3);
  CHECK(inventory::ParentWeaponIndex(kGrip, 15, items) == kWideWeaponLink);
  CHECK(inventory::ParentWeaponIndex(kGem, 0, items) == 2);
  CHECK(inventory::ParentWeaponIndex(kGem, 47, items) == 0);
  CHECK(inventory::ParentWeaponIndex(kBlade, 0, items) == 0);
}

TEST_CASE("Inventory ParentWeaponIndex skips other categories without reads",
          "[inventory-rows][F02593]") {
  const InventoryContents items{};
  for (const int category : {kWeapon, kShield, kArmor, kMisc, kManage, -1}) {
    CHECK(inventory::ParentWeaponIndex(category, 1000, items) == 0);
  }
  CHECK(inventory::ParentWeaponIndex(kBlade, 16, items) == std::nullopt);
  CHECK(inventory::ParentWeaponIndex(kGrip, -1, items) == std::nullopt);
  CHECK(inventory::ParentWeaponIndex(kGem, 48, items) == std::nullopt);
}

TEST_CASE("Inventory ParentShieldIndex keeps the slot bits of shield gems",
          "[inventory-rows][F02594]") {
  InventoryContents items{};
  items.gems[0].set_item_index = kShieldThreeGem;
  items.gems[1].set_item_index = 3;
  items.gems[2].set_item_index = VS::Items::kShieldGemFlag;
  items.gems[3].set_item_index = kAllShieldBits;
  CHECK(inventory::ParentShieldIndex(kGem, 0, items) == 3);
  CHECK(inventory::ParentShieldIndex(kGem, 1, items) == 0);
  CHECK(inventory::ParentShieldIndex(kGem, 2, items) == 0);
  CHECK(inventory::ParentShieldIndex(kGem, 3, items) == kSlotMask);
  CHECK(inventory::ParentShieldIndex(kBlade, 0, items) == 0);
  CHECK(inventory::ParentShieldIndex(kWeapon, 1000, items) == 0);
  CHECK(inventory::ParentShieldIndex(kGem, 48, items) == std::nullopt);
  CHECK(inventory::ParentShieldIndex(kGem, -1, items) == std::nullopt);
}

TEST_CASE("Inventory BuildItemRows formats the order until its first gap",
          "[inventory-rows][F02595]") {
  InventoryContents items{};
  items.weapons[1].is_equipped = 1;
  const std::array<std::uint8_t, 4> order{2, 1, 0, 3};
  FakeRowsWorld world;
  const auto rows = inventory::BuildItemRows(
      Input(ItemCategory::kWeapon, order), items, world);
  REQUIRE(rows == 2);
  CHECK(world.formats ==
        std::vector<FormatCall>{{ItemCategory::kWeapon, 1, 0},
                                {ItemCategory::kWeapon, 0, 1}});
  CHECK(world.types[0] == (kFormattedType | inventory::kEquippedRowFlags));
  CHECK(world.types[1] == kFormattedType);
  CHECK(world.actions.empty());
}

TEST_CASE("Inventory BuildItemRows flags equipped and assembled parts",
          "[inventory-rows][F02595]") {
  const auto items = AssembledItems();
  const std::array<std::uint8_t, 3> three{1, 2, 3};
  FakeRowsWorld blades;
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kBlade, three), items,
                                   blades) == 3);
  CHECK(blades.types[0] == (kFormattedType | inventory::kEquippedRowFlags));
  CHECK(blades.types[1] == (kFormattedType | inventory::kAssembledRowFlags));
  CHECK(blades.types[2] == kFormattedType);
  FakeRowsWorld armor;
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kArmor, three), items,
                                   armor) == 3);
  CHECK(armor.types[0] == (kFormattedType | inventory::kEquippedRowFlags));
  CHECK(armor.types[1] == kFormattedType);
}

TEST_CASE("Inventory BuildItemRows flags gems by their weapon or shield",
          "[inventory-rows][F02595]") {
  const auto items = AssembledItems();
  const std::array<std::uint8_t, 3> three{1, 2, 3};
  FakeRowsWorld gems;
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kGem, three), items,
                                   gems) == 3);
  CHECK(gems.types[0] == (kFormattedType | inventory::kEquippedRowFlags));
  CHECK(gems.types[1] == (kFormattedType | inventory::kAssembledRowFlags));
  CHECK(gems.types[2] == (kFormattedType | inventory::kEquippedRowFlags));
  FakeRowsWorld shields;
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kShield, three), items,
                                   shields) == 3);
  CHECK(shields.types[2] == (kFormattedType | inventory::kEquippedRowFlags));
  CHECK(shields.types[0] == kFormattedType);
}

TEST_CASE("Inventory BuildItemRows steps item menu misc actions when unlocked",
          "[inventory-rows][F02595]") {
  InventoryContents items{};
  auto map = MiscMap();
  map[0] = kLowAction;
  map[1] = kUpgradable;
  map[2] = kLastUpgradable;
  map[3] = kAboveUpgradable;
  for (std::size_t slot = 0; slot < 4; ++slot) {
    items.misc[slot].id = static_cast<std::uint16_t>(kFirstMisc + slot);
  }
  items.misc[4].id = kLastMisc;
  const std::array<std::uint8_t, 5> order{1, 2, 3, 4, 5};
  FakeRowsWorld world;
  world.unlocked = {kLowAction,      kUpgradable,          kUpgradable + 1U,
                    kLastUpgradable, kLastUpgradable + 1U, kLastUpgradable + 2U,
                    kAboveUpgradable};
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kMisc, order, map),
                                   items, world) == 5);
  using Action = std::pair<std::size_t, std::uint8_t>;
  CHECK(world.actions == std::vector<Action>{{0, kLowAction},
                                             {1, kUpgradable + 2},
                                             {2, kLastUpgradable + 3},
                                             {3, kAboveUpgradable},
                                             {4, kMiscAction}});
  CHECK(world.types[0] == kFormattedType);
}

TEST_CASE("Inventory BuildItemRows loot lists skip the misc action map",
          "[inventory-rows][F02595]") {
  InventoryContents items{};
  items.misc[0].id = 1;
  const std::array<std::uint8_t, 2> order{1, 0};
  FakeRowsWorld world;
  REQUIRE(inventory::BuildItemRows(
              Input(ItemCategory::kMisc, order, {}, ItemRowsContext::kLoot),
              items, world) == 1);
  CHECK(world.actions.empty());
  CHECK(world.formats.size() == 1);
  FakeRowsWorld menu;
  CHECK(inventory::BuildItemRows(Input(ItemCategory::kMisc, order), items, menu)
            .error() == ItemRowsRefusal::kMiscItem);
}

TEST_CASE(
    "Inventory BuildItemRows stops at the capacity and formats no manage rows",
    "[inventory-rows][F02595]") {
  const InventoryContents items{};
  const std::array<std::uint8_t, 4> order{1, 2, 3, 4};
  auto input = Input(ItemCategory::kWeapon, order);
  input.capacity = 2;
  FakeRowsWorld world;
  REQUIRE(inventory::BuildItemRows(input, items, world) == 2);
  CHECK(world.formats.size() == 2);
  input.capacity = 0;
  FakeRowsWorld empty;
  REQUIRE(inventory::BuildItemRows(input, items, empty) == 0);
  CHECK(empty.writes == 0);
  const std::array<std::uint8_t, 3> manage{1, 0xFF, 0};
  FakeRowsWorld rows;
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kManage, manage), items,
                                   rows) == 2);
  CHECK(rows.formats.empty());
  CHECK(rows.types[0] == 0);
  CHECK(rows.writes == 2);
}

TEST_CASE("Inventory BuildItemRows refuses invalid lists before writing",
          "[inventory-rows][F02595]") {
  InventoryContents items{};
  items.blades[0].assembled_weapon_index = kOutsideWeapon;
  const auto refusal = [&](const inventory::ItemRowsInput& input) {
    return Refusal(input, items);
  };
  const std::array<std::uint8_t, 2> two{1, 2};
  auto past_order = Input(ItemCategory::kWeapon, two);
  past_order.capacity = 3;
  CHECK(refusal(past_order) == ItemRowsRefusal::kOrder);
  const std::array<std::uint8_t, 2> past_slot{1, 9};
  CHECK(refusal(Input(ItemCategory::kWeapon, past_slot)) ==
        ItemRowsRefusal::kSlot);
  const std::array<std::uint8_t, 1> one{1};
  CHECK(refusal(Input(ItemCategory::kBlade, one)) ==
        ItemRowsRefusal::kParentWeapon);
  items.gems[0].set_item_index = VS::Items::kShieldGemFlag | 9U;
  CHECK(refusal(Input(ItemCategory::kGem, one)) ==
        ItemRowsRefusal::kParentShield);
  auto rows = Input(ItemCategory::kWeapon, two);
  rows.row_capacity = 1;
  CHECK(refusal(rows) == ItemRowsRefusal::kRows);
}

TEST_CASE("Inventory BuildItemRows refuses invalid misc lists before writing",
          "[inventory-rows][F02595]") {
  InventoryContents items{};
  const auto refusal = [&](const inventory::ItemRowsInput& input) {
    return Refusal(input, items);
  };
  const std::array<std::uint8_t, 1> one{1};
  const auto map = MiscMap();
  items.misc[0].id = kFirstMisc - 1;
  CHECK(refusal(Input(ItemCategory::kMisc, one, map)) ==
        ItemRowsRefusal::kMiscItem);
  items.misc[0].id = kLastMisc + 1;
  CHECK(refusal(Input(ItemCategory::kMisc, one, map)) ==
        ItemRowsRefusal::kMiscItem);
  items.misc.fill({.id = kFirstMisc, .count = 1, .index = 0});
  std::array<std::uint8_t, inventory::kMiscActionRows + 1> long_misc{};
  long_misc.fill(1);
  CHECK(refusal(Input(ItemCategory::kMisc, long_misc, map)) ==
        ItemRowsRefusal::kMiscRows);
}

TEST_CASE("Inventory BuildItemRows rebuilds identical rows on reentry",
          "[inventory-rows][F02595]") {
  InventoryContents items{};
  items.weapons[2].is_equipped = 1;
  items.grips[0].assembled_weapon_index = 3;
  const std::array<std::uint8_t, 2> order{1, 0};
  FakeRowsWorld world;
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kGrip, order), items,
                                   world) == 1);
  const auto first = world.types[0];
  REQUIRE(inventory::BuildItemRows(Input(ItemCategory::kGrip, order), items,
                                   world) == 1);
  CHECK(world.types[0] == first);
  CHECK(first == (kFormattedType | inventory::kEquippedRowFlags));
  CHECK(world.formats.size() == 2);
}

TEST_CASE("Inventory RestoreItemRows finishes the list and reselects its row",
          "[inventory-rows][F02596]") {
  FakeRestoreWorld world;
  world.list_row = 13;
  world.memory[CursorIndex(kGem)] = kRememberedRow;
  auto& sliding = world.rows[kSlidingRow];
  sliding.state = framework::RowState::kSlideX;
  sliding.x = kSlidingX;
  sliding.target0 = kTargetX;
  world.rows[0].selected = 1;
  world.rows[30].selected = 1;
  world.rows[31].state = framework::RowState::kSlideY;
  REQUIRE(inventory::RestoreItemRows(world, kGem).has_value());
  CHECK(world.advances == 3);
  CHECK(sliding.state == framework::RowState::kIdle);
  CHECK(sliding.x == kTargetX);
  CHECK(world.rows[31].state == framework::RowState::kSlideY);
  CHECK(world.rows[0].selected == 1);
  for (int row = inventory::kFirstItemListRow; row < framework::kNoRow; ++row) {
    CHECK(world.rows[static_cast<std::size_t>(row)].selected ==
          (row == inventory::kFirstItemListRow + kRememberedRow ? 1 : 0));
  }
}

TEST_CASE("Inventory RestoreItemRows rereads the cursor for every row",
          "[inventory-rows][F02596]") {
  FakeRestoreWorld world;
  world.memory[CursorIndex(kWeapon)] = 1;
  // The 20 checks precede the main pass; its sixth row rewrites the cursor.
  world.rewrite_call = 26;
  world.rewrite_index = CursorIndex(kWeapon);
  world.rewrite_value = 9;
  REQUIRE(inventory::RestoreItemRows(world, kWeapon).has_value());
  CHECK(world.rows[21].selected == 1);
  CHECK(world.rows[29].selected == 1);
  CHECK(world.advances == 0);
}

TEST_CASE("Inventory RestoreItemRows wraps the cursor offset in 32 bits",
          "[inventory-rows][F02596]") {
  FakeRestoreWorld world;
  world.memory[kWrappedCursor] = kRememberedRow;
  REQUIRE(inventory::RestoreItemRows(world,
                                     std::numeric_limits<std::int32_t>::max())
              .has_value());
  CHECK(world.rows[inventory::kFirstItemListRow + kRememberedRow].selected ==
        1);
}

TEST_CASE("Inventory RestoreItemRows refuses stalls missing rows and cursors",
          "[inventory-rows][F02596]") {
  const auto untouched = [](const FakeRestoreWorld& world) {
    return std::ranges::all_of(world.rows, [](const framework::MenuRow& row) {
      return row.selected == 0;
    });
  };
  for (const int category : {-31, 200}) {
    FakeRestoreWorld world;
    CHECK(inventory::RestoreItemRows(world, category).error() ==
          ItemRowsRefusal::kCursorMemory);
    CHECK(untouched(world));
  }
  FakeRestoreWorld stalled;
  stalled.list_row = 4;
  stalled.step = 0;
  CHECK(inventory::RestoreItemRows(stalled, kWeapon).error() ==
        ItemRowsRefusal::kStalled);
  CHECK(stalled.advances == 1);
  FakeRestoreWorld missing;
  missing.list_row = 4;
  missing.missing_row = 39;
  CHECK(inventory::RestoreItemRows(missing, kWeapon).error() ==
        ItemRowsRefusal::kRow);
  CHECK(missing.advances == 0);
  CHECK(untouched(missing));
}

namespace {
constexpr std::size_t kHeapBytes = 0x20000;
// Retained legacy caller layout (#404): 64 rows of text, 64 row types
// at 0x1800 and 128 native row text pointers at 0x1900.
constexpr std::size_t kCallerTypes = 0x1800;
constexpr std::size_t kCallerTexts = 0x1900;
constexpr std::size_t kPairBytes = 2 * sizeof(char*);
constexpr std::size_t kCallerRows = 64;
constexpr std::size_t kCallerBytes = kCallerTexts + (kCallerRows * kPairBytes);

struct RowsLiveFixture : inventory::sorting_test::LiveFixture {
  std::vector<std::byte> arena = std::vector<std::byte>(kHeapBytes);
  VS::Core::Memory::ScopedLiveGameHeap heap{arena};
  std::array<char, inventory::kMiscActionRows> saved_actions{};
  std::byte* block = nullptr;

  RowsLiveFixture() {
    std::ranges::copy(Menu_Inventory_unusedMiscActions, saved_actions.begin());
    block = static_cast<std::byte*>(
        VS::Core::Memory::AllocateLiveFromHighEnd(kCallerBytes));
  }
  ~RowsLiveFixture() {
    std::ranges::copy(saved_actions, Menu_Inventory_unusedMiscActions);
  }
  RowsLiveFixture(const RowsLiveFixture&) = delete;
  RowsLiveFixture& operator=(const RowsLiveFixture&) = delete;
  RowsLiveFixture(RowsLiveFixture&&) = delete;
  RowsLiveFixture& operator=(RowsLiveFixture&&) = delete;

  [[nodiscard]] int* Types() const {
    return reinterpret_cast<int*>(block + kCallerTypes);
  }
  [[nodiscard]] char** Texts() const {
    return reinterpret_cast<char**>(block + kCallerTexts);
  }
  [[nodiscard]] char* Buffer() const { return reinterpret_cast<char*>(block); }
  int Build(int category, bool loot = false) const {
    return loot ? inventory::LiveBuildLootItemRows(category, Texts(), Types(),
                                                   Buffer())
                : inventory::LiveBuildItemRows(category, Texts(), Types(),
                                               Buffer());
  }
};

int FormattedMiscType(Items_InventoryMisc misc) {
  std::array<char*, 2> texts{};
  int type = 0;
  std::array<char, framework::kRowDescriptionBufferBytes> buffer{};
  framework::FormatLiveMiscItemRow(&misc, texts.data(), &type, buffer.data());
  return type;
}
}  // namespace

TEST_CASE("Inventory live parent links read the inventory and refuse strays",
          "[inventory-rows-live][F02593][F02594]") {
  const inventory::sorting_test::LiveFixture fixture;
  Audio_Seq_unusedInventory.blades[0].assembledWeaponIndex = 2;
  Audio_Seq_unusedInventory.gems[1].setItemIndex = kShieldThreeGem;
  Audio_Seq_unusedInventory.blades[1].assembledWeaponIndex = 9;
  CHECK(inventory::LiveParentWeaponIndex(kBlade, 0) == 2);
  CHECK(inventory::LiveParentWeaponIndex(kGem, 1) == 0);
  CHECK(inventory::LiveParentShieldIndex(kGem, 1) == 3);
  const auto before = inventory::ItemListRowsLiveRefusalCount();
  CHECK(inventory::LiveParentWeaponIndex(kBlade, 1) == 0);
  CHECK(inventory::LiveParentWeaponIndex(kBlade, 16) == 0);
  CHECK(inventory::LiveParentShieldIndex(kGem, 48) == 0);
  CHECK(inventory::ItemListRowsLiveRefusalCount() == before + 3);
}

TEST_CASE("Inventory rows registry binds the MENU3 and MENUB addresses",
          "[inventory-rows-live][F02593][F02594][F02595][F02596]") {
  struct Entry {
    const char* module;
    unsigned address;
    const void* target;
  };
  // items2.h is compiled into both overlays; MENUB.PRG's row builder is the
  // build without _ITEMMENU (#406).
  const std::array entries = {
      Entry{"MENU/MENU3.PRG", 0x80104EC0,
            reinterpret_cast<const void*>(&inventory::LiveParentWeaponIndex)},
      Entry{"MENU/MENUB.PRG", 0x80104FB4,
            reinterpret_cast<const void*>(&inventory::LiveParentWeaponIndex)},
      Entry{"MENU/MENU3.PRG", 0x80104F50,
            reinterpret_cast<const void*>(&inventory::LiveParentShieldIndex)},
      Entry{"MENU/MENUB.PRG", 0x80105044,
            reinterpret_cast<const void*>(&inventory::LiveParentShieldIndex)},
      Entry{"MENU/MENU3.PRG", 0x80104F94,
            reinterpret_cast<const void*>(&inventory::LiveBuildItemRows)},
      Entry{"MENU/MENUB.PRG", 0x80105088,
            reinterpret_cast<const void*>(&inventory::LiveBuildLootItemRows)},
      Entry{"MENU/MENU3.PRG", 0x80105314,
            reinterpret_cast<const void*>(&inventory::LiveRestoreItemRows)},
      Entry{"MENU/MENUB.PRG", 0x8010537C,
            reinterpret_cast<const void*>(&inventory::LiveRestoreItemRows)},
  };
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const Entry& entry : entries) {
    CAPTURE(entry.module, entry.address);
    CHECK(VS::Content::ResolveModuleAddress(symbols, entry.module,
                                            entry.address) == entry.target);
  }
}

TEST_CASE("Inventory live rows fill the caller heap block and misc actions",
          "[inventory-rows-live][F02595]") {
  RowsLiveFixture fixture;
  REQUIRE(fixture.block != nullptr);
  auto& items = Audio_Seq_unusedInventory;
  items.misc[0] = {.id = kFirstMisc, .count = 1, .index = 0};
  items.misc[1] = {.id = kFirstMisc + 1, .count = 2, .index = 0};
  Audio_Seq_unusedInventoryIndices.misc[0] = 2;
  Audio_Seq_unusedInventoryIndices.misc[1] = 1;
  std::ranges::fill(Menu_Inventory_unusedMiscActions, char{0});
  REQUIRE(fixture.Build(kMisc) == 2);
  CHECK(fixture.Types()[0] == FormattedMiscType(items.misc[1]));
  CHECK(fixture.Types()[1] == FormattedMiscType(items.misc[0]));
  CHECK(static_cast<std::uint8_t>(Menu_Inventory_unusedMiscActions[0]) ==
        Menu_Inventory_miscItemToActionMap[1]);
  CHECK(static_cast<std::uint8_t>(Menu_Inventory_unusedMiscActions[1]) ==
        Menu_Inventory_miscItemToActionMap[0]);
}

TEST_CASE("Inventory live loot rows leave the misc actions alone",
          "[inventory-rows-live][F02595]") {
  RowsLiveFixture fixture;
  REQUIRE(fixture.block != nullptr);
  Audio_Seq_unusedInventory.misc[0] = {.id = 1, .count = 1, .index = 0};
  Audio_Seq_unusedInventoryIndices.misc[0] = 1;
  std::ranges::fill(Menu_Inventory_unusedMiscActions, char{0x55});
  const auto before = inventory::ItemListRowsLiveRefusalCount();
  REQUIRE(fixture.Build(kMisc, true) == 1);
  CHECK(std::ranges::all_of(Menu_Inventory_unusedMiscActions,
                            [](char value) { return value == 0x55; }));
  CHECK(inventory::ItemListRowsLiveRefusalCount() == before);
  REQUIRE(fixture.Build(kMisc) == 0);
  CHECK(inventory::ItemListRowsLiveRefusalCount() == before + 1);
}

TEST_CASE(
    "Inventory live rows fill 64 misc and 48 gem rows in the caller block",
    "[inventory-rows-live][F02595]") {
  RowsLiveFixture fixture;
  REQUIRE(fixture.block != nullptr);
  auto& items = Audio_Seq_unusedInventory;
  auto& order = Audio_Seq_unusedInventoryIndices;
  const auto refusals = inventory::ItemListRowsLiveRefusalCount();
  const auto damaged = VS::Core::Memory::DamagedLiveHeapGuardCount();
  for (std::size_t row = 0; row < std::size(order.misc); ++row) {
    std::span(items.misc)[row] = {.id = kFirstMisc, .count = 1, .index = 0};
    std::span(order.misc)[row] = static_cast<std::uint8_t>(row + 1);
  }
  CHECK(fixture.Build(kMisc) == static_cast<int>(kCallerRows));
  for (std::size_t row = 0; row < std::size(order.gems); ++row) {
    std::span(order.gems)[row] = static_cast<std::uint8_t>(row + 1);
  }
  CHECK(fixture.Build(kGem) == static_cast<int>(std::size(order.gems)));
  CHECK(inventory::ItemListRowsLiveRefusalCount() == refusals);
  VS::Core::Memory::FreeLiveFromHighEnd(fixture.block);
  fixture.block = nullptr;
  CHECK(VS::Core::Memory::DamagedLiveHeapGuardCount() == damaged);
}

TEST_CASE("Inventory live rows refuse row buffers outside the live heap",
          "[inventory-rows-live][F02595]") {
  const RowsLiveFixture fixture;
  Audio_Seq_unusedInventoryIndices.gems[0] = 1;
  const auto before = inventory::ItemListRowsLiveRefusalCount();
  std::array<char*, 2> texts{};
  std::array<int, 1> types{};
  std::array<char, framework::kRowDescriptionBufferBytes> buffer{};
  CHECK(inventory::LiveBuildItemRows(kGem, texts.data(), types.data(),
                                     buffer.data()) == 0);
  CHECK(types[0] == 0);
  CHECK(inventory::ItemListRowsLiveRefusalCount() == before + 1);
}

TEST_CASE("Inventory live rows refuse categories without an order table",
          "[inventory-rows-live][F02595]") {
  RowsLiveFixture fixture;
  const auto before = inventory::ItemListRowsLiveRefusalCount();
  CHECK(fixture.Build(kManage) == 0);
  CHECK(fixture.Build(-1) == 0);
  Menu_Inventory_inventoryIndices[kWeapon] = nullptr;
  CHECK(fixture.Build(kWeapon) == 0);
  CHECK(inventory::ItemListRowsLiveRefusalCount() == before + 3);
}

TEST_CASE("Inventory live restore settles the live menu rows",
          "[inventory-rows-live][F02596]") {
  std::array<Menu_Framework_MenuRow, framework::kRowCount> rows{};
  auto* const saved_rows = Menu_Framework_menuItems;
  const auto saved_list_row = Menu_Framework_itemsListRow;
  const auto memory = framework::LiveMenuCursorMemory();
  const auto saved_cursor = memory[CursorIndex(kArmor)];
  rows[kSlidingRow].state = 2;
  rows[kSlidingRow].targetPosition0 = kTargetX;
  Menu_Framework_menuItems = rows.data();
  Menu_Framework_itemsListRow = 16;
  memory[CursorIndex(kArmor)] = kRememberedRow;
  inventory::LiveRestoreItemRows(kArmor);
  CHECK(rows[kSlidingRow].state == 1);
  CHECK(rows[kSlidingRow].x == kTargetX);
  CHECK(rows[inventory::kFirstItemListRow + kRememberedRow].selected == 1);
  Menu_Framework_menuItems = nullptr;
  const auto before = inventory::ItemListRowsLiveRefusalCount();
  inventory::LiveRestoreItemRows(kArmor);
  CHECK(inventory::ItemListRowsLiveRefusalCount() == before + 1);
  Menu_Framework_menuItems = saved_rows;
  Menu_Framework_itemsListRow = saved_list_row;
  memory[CursorIndex(kArmor)] = saved_cursor;
}
