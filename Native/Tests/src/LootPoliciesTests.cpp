#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Items/ItemRecords.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Loot/LootPolicies.hpp"
#include "Menu/Loot/LootPoliciesLive.hpp"
#include "Menu/Loot/LootRows.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Loot/LootScreen.h"
#include "Menu/Loot/Paging.h"
}

namespace {
namespace items = VS::Items;
namespace loot = VS::Menu::Loot;
namespace live = VS::Menu::Loot::Live;
using items::ItemCategory;
using items::ItemsStatus;
using VS::Menu::Framework::MenuRow;
using VS::Menu::Framework::RowPlacement;
using VS::Menu::Framework::RowState;

loot::LootRow Row(ItemCategory category, std::uint8_t index,
                  std::uint8_t state = 1) {
  return {.category = static_cast<std::uint8_t>(category),
          .item_index = index,
          .state = state};
}

struct Drawn {
  std::string what;
  std::uint32_t xy{};
  std::uint32_t color{};
  bool operator==(const Drawn&) const = default;
};

class FakeCapacityHost final : public loot::CapacityHost {
 public:
  void DrawLabel(loot::CapacityLabel label, std::uint32_t xy) override {
    drawn.push_back({Name(label, 0), xy, 0});
  }
  void DrawColoredLabel(loot::CapacityLabel label, std::uint8_t category,
                        std::uint32_t xy, std::uint32_t color) override {
    drawn.push_back({Name(label, category), xy, color});
  }
  std::int32_t CountItems(std::uint8_t category) override {
    return counts.at(category);
  }
  std::int32_t CountContained(std::uint8_t category,
                              const loot::LootRow& /*row*/) override {
    ++contained_calls;
    return contained.at(category);
  }
  void DrawInteger(std::int32_t value, std::uint32_t xy,
                   std::uint32_t color) override {
    drawn.push_back({std::to_string(value), xy, color});
  }
  void DrawButtonBackground(std::int32_t /*x*/, std::int32_t /*y*/,
                            std::int32_t w, std::int32_t /*h*/) override {
    backgrounds.push_back(w);
  }
  void DrawButton(loot::PromptButton button, std::int32_t /*x*/,
                  std::int32_t /*y*/) override {
    buttons.push_back(button);
  }

  [[nodiscard]] const Drawn* Find(const std::string& what) const {
    for (const auto& entry : drawn) {
      if (entry.what == what) return &entry;
    }
    return nullptr;
  }

  std::array<std::int32_t, 7> counts{};
  std::array<std::int32_t, 7> contained{};
  int contained_calls = 0;
  std::vector<Drawn> drawn;
  std::vector<std::int32_t> backgrounds;
  std::vector<loot::PromptButton> buttons;

 private:
  static std::string Name(loot::CapacityLabel label, std::uint8_t category) {
    switch (label) {
      case loot::CapacityLabel::kGadgetBag:
        return "GADGET BAG";
      case loot::CapacityLabel::kSlash:
        return "/" + std::to_string(category);
      case loot::CapacityLabel::kCategory:
        return "header" + std::to_string(category);
      case loot::CapacityLabel::kGet:
        return "GET";
      case loot::CapacityLabel::kAll:
        return "ALL";
      case loot::CapacityLabel::kEnd:
        return "END";
    }
    return {};
  }
};

constexpr std::array<std::uint8_t, 8> kCapacities{8, 16, 16, 8, 16, 48, 64, 0};

std::uint32_t Xy(std::int32_t x, std::int32_t y) {
  return static_cast<std::uint32_t>(x) | (static_cast<std::uint32_t>(y) << 16);
}

class FakeRowsHost final : public loot::LootRowsHost {
 public:
  MenuRow& Row(int row) override { return rows.at(row); }
  MenuRow& SetRow(const RowPlacement& placement,
                  std::span<const std::uint8_t> text) override {
    placements.push_back(placement);
    texts.emplace_back(text.begin(), text.end());
    MenuRow& row = rows.at(placement.row);
    row = {};
    row.state = RowState::kIdle;
    row.x = static_cast<std::int16_t>(placement.x);
    return row;
  }
  std::span<const std::uint8_t> ItemName(std::uint16_t id) override {
    names.push_back(id);
    return {};
  }

  std::array<MenuRow, 40> rows{};
  std::vector<RowPlacement> placements;
  std::vector<std::vector<std::uint8_t>> texts;
  std::vector<std::uint16_t> names;
};

class FakeDescriptionHost final : public loot::DescriptionHost {
 public:
  void BackupInventory() override { calls.emplace_back("backup"); }
  void LoadStagedInventory() override { calls.emplace_back("stage"); }
  std::span<const std::uint8_t> FormatDescription(ItemCategory category,
                                                  std::uint8_t index) override {
    calls.push_back("format" + std::to_string(static_cast<int>(category)) +
                    ":" + std::to_string(index));
    return text;
  }
  void ShowInformation(std::span<const std::uint8_t> message) override {
    calls.push_back("show" + std::to_string(message.size()));
  }
  void RestoreInventory() override { calls.emplace_back("restore"); }

  std::array<std::uint8_t, 3> text{1, 2, 3};
  std::vector<std::string> calls;
};

class FakePromptHost final : public loot::PromptHost {
 public:
  MenuRow& SetRow(const RowPlacement& placement,
                  loot::PromptText text) override {
    set_rows.push_back(placement.row);
    set_texts.push_back(text);
    return rows.at(placement.row);
  }
  MenuRow& Row(int row) override { return rows.at(row); }
  void InitInformationTextBox() override { calls.emplace_back("textbox"); }
  void InitInformationBox() override { calls.emplace_back("infobox"); }
  void ShowInformation(loot::PromptText text) override {
    shown.push_back(text);
  }
  bool RowsIdle() override { return idle; }
  void FlyOutRight(int row) override { flown.push_back(row); }
  void DismissInformationBox() override { calls.emplace_back("dismiss"); }
  void PlayLeaveSound() override { calls.emplace_back("leave"); }
  void PlayChangeSound() override { calls.emplace_back("change"); }
  void PlaySelectSound() override { calls.emplace_back("select"); }
  void PlayDiscardSound() override { calls.emplace_back("discard"); }
  std::uint8_t DrawCursor(std::uint8_t step, int position) override {
    cursor_positions.push_back(position);
    return static_cast<std::uint8_t>(step + 1);
  }
  void SetStringItem(std::uint16_t item_id) override {
    string_items.push_back(item_id);
  }

  bool idle = false;
  std::array<MenuRow, 40> rows{};
  std::vector<int> set_rows;
  std::vector<loot::PromptText> set_texts;
  std::vector<loot::PromptText> shown;
  std::vector<int> flown;
  std::vector<int> cursor_positions;
  std::vector<std::uint16_t> string_items;
  std::vector<std::string> calls;
};

constexpr std::uint32_t kCross = 0x20;
constexpr std::uint32_t kCircle = 0x40;
constexpr std::uint32_t kUp = 0x1000;

loot::PromptInput Pressed(std::uint32_t pressed) {
  return {.pressed = pressed, .repeat = 0, .information_shown = false};
}

loot::PromptInput Repeated(std::uint32_t repeat) {
  return {.pressed = 0, .repeat = repeat, .information_shown = true};
}

// Opens the leave confirmation through its slide-in frames.
loot::LeaveConfirmation OpenLeave(FakePromptHost& host, bool claimed) {
  loot::LeaveConfirmation prompt{};
  items::InventoryContents inventory{};
  const std::array rows{Row(ItemCategory::kGem, 0)};
  REQUIRE(loot::InitLeaveConfirmation(
              prompt, {.rows = rows, .count = 1, .claimed = claimed},
              inventory) == ItemsStatus::kOk);
  host.idle = true;
  for (int frame = 0; frame < 3; ++frame) {
    REQUIRE(loot::UpdateLeaveConfirmation(prompt, claimed, {}, host) ==
            std::optional<std::int32_t>(0));
  }
  return prompt;
}

loot::OrganizeMenu OpenOrganize(FakePromptHost& host, const loot::LootRow& row,
                                const items::InventoryContents& inventory) {
  loot::OrganizeMenu menu{};
  REQUIRE(loot::InitOrganizeMenu(menu, row, inventory, host) ==
          ItemsStatus::kOk);
  host.idle = true;
  REQUIRE(loot::UpdateOrganizeMenu(menu, {}, host) ==
          std::optional<std::int32_t>(0));
  REQUIRE(loot::UpdateOrganizeMenu(menu, {}, host) ==
          std::optional<std::int32_t>(0));
  return menu;
}

struct LivePoliciesFixture {
  Items_Inventory staged{};
  Items_Inventory backup{};
  std::array<Menu_Loot_LootListItem, kMenuLootRowCapacity> rows{};
  std::array<Menu_Framework_MenuRow, 40> menu_rows{};
  Menu_Framework_MenuRow* previous_menu_rows{Menu_Framework_menuItems};
  Items_Inventory* previous_inventory{Menu_Loot_inventory};
  Items_Inventory* previous_backup{Menu_Loot_inventoryBackup};
  Menu_Loot_LootListItem* previous_rows{Menu_Loot_lootList};
  Items_BattleLoot* previous_entry{Menu_Framework_lootEntry};
  Items_Inventory previous_ashley{Audio_Seq_unusedInventory};
  char previous_claimed{Menu_Loot_lootClaimed};

  LivePoliciesFixture() {
    live::ResetPolicyStatics();
    Menu_Framework_menuItems = menu_rows.data();
    Menu_Loot_inventory = &staged;
    Menu_Loot_inventoryBackup = &backup;
    Menu_Loot_lootList = rows.data();
    Menu_Loot_lootListCount = 0;
    Menu_Loot_lootListOffset = 0;
    Menu_Loot_lootClaimed = 0;
    Menu_Framework_lootEntry = nullptr;
  }

  ~LivePoliciesFixture() {
    live::ResetPolicyStatics();
    Menu_Framework_menuItems = previous_menu_rows;
    Menu_Loot_inventory = previous_inventory;
    Menu_Loot_inventoryBackup = previous_backup;
    Menu_Loot_lootList = previous_rows;
    Menu_Loot_lootListCount = 0;
    Menu_Framework_lootEntry = previous_entry;
    Audio_Seq_unusedInventory = previous_ashley;
    Menu_Loot_lootClaimed = previous_claimed;
  }

  LivePoliciesFixture(const LivePoliciesFixture&) = delete;
  LivePoliciesFixture& operator=(const LivePoliciesFixture&) = delete;
};
}  // namespace

TEST_CASE("F02856 without a preview lists every category in neutral colors",
          "[loot-policies][F02856]") {
  FakeCapacityHost host;
  host.counts = {1, 2, 3, 4, 5, 6, 7};
  const loot::CapacityPanel panel{.rows = {},
                                  .cursor = {},
                                  .previewed = 0,
                                  .controls_enabled = 1,
                                  .capacities = kCapacities};
  REQUIRE(loot::RenderBagCapacities(10, panel, host));
  CHECK(host.drawn.front() == Drawn{"GADGET BAG", Xy(186, 12), 0});
  CHECK(*host.Find("header2") == Drawn{"header2", Xy(202, 48), 0x808080});
  CHECK(*host.Find("16") == Drawn{"16", Xy(306, 36), 0x808080});
  CHECK(*host.Find("7") == Drawn{"7", Xy(282, 96), 0x804020});
  CHECK(host.contained_calls == 0);
  CHECK(host.backgrounds == std::vector<std::int32_t>{112, 64});
  CHECK(host.buttons == std::vector{loot::PromptButton::kCircle,
                                    loot::PromptButton::kSquare,
                                    loot::PromptButton::kCross});
}

TEST_CASE("F02856 a previewed row lights the categories it adds to",
          "[loot-policies][F02856]") {
  FakeCapacityHost host;
  host.counts = {7, 0, 0, 0, 0, 0, 0};
  host.contained = {2, 0, 0, 0, 0, 0, 0};
  const std::array rows{Row(ItemCategory::kWeapon, 0, 0),
                        Row(ItemCategory::kWeapon, 1, 0)};
  const loot::CapacityPanel panel{.rows = rows,
                                  .cursor = {.count = 2, .offset = 1},
                                  .previewed = 1,
                                  .capacities = kCapacities};
  REQUIRE(loot::RenderBagCapacities(0, panel, host));
  CHECK(host.Find("/0")->color == 0x808080);
  CHECK(host.Find("/1")->color == 0x404040);
  CHECK(host.Find("7")->color == 0x204080);
  CHECK(host.Find("ALL")->color == 0x404040);
  CHECK(host.Find("GET")->color == 0x404040);
}

TEST_CASE("F02856 previewing every row lights all categories",
          "[loot-policies][F02856]") {
  FakeCapacityHost host;
  const loot::CapacityPanel panel{.rows = {},
                                  .cursor = {},
                                  .previewed = 0x10,
                                  .controls_enabled = 1,
                                  .capacities = kCapacities};
  REQUIRE(loot::RenderBagCapacities(0, panel, host));
  CHECK(host.Find("/6")->color == 0x808080);
  CHECK(host.Find("ALL")->color == 0x808080);
  CHECK(host.Find("END")->color == 0x808080);
  CHECK(host.contained_calls == 0);
}

TEST_CASE("F02856 refuses a preview outside the loot list",
          "[loot-policies][F02856]") {
  FakeCapacityHost host;
  const std::array rows{Row(ItemCategory::kWeapon, 0)};
  loot::CapacityPanel panel{.rows = rows,
                            .cursor = {.count = 1, .offset = 0},
                            .previewed = 2,
                            .capacities = kCapacities};
  CHECK_FALSE(loot::RenderBagCapacities(0, panel, host));
  panel.previewed = 1;
  panel.cursor.count = 2;
  CHECK_FALSE(loot::RenderBagCapacities(0, panel, host));
  panel.capacities = std::span(kCapacities).first(6);
  panel.cursor.count = 1;
  CHECK_FALSE(loot::RenderBagCapacities(0, panel, host));
  CHECK(host.drawn.empty());
}

namespace {
struct CategoryRowsFixture {
  items::InventoryContents inventory{};
  std::array<loot::LootRow, 3> rows{Row(ItemCategory::kWeapon, 0),
                                    Row(ItemCategory::kGrip, 0, 2),
                                    Row(ItemCategory::kMisc, 0, 4)};
  loot::LootRowsView view{.rows = rows, .cursor = {.count = 3}};
  FakeRowsHost host;

  CategoryRowsFixture() {
    inventory.weapons[0].blade = 2;
    inventory.weapons[0].name[0] = 'A';
    inventory.blades[1] = {.id = 9, .category = 4, .material = 0x103};
    inventory.grips[0].id = 300;
    inventory.grips[0].category = 2;
    inventory.misc[0] = {.id = 458, .count = 5};
  }
};

struct SlidingRowsFixture {
  items::InventoryContents inventory{};
  std::array<loot::LootRow, 10> rows{};
  loot::LootRowsView view{.rows = rows,
                          .cursor = {.count = 10, .offset = 1},
                          .gradient_state_index = 0};
  FakeRowsHost host;

  SlidingRowsFixture() {
    rows.fill(Row(ItemCategory::kGem, 0));
    rows[1].slide_step = 2;
    rows[2].state = 5;
    rows[3].state = 0;
    host.rows[33].gradient_state = 7;
  }
};
}  // namespace

TEST_CASE("F02860 places the visible rows and their names",
          "[loot-policies][F02860]") {
  CategoryRowsFixture f;
  REQUIRE(loot::RenderLootRows(4, f.view, f.inventory, f.host) ==
          ItemsStatus::kOk);
  CHECK(f.host.placements[1].y == 66);
  CHECK(f.host.placements[0].x == 20);
  CHECK(f.host.texts[0].front() == 'A');
}

TEST_CASE("F02860 decorates rows by category", "[loot-policies][F02860]") {
  CategoryRowsFixture f;
  REQUIRE(loot::RenderLootRows(4, f.view, f.inventory, f.host) ==
          ItemsStatus::kOk);
  CHECK(f.host.rows[32].row_icon == 4);
  CHECK(f.host.rows[32].material == 3);
  CHECK(f.host.rows[33].row_icon == 12);
  CHECK(f.host.rows[33].background_width == 0x18);
  CHECK(f.host.rows[34].count == 5);
  CHECK(f.host.rows[34].font_color == 1);
}

TEST_CASE("F02860 slides and indents rows", "[loot-policies][F02860]") {
  SlidingRowsFixture f;
  REQUIRE(loot::RenderLootRows(0, f.view, f.inventory, f.host) ==
          ItemsStatus::kOk);
  CHECK(f.host.rows[32].gradient_state == 7);
  CHECK(f.host.rows[32].x == 24 - 96);
  CHECK(f.rows[1].slide_step == 3);
  CHECK(f.host.rows[33].x == 24 + 14);
  CHECK(f.host.rows[34].unselectable == 1);
}

TEST_CASE("F02860 marks the scroll arrows and saves the offset",
          "[loot-policies][F02860]") {
  SlidingRowsFixture f;
  REQUIRE(loot::RenderLootRows(0, f.view, f.inventory, f.host) ==
          ItemsStatus::kOk);
  CHECK(f.host.rows[32].is_scrollable == 1);
  CHECK(f.host.rows[39].is_scrollable == 2);
  CHECK(f.view.gradient_state_index == 1);
  CHECK(f.host.rows[32].state == RowState::kIdle);
}

TEST_CASE("F02860 keeps rows it cannot read and stops at the list end",
          "[loot-policies][F02860]") {
  items::InventoryContents inventory{};
  std::array rows{Row(ItemCategory::kWeapon, 0), Row(ItemCategory::kMisc, 64),
                  Row(ItemCategory::kManage, 0)};
  loot::LootRowsView view{.rows = rows, .cursor = {.count = 4}};
  FakeRowsHost host;
  host.rows[33].x = 99;
  CHECK(loot::RenderLootRows(0, view, inventory, host) ==
        ItemsStatus::kSlotOutOfRange);
  CHECK(host.placements.size() == 1);
  CHECK(host.rows[32].row_icon == 0);
  CHECK(host.rows[33].x == 99);
  CHECK(host.rows[34].state == RowState::kInactive);
  CHECK(host.rows[35].state == RowState::kInactive);
}

TEST_CASE("F02861 formats the description over the staged inventory",
          "[loot-policies][F02861]") {
  FakeDescriptionHost host;
  REQUIRE(loot::ShowLootDescription(Row(ItemCategory::kArmor, 15), host) ==
          ItemsStatus::kOk);
  CHECK(host.calls == std::vector<std::string>{"backup", "stage", "format4:15",
                                               "show3", "restore"});
}

TEST_CASE("F02861 refuses rows outside the item tables",
          "[loot-policies][F02861]") {
  FakeDescriptionHost host;
  CHECK(loot::ShowLootDescription(Row(ItemCategory::kManage, 0), host) ==
        ItemsStatus::kCategoryOutOfRange);
  CHECK(loot::ShowLootDescription(Row(ItemCategory::kShield, 8), host) ==
        ItemsStatus::kSlotOutOfRange);
  CHECK(host.calls.empty());
}

TEST_CASE("F02862 opening picks the empty, single-entry and key-item states",
          "[loot-policies][F02862]") {
  items::InventoryContents inventory{};
  inventory.misc[1].id = 460;
  inventory.misc[2].id = 470;
  const std::array rows{Row(ItemCategory::kMisc, 1), Row(ItemCategory::kGem, 1),
                        Row(ItemCategory::kMisc, 2)};
  loot::LeaveConfirmation prompt{.state = 9, .choice = 0};
  REQUIRE(loot::InitLeaveConfirmation(prompt, {.rows = rows}, inventory) ==
          ItemsStatus::kOk);
  CHECK(prompt.state == 4);
  CHECK(prompt.choice == 1);
  REQUIRE(loot::InitLeaveConfirmation(
              prompt, {.rows = rows, .count = 3, .single_entry = true},
              inventory) == ItemsStatus::kOk);
  CHECK(prompt.state == 5);
  REQUIRE(loot::InitLeaveConfirmation(
              prompt, {.rows = rows, .count = 3, .claimed = true}, inventory) ==
          ItemsStatus::kOk);
  CHECK(prompt.state == 6);
  CHECK(prompt.kept_item == 470);
}

TEST_CASE("F02862 opening refuses rows outside the misc table",
          "[loot-policies][F02862]") {
  items::InventoryContents inventory{};
  const std::array rows{Row(ItemCategory::kMisc, 64)};
  loot::LeaveConfirmation prompt{.kept_item = 3};
  CHECK(loot::InitLeaveConfirmation(prompt,
                                    {.rows = rows, .count = 2, .claimed = true},
                                    inventory) == ItemsStatus::kSlotOutOfRange);
  CHECK(prompt.state == 0);
  CHECK(prompt.kept_item == 3);
}

TEST_CASE("F02862 slides in yes and no with the claim message",
          "[loot-policies][F02862]") {
  FakePromptHost host;
  const auto prompt = OpenLeave(host, false);
  CHECK(prompt.state == 3);
  CHECK(host.set_rows == std::vector{30, 31});
  CHECK(host.rows[30].state == RowState::kSlideX);
  CHECK(host.rows[31].target0 == 0xC2);
  CHECK(host.shown == std::vector{loot::PromptText::kDiscardAllConfirm});
  FakePromptHost claimed_host;
  (void)OpenLeave(claimed_host, true);
  CHECK(claimed_host.shown == std::vector{loot::PromptText::kItemsRemaining});
}

TEST_CASE("F02862 confirming yes leaves the rest", "[loot-policies][F02862]") {
  FakePromptHost host;
  auto prompt = OpenLeave(host, true);
  CHECK(loot::UpdateLeaveConfirmation(prompt, true, Repeated(kUp), host) ==
        std::optional<std::int32_t>(0));
  CHECK(prompt.choice == 0);
  CHECK(host.cursor_positions.back() == 8);
  CHECK(loot::UpdateLeaveConfirmation(prompt, true, Pressed(kCross), host) ==
        std::optional<std::int32_t>(1));
  CHECK(host.flown == std::vector{30, 31});
  CHECK(host.calls.back() == "discard");
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, Pressed(kCross), host) ==
        std::optional<std::int32_t>(2));
}

TEST_CASE("F02862 cancelling or choosing no stays", "[loot-policies][F02862]") {
  FakePromptHost host;
  auto prompt = OpenLeave(host, false);
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, Pressed(kCross), host) ==
        std::optional<std::int32_t>(-1));
  CHECK(host.calls.back() == "leave");
  CHECK(host.rows[31].selected == 1);
  prompt.choice = 0;
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, Pressed(kCircle), host) ==
        std::optional<std::int32_t>(-1));
  prompt.choice = 2;
  CHECK_FALSE(
      loot::UpdateLeaveConfirmation(prompt, false, {}, host).has_value());
}

TEST_CASE("F02862 a key item blocks leaving until a button",
          "[loot-policies][F02862]") {
  FakePromptHost host;
  loot::LeaveConfirmation prompt{.state = 6, .kept_item = 470};
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, {}, host) ==
        std::optional<std::int32_t>(0));
  CHECK(host.string_items == std::vector<std::uint16_t>{470});
  CHECK(host.shown == std::vector{loot::PromptText::kInvalidDiscard});
  CHECK(prompt.state == 7);
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, Pressed(0x100), host) ==
        std::optional<std::int32_t>(0));
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, Pressed(1), host) ==
        std::optional<std::int32_t>(-1));
  prompt.state = 5;
  CHECK(loot::UpdateLeaveConfirmation(prompt, false, {}, host) ==
        std::optional<std::int32_t>(2));
}

TEST_CASE("F02863 offers disassembly for weapons and shields",
          "[loot-policies][F02863]") {
  FakePromptHost host;
  items::InventoryContents inventory{};
  auto menu = OpenOrganize(host, Row(ItemCategory::kShield, 0), inventory);
  CHECK(menu.action == 1);
  CHECK(menu.selected_row == 0);
  CHECK(host.set_texts == std::vector{loot::PromptText::kDisassemble,
                                      loot::PromptText::kOrganizeInventory});
  CHECK(host.calls.front() == "infobox");
  CHECK(loot::UpdateOrganizeMenu(menu, Repeated(0), host) ==
        std::optional<std::int32_t>(0));
  CHECK(host.shown.back() == loot::PromptText::kDisassembleInfo);
  CHECK(loot::UpdateOrganizeMenu(menu, Pressed(kCross), host) ==
        std::optional<std::int32_t>(2));
  CHECK(menu.selected_row == 2);
}

TEST_CASE("F02863 offers stacking for ordinary misc items",
          "[loot-policies][F02863]") {
  FakePromptHost host;
  items::InventoryContents inventory{};
  inventory.misc[3].id = 457;
  auto menu = OpenOrganize(host, Row(ItemCategory::kMisc, 3), inventory);
  CHECK(menu.action == 2);
  CHECK(host.set_texts.front() == loot::PromptText::kOrganize);
  CHECK(loot::UpdateOrganizeMenu(menu, Repeated(kUp), host) ==
        std::optional<std::int32_t>(0));
  CHECK(menu.selected_row == 1);
  CHECK(host.shown.back() == loot::PromptText::kOrganizeInventoryInfo);
  CHECK(host.cursor_positions.back() == 9);
  CHECK(loot::UpdateOrganizeMenu(menu, Pressed(kCross), host) ==
        std::optional<std::int32_t>(1));
}

TEST_CASE("F02863 other items only organize the inventory",
          "[loot-policies][F02863]") {
  FakePromptHost host;
  items::InventoryContents inventory{};
  inventory.misc[3].id = 458;
  auto menu = OpenOrganize(host, Row(ItemCategory::kMisc, 3), inventory);
  CHECK(menu.action == 0);
  CHECK(menu.selected_row == 1);
  CHECK(host.set_rows == std::vector{31});
  CHECK(loot::UpdateOrganizeMenu(menu, Repeated(kUp), host) ==
        std::optional<std::int32_t>(0));
  CHECK(menu.selected_row == 1);
  CHECK(loot::UpdateOrganizeMenu(menu, Pressed(kCircle), host) ==
        std::optional<std::int32_t>(-1));
  CHECK(host.calls.back() == "leave");
}

TEST_CASE("F02863 refuses misc rows and choices outside the tables",
          "[loot-policies][F02863]") {
  FakePromptHost host;
  items::InventoryContents inventory{};
  loot::OrganizeMenu menu{.state = 2, .category = 6};
  CHECK(loot::InitOrganizeMenu(menu, Row(ItemCategory::kMisc, 64), inventory,
                               host) == ItemsStatus::kSlotOutOfRange);
  CHECK(menu.state == 2);
  CHECK(host.calls.empty());
  menu.selected_row = 2;
  CHECK_FALSE(loot::UpdateOrganizeMenu(menu, {}, host).has_value());
}

TEST_CASE("Live leave confirmation returns the empty and single-entry results",
          "[loot-policies-live][F02862]") {
  LivePoliciesFixture fixture;
  CHECK(live::UpdateLeaveConfirmation(1) == 0);
  CHECK(live::UpdateLeaveConfirmation(0) == 1);
  Items_BattleLoot chest{};
  Menu_Framework_lootEntry = &chest;
  Menu_Loot_lootListCount = 1;
  CHECK(live::UpdateLeaveConfirmation(1) == 0);
  CHECK(live::UpdateLeaveConfirmation(0) == 2);
  Menu_Loot_lootClaimed = 1;
  CHECK(live::UpdateLeaveConfirmation(1) == 0);
}

TEST_CASE("Live loot policies refuse missing lists and rows",
          "[loot-policies-live][F02856][F02860][F02861][F02863]") {
  LivePoliciesFixture fixture;
  fixture.staged.misc[0].id = 77;
  Audio_Seq_unusedInventory = {};
  live::ShowLootDescription(-1);
  live::ShowLootDescription(static_cast<int>(kMenuLootRowCapacity));
  CHECK(Audio_Seq_unusedInventory.misc[0].id == 0);
  Menu_Loot_lootList = nullptr;
  live::RenderBagCapacities(0);
  live::RenderLootRows(0);
  live::ShowLootDescription(0);
  Menu_Loot_inventory = nullptr;
  Menu_Loot_LootListItem row{};
  CHECK(live::UpdateOrganizationMenu(&row) == 0);
  CHECK(fixture.backup.misc[0].id == 0);
}

TEST_CASE("Live loot policy reset reopens the prompts",
          "[loot-policies-live][F02862][F02863]") {
  LivePoliciesFixture fixture;
  CHECK(live::UpdateLeaveConfirmation(1) == 0);
  live::ResetPolicyStatics();
  CHECK(live::UpdateLeaveConfirmation(0) == 0);
  CHECK(VS::Menu::Framework::LiveMenuRow(30).state == RowState::kSlideX);
  CHECK(VS::Menu::Framework::LiveMenuRow(30).target0 == 0xC2);
  CHECK(live::UpdateOrganizationMenu(nullptr) == 0);
  CHECK(VS::Menu::Framework::LiveMenuRow(31).state == RowState::kSlideX);
  CHECK(VS::Menu::Framework::LiveMenuRow(31).y == 0xA2);
}
