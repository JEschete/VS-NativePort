#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Menu/BattleAbilitiesTerminal.hpp"

namespace VS::Test {  // NOLINT(readability-identifier-naming)
namespace ability_terminal {
namespace abilities = Menu::BattleAbilities;
using Call = std::vector<std::int64_t>;

struct RowRecord {
  int state = 1;
  std::uint8_t gradient = 0;
  std::uint8_t scroll = 0;
  std::uint8_t selected = 0;
  int target = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int text_offset = 0;
  friend bool operator==(const RowRecord&, const RowRecord&) = default;
};

struct AssignmentWorld : abilities::AssignmentHost {
  std::vector<std::string> refusals;
  std::vector<Call> calls;
  std::array<std::uint8_t, 16> chain{};
  std::array<std::uint8_t, 16> defense{};
  std::array<std::uint8_t, 4> chain_mapping{};
  std::array<std::uint8_t, 4> defense_mapping{};
  std::size_t mapping_count = 3;
  std::array<std::uint16_t,
             abilities::kAbilityCapacity * abilities::kAbilityRowWords>
      text{};
  std::size_t text_size = text.size();
  std::array<RowRecord, 8> items{};
  std::array<std::uint8_t, 64> memories{};
  abilities::AssignmentInput input{};
  bool has_text = true;
  bool ready = true;
  int missing_action = -1;
  int missing_row = -1;
  bool fail_show = false;
  int display_cost = 0;
  int risk_action = -1;
  int free_count = 0;
  std::uint32_t cursor_result = 0;
  std::vector<std::uint16_t> description_copy;
  std::function<void(abilities::AssignmentSound)> on_sound;

  AssignmentWorld() {
    for (std::size_t i = 0; i < chain.size(); ++i) {
      chain[i] = static_cast<std::uint8_t>(24 + i);
      defense[i] = static_cast<std::uint8_t>(40 + i);
    }
    for (std::size_t i = 0; i < text.size(); ++i) {
      text[i] = static_cast<std::uint16_t>(i);
    }
  }
  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  std::optional<std::uint8_t> LearnedAction(abilities::AbilityMenuType type,
                                            int index) const override {
    if (index == missing_action || index < 0 ||
        index >= static_cast<int>(chain.size())) {
      return std::nullopt;
    }
    return (type == abilities::AbilityMenuType::kChain ? chain : defense)
        .at(static_cast<std::size_t>(index));
  }
  std::span<std::uint8_t> Mappings(abilities::AbilityMenuType type) override {
    return std::span(type == abilities::AbilityMenuType::kChain
                         ? chain_mapping
                         : defense_mapping)
        .first(mapping_count);
  }
  void PlaySound(abilities::AssignmentSound sound) override {
    calls.push_back({1 + static_cast<int>(sound)});
    if (on_sound) on_sound(sound);
  }
  std::span<const std::uint16_t> AbilityText() const override {
    return has_text ? std::span(text).first(text_size)
                    : std::span<const std::uint16_t>{};
  }
  bool ShowRow(const abilities::AssignmentRow& row,
               std::span<const std::uint16_t> name) override {
    if (fail_show) return false;
    const auto offset = static_cast<int>(name.data() - text.data());
    calls.push_back({4, row.row, row.x, row.y, row.width, 0, offset});
    items.at(static_cast<std::size_t>(row.row - 20)) = {
        .state = row.entering ? 2 : 1,
        .gradient = row.gradient.value_or(0),
        .scroll = static_cast<std::uint8_t>(row.scroll),
        .selected = 0,
        .target = row.target_x,
        .x = row.x,
        .y = row.y,
        .width = row.width,
        .text_offset = offset};
    return true;
  }
  std::optional<std::uint8_t> RowGradient(int row) override {
    calls.push_back({5, row});
    if (row == missing_row || row < 20 || row >= 28) return std::nullopt;
    return items.at(static_cast<std::size_t>(row - 20)).gradient;
  }
  bool SelectRow(int row, bool selected) override {
    const auto gradient = RowGradient(row);
    if (!gradient) return false;
    items.at(static_cast<std::size_t>(row - 20)).selected = selected ? 1 : 0;
    return true;
  }
  void ShowDescription(std::span<const std::uint16_t> value) override {
    calls.push_back({6, value.data() - text.data()});
    description_copy.assign(value.begin(), value.end());
  }
  bool RowsIdle() override {
    calls.push_back({7});
    return ready;
  }
  void HideActionCost() override { display_cost = 0; }
  void DrawButton(abilities::AssignmentButton button, int x, int y) override {
    const std::array<int, 3> icons{2, 0, 1};
    calls.push_back({8, icons.at(static_cast<std::size_t>(button)), x, y});
  }
  abilities::AssignmentInput Input() const override { return input; }
  void ReleaseAbilityText() override {
    calls.push_back({9});
    has_text = false;
    ++free_count;
  }
  void RememberCursor(abilities::AbilityMenuType type,
                      abilities::AbilityCursor cursor) override {
    const auto offset = static_cast<std::size_t>(type) * 2;
    memories.at(offset) = static_cast<std::uint8_t>(cursor.highlighted);
    memories.at(offset + 1) = static_cast<std::uint8_t>(cursor.first);
  }
  int DrawCursor(int animation, int position) override {
    calls.push_back({10, static_cast<std::uint32_t>(animation), position});
    return std::bit_cast<int>(cursor_result);
  }
  void SetRiskCost(std::uint8_t action) override {
    calls.push_back({11, action});
    risk_action = action;
    display_cost = 1;
  }
};

struct ListWorld : abilities::AbilityListHost {
  std::vector<std::string> refusals;
  std::vector<Call> calls;
  std::array<std::uint8_t, 16> chain{};
  std::array<std::uint8_t, 16> defense{};
  std::array<std::uint8_t, 16> learned_chain{};
  std::array<std::uint8_t, 16> learned_defense{};
  std::array<bool, 256> unlocked{};
  std::size_t order_size = 16;
  bool shortcut = false;
  bool ready = true;
  int assignment_result = 0;
  std::vector<abilities::LearnedAbility> listed;
  abilities::AbilityKind listed_kind{};
  std::function<void()> on_create;
  std::function<void()> on_update;

  ListWorld() {
    for (std::size_t i = 0; i < chain.size(); ++i) {
      chain[i] = static_cast<std::uint8_t>(24 + i);
      defense[i] = static_cast<std::uint8_t>(40 + i);
    }
    learned_chain.fill(0xCD);
    learned_defense.fill(0xDE);
  }
  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  void FlyOutCategory(abilities::AbilityKind kind) override {
    calls.push_back({20, static_cast<int>(kind), 1});
  }
  bool ShortcutInvoked() const override { return shortcut; }
  bool RowsIdle() override {
    calls.push_back({7});
    return ready;
  }
  std::span<const std::uint8_t> ActionOrder(
      abilities::AbilityKind kind) const override {
    return std::span(kind == abilities::AbilityKind::kChain ? chain : defense)
        .first(order_size);
  }
  bool ActionUnlocked(std::uint8_t action) const override {
    return unlocked.at(action);
  }
  void StoreLearnedAction(abilities::AbilityKind kind, int index,
                          std::uint8_t action) override {
    (kind == abilities::AbilityKind::kChain ? learned_chain : learned_defense)
        .at(static_cast<std::size_t>(index)) = action;
  }
  void CreateRows(abilities::AbilityKind kind,
                  std::span<const abilities::LearnedAbility> rows) override {
    listed_kind = kind;
    listed.assign(rows.begin(), rows.end());
    calls.push_back({21, static_cast<std::int64_t>(rows.size()),
                     kind == abilities::AbilityKind::kChain ? 24 : 25});
    if (on_create) on_create();
  }
  int UpdateAssignments() override {
    calls.push_back({22});
    if (on_update) on_update();
    return assignment_result;
  }
  void ClearAbilityRows() override { calls.push_back({23, 2}); }
};

struct PointsWorld : abilities::UnlockPointsHost {
  std::vector<std::string> refusals;
  int count = 0;
  std::uint16_t kills = 0;
  std::array<std::uint16_t, 22> requirements{};
  bool missing_requirement = false;
  std::vector<std::string> texts;
  std::vector<std::uint32_t> positions;

  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  int UnlockCount() const override { return count; }
  std::uint16_t TotalKills() const override { return kills; }
  std::optional<std::uint16_t> Requirement(int index) const override {
    if (missing_requirement || index < 0 ||
        index >= static_cast<int>(requirements.size())) {
      return std::nullopt;
    }
    return requirements.at(static_cast<std::size_t>(index));
  }
  void DrawCompactText(std::string_view text, std::uint32_t position) override {
    texts.emplace_back(text);
    positions.push_back(position);
  }
};

struct ScreenWorld : abilities::AbilityScreenHost {
  std::vector<std::string> refusals;
  std::vector<Call> calls;
  std::array<bool, 2> readiness{true, true};
  std::size_t ready_index = 0;
  int shortcut = 0;
  bool header_available = true;
  bool chain_available = true;
  bool defense_available = true;
  int selected = -1;
  int list_result = 0;
  int current_menu = 81;
  std::array<std::uint8_t, 36> offsets{};
  std::optional<abilities::AbilityShortcutRow> header;
  std::function<void(abilities::AbilityKind, bool)> on_list;
  std::function<void()> on_points;

  ScreenWorld() {
    for (std::size_t i = 0; i < offsets.size(); ++i) {
      offsets[i] = static_cast<std::uint8_t>(i * 13);
    }
  }
  void Refuse(std::string_view reason) override {
    refusals.emplace_back(reason);
  }
  bool RowsIdle() override {
    calls.push_back({7});
    const bool ready =
        readiness.at(std::min(ready_index, readiness.size() - 1));
    ++ready_index;
    return ready;
  }
  void InitializeInformationBox() override { calls.push_back({30}); }
  int Shortcut() const override { return shortcut; }
  bool ShowShortcutCategory(const abilities::AbilityShortcutRow& row) override {
    header = row;
    calls.push_back({4, row.row, row.x, row.y, row.width, row.background_width,
                     static_cast<int>(row.kind)});
    return header_available;
  }
  bool HasLearnedAbility(abilities::AbilityKind kind) override {
    calls.push_back({31, static_cast<int>(kind)});
    return kind == abilities::AbilityKind::kChain ? chain_available
                                                  : defense_available;
  }
  void ShowCategories(bool chain, bool defense, bool force_cursor) override {
    calls.push_back({32, chain ? 1 : 0, defense ? 1 : 0, force_cursor ? 1 : 0});
  }
  int SelectedCategory() override {
    calls.push_back({33});
    return selected;
  }
  int UpdateAbilityList(abilities::AbilityKind kind, bool initialize) override {
    calls.push_back({34, static_cast<int>(kind), initialize ? 1 : 0});
    if (on_list) on_list(kind, initialize);
    return list_result;
  }
  void ClearMenu() override { calls.push_back({23, 40}); }
  void DismissInformationBox() override { calls.push_back({35}); }
  void ClearCommand() override { calls.push_back({36, 0}); }
  void ReturnToBattleMenu() override { current_menu = 3; }
  std::optional<std::uint8_t> AnimationOffset(
      std::uint8_t step) const override {
    if (step >= offsets.size()) return std::nullopt;
    return offsets.at(step);
  }
  void DrawNextUnlockPoints(int offset) override {
    calls.push_back({37, offset});
    if (on_points) on_points();
  }
};
}  // namespace ability_terminal
}  // namespace VS::Test
