#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/Inventory/DetailsPolicies.hpp"

namespace inventory_detail_tests {
namespace inventory = VS::Menu::Inventory;

enum class Event {
  kClear,
  kRow,
  kPreview,
  kCategories,
  kCommand,
  kContinue,
  kPages,
  kPrimary,
  kLeave,
  kInvalid,
  kDismiss,
  kHeader,
  kReturn,
  kBars,
};

struct Call {
  Event event;
  int argument = 0;
  friend bool operator==(Call, Call) = default;
};

class World final : public inventory::DetailsWorld {
 public:
  std::uint32_t held = 0;
  std::uint32_t repeat = 0;
  std::uint32_t pressed = 0;
  int held_reads = 0;
  int repeat_reads = 0;
  int pressed_reads = 0;
  int count_reads = 0;
  std::int32_t count = 8;
  std::array<VS::Menu::Framework::MenuRow, VS::Menu::Framework::kRowCount>
      rows{};
  decltype(rows) replacement_rows{};
  bool replacement_owner = false;
  std::array<std::uint8_t, 8> order{1, 2, 3, 4, 5, 6, 7, 8};
  std::span<const std::uint8_t> displayed = order;
  std::array<inventory::CursorMemory, 8> cursors{};
  std::size_t category = 0;
  inventory::PanelVisibility panels{5, 6, 7};
  inventory::PreviewLayout preview = inventory::PreviewLayout::kItems;
  inventory::CategoryHeaderPlacement header =
      inventory::CategoryHeaderPlacement::kReturning;
  inventory::BattleReturnIntent return_intent =
      inventory::BattleReturnIntent::kMenu;
  std::vector<Call> calls;
  std::function<void()> on_count;
  std::function<void()> on_clear;
  std::function<void()> on_remember;
  std::function<void()> on_primary;
  std::function<void(int)> on_row;

  std::uint32_t HeldButtons() override {
    ++held_reads;
    return held;
  }
  std::uint32_t RepeatButtons() override {
    ++repeat_reads;
    return repeat;
  }
  std::uint32_t PressedButtons() override {
    ++pressed_reads;
    return pressed;
  }
  std::int32_t ItemCount() override {
    ++count_reads;
    if (on_count) on_count();
    return count;
  }
  void RememberPage(inventory::CursorMemory memory) override {
    cursors.at(category) = memory;
    if (on_remember) on_remember();
  }
  std::span<const std::uint8_t> DisplayOrder() override { return displayed; }
  [[noreturn]] void Refuse(std::string_view reason) override {
    throw std::out_of_range(std::string(reason));
  }
  void ClearRows() override {
    calls.push_back({Event::kClear});
    if (on_clear) on_clear();
  }
  VS::Menu::Framework::MenuRow& Row(inventory::DetailRowId row) override {
    calls.push_back({Event::kRow, row.index});
    if (on_row) on_row(row.index);
    return (replacement_owner ? replacement_rows : rows)
        .at(static_cast<std::size_t>(row.index));
  }
  void SetPreviewLayout(inventory::PreviewLayout layout) override {
    preview = layout;
    calls.push_back({Event::kPreview, static_cast<int>(layout)});
  }
  void SetCategoryVisibility(std::uint8_t visibility) override {
    panels.category_icons = visibility;
    calls.push_back({Event::kCategories, visibility});
  }
  void SetCommandVisibility(std::uint8_t visibility) override {
    panels.status_command = visibility;
    calls.push_back({Event::kCommand, visibility});
  }
  void ContinueTransition(inventory::DetailRowId row) override {
    calls.push_back({Event::kContinue, row.index});
  }
  void ShowStatPages(std::int32_t pages) override {
    calls.push_back({Event::kPages, pages});
  }
  void MovePrimaryStats(inventory::StatPanelMotion motion) override {
    calls.push_back({Event::kPrimary, static_cast<int>(motion)});
    if (on_primary) on_primary();
  }
  void PlayLeaveSound() override { calls.push_back({Event::kLeave}); }
  void PlayInvalidSound() override { calls.push_back({Event::kInvalid}); }
  void DismissCommandRow() override { calls.push_back({Event::kDismiss}); }
  void SetHeaderPlacement(
      inventory::CategoryHeaderPlacement placement) override {
    header = placement;
    calls.push_back({Event::kHeader, static_cast<int>(placement)});
  }
  void SetBattleReturnIntent(inventory::BattleReturnIntent intent) override {
    return_intent = intent;
    calls.push_back({Event::kReturn, static_cast<int>(intent)});
  }
  void HideDpPpBars() override { calls.push_back({Event::kBars}); }
};
}  // namespace inventory_detail_tests
