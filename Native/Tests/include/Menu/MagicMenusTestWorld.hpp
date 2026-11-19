#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Menu/MagicMenus.hpp"

namespace VS::Menu::Magic::test {
class Packets final : public Render::PacketQueue {
 public:
  std::vector<std::vector<std::uint32_t>> records;
  std::size_t capacity = 256;
  std::uint32_t ChainLink() const override {
    return static_cast<std::uint32_t>(records.size());
  }
  bool Emit(std::span<const std::uint32_t> packet) override {
    if (records.size() >= capacity) {
      return false;
    }
    records.emplace_back(packet.begin(), packet.end());
    return true;
  }
};

class World final : public MenuHost {
 public:
  World() {
    for (std::size_t i = 0; i < strings.size(); ++i) {
      strings[i] = static_cast<std::uint16_t>(100 + i);
    }
    for (std::size_t i = 0; i < ids.size(); ++i) {
      ids[i] = static_cast<std::uint8_t>(140 + i);
    }
    unlocked.fill(true);
    widths.fill(0x55);
    brightness.fill(128);
  }

  std::array<std::uint16_t, 128> strings{};
  std::size_t string_count = strings.size();
  std::array<std::uint8_t, 18> ids{};
  std::size_t id_count = ids.size();
  std::array<bool, 256> unlocked{};
  std::array<std::uint32_t, 256> restrictions{};
  std::uint32_t action_restrictions = 0;
  std::uint8_t shortcut = 0;
  bool idle = true;
  int selected_row = -1;
  int highlighted_row = 0;
  std::uint8_t cursor_setting = 0;
  RankCursor cursor{0, 2, 0};
  RankInput input;
  bool display_cost = true;
  std::array<bool, kSchoolCount> schools{true, true, true, true};
  std::uint8_t teleport_availability = 0;
  std::array<std::uint8_t, 48> save_points{};
  std::size_t save_point_count = save_points.size();
  std::array<bool, 3> at_save_point{true, true, true};
  std::size_t save_point_query = 0;
  std::int32_t child_result = 0;
  std::int32_t teleport_result = 0;
  std::int32_t action = 0;
  std::int16_t selection_kind = 0;
  std::uint8_t ability_type = 0;
  std::uint8_t current_menu = 0;
  std::uint8_t return_menu = 0;
  bool refuse_action = false;
  bool refuse_flags = false;
  bool refuse_rows = false;
  bool refuse_title = false;
  bool refuse_cost = false;
  bool refuse_header = false;
  bool refuse_child = false;
  bool refuse_drawing = false;
  std::uint8_t* caller_state = nullptr;
  std::function<void(int)> callback;
  std::vector<std::vector<std::int64_t>> calls;
  std::vector<std::string> refusals;
  std::vector<SpellRow> rows;
  int style = 0;
  int cursor_at_create = 0;
  std::array<std::uint8_t, 92> widths{};
  std::array<std::uint8_t, 264> trims{};
  std::array<std::uint8_t, 60> glyphs{};
  std::array<std::uint8_t, 16> brightness{};
  std::size_t width_count = widths.size();
  Packets text;
  Packets icons;

  void Record(std::initializer_list<std::int64_t> values) {
    calls.emplace_back(values);
    if (callback && values.size() != 0) {
      callback(static_cast<int>(*values.begin()));
    }
  }
  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  std::span<const std::uint16_t> MagicStrings() override {
    return std::span(strings).first(string_count);
  }
  std::span<const std::uint8_t> SpellIds(School) override {
    return std::span(ids).first(id_count);
  }
  std::optional<bool> SpellUnlocked(int id) override {
    if (refuse_action || id < 0 ||
        static_cast<std::size_t>(id) >= unlocked.size()) {
      return std::nullopt;
    }
    return unlocked[static_cast<std::size_t>(id)];
  }
  std::optional<std::uint32_t> SpellRestrictions(int id) override {
    Record({2, id});
    if (refuse_flags || id < 0 ||
        static_cast<std::size_t>(id) >= restrictions.size()) {
      return std::nullopt;
    }
    return restrictions[static_cast<std::size_t>(id)];
  }
  std::uint32_t ActionRestrictions() override { return action_restrictions; }
  std::uint8_t ShortcutInvoked() override { return shortcut; }
  bool RowsIdle() override {
    Record({0});
    return idle;
  }
  int SelectedRow() override {
    Record({4});
    return selected_row;
  }
  int HighlightedRow() override {
    Record({5});
    return highlighted_row;
  }
  std::uint8_t CursorMemory() override { return cursor_setting; }
  void SetCursorMemory(std::uint8_t value) override { cursor_setting = value; }
  bool CreateRows(int row_style, std::span<const SpellRow> source) override {
    if (refuse_rows) {
      Refuse("rows refused");
      return false;
    }
    rows.assign(source.begin(), source.end());
    style = row_style;
    cursor_at_create = cursor_setting;
    calls.push_back(
        {3, style, cursor_setting, static_cast<std::int64_t>(rows.size())});
    for (const auto& row : rows) {
      auto& call = calls.back();
      call.insert(call.end(), {static_cast<int>(row.name.table), row.name.index,
                               static_cast<int>(row.description.table),
                               row.description.index, row.type});
    }
    if (callback) {
      callback(3);
    }
    return true;
  }
  void FlyOutRows(int row, int offset) override { Record({1, row, offset}); }
  void FlyOutRow(int row) override { Record({11, row}); }
  void DisplayCost(bool enabled) override { display_cost = enabled; }
  bool SpellCost(int id) override {
    Record({6, id});
    if (refuse_cost) {
      Refuse("cost refused");
    }
    return !refuse_cost;
  }
  void ClearRows(int except) override { Record({7, except}); }
  void DismissInformation() override { Record({8}); }
  void InitializeInformation() override { Record({12}); }
  void SetCommand(Framework::MenuCommand command) override {
    Record({9, std::to_underlying(command)});
  }
  RankCursor ReadRankCursor() override { return cursor; }
  void SetRankCount(std::uint8_t count) override { cursor.count = count; }
  void SetRankSelection(std::uint8_t selected) override {
    cursor.selected = selected;
  }
  RankInput ReadRankInput() override { return input; }
  void PlaySound(MenuSound sound) override {
    Record({10, static_cast<int>(sound)});
  }
  std::optional<RankTarget> RankDrawing() override {
    if (refuse_drawing) {
      return std::nullopt;
    }
    return RankTarget{text,
                      icons,
                      {.widths = std::span(widths).first(width_count),
                       .left_trim = trims,
                       .indices = glyphs},
                      brightness};
  }
  bool SchoolUnlocked(School school) override {
    Record({13, static_cast<int>(school)});
    return schools[static_cast<std::size_t>(school)];
  }
  std::uint8_t TeleportAvailability() override { return teleport_availability; }
  std::span<const std::uint8_t> SavePointStates() override {
    return std::span(save_points).first(save_point_count);
  }
  bool AtSavePoint() override {
    Record({14});
    const auto index = std::min(save_point_query++, at_save_point.size() - 1);
    return at_save_point[index];
  }
  bool SetSchoolTitle(School school, TextRef label, int width) override {
    Record({15, static_cast<int>(school), static_cast<int>(label.table),
            label.index, width});
    if (refuse_title) {
      Refuse("title refused");
    }
    return !refuse_title;
  }
  bool CreateHeader() override {
    Record({16});
    if (refuse_header) {
      Refuse("header refused");
    }
    return !refuse_header;
  }
  std::optional<int> RunSchool(School school, unsigned int init) override {
    Record({17, static_cast<int>(school), init,
            caller_state != nullptr ? *caller_state : 0});
    if (refuse_child) {
      Refuse("child refused");
      return std::nullopt;
    }
    return child_result;
  }
  int RunTeleport(int init) override {
    Record({18, init, caller_state != nullptr ? *caller_state : 0});
    return teleport_result;
  }
  void SetActionSelection(std::int32_t value) override { action = value; }
  void SetAbilityType(std::uint8_t value) override { ability_type = value; }
  void SetSelectionKind(std::int16_t value) override { selection_kind = value; }
  std::uint8_t CurrentMenu() override { return current_menu; }
  void SetCurrentMenu(std::uint8_t value) override { current_menu = value; }
  void SetReturnMenu(std::uint8_t value) override { return_menu = value; }
};
}  // namespace VS::Menu::Magic::test
