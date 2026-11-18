#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Seq/Channel.h"
#include "Battle/Hud.h"
#include "Items/Inventory.h"
#include "Menu/ContainerMenuDialogsLiveFixture.hpp"
#include "Menu/Framework/EquipmentDetailsLiveState.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/ItemRowsLiveState.h"
#include "Menu/Inventory/EquipmentMenus.hpp"
#include "Menu/Inventory/EquipmentMenusLive.hpp"
#include "Menu/Inventory/EquipmentRowsLive.hpp"
#include "Menu/Inventory/Inventory.h"

// Contracts of the equipment menu (MENU3.PRG 16C.c:343-1233): the four slot
// menus, the handler table that picks one for a top-menu row, and the top
// menu.
namespace {
namespace inv = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;
using inv::EquipmentMenu;
using inv::EquipmentTable;
using Calls = std::vector<std::string>;

// _getSelectedItemRow while the list is open, after Cross and Triangle.
constexpr std::int32_t kListOpen = -1;
constexpr std::int32_t kListBack = -2;
constexpr std::int32_t kListQuit = -3;
// _menuText's halfword offsets the fake host serves for "none" and hints.
constexpr std::uint16_t kNoneName = 300;
constexpr std::uint16_t kNoneDescription = 310;
constexpr std::uint16_t kFirstHint = 400;
constexpr int kSlotShift = 19;
constexpr int kRowTypeShift = 9;
constexpr std::array<std::int32_t, inv::kEquipmentTopRows> kTopRowTypes{
    0x78, 0x79, 0x69, 0x6A, 0x68, 0x6D, 0x6B, 0x7A,
};

[[nodiscard]] constexpr std::size_t Index(EquipmentTable table) {
  return static_cast<std::size_t>(table);
}

// The flags the fake host's formatter returns for a 0-based slot.
[[nodiscard]] constexpr std::int32_t FormatFlags(EquipmentTable table,
                                                 std::int32_t slot) {
  return (0x100 * (static_cast<std::int32_t>(table) + 1)) + slot;
}

// A choice row's flags: the formatter's, then the 1-based slot.
[[nodiscard]] constexpr std::int32_t ChoiceFlags(EquipmentTable table,
                                                 std::int32_t slot) {
  return FormatFlags(table, slot) | ((slot + 1) << kSlotShift);
}

// Records each call the menus make, in order, and serves configurable
// inventory, rows and input.
class FakeHost final : public inv::EquipmentMenusHost {
 public:
  Calls calls;
  std::array<std::int32_t, 5> counts{};
  std::array<std::array<std::uint8_t, 16>, 3> listed{};
  std::array<bool, 8> weapons_held{};
  std::array<bool, 8> shields_held{};
  std::array<std::uint8_t, 16> limbs{};
  std::array<std::uint8_t, 16> categories{};
  std::array<std::uint8_t, 8> weapon_blades{};
  std::array<inv::BladeModel, 16> blades{};
  std::array<inv::ShieldModel, 8> shields{};
  std::optional<inv::HeldBlade> held = inv::HeldBlade{};
  std::array<bool, 11> two_handed{};
  bool rows_idle = true;
  std::array<std::uint16_t, inv::kEquipmentMenuTextEntries> menu_text{};
  std::int32_t selected_choice = kListOpen;
  std::uint8_t highlighted_item = 0;
  // The list's input moves the highlight here.
  std::optional<std::uint8_t> item_after_input;
  int reload_frames = 0;
  std::vector<std::int32_t> choice_flags;
  std::array<std::int32_t, inv::kEquipmentTopRows> top_flags{};
  std::deque<fw::MenuRow> items;
  bool square = false;
  std::int32_t selected_row = kListOpen;
  std::int32_t highlighted_row = 0;
  std::int32_t dispatch_result = 0;

  FakeHost() {
    menu_text[20] = kNoneName;
    menu_text[21] = kNoneDescription;
    for (std::uint16_t row = 0; row < inv::kEquipmentTopRows; ++row) {
      menu_text[12U + row] = static_cast<std::uint16_t>(kFirstHint + row);
    }
  }

  void Log(std::string_view name, std::initializer_list<std::int64_t> values) {
    std::string call(name);
    for (const std::int64_t value : values) {
      call += ' ';
      call += std::to_string(value);
    }
    calls.push_back(call);
  }

  std::int32_t CountItems(std::int32_t category) override {
    Log("count", {category});
    return counts.at(static_cast<std::size_t>(category));
  }
  std::uint8_t ListedSlot(EquipmentTable table,
                          std::int32_t position) override {
    return listed.at(Index(table)).at(static_cast<std::size_t>(position));
  }
  bool IsEquipped(EquipmentTable table, std::int32_t slot) override {
    const auto index = static_cast<std::size_t>(slot);
    switch (table) {
      case EquipmentTable::kWeapons:
        return weapons_held.at(index);
      case EquipmentTable::kShields:
        return shields_held.at(index);
      case EquipmentTable::kArmor:
        break;
    }
    return false;
  }
  void MarkEquipped(EquipmentTable table, std::int32_t slot) override {
    Log("mark", {static_cast<std::int64_t>(table), slot});
  }
  std::uint8_t ArmorLimb(std::int32_t slot) override {
    return limbs.at(static_cast<std::size_t>(slot));
  }
  std::uint8_t ArmorCategory(std::int32_t slot) override {
    return categories.at(static_cast<std::size_t>(slot));
  }
  void SetArmorLimb(std::int32_t slot, std::uint8_t limb) override {
    Log("limb", {slot, limb});
    limbs.at(static_cast<std::size_t>(slot)) = limb;
  }
  std::uint8_t WeaponBlade(std::int32_t slot) override {
    return weapon_blades.at(static_cast<std::size_t>(slot));
  }
  inv::BladeModel Blade(std::int32_t slot) override {
    return blades.at(static_cast<std::size_t>(slot));
  }
  inv::ShieldModel Shield(std::int32_t slot) override {
    return shields.at(static_cast<std::size_t>(slot));
  }
  std::optional<inv::HeldBlade> HeldWeapon() override {
    Log("held", {});
    return held;
  }
  bool IsTwoHanded(std::uint8_t category) override {
    Log("hands", {category});
    return two_handed.at(category);
  }
  bool RowsIdle() override { return rows_idle; }
  void SetRowText(std::int32_t row, std::uint16_t name,
                  std::uint16_t description) override {
    Log("text", {row, name, description});
  }
  std::uint16_t MenuTextEntry(std::size_t index) override {
    return menu_text.at(index);
  }
  std::int32_t FormatRow(EquipmentTable table, std::int32_t slot,
                         std::int32_t row) override {
    Log("format", {static_cast<std::int64_t>(table), slot, row});
    return FormatFlags(table, slot);
  }
  void CreateChoiceRows(std::span<const std::int32_t> flags) override {
    Log("rows", {static_cast<std::int64_t>(flags.size())});
    choice_flags.assign(flags.begin(), flags.end());
  }
  void UpdateChoices(std::int32_t has_sfx) override {
    Log("choices", {has_sfx});
    if (item_after_input) {
      highlighted_item = *item_after_input;
    }
  }
  std::int32_t SelectedChoice() override { return selected_choice; }
  std::uint8_t HighlightedItem() override { return highlighted_item; }
  void PreviewItem(EquipmentMenu menu, std::int32_t item) override {
    Log("preview", {static_cast<std::int64_t>(menu), item});
  }
  void UpdateStatPages(std::int32_t command) override {
    Log("pages", {command});
  }
  void UpdateDpPpBars(std::int32_t flags) override { Log("bars", {flags}); }
  void UpdatePrimaryStatPanel(std::int32_t animate) override {
    Log("panel", {animate});
  }
  void ClearMenuExcept(std::int32_t row) override { Log("clear", {row}); }
  void FlyOutRowLeft(std::int32_t row) override { Log("flyout", {row}); }
  void UnequipWeapon() override { Log("unequip weapon", {}); }
  void UnequipShield() override { Log("unequip shield", {}); }
  void EquipWeapon(std::int32_t slot) override { Log("equip weapon", {slot}); }
  void EquipShield(std::int32_t slot) override { Log("equip shield", {slot}); }
  void EquipArmor(std::int32_t part,
                  std::optional<std::int32_t> slot) override {
    Log("equip armor", {part, slot.value_or(-1)});
  }
  void EquipAccessory(std::optional<std::int32_t> slot) override {
    Log("equip accessory", {slot.value_or(-1)});
  }
  bool ReloadModels(std::int32_t category, std::int32_t model_id,
                    std::int32_t material) override {
    Log("reload", {category, model_id, material});
    if (reload_frames > 0) {
      --reload_frames;
      return true;
    }
    return false;
  }
  fw::MenuRow& SetMenuItem(const fw::RowPlacement& placement,
                           std::uint16_t text) override {
    Log("item", {placement.row, placement.x, placement.y, placement.w,
                 placement.background_width, text});
    return items.emplace_back();
  }
  void CreateTopRows(
      std::span<const std::int32_t, inv::kEquipmentTopRows> flags,
      std::int32_t row_info) override {
    Log("top rows", {row_info});
    std::ranges::copy(flags, top_flags.begin());
  }
  bool SquarePressed() override { return square; }
  void PlayInvalidSound() override { Log("invalid", {}); }
  std::int32_t SelectedRow() override { return selected_row; }
  std::int32_t HighlightedRow() override { return highlighted_row; }
  void StartTransition(std::int32_t row) override { Log("start", {row}); }
  void ContinueTransition(std::int32_t row) override { Log("continue", {row}); }
  void SetPreviewState(std::int32_t state) override {
    Log("preview state", {state});
  }
  void SetCategoryIconsEnabled(std::uint8_t enabled) override {
    Log("icons", {enabled});
  }
  void SetStatusCommandState(std::uint8_t state) override {
    Log("status", {state});
  }
  void SetCategoryIconOnTop(std::int32_t icon) override { Log("icon", {icon}); }
  std::int32_t Dispatch(std::int32_t row) override {
    Log("dispatch", {row});
    return dispatch_result;
  }
};

[[nodiscard]] bool Called(const FakeHost& host, std::string_view call) {
  return std::ranges::find(host.calls, call) != host.calls.end();
}

// The calls a slot menu makes when a row is chosen: the closing rows and
// panels, with the DP/PP bars unless it is the accessory menu.
[[nodiscard]] Calls Closing(bool bars) {
  Calls calls{"clear 40", "flyout -1", "pages -1"};
  if (bars) {
    calls.emplace_back("bars 4");
  }
  calls.emplace_back("panel 2");
  return calls;
}

[[nodiscard]] Calls Join(Calls first, const Calls& second) {
  first.insert(first.end(), second.begin(), second.end());
  return first;
}

constexpr auto kArmor = EquipmentTable::kArmor;
constexpr auto kWeapons = EquipmentTable::kWeapons;
constexpr auto kShields = EquipmentTable::kShields;

// ---------------------------------------------------------------------------
// F02630 _equipAccessoryMenu

TEST_CASE("Accessory menu opens on the worn accessory's stat preview",
          "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kArmor)] = {3, 2};
  host.limbs[1] = inv::kAccessoryLimb;
  inv::AccessoryMenuState state{.selected = 5, .step = 3};
  CHECK(inv::UpdateAccessoryMenu(state, host, 1) == 0);
  CHECK(state.step == 0);
  CHECK(state.equipped_accessory == 2);
  CHECK(host.calls == Calls{"preview 3 2", "pages 7", "panel 1"});

  FakeHost bare;
  inv::AccessoryMenuState none{.equipped_accessory = 4};
  CHECK(inv::UpdateAccessoryMenu(none, bare, 1) == 0);
  CHECK(none.equipped_accessory == 0);
  CHECK(bare.calls.front() == "preview 3 0");
}

TEST_CASE("Accessory menu lists the worn accessory then spare ones",
          "[equipment-menus]") {
  FakeHost host;
  host.counts[4] = 3;
  host.listed[Index(kArmor)] = {3, 2, 1};
  host.limbs[1] = inv::kAccessoryLimb;
  host.categories[0] = inv::kAccessoryLimb;
  host.categories[2] = inv::kAccessoryLimb;
  host.limbs[2] = 3;
  inv::AccessoryMenuState state{.equipped_accessory = 2};
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(state.step == 1);
  CHECK(host.calls ==
        Calls{"count 4", "format 2 1 0", "format 2 0 1", "rows 2"});
  CHECK(host.choice_flags ==
        std::vector{ChoiceFlags(kArmor, 1), ChoiceFlags(kArmor, 0)});
}

TEST_CASE("Accessory menu shows none without a worn accessory",
          "[equipment-menus]") {
  FakeHost host;
  host.counts[4] = 1;
  host.listed[Index(kArmor)] = {1};
  host.categories[0] = inv::kAccessoryLimb;
  inv::AccessoryMenuState state{};
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(host.calls ==
        Calls{"count 4", "text 0 300 310", "format 2 0 1", "rows 2"});
  CHECK(host.choice_flags == std::vector{1, ChoiceFlags(kArmor, 0)});
}

TEST_CASE("Accessory menu waits for idle rows before listing",
          "[equipment-menus]") {
  FakeHost host;
  host.rows_idle = false;
  inv::AccessoryMenuState state{};
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(state.step == 0);
  CHECK(host.calls.empty());
}

TEST_CASE("Accessory menu swaps the worn accessory for the chosen one",
          "[equipment-menus]") {
  FakeHost host;
  host.limbs[1] = inv::kAccessoryLimb;
  host.highlighted_item = 2;
  host.selected_choice = 1;
  host.item_after_input = 1;
  inv::AccessoryMenuState state{.step = 1, .equipped_accessory = 2};
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 3 2", "choices 1"}, Closing(false)),
                           {"limb 1 0", "limb 0 7", "equip accessory 0"}));
  CHECK(state.selected == 2);
  CHECK(state.step == 2);
  host.calls.clear();
  host.rows_idle = false;
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(state.step == 3);
  host.rows_idle = true;
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 2);
  CHECK(host.calls.empty());
}

TEST_CASE("Accessory menu none row takes the accessory off",
          "[equipment-menus]") {
  FakeHost host;
  host.limbs[1] = inv::kAccessoryLimb;
  host.selected_choice = 0;
  inv::AccessoryMenuState state{.step = 1, .equipped_accessory = 2};
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 3 0", "choices 1"}, Closing(false)),
                           {"limb 1 0", "equip accessory -1"}));
  CHECK(host.limbs[1] == 0);
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 1);
}

TEST_CASE("Accessory menu closes without equipping on back and quit",
          "[equipment-menus]") {
  for (const auto [choice, result] :
       {std::pair{kListBack, -1}, std::pair{kListQuit, -2}}) {
    FakeHost host;
    host.selected_choice = choice;
    host.limbs[1] = inv::kAccessoryLimb;
    inv::AccessoryMenuState state{.step = 1, .equipped_accessory = 2};
    CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
    CHECK(state.step == 3);
    CHECK(host.calls == Join({"preview 3 0", "choices 1"}, Closing(false)));
    host.rows_idle = false;
    CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
    host.rows_idle = true;
    CHECK(inv::UpdateAccessoryMenu(state, host, 0) == result);
    CHECK(host.limbs[1] == inv::kAccessoryLimb);
  }
}

TEST_CASE("Accessory menu keeps the list open until a row is chosen",
          "[equipment-menus]") {
  FakeHost open;
  inv::AccessoryMenuState state{.step = 1};
  CHECK(inv::UpdateAccessoryMenu(state, open, 0) == 0);
  CHECK(state.step == 1);
  CHECK(state.selected == 0);
}

TEST_CASE("Accessory menu refuses listed slots and counts past the table",
          "[equipment-menus]") {
  FakeHost listed;
  listed.listed[Index(kArmor)] = {17};
  inv::AccessoryMenuState state{.selected = 4, .step = 3};
  CHECK_FALSE(inv::UpdateAccessoryMenu(state, listed, 1).has_value());
  CHECK(state.step == 3);
  CHECK(listed.calls.empty());

  state.step = 0;
  listed.counts[4] = 17;
  CHECK_FALSE(inv::UpdateAccessoryMenu(state, listed, 0).has_value());
  listed.counts[4] = 1;
  CHECK_FALSE(inv::UpdateAccessoryMenu(state, listed, 0).has_value());
  CHECK(listed.calls == Calls{"count 4", "count 4"});
  CHECK(state.step == 0);
}

TEST_CASE("Accessory menu refuses previews and rows past the table",
          "[equipment-menus]") {
  inv::AccessoryMenuState state{.selected = 4, .step = 1};
  FakeHost previewed;
  previewed.highlighted_item = 17;
  CHECK_FALSE(inv::UpdateAccessoryMenu(state, previewed, 0).has_value());
  CHECK(previewed.calls.empty());
  FakeHost row;
  row.selected_choice = 17;
  CHECK_FALSE(inv::UpdateAccessoryMenu(state, row, 0).has_value());
  for (const std::uint8_t item : {std::uint8_t{0}, std::uint8_t{17}}) {
    FakeHost chosen;
    chosen.selected_choice = 1;
    chosen.item_after_input = item;
    CHECK_FALSE(inv::UpdateAccessoryMenu(state, chosen, 0).has_value());
    CHECK_FALSE(Called(chosen, "clear 40"));
  }
  CHECK(state.selected == 4);
  CHECK(state.step == 1);
}

TEST_CASE("Accessory menu refuses a worn slot past the table",
          "[equipment-menus]") {
  FakeHost worn;
  worn.selected_choice = 0;
  inv::AccessoryMenuState stale{.step = 1, .equipped_accessory = 17};
  CHECK_FALSE(inv::UpdateAccessoryMenu(stale, worn, 0).has_value());
  CHECK_FALSE(Called(worn, "clear 40"));
  CHECK(stale.step == 1);
}

TEST_CASE("Accessory menu reopens on the newly worn accessory",
          "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kArmor)] = {2, 1};
  host.limbs[1] = inv::kAccessoryLimb;
  host.selected_choice = 2;
  host.item_after_input = 1;
  inv::AccessoryMenuState state{};
  CHECK(inv::UpdateAccessoryMenu(state, host, 1) == 0);
  state.step = 1;
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 3);
  host.calls.clear();
  CHECK(inv::UpdateAccessoryMenu(state, host, 1) == 0);
  CHECK(state.equipped_accessory == 1);
  CHECK(state.step == 0);
  CHECK(host.calls.front() == "preview 3 1");
}

TEST_CASE("Accessory menu stays idle for an unknown step",
          "[equipment-menus]") {
  FakeHost host;
  inv::AccessoryMenuState state{.selected = 2, .step = 9};
  CHECK(inv::UpdateAccessoryMenu(state, host, 0) == 0);
  CHECK(host.calls.empty());
  CHECK(state.step == 9);
}

// ---------------------------------------------------------------------------
// F02631 _equipWeaponMenu

TEST_CASE("Weapon menu opens on the held weapon with DP and PP bars",
          "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kWeapons)] = {3, 1};
  host.weapons_held[0] = true;
  inv::WeaponMenuState state{.step = 2};
  CHECK(inv::UpdateWeaponMenu(state, host, 1) == 0);
  CHECK(state.step == 0);
  CHECK(host.calls == Calls{"preview 1 1", "pages 7", "bars 3", "panel 1"});
}

TEST_CASE("Weapon menu lists the held weapon then the spare weapons",
          "[equipment-menus]") {
  FakeHost host;
  host.counts[0] = 2;
  host.listed[Index(kWeapons)] = {3, 1};
  host.weapons_held[0] = true;
  inv::WeaponMenuState state{};
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(state.step == 1);
  CHECK(host.calls ==
        Calls{"count 0", "format 0 0 0", "format 0 2 1", "rows 2"});
  CHECK(host.choice_flags ==
        std::vector{ChoiceFlags(kWeapons, 0), ChoiceFlags(kWeapons, 2)});
}

TEST_CASE("Weapon menu shows none when the held weapon is unlisted",
          "[equipment-menus]") {
  FakeHost host;
  host.counts[0] = 1;
  host.listed[Index(kWeapons)] = {1};
  host.weapons_held[5] = true;
  inv::WeaponMenuState state{};
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(host.calls ==
        Calls{"count 0", "text 0 300 310", "format 0 0 1", "rows 2"});
  CHECK(host.choice_flags == std::vector{1, ChoiceFlags(kWeapons, 0)});
}

TEST_CASE("Weapon menu equips a one-handed weapon with its model",
          "[equipment-menus]") {
  FakeHost host;
  host.highlighted_item = 1;
  host.selected_choice = 1;
  host.item_after_input = 3;
  host.weapon_blades[2] = 5;
  host.blades[4] = {.category = 2, .model_id = 7, .material = 3};
  host.reload_frames = 1;
  inv::WeaponMenuState state{.step = 1};
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(host.calls ==
        Join(Join({"preview 1 1", "choices 0"}, Closing(true)),
             {"unequip weapon", "mark 0 2", "hands 2", "equip weapon 2"}));
  CHECK(state.selected == 2);
  CHECK(state.step == 2);
  CHECK(state.category == 2);
  CHECK(state.model_id == 7);
  CHECK(state.material == 3);
}

TEST_CASE("Weapon menu waits for the model reload before closing",
          "[equipment-menus]") {
  FakeHost host;
  host.reload_frames = 1;
  inv::WeaponMenuState state{
      .selected = 2, .step = 2, .category = 2, .model_id = 7, .material = 3};
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(state.step == 2);
  host.rows_idle = false;
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(state.step == 3);
  CHECK(host.calls == Calls{"reload 2 7 3", "reload 2 7 3"});
  host.rows_idle = true;
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 2);
}

TEST_CASE("Weapon menu drops the shield for a two-handed weapon",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = 1;
  host.item_after_input = 1;
  host.weapon_blades[0] = 16;
  host.blades[15] = {.category = 10, .model_id = 4, .material = 1};
  host.two_handed[10] = true;
  inv::WeaponMenuState state{.step = 1};
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 1 0", "choices 0"}, Closing(true)),
                           {"unequip weapon", "mark 0 0", "hands 10",
                            "unequip shield", "equip weapon 0"}));
  CHECK(state.category == 10);
}

TEST_CASE("Weapon menu none row leaves Ashley bare-handed",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = 0;
  inv::WeaponMenuState state{
      .step = 1, .category = 2, .model_id = 7, .material = 3};
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 1 0", "choices 0"}, Closing(true)),
                           {"unequip weapon"}));
  CHECK(state.category == inv::kUnarmedCategory);
  CHECK(state.model_id == 0);
  CHECK(state.material == 0);
  host.calls.clear();
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 1);
  CHECK(host.calls == Calls{"reload 10 0 0"});
}

TEST_CASE("Weapon menu back and quit close without a model reload",
          "[equipment-menus]") {
  for (const auto [choice, result] :
       {std::pair{kListBack, -1}, std::pair{kListQuit, -2}}) {
    FakeHost host;
    host.selected_choice = choice;
    inv::WeaponMenuState state{.step = 1};
    CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
    CHECK(state.step == 3);
    CHECK(host.calls == Join({"preview 1 0", "choices 0"}, Closing(true)));
    host.calls.clear();
    CHECK(inv::UpdateWeaponMenu(state, host, 0) == result);
    CHECK(host.calls.empty());
  }
}

TEST_CASE("Weapon menu refuses blades and categories outside the tables",
          "[equipment-menus]") {
  struct Case {
    std::uint8_t blade;
    std::uint8_t category;
  };
  for (const Case bad : {Case{0, 1}, Case{17, 1}, Case{1, 0}, Case{1, 11}}) {
    FakeHost host;
    host.selected_choice = 1;
    host.item_after_input = 1;
    host.weapon_blades[0] = bad.blade;
    host.blades[0].category = bad.category;
    inv::WeaponMenuState state{.step = 1};
    CHECK_FALSE(inv::UpdateWeaponMenu(state, host, 0).has_value());
    CHECK(state.selected == 0);
    CHECK(state.step == 1);
    CHECK_FALSE(Called(host, "clear 40"));
  }
  FakeHost listed;
  listed.listed[Index(kWeapons)] = {9};
  inv::WeaponMenuState state{.step = 3};
  CHECK_FALSE(inv::UpdateWeaponMenu(state, listed, 1).has_value());
  CHECK(state.step == 3);
}

TEST_CASE("Weapon menu reopens on the newly held weapon", "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kWeapons)] = {1, 2};
  host.weapons_held[0] = true;
  host.selected_choice = 1;
  host.item_after_input = 2;
  host.weapon_blades[1] = 1;
  host.blades[0].category = 3;
  inv::WeaponMenuState state{};
  CHECK(inv::UpdateWeaponMenu(state, host, 1) == 0);
  state.step = 1;
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 0);
  CHECK(inv::UpdateWeaponMenu(state, host, 0) == 2);
  host.weapons_held = {false, true};
  host.calls.clear();
  CHECK(inv::UpdateWeaponMenu(state, host, 1) == 0);
  CHECK(state.step == 0);
  CHECK(host.calls.front() == "preview 1 2");
}

// ---------------------------------------------------------------------------
// F02632 _equipShieldMenu

TEST_CASE("Shield menu opens on the held shield with DP and PP bars",
          "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kShields)] = {2};
  host.shields_held[1] = true;
  inv::ShieldMenuState state{.step = 3};
  CHECK(inv::UpdateShieldMenu(state, host, 1) == 0);
  CHECK(state.step == 0);
  CHECK(host.calls == Calls{"preview 2 2", "pages 7", "bars 3", "panel 1"});
}

TEST_CASE("Shield menu lists the held shield then the spare shields",
          "[equipment-menus]") {
  FakeHost host;
  host.counts[3] = 3;
  host.listed[Index(kShields)] = {2, 1, 4};
  host.shields_held[1] = true;
  inv::ShieldMenuState state{};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(state.step == 1);
  CHECK(host.calls == Calls{"count 3", "format 1 1 0", "format 1 0 1",
                            "format 1 3 2", "rows 3"});
  CHECK(host.choice_flags == std::vector{ChoiceFlags(kShields, 1),
                                         ChoiceFlags(kShields, 0),
                                         ChoiceFlags(kShields, 3)});
}

TEST_CASE("Shield menu keeps a one-handed weapon when equipping",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = 1;
  host.item_after_input = 2;
  host.held = inv::HeldBlade{.id = 5, .category = 2};
  host.shields[1] = {.model_id = 9, .material = 4};
  inv::ShieldMenuState state{.step = 1};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(host.calls ==
        Join(Join({"preview 2 0", "choices 0", "held"}, Closing(true)),
             {"unequip shield", "mark 1 1", "hands 2", "equip shield 1"}));
  CHECK(state.model_id == 9);
  CHECK(state.material == 4);
  host.calls.clear();
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 2);
  CHECK(host.calls == Calls{"reload 0 9 4"});
}

TEST_CASE("Shield menu frees the hand a two-handed weapon holds",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = 1;
  host.item_after_input = 1;
  host.held = inv::HeldBlade{.id = 5, .category = 6};
  host.two_handed[6] = true;
  inv::ShieldMenuState state{.step = 1};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(host.calls ==
        Join(Join({"preview 2 0", "choices 0", "held"}, Closing(true)),
             {"unequip shield", "mark 1 0", "hands 6", "unequip weapon",
              "equip shield 0"}));
}

TEST_CASE("Shield menu skips the hand check for an empty hand",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = 1;
  host.item_after_input = 1;
  host.held = inv::HeldBlade{.id = 0, .category = 0};
  inv::ShieldMenuState state{.step = 1};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK_FALSE(Called(host, "hands 0"));
  CHECK_FALSE(Called(host, "unequip weapon"));
  CHECK(Called(host, "equip shield 0"));
}

TEST_CASE("Shield menu none row removes the shield", "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = 0;
  host.held.reset();
  inv::ShieldMenuState state{.step = 1, .model_id = 9, .material = 4};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 2 0", "choices 0"}, Closing(true)),
                           {"unequip shield"}));
  CHECK(state.model_id == 0);
  CHECK(state.material == 0);
  host.calls.clear();
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 1);
  CHECK(host.calls == Calls{"reload 0 0 0"});
}

TEST_CASE("Shield menu refuses a shield without Ashley's battle stats",
          "[equipment-menus]") {
  for (const std::optional<inv::HeldBlade> held :
       {std::optional<inv::HeldBlade>{},
        std::optional{inv::HeldBlade{.id = 3, .category = 11}},
        std::optional{inv::HeldBlade{.id = 3, .category = 0}}}) {
    FakeHost host;
    host.selected_choice = 1;
    host.item_after_input = 1;
    host.held = held;
    inv::ShieldMenuState state{.step = 1};
    CHECK_FALSE(inv::UpdateShieldMenu(state, host, 0).has_value());
    CHECK(state.selected == 0);
    CHECK(state.step == 1);
    CHECK_FALSE(Called(host, "clear 40"));
  }
}

TEST_CASE("Shield menu waits for the model reload before closing",
          "[equipment-menus]") {
  FakeHost host;
  host.reload_frames = 2;
  inv::ShieldMenuState state{
      .selected = 2, .step = 2, .model_id = 9, .material = 4};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(state.step == 2);
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 2);
  CHECK(state.step == 3);
  CHECK(host.calls == Calls{"reload 0 9 4", "reload 0 9 4", "reload 0 9 4"});
}

TEST_CASE("Shield menu back closes without a model reload",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_choice = kListBack;
  inv::ShieldMenuState state{.step = 1};
  CHECK(inv::UpdateShieldMenu(state, host, 0) == 0);
  CHECK(host.calls == Join({"preview 2 0", "choices 0"}, Closing(true)));
  host.calls.clear();
  CHECK(inv::UpdateShieldMenu(state, host, 0) == -1);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------------------
// F02633 _equipArmorMenu

TEST_CASE("Armor menu opens on the armor worn on the body part",
          "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kArmor)] = {4, 2};
  host.limbs[3] = 1;
  host.limbs[1] = 3;
  inv::ArmorMenuState state{.step = 3};
  CHECK(inv::UpdateArmorMenu(state, host, 3) == 0);
  CHECK(state.step == 0);
  CHECK(state.body_part == 3);
  CHECK(state.armor_index == 2);
  CHECK(host.calls == Calls{"preview 0 2", "pages 7", "bars 1", "panel 1"});
}

TEST_CASE("Armor menu lists spares of the body part's category",
          "[equipment-menus]") {
  FakeHost host;
  host.counts[4] = 4;
  host.listed[Index(kArmor)] = {4, 2, 1, 3};
  host.limbs[1] = 3;
  host.categories[3] = 2;
  host.categories[0] = 2;
  host.limbs[0] = 4;
  host.categories[2] = 5;
  inv::ArmorMenuState state{.armor_index = 2, .body_part = 3};
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 0);
  CHECK(state.step == 1);
  CHECK(host.calls ==
        Calls{"count 4", "format 2 1 0", "format 2 3 1", "rows 2"});
  CHECK(host.choice_flags ==
        std::vector{ChoiceFlags(kArmor, 1), ChoiceFlags(kArmor, 3)});

  FakeHost arms;
  arms.counts[4] = 1;
  arms.listed[Index(kArmor)] = {1};
  arms.categories[0] = 5;
  inv::ArmorMenuState left{.body_part = 2};
  CHECK(inv::UpdateArmorMenu(left, arms, 0) == 0);
  CHECK(arms.choice_flags == std::vector{1, ChoiceFlags(kArmor, 0)});
}

TEST_CASE("Armor menu moves the chosen armor onto the body part",
          "[equipment-menus]") {
  FakeHost host;
  host.limbs[1] = 3;
  host.highlighted_item = 2;
  host.selected_choice = 1;
  host.item_after_input = 4;
  inv::ArmorMenuState state{.step = 1, .armor_index = 2, .body_part = 3};
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 0 2", "choices 1"}, Closing(true)),
                           {"limb 1 0", "limb 3 3", "equip armor 2 3"}));
  CHECK(state.step == 2);
  host.calls.clear();
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 2);
  CHECK(host.calls.empty());
}

TEST_CASE("Armor menu none row empties the body part", "[equipment-menus]") {
  FakeHost host;
  host.limbs[1] = 5;
  host.selected_choice = 0;
  inv::ArmorMenuState state{.step = 1, .armor_index = 2, .body_part = 5};
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 0);
  CHECK(host.calls == Join(Join({"preview 0 0", "choices 1"}, Closing(true)),
                           {"limb 1 0", "equip armor 4 -1"}));
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 1);
}

TEST_CASE("Armor menu refuses body parts outside one to seven",
          "[equipment-menus]") {
  FakeHost host;
  inv::ArmorMenuState state{.step = 3, .armor_index = 4, .body_part = 2};
  for (const std::int32_t part : {8, -1, 256}) {
    CHECK_FALSE(inv::UpdateArmorMenu(state, host, part).has_value());
  }
  CHECK(state.step == 3);
  CHECK(state.body_part == 2);
  CHECK(host.calls.empty());
}

TEST_CASE("Armor menu refuses a stale body part or armor slot",
          "[equipment-menus]") {
  FakeHost host;
  inv::ArmorMenuState unset{};
  CHECK_FALSE(inv::UpdateArmorMenu(unset, host, 0).has_value());
  FakeHost chosen;
  chosen.selected_choice = 0;
  inv::ArmorMenuState stale{.step = 1, .body_part = 9};
  CHECK_FALSE(inv::UpdateArmorMenu(stale, chosen, 0).has_value());
  inv::ArmorMenuState worn{.step = 1, .armor_index = 17, .body_part = 1};
  CHECK_FALSE(inv::UpdateArmorMenu(worn, chosen, 0).has_value());
  CHECK_FALSE(Called(chosen, "clear 40"));
}

TEST_CASE("Armor menu opens on body part seven", "[equipment-menus]") {
  inv::ArmorMenuState state{.step = 3, .armor_index = 4, .body_part = 2};
  FakeHost accessory;
  accessory.listed[Index(kArmor)] = {1};
  accessory.limbs[0] = inv::kAccessoryLimb;
  CHECK(inv::UpdateArmorMenu(state, accessory, 7) == 0);
  CHECK(state.armor_index == 1);
}

TEST_CASE("Armor menu back closes the bars without equipping",
          "[equipment-menus]") {
  FakeHost host;
  host.limbs[1] = 3;
  host.selected_choice = kListQuit;
  inv::ArmorMenuState state{.step = 1, .armor_index = 2, .body_part = 3};
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 0);
  CHECK(host.calls == Join({"preview 0 0", "choices 1"}, Closing(true)));
  CHECK(host.limbs[1] == 3);
  host.rows_idle = false;
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 0);
  host.rows_idle = true;
  CHECK(inv::UpdateArmorMenu(state, host, 0) == -2);
}

TEST_CASE("Armor menu reopens on another body part", "[equipment-menus]") {
  FakeHost host;
  host.listed[Index(kArmor)] = {1, 2};
  host.limbs[0] = 3;
  host.limbs[1] = 5;
  host.selected_choice = kListBack;
  inv::ArmorMenuState state{};
  CHECK(inv::UpdateArmorMenu(state, host, 3) == 0);
  state.step = 1;
  CHECK(inv::UpdateArmorMenu(state, host, 0) == 0);
  CHECK(inv::UpdateArmorMenu(state, host, 0) == -1);
  host.calls.clear();
  CHECK(inv::UpdateArmorMenu(state, host, 5) == 0);
  CHECK(state.body_part == 5);
  CHECK(state.armor_index == 2);
  CHECK(state.step == 0);
  CHECK(host.calls.front() == "preview 0 2");
}

// ---------------------------------------------------------------------------
// F02634 _dispatchEquipMenu

std::vector<std::pair<int, int>> g_handled;

template <int Menu>
int Record(int argument) {
  g_handled.emplace_back(Menu, argument);
  return (Menu * 100) + argument;
}

constexpr inv::EquipmentMenuHandlers kRecorders{
    Record<0>,
    Record<1>,
    Record<2>,
    Record<3>,
};

TEST_CASE("Dispatch opens the weapon and shield menus on rows 1 and 2",
          "[equipment-menus]") {
  g_handled.clear();
  std::uint8_t current = 3;
  CHECK(inv::DispatchEquipmentMenu(current, kRecorders, 1) == 101);
  CHECK(current == 1);
  CHECK(inv::DispatchEquipmentMenu(current, kRecorders, 2) == 202);
  CHECK(current == 2);
  CHECK(g_handled == std::vector<std::pair<int, int>>{{1, 1}, {2, 2}});
}

TEST_CASE("Dispatch opens the armor menu on parts 1 to 5 for rows 3 to 7",
          "[equipment-menus]") {
  for (int row = 3; row <= 7; ++row) {
    g_handled.clear();
    std::uint8_t current = 1;
    CHECK(inv::DispatchEquipmentMenu(current, kRecorders, row) == row - 2);
    CHECK(current == 0);
    CHECK(g_handled == std::vector<std::pair<int, int>>{{0, row - 2}});
  }
}

TEST_CASE("Dispatch opens the accessory menu on row 8", "[equipment-menus]") {
  g_handled.clear();
  std::uint8_t current = 0;
  CHECK(inv::DispatchEquipmentMenu(current, kRecorders, 8) == 308);
  CHECK(current == 3);
  CHECK(g_handled == std::vector<std::pair<int, int>>{{3, 8}});
}

TEST_CASE("Dispatch row 0 updates the open menu", "[equipment-menus]") {
  g_handled.clear();
  std::uint8_t current = 2;
  CHECK(inv::DispatchEquipmentMenu(current, kRecorders, 0) == 200);
  CHECK(inv::DispatchEquipmentMenu(current, kRecorders, 0) == 200);
  CHECK(current == 2);
  current = 0;
  CHECK(inv::DispatchEquipmentMenu(current, kRecorders, 0) == 0);
  CHECK(g_handled == std::vector<std::pair<int, int>>{{2, 0}, {2, 0}, {0, 0}});
}

TEST_CASE("Dispatch calls whichever handler the table binds",
          "[equipment-menus]") {
  g_handled.clear();
  constexpr inv::EquipmentMenuHandlers kReversed{Record<3>, Record<2>,
                                                 Record<1>, Record<0>};
  std::uint8_t current = 0;
  CHECK(inv::DispatchEquipmentMenu(current, kReversed, 1) == 201);
  CHECK(inv::DispatchEquipmentMenu(current, kReversed, 8) == 8);
  CHECK(inv::DispatchEquipmentMenu(current, kReversed, 4) == 302);
  CHECK(current == 0);
  CHECK(g_handled == std::vector<std::pair<int, int>>{{2, 1}, {0, 8}, {3, 2}});
}

TEST_CASE("Dispatch refuses rows outside the menu and unbound handlers",
          "[equipment-menus]") {
  g_handled.clear();
  std::uint8_t current = 2;
  CHECK_FALSE(inv::DispatchEquipmentMenu(current, kRecorders, -1));
  CHECK_FALSE(inv::DispatchEquipmentMenu(current, kRecorders, 9));
  CHECK(current == 2);
  constexpr inv::EquipmentMenuHandlers kUnbound{Record<0>, nullptr, Record<2>,
                                                Record<3>};
  CHECK_FALSE(inv::DispatchEquipmentMenu(current, kUnbound, 1));
  CHECK(current == 2);
  current = 7;
  CHECK_FALSE(inv::DispatchEquipmentMenu(current, kRecorders, 0));
  CHECK(current == 7);
  CHECK(g_handled.empty());
}

// ---------------------------------------------------------------------------
// F02635 _equipMenu

[[nodiscard]] Calls NoneTexts() {
  Calls calls;
  for (int row = 0; row < 8; ++row) {
    calls.push_back("text " + std::to_string(row) + " 300 310");
  }
  return calls;
}

TEST_CASE("Equipment menu initializer clears the rows and lights icon 1",
          "[equipment-menus]") {
  FakeHost host;
  inv::EquipmentMenuState state{.selected_row = 4, .step = 5};
  CHECK(inv::UpdateEquipmentMenu(state, host, 1) == 0);
  CHECK(state.step == 0);
  CHECK(state.selected_row == 4);
  CHECK(host.calls == Calls{"clear 40", "icon 1"});
}

TEST_CASE("Equipment menu shows the title then builds the top rows",
          "[equipment-menus]") {
  FakeHost host;
  host.highlighted_row = 3;
  inv::EquipmentMenuState state{};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(state.step == 2);
  CHECK(host.calls ==
        Join(Join({"item 31 320 18 126 8 53", "icon 1"}, NoneTexts()),
             {"top rows 102708", "item 31 180 18 140 8 403"}));
  REQUIRE(host.items.size() == 2);
  CHECK(host.items[0].state == fw::RowState::kSlideX);
  CHECK(host.items[0].target0 == 180);
  CHECK(host.items[0].selected == 1);
  CHECK(host.items[1].selected == 1);
  for (std::size_t row = 0; row < inv::kEquipmentTopRows; ++row) {
    CHECK(host.top_flags.at(row) == kTopRowTypes.at(row) << kRowTypeShift);
  }
}

TEST_CASE("Equipment menu top rows show the first equipped items",
          "[equipment-menus]") {
  FakeHost host;
  host.weapons_held[2] = true;
  host.weapons_held[5] = true;
  host.shields_held[4] = true;
  host.limbs[3] = 1;
  host.limbs[9] = 1;
  host.limbs[6] = 5;
  host.limbs[7] = inv::kAccessoryLimb;
  host.limbs[8] = 6;
  inv::EquipmentMenuState state{.step = 1};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls ==
        Join(Join({"icon 1"}, NoneTexts()),
             {"format 0 2 0", "format 1 4 1", "format 2 3 2", "format 2 6 6",
              "format 2 7 7", "top rows 102708", "item 31 180 18 140 8 400"}));
  const std::array expected{FormatFlags(kWeapons, 2) | (0x78 << kRowTypeShift),
                            FormatFlags(kShields, 4) | (0x79 << kRowTypeShift),
                            FormatFlags(kArmor, 3) | (0x69 << kRowTypeShift),
                            0x6A << kRowTypeShift,
                            0x68 << kRowTypeShift,
                            0x6D << kRowTypeShift,
                            FormatFlags(kArmor, 6) | (0x6B << kRowTypeShift),
                            FormatFlags(kArmor, 7) | (0x7A << kRowTypeShift)};
  CHECK(host.top_flags == expected);
}

TEST_CASE("Equipment menu waits for idle rows before the title and rows",
          "[equipment-menus]") {
  FakeHost host;
  host.rows_idle = false;
  for (const std::uint8_t step : {std::uint8_t{0}, std::uint8_t{1}}) {
    inv::EquipmentMenuState state{.step = step};
    CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
    CHECK(state.step == step);
  }
  CHECK(host.calls.empty());
}

TEST_CASE("Equipment menu opens the chosen slot menu through dispatch",
          "[equipment-menus]") {
  FakeHost host;
  host.selected_row = 1;
  inv::EquipmentMenuState state{.step = 2};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls == Calls{"start 11"});
  CHECK(state.selected_row == 2);
  CHECK(state.step == 4);
  host.calls.clear();
  host.rows_idle = false;
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls.empty());
  host.rows_idle = true;
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls == Calls{"preview state 1", "continue 11", "icons 0",
                            "status 0", "dispatch 2"});
  CHECK(state.step == 5);
}

TEST_CASE("Equipment menu returns to the top after a slot menu closes",
          "[equipment-menus]") {
  FakeHost host;
  inv::EquipmentMenuState state{.selected_row = 3, .step = 5};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls == Calls{"dispatch 0"});
  CHECK(state.step == 5);
  for (const std::int32_t result : {-1, 3}) {
    host.calls.clear();
    host.dispatch_result = result;
    state.step = 5;
    CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
    CHECK(host.calls == Calls{"dispatch 0", "preview state 0", "icons 1",
                              "icon 2", "status 1"});
    CHECK(state.selected_row == result);
    CHECK(state.step == 0);
  }
}

TEST_CASE("Equipment menu quits at once when a slot menu quits",
          "[equipment-menus]") {
  FakeHost host;
  host.dispatch_result = -2;
  inv::EquipmentMenuState state{.step = 5};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(state.step == 3);
  host.rows_idle = false;
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == -2);
  CHECK(host.calls == Calls{"dispatch 0"});
}

TEST_CASE("Equipment menu back and quit light the header icons",
          "[equipment-menus]") {
  for (const auto [row, result] :
       {std::pair{kListBack, -1}, std::pair{kListQuit, -2}}) {
    FakeHost host;
    host.selected_row = row;
    inv::EquipmentMenuState state{.step = 2};
    CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
    CHECK(host.calls == Calls{"icon " + std::to_string(-result), "clear 40"});
    CHECK(state.step == 3);
    host.rows_idle = false;
    CHECK(inv::UpdateEquipmentMenu(state, host, 0) == (result == -2 ? -2 : 0));
    host.rows_idle = true;
    CHECK(inv::UpdateEquipmentMenu(state, host, 0) == result);
  }
}

TEST_CASE("Equipment menu shows the highlighted row's hint",
          "[equipment-menus]") {
  FakeHost host;
  host.highlighted_row = 5;
  inv::EquipmentMenuState state{.step = 2};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls == Calls{"item 31 180 18 140 8 405"});
  REQUIRE(host.items.size() == 1);
  CHECK(host.items[0].selected == 1);
  CHECK(state.step == 2);
  CHECK(state.selected_row == 0);
}

TEST_CASE("Equipment menu plays the invalid sound for Square",
          "[equipment-menus]") {
  FakeHost host;
  host.square = true;
  inv::EquipmentMenuState state{.step = 2};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls == Calls{"invalid", "item 31 180 18 140 8 400"});
  host.calls.clear();
  host.square = false;
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK_FALSE(Called(host, "invalid"));
}

TEST_CASE("Equipment menu refuses rows outside the menu", "[equipment-menus]") {
  FakeHost selected;
  selected.selected_row = 8;
  inv::EquipmentMenuState state{.selected_row = 4, .step = 2};
  CHECK_FALSE(inv::UpdateEquipmentMenu(state, selected, 0).has_value());
  CHECK(state.selected_row == 4);
  CHECK(state.step == 2);
  CHECK(selected.calls.empty());

  for (const std::int32_t row : {-1, 8}) {
    FakeHost highlighted;
    highlighted.highlighted_row = row;
    CHECK_FALSE(inv::UpdateEquipmentMenu(state, highlighted, 0).has_value());
    CHECK(highlighted.items.empty());
  }
}

TEST_CASE("Equipment menu refuses hint and none texts past the menu",
          "[equipment-menus]") {
  inv::EquipmentMenuState state{.selected_row = 4, .step = 2};
  FakeHost hint;
  hint.menu_text[12] = inv::kEquipmentMenuTextEntries;
  CHECK_FALSE(inv::UpdateEquipmentMenu(state, hint, 0).has_value());
  CHECK(hint.items.empty());

  FakeHost none;
  none.menu_text[21] = inv::kEquipmentMenuTextEntries;
  inv::EquipmentMenuState building{.step = 1};
  CHECK_FALSE(inv::UpdateEquipmentMenu(building, none, 0).has_value());
  CHECK(building.step == 1);
  CHECK(none.calls.empty());
}

TEST_CASE("Equipment menu stays idle for an unknown step",
          "[equipment-menus]") {
  FakeHost host;
  inv::EquipmentMenuState state{.selected_row = 2, .step = 6};
  CHECK(inv::UpdateEquipmentMenu(state, host, 0) == 0);
  CHECK(host.calls.empty());
  CHECK(state.step == 6);
}

// ---------------------------------------------------------------------------
// Live entries against the generated bodies over the same inventory, rows,
// panels and input.

namespace detail = equipment_detail_tests;
namespace dialogs = container_dialog_live_tests;
using Entry = int (*)(int);

constexpr std::int64_t kNullText = -1;
constexpr std::int64_t kForeignText = -2;
constexpr std::int64_t kHeapText = 0x1000000;
constexpr std::int64_t kStringBufferText = 0x2000000;
constexpr std::uint64_t kFnvOffsetBasis = 0xCBF29CE484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001B3ULL;
constexpr std::uint64_t kByteMask = 0xFF;
constexpr std::uint64_t kAccessoryMenuReferenceDigest = 0x287517A37AA6123FULL;
constexpr std::uint64_t kWeaponMenuReferenceDigest = 0xED19CA62CD17CF16ULL;
constexpr std::uint64_t kShieldMenuReferenceDigest = 0x21B68704F6CED140ULL;
constexpr std::uint64_t kArmorMenuReferenceDigest = 0xDC62BA46800779A3ULL;
constexpr std::uint64_t kDispatchMenuReferenceDigest = 0x006F6CABF6D0ADC9ULL;
constexpr std::uint64_t kEquipmentMenuReferenceDigest = 0xF5FF06A0898FEA0BULL;

// MENU3.PRG's list and preview globals the menus write; restored after each
// run (constructed before the fixture loads _menuText).
struct MenuGlobals {
  detail::ValueRestore<u_char> step{Menu_Inventory_equipListStep};
  detail::ValueRestore<int> selected{Menu_Inventory_selectedItemRow};
  detail::ValueRestore<char> count{Menu_Inventory_menuRowCount};
  detail::ValueRestore<char> first{Menu_Inventory_itemsFirstRow};
  detail::ValueRestore<char> equipped{Menu_Inventory_equippedItem};
  detail::ValueRestore<char*> items{Menu_Inventory_itemsText};
  detail::ValueRestore<char> load{Menu_Inventory_equipmentModelLoadStep};
  detail::ArrayRestore<u_short, std::size(Menu_Inventory_menuText)> text{
      Menu_Inventory_menuText};
  detail::ArrayRestore<u_char, std::size(Menu_Framework_weaponHands)> hands{
      Menu_Framework_weaponHands};

  MenuGlobals() {
    Menu_Inventory_itemsText = nullptr;
    Menu_Inventory_equipListStep = 0;
    std::ranges::fill(Menu_Framework_weaponHands, u_char{0});
    inv::ResetEquipmentRowsLive();
    inv::ResetEquipmentMenusLive();
  }
  ~MenuGlobals() {
    inv::ResetEquipmentRowsLive();
    inv::ResetEquipmentMenusLive();
  }
  MenuGlobals(const MenuGlobals&) = delete;
  MenuGlobals& operator=(const MenuGlobals&) = delete;
};

struct MenusFixture : MenuGlobals, dialogs::EquippedFixture {
  static void AddArmor(std::size_t slot, std::uint8_t id, std::uint8_t category,
                       std::uint8_t limb) {
    VS::Items::InventoryArmor armor{.id = id,
                                    .category = category,
                                    .max_dp = 100,
                                    .current_dp = 50,
                                    .strength = 2,
                                    .material = 1,
                                    .limb = limb};
    VS::Items::Live::Commit(std::span(Audio_Seq_unusedInventory.armor)[slot],
                            armor);
  }
  // A shield, two arm armors (one worn on part 1) and two accessories (one
  // worn), besides the fixture's spare weapon.
  void AddEquipment() {
    AddShield();
    AddArmor(0, 143, 5, 0);
    AddArmor(3, 144, 5, 1);
    AddArmor(5, 223, inv::kAccessoryLimb, 0);
    AddArmor(9, 224, inv::kAccessoryLimb, inv::kAccessoryLimb);
    const std::array<u_char, 4> armor{4, 1, 10, 6};
    std::ranges::copy(armor,
                      std::begin(Audio_Seq_unusedInventoryIndices.armor));
  }

  [[nodiscard]] std::int64_t Classify(const char* text) const {
    if (text == nullptr) {
      return kNullText;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(text);
    const auto menu = reinterpret_cast<std::uintptr_t>(Menu_Inventory_menuText);
    if (address >= menu && address < menu + sizeof Menu_Inventory_menuText) {
      return static_cast<std::int64_t>(address - menu);
    }
    const auto heap = reinterpret_cast<std::uintptr_t>(heap_bytes.data());
    if (address >= heap && address < heap + heap_bytes.size()) {
      return kHeapText + static_cast<std::int64_t>(address - heap);
    }
    const auto buffer = reinterpret_cast<std::uintptr_t>(description.data());
    if (address >= buffer && address < buffer + description.size()) {
      return kStringBufferText + static_cast<std::int64_t>(address - buffer);
    }
    return kForeignText;
  }

  // The generated bodies format their rows into uninitialized stack buffers
  // and the lists copy fixed-size name and description fields, so the bytes
  // after each text's terminator are stack leftovers there but zero in the
  // Live entries. No list draws them: blank them in the open lists' records
  // (the slot menus' choices and the top menu's rows) and in those of lists
  // already freed.
  void MaskTextTails(std::span<std::uint8_t> heap) {
    Track(Menu_Inventory_itemsText, Menu_Inventory_menuRowCount);
    Track(reinterpret_cast<const char*>(Menu_Framework_itemsText),
          Menu_Framework_subMenuRowCount);
    for (const std::size_t record : text_records) {
      if (record + sizeof(fw::ItemText) > heap.size()) {
        continue;
      }
      const auto bytes = heap.subspan(record, sizeof(fw::ItemText));
      BlankAfterTerminator(
          bytes.subspan(offsetof(fw::ItemText, name), fw::kItemNameBytes));
      BlankAfterTerminator(bytes.subspan(offsetof(fw::ItemText, description),
                                         fw::kItemDescriptionBytes));
    }
  }
  void Track(const char* records, int count) {
    const std::int64_t text = Classify(records);
    if (text < kHeapText || text >= kStringBufferText) {
      return;
    }
    const auto first = static_cast<std::size_t>(text - kHeapText);
    for (int row = 0; row < count; ++row) {
      const std::size_t record =
          first + static_cast<std::size_t>(row) * sizeof(fw::ItemText);
      if (std::ranges::find(text_records, record) == text_records.end()) {
        text_records.push_back(record);
      }
    }
  }
  static void BlankAfterTerminator(std::span<std::uint8_t> field) {
    constexpr std::uint8_t kTerminator = 0xE7;
    const auto end = std::ranges::find(field, kTerminator);
    if (end != field.end()) {
      std::fill(std::next(end), field.end(), std::uint8_t{0});
    }
  }

  std::vector<std::size_t> text_records;
};

template <typename T>
[[nodiscard]] std::vector<std::uint8_t> Bytes(const T& value) {
  const auto bytes = std::as_bytes(std::span(&value, 1));
  std::vector<std::uint8_t> copy(bytes.size());
  std::ranges::transform(bytes, copy.begin(), [](std::byte byte) {
    return static_cast<std::uint8_t>(byte);
  });
  return copy;
}

[[nodiscard]] std::uint64_t Fnv(std::span<const std::uint8_t> bytes,
                                std::uint64_t hash = 0xCBF29CE484222325ULL) {
  for (const std::uint8_t byte : bytes) {
    hash = (hash ^ byte) * 0x100000001B3ULL;
  }
  return hash;
}

// The state a menu frame leaves behind.
struct Snapshot {
  std::vector<std::uint8_t> inventory;
  std::vector<std::uint8_t> stats;
  std::vector<std::uint8_t> rows;
  std::vector<std::int64_t> sub_texts;
  std::vector<std::int64_t> scalars;
  std::vector<std::int16_t> panels;
  std::uint64_t heap = 0;
};

[[nodiscard]] Snapshot Take(MenusFixture& fixture) {
  Snapshot snapshot;
  snapshot.inventory = Bytes(Audio_Seq_unusedInventory);
  const auto indices = Bytes(Audio_Seq_unusedInventoryIndices);
  snapshot.inventory.insert(snapshot.inventory.end(), indices.begin(),
                            indices.end());
  snapshot.stats = Bytes(fixture.stats);
  constexpr std::size_t kRowBytes = offsetof(Menu_Framework_MenuRow, text) +
                                    sizeof(Menu_Framework_MenuRow{}.text);
  for (const Menu_Framework_MenuRow& row : fixture.rows) {
    const auto bytes = Bytes(row);
    snapshot.rows.insert(snapshot.rows.end(), bytes.begin(),
                         bytes.begin() + kRowBytes);
    snapshot.sub_texts.push_back(fixture.Classify(row.subText));
  }
  snapshot.scalars = {Menu_Inventory_ashleyRenderState,
                      Menu_Inventory_itemCategoryIconsEnabled,
                      Menu_Inventory_statusCommandState,
                      Menu_Inventory_selectedItemCategoryIconOnTop,
                      Menu_Inventory_equippedItem,
                      Menu_Inventory_equipListStep,
                      Menu_Inventory_selectedItemRow,
                      Menu_Inventory_menuRowCount,
                      Menu_Inventory_itemsFirstRow,
                      fixture.Classify(Menu_Inventory_itemsText),
                      Menu_Inventory_equipmentModelLoadStep,
                      Menu_Framework_enabledStatPages,
                      Menu_Framework_itemStatPage,
                      Menu_Framework_equipmentSubtype,
                      Menu_Framework_currentStatusViewItem,
                      fw::LiveHighlightedRow()};
  const auto append = [&snapshot](std::span<const std::int16_t> values) {
    snapshot.panels.insert(snapshot.panels.end(), values.begin(), values.end());
  };
  append(Menu_Framework_equipmentStats);
  append(Menu_Framework_strIntAgi);
  append(Menu_Framework_comparisonStatDeltas);
  append(Menu_Framework_comparisonStatTotals);
  append(Menu_Framework_comparisonAccessoryStats);
  const auto heap = std::span(
      reinterpret_cast<const std::uint8_t*>(fixture.heap_bytes.data()),
      fixture.heap_bytes.size());
  std::vector<std::uint8_t> canonical(heap.begin(), heap.end());
  fixture.MaskTextTails(canonical);
  snapshot.heap = Fnv(canonical);
  return snapshot;
}

struct MenuRun {
  std::vector<int> results;
  std::vector<Snapshot> snapshots;
};

// One menu entry driven through real frames and input.
class Script {
 public:
  Script(MenusFixture& fixture, Entry entry)
      : fixture_(fixture), entry_(entry) {}
  void Open(int argument) { run_.results.push_back(entry_(argument)); }
  void Press(std::uint32_t buttons) {
    run_.results.push_back(fixture_.Frame(entry_, buttons));
  }
  void Wait(int frames = dialogs::kDialogFrames) {
    for (int frame = 0; frame < frames; ++frame) {
      Press(0);
    }
  }
  void Snap() { run_.snapshots.push_back(Take(fixture_)); }
  [[nodiscard]] MenuRun Result() const { return run_; }

 private:
  MenusFixture& fixture_;
  Entry entry_;
  MenuRun run_;
};

void HashU64(std::uint64_t& hash, std::uint64_t value) {
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    hash = (hash ^ static_cast<std::uint8_t>(value & kByteMask)) * kFnvPrime;
    value >>= 8;
  }
}

void HashBytes(std::uint64_t& hash, const std::vector<std::uint8_t>& bytes) {
  HashU64(hash, bytes.size());
  hash = Fnv(bytes, hash);
}

template <typename T>
void HashNumbers(std::uint64_t& hash, const std::vector<T>& values) {
  using Unsigned = std::make_unsigned_t<T>;
  HashU64(hash, values.size());
  for (const T value : values) {
    Unsigned bits = static_cast<Unsigned>(value);
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
      hash = (hash ^ static_cast<std::uint8_t>(bits & kByteMask)) * kFnvPrime;
      bits >>= 8;
    }
  }
}

[[nodiscard]] std::uint64_t Digest(const MenuRun& run) {
  std::uint64_t hash = kFnvOffsetBasis;
  HashNumbers(hash, run.results);
  HashU64(hash, run.snapshots.size());
  for (const Snapshot& snapshot : run.snapshots) {
    HashBytes(hash, snapshot.inventory);
    HashBytes(hash, snapshot.stats);
    HashBytes(hash, snapshot.rows);
    HashNumbers(hash, snapshot.sub_texts);
    HashNumbers(hash, snapshot.scalars);
    HashNumbers(hash, snapshot.panels);
    HashU64(hash, snapshot.heap);
  }
  return hash;
}

template <typename Setup, typename Steps>
[[nodiscard]] MenuRun Play(Entry entry, Setup setup, Steps steps) {
  MenusFixture fixture;
  detail::ValueRestore<char> saved_categories{
      Menu_Inventory_itemCategoryIconsEnabled};
  detail::ValueRestore<char> saved_command{Menu_Inventory_statusCommandState};
  detail::ValueRestore<char> saved_equipped{Menu_Inventory_equippedItem};
  detail::ValueRestore<int> saved_selected{Menu_Inventory_selectedItemRow};
  detail::ValueRestore<char> saved_row_count{Menu_Inventory_menuRowCount};
  detail::ArrayRestore<char*, std::size(Battle_limbNames)> saved_limb_names{
      Battle_limbNames};
  Menu_Inventory_itemCategoryIconsEnabled = 0;
  Menu_Inventory_statusCommandState = 0;
  Menu_Inventory_equippedItem = 0;
  Menu_Inventory_selectedItemRow = 0;
  Menu_Inventory_menuRowCount = 0;
  std::ranges::fill(Battle_limbNames, nullptr);
  setup(fixture);
  Script script(fixture, entry);
  steps(script);
  return script.Result();
}

void CheckFrozenRun(const MenuRun& live, std::uint64_t expected) {
  CAPTURE(Digest(live), expected);
  CHECK(Digest(live) == expected);
}

[[nodiscard]] bool Returned(const MenuRun& run, int result) {
  return std::ranges::find(run.results, result) != run.results.end();
}

// Opens a slot menu, backs out, reopens it and chooses the second row.
void BackThenChoose(Script& script, int argument) {
  script.Open(argument);
  script.Wait();
  script.Snap();
  script.Press(VS::Input::Cross);
  script.Wait();
  script.Snap();
  script.Open(argument);
  script.Wait();
  script.Press(VS::Input::Down);
  script.Press(0);
  script.Snap();
  script.Press(VS::Input::Circle);
  script.Snap();
  script.Wait();
  script.Snap();
}

void AddEquipment(MenusFixture& fixture) { fixture.AddEquipment(); }

TEST_CASE("Live accessory menu matches the frozen MENU3 digest",
          "[equipment-menus-live]") {
  const auto steps = [](Script& script) { BackThenChoose(script, 1); };
  const MenuRun live = Play(inv::UpdateLiveAccessoryMenu, AddEquipment, steps);
  CheckFrozenRun(live, kAccessoryMenuReferenceDigest);
  CHECK(Returned(live, -1));
  CHECK(Returned(live, 2));
}

TEST_CASE("Live weapon menu matches the frozen MENU3 digest",
          "[equipment-menus-live]") {
  // A two-handed blade with a shield held: equipping drops the shield.
  const auto setup = [](MenusFixture& fixture) {
    fixture.AddEquipment();
    Audio_Seq_unusedInventory.shields[0].isEquipped = 1;
    Menu_Framework_weaponHands[0] = 1;
  };
  const auto steps = [](Script& script) { BackThenChoose(script, 1); };
  const MenuRun live = Play(inv::UpdateLiveWeaponMenu, setup, steps);
  CheckFrozenRun(live, kWeaponMenuReferenceDigest);
  CHECK(Returned(live, -1));
  CHECK(Returned(live, 2));
}

TEST_CASE("Live shield menu matches the frozen MENU3 digest",
          "[equipment-menus-live]") {
  // Ashley holds a two-handed blade: the shield frees its hand.
  const auto setup = [](MenusFixture& fixture) {
    fixture.AddEquipment();
    fixture.stats.weapon.blade.id = 1;
    fixture.stats.weapon.blade.category = 1;
    Menu_Framework_weaponHands[0] = 1;
  };
  const auto steps = [](Script& script) { BackThenChoose(script, 1); };
  const MenuRun live = Play(inv::UpdateLiveShieldMenu, setup, steps);
  CheckFrozenRun(live, kShieldMenuReferenceDigest);
  CHECK(Returned(live, -1));
  CHECK(Returned(live, 2));
}

TEST_CASE("Live armor menu matches the frozen MENU3 digest",
          "[equipment-menus-live]") {
  const auto steps = [](Script& script) { BackThenChoose(script, 1); };
  const MenuRun live = Play(inv::UpdateLiveArmorMenu, AddEquipment, steps);
  CheckFrozenRun(live, kArmorMenuReferenceDigest);
  CHECK(Returned(live, -1));
  CHECK(Returned(live, 2));
}

TEST_CASE("Live dispatch matches the frozen MENU3 digest",
          "[equipment-menus-live]") {
  const auto steps = [](Script& script) {
    for (const int row : {2, 8, 4, 1}) {
      script.Open(row);
      script.Wait();
      script.Snap();
      script.Press(VS::Input::Down);
      script.Press(0);
      script.Press(VS::Input::Circle);
      script.Wait();
      script.Snap();
    }
  };
  const MenuRun live =
      Play(inv::DispatchLiveEquipmentMenu, AddEquipment, steps);
  CheckFrozenRun(live, kDispatchMenuReferenceDigest);
  CHECK(Returned(live, 2));
}

TEST_CASE("Live equipment menu matches the frozen MENU3 digest",
          "[equipment-menus-live]") {
  const auto steps = [](Script& script) {
    script.Open(1);
    script.Wait();
    script.Snap();
    // The weapon row opens the weapon menu; the spare weapon is chosen.
    script.Press(VS::Input::Circle);
    script.Wait();
    script.Snap();
    script.Press(VS::Input::Down);
    script.Press(0);
    script.Press(VS::Input::Circle);
    script.Wait();
    script.Snap();
    // The first armor row opens part 1's list, which is backed out of.
    script.Press(VS::Input::Down);
    script.Press(0);
    script.Press(VS::Input::Down);
    script.Press(0);
    script.Press(VS::Input::Circle);
    script.Wait();
    script.Snap();
    script.Press(VS::Input::Cross);
    script.Wait();
    script.Snap();
    script.Press(VS::Input::Square);
    script.Press(VS::Input::Cross);
    script.Wait();
    script.Snap();
  };
  const MenuRun live = Play(inv::UpdateLiveEquipmentMenu, AddEquipment, steps);
  CheckFrozenRun(live, kEquipmentMenuReferenceDigest);
  CHECK(Returned(live, -1));
}

TEST_CASE("Live handler table binds the four Live slot menus",
          "[equipment-menus-live]") {
  const inv::EquipmentMenuHandlers& handlers = inv::LiveEquipmentMenuHandlers();
  CHECK(handlers[0] == &inv::UpdateLiveArmorMenu);
  CHECK(handlers[1] == &inv::UpdateLiveWeaponMenu);
  CHECK(handlers[2] == &inv::UpdateLiveShieldMenu);
  CHECK(handlers[3] == &inv::UpdateLiveAccessoryMenu);
}

TEST_CASE("Live equipment menus refuse slots, parts and rows outside",
          "[equipment-menus-live]") {
  MenusFixture fixture;
  fixture.AddEquipment();
  const std::uint64_t before = inv::LiveEquipmentMenusRefusalCount();
  const char pages = Menu_Framework_enabledStatPages;
  Audio_Seq_unusedInventoryIndices.armor[0] = 17;
  CHECK(inv::UpdateLiveArmorMenu(1) == 0);
  CHECK(inv::UpdateLiveAccessoryMenu(1) == 0);
  CHECK(inv::UpdateLiveArmorMenu(8) == 0);
  CHECK(inv::DispatchLiveEquipmentMenu(9) == 0);
  CHECK(inv::DispatchLiveEquipmentMenu(-1) == 0);
  CHECK(inv::LiveEquipmentMenusRefusalCount() == before + 5);
  CHECK(Menu_Framework_enabledStatPages == pages);
}

TEST_CASE("Live equipment menu reset forgets the open slot menu",
          "[equipment-menus-live]") {
  MenusFixture fixture;
  fixture.AddEquipment();
  CHECK(inv::DispatchLiveEquipmentMenu(2) == 0);
  std::uint64_t before = inv::LiveEquipmentMenusRefusalCount();
  CHECK(inv::DispatchLiveEquipmentMenu(0) == 0);
  CHECK(inv::LiveEquipmentMenusRefusalCount() == before);
  inv::ResetEquipmentMenusLive();
  before = inv::LiveEquipmentMenusRefusalCount();
  // The reset menu is the armor menu with no body part chosen yet.
  CHECK(inv::DispatchLiveEquipmentMenu(0) == 0);
  CHECK(inv::LiveEquipmentMenusRefusalCount() == before + 1);
}
}  // namespace
