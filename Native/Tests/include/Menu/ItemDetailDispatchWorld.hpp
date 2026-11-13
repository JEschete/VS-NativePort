#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/Inventory/ItemDetailDispatch.hpp"

namespace item_detail_tests {
namespace inventory = VS::Menu::Inventory;
namespace fw = VS::Menu::Framework;

struct ItemWorld final : inventory::ItemDetailWorld {
  std::int32_t ready = 1;
  std::uint32_t pressed = 0;
  inventory::DetailPage next_page{0};
  std::array<std::uint8_t, 48> direct{};
  std::array<std::uint8_t, 48> bound{};
  std::array<std::uint8_t, 48> armor_categories{};
  std::array<char, 2> name{'n', '\0'};
  std::array<char, 2> description{'d', '\0'};
  std::uint32_t row_type = 0xE1234567;
  inventory::BattleReturnIntent return_intent =
      inventory::BattleReturnIntent::kMenu;
  std::vector<std::string> calls;
  std::vector<int> references;
  std::vector<int> selected_pages;
  std::function<void(std::string_view)> callback;

  ItemWorld() {
    direct.fill(1);
    bound.fill(1);
    armor_categories.fill(1);
  }
  void Call(std::string_view name_of_call) {
    calls.emplace_back(name_of_call);
    if (callback) {
      callback(name_of_call);
    }
  }
  std::int32_t RowsReady() override {
    Call("ready");
    return ready;
  }
  std::uint32_t PressedButtons() override { return pressed; }
  void BeginTransition(inventory::DetailRowId row) override {
    references.push_back(row.index);
    Call("begin");
  }
  void InitializeLayout(inventory::DetailRowId row,
                        inventory::DetailStatPages pages) override {
    references.push_back(row.index);
    references.push_back(static_cast<int>(pages));
    Call("layout");
  }
  inventory::InventorySlot CurrentItem(inventory::DetailPage page) override {
    selected_pages.push_back(page.index);
    return {direct.at(static_cast<std::size_t>(page.index))};
  }
  void SetStatistics(inventory::InventorySlot slot) override {
    references.push_back(slot.one_based);
    Call("stats");
  }
  bool ArmorShowsDp(inventory::InventorySlot slot) override {
    references.push_back(slot.one_based);
    Call("armor-category");
    return armor_categories.at(slot.one_based - 1) != 7;
  }
  void OpenDpBar() override { Call("dp-only"); }
  void Exit(bool clear_dp_pp) override {
    references.push_back(clear_dp_pp ? 1 : 0);
    Call("exit");
  }
  void SetBattleReturnIntent(inventory::BattleReturnIntent intent) override {
    return_intent = intent;
    Call("return-intent");
  }
  inventory::DetailPage PollPaging(inventory::DetailPage) override {
    Call("paging");
    return next_page;
  }
  inventory::InventorySlot SelectItem(inventory::DetailPage page) override {
    selected_pages.push_back(page.index);
    Call("select");
    return {bound.at(static_cast<std::size_t>(page.index))};
  }
  inventory::DetailRowView FormatItem(inventory::InventorySlot slot) override {
    references.push_back(slot.one_based);
    Call("format");
    return {.name = name, .description = description, .row_type = row_type};
  }
  void ChangePage(inventory::DetailRowId row,
                  const inventory::DetailRowView& text,
                  inventory::DetailPage page) override {
    references.push_back(row.index);
    references.push_back(page.index);
    references.push_back(static_cast<unsigned char>(text.name.front()));
    references.push_back(static_cast<unsigned char>(text.description.front()));
    Call("change");
  }
};

struct IconWorld final : inventory::CategoryIconWorld {
  std::uint8_t category = 1;
  fw::MenuRow header{};
  std::vector<inventory::CategoryIcon> icons;
  int header_reads = 0;
  int tab_draws = 0;
  std::function<void()> tabs_callback;
  std::uint8_t CurrentCategory() override { return category; }
  void DrawListTabNavigation() override {
    ++tab_draws;
    if (tabs_callback) {
      tabs_callback();
    }
  }
  fw::MenuRow& CategoryHeader() override {
    ++header_reads;
    return header;
  }
  void DrawIcon(inventory::CategoryIcon icon) override {
    icons.push_back(icon);
  }
};

struct DispatchWorld final : inventory::ItemDetailDispatcherWorld {
  inventory::DetailHeaderPlacement placement =
      inventory::DetailHeaderPlacement::kList;
  inventory::BattleReturnIntent intent = inventory::BattleReturnIntent::kMenu;
  inventory::PreviewLayout preview = inventory::PreviewLayout::kDetails;
  inventory::DetailCommand command = inventory::DetailCommand::kMenu;
  VS::Items::ItemCategory dispatched = VS::Items::ItemCategory::kWeapon;
  std::optional<inventory::DetailSelection> selection;
  fw::MenuRow header{};
  std::uint8_t categories = 0;
  std::uint8_t commands = 0;
  std::uint8_t selected_element = 9;
  bool bright = true;
  std::int32_t child_result = 0;
  std::int32_t ready = 1;
  std::vector<std::string> calls;
  std::function<void(std::string_view)> callback;

  void Call(std::string_view name_of_call) {
    calls.emplace_back(name_of_call);
    if (callback) {
      callback(name_of_call);
    }
  }
  void SetHeaderPlacement(inventory::DetailHeaderPlacement value) override {
    placement = value;
    Call("header-placement");
  }
  void SetBattleReturnIntent(inventory::BattleReturnIntent value) override {
    intent = value;
    Call("return-intent");
  }
  void ResetDetailCursor() override {
    selected_element = 9;
    Call("reset-cursor");
  }
  fw::MenuRow& CategoryHeader() override {
    Call("header");
    return header;
  }
  void SetCommand(inventory::DetailCommand value) override {
    command = value;
    Call("command");
  }
  std::int32_t RunController(
      VS::Items::ItemCategory category,
      std::optional<inventory::DetailSelection> item) override {
    dispatched = category;
    selection = item;
    Call("controller");
    return child_result;
  }
  bool ReturnsToBattle() override {
    Call("battle-check");
    return intent == inventory::BattleReturnIntent::kBattle;
  }
  void SetCategoryVisibility(std::uint8_t value) override {
    categories = value;
    Call("categories");
  }
  void SetCommandVisibility(std::uint8_t value) override {
    commands = value;
    Call("commands");
  }
  void SetPreviewLayout(inventory::PreviewLayout value) override {
    preview = value;
    Call("preview");
  }
  std::int32_t RowsReady() override {
    Call("ready");
    return ready;
  }
  std::uint8_t SelectedDetailElement() override { return selected_element; }
  void SetCursorBright(bool value) override {
    bright = value;
    Call("cursor");
  }
  void DrawDetailTabNavigation() override { Call("tabs"); }
  void DrawItemDetails() override { Call("details"); }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::runtime_error(std::string(reason));
  }
};
}  // namespace item_detail_tests
