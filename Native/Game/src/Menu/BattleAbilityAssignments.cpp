#include <algorithm>

#include "Menu/BattleAbilitiesTerminal.hpp"

namespace VS::Menu::BattleAbilities {
namespace {
constexpr int kFirstAbilityRow = 20;
constexpr int kRowHeight = 16;
constexpr int kFirstRowY = 50;
constexpr int kRowWidth = 126;
constexpr int kRowTargetX = 194;
constexpr int kRowEntryX = 320;
constexpr int kButtonX = 199;
constexpr int kFirstButtonY = 48;
constexpr int kCursorRowOffset = 2;
constexpr std::uint32_t kWaitReady = 16;
constexpr std::uint32_t kAssignments = 17;
constexpr std::uint32_t kEntryPhaseLimit = 10;

bool ValidType(AbilityMenuType type) {
  return type == AbilityMenuType::kChain || type == AbilityMenuType::kDefense;
}

bool ValidateRows(const AbilityRowsState& rows, AssignmentHost& host) {
  if (!ValidType(rows.type) || rows.count <= 0 ||
      rows.count > static_cast<int>(kAbilityCapacity)) {
    host.Refuse("invalid ability assignment type or count");
    return false;
  }
  const int visible = std::min(rows.count, kVisibleAbilityRows);
  const int overflow = rows.count - visible;
  if (rows.cursor.first < 0 || rows.cursor.first > overflow ||
      rows.cursor.highlighted < 0 || rows.cursor.highlighted >= visible ||
      (rows.cursor.first != 0 && rows.cursor.highlighted == 0) ||
      (rows.cursor.first < overflow &&
       rows.cursor.highlighted == kVisibleAbilityRows - 1)) {
    host.Refuse("ability assignment cursor outside selectable rows");
    return false;
  }
  if (host.AbilityText().size() <
      static_cast<std::size_t>(rows.count) * kAbilityRowWords) {
    host.Refuse("ability assignment text has no complete row owner");
    return false;
  }
  return true;
}

ScrollMarker RowScroll(const AbilityRowsState& rows, int row) {
  if (row == 0 && rows.cursor.first != 0) return ScrollMarker::kAbove;
  if (row == kVisibleAbilityRows - 1 &&
      rows.cursor.first + kVisibleAbilityRows < rows.count) {
    return ScrollMarker::kBelow;
  }
  return ScrollMarker::kNone;
}

bool ShowAbilityRow(AssignmentHost& host, const AbilityRowsState& rows, int row,
                    bool entering,
                    std::optional<std::uint8_t> gradient = std::nullopt) {
  const auto text = host.AbilityText();
  const auto offset =
      static_cast<std::size_t>(rows.cursor.first + row) * kAbilityRowWords;
  if (offset > text.size() || kAbilityRowWords > text.size() - offset) {
    host.Refuse("ability row text disappeared during assignment update");
    return false;
  }
  const AssignmentRow placement{.row = row + kFirstAbilityRow,
                                .x = entering ? kRowEntryX : kRowTargetX,
                                .y = row * kRowHeight + kFirstRowY,
                                .width = kRowWidth,
                                .entering = entering,
                                .target_x = entering ? kRowTargetX : 0,
                                .scroll = RowScroll(rows, row),
                                .gradient = gradient};
  return host.ShowRow(placement, text.subspan(offset, kAbilityNameWords + 2));
}

bool DrawAssignments(const AbilityRowsState& rows, AssignmentHost& host) {
  const auto mappings = host.Mappings(rows.type);
  if (mappings.size() < kAssignmentButtonCount) {
    host.Refuse("ability assignment mapping has fewer than three buttons");
    return false;
  }
  const int visible = std::min(rows.count, kVisibleAbilityRows);
  for (int row = 0; row < visible; ++row) {
    const auto action = host.LearnedAction(rows.type, row + rows.cursor.first);
    if (!action) {
      host.Refuse("visible ability has no learned action");
      return false;
    }
    for (std::size_t button = 0; button < kAssignmentButtonCount; ++button) {
      if (mappings[button] == *action) {
        host.DrawButton(static_cast<AssignmentButton>(button), kButtonX,
                        row * kRowHeight + kFirstButtonY);
      }
    }
  }
  return true;
}

void MoveUp(AbilityRowsState& rows, const AssignmentInput& input) {
  auto& cursor = rows.cursor;
  if (rows.count <= kVisibleAbilityRows) {
    cursor.highlighted =
        cursor.highlighted == 0 ? rows.count - 1 : cursor.highlighted - 1;
  } else if (cursor.first == 0) {
    if (cursor.highlighted != 0) {
      --cursor.highlighted;
    } else if (input.pressed_up) {
      cursor.highlighted = kVisibleAbilityRows - 1;
      cursor.first = rows.count - kVisibleAbilityRows;
    }
  } else if (cursor.highlighted == 1) {
    --cursor.first;
  } else {
    --cursor.highlighted;
  }
}

void MoveDown(AbilityRowsState& rows, const AssignmentInput& input) {
  auto& cursor = rows.cursor;
  if (rows.count <= kVisibleAbilityRows) {
    cursor.highlighted =
        cursor.highlighted == rows.count - 1 ? 0 : cursor.highlighted + 1;
  } else if (cursor.first == rows.count - kVisibleAbilityRows) {
    if (cursor.highlighted != kVisibleAbilityRows - 1) {
      ++cursor.highlighted;
    } else if (input.pressed_down) {
      cursor.highlighted = 0;
      cursor.first = 0;
    }
  } else if (cursor.highlighted == kVisibleAbilityRows - 2) {
    ++cursor.first;
  } else {
    ++cursor.highlighted;
  }
}

bool RefreshRows(const AbilityRowsState& rows, int previous_first,
                 AssignmentHost& host) {
  std::array<std::uint8_t, kAbilityCapacity> gradients{};
  const int visible = std::min(rows.count, kVisibleAbilityRows);
  for (int row = 0; row < visible; ++row) {
    const auto gradient = host.RowGradient(row + kFirstAbilityRow);
    if (!gradient) {
      host.Refuse("missing previous ability menu row");
      return false;
    }
    gradients[static_cast<std::size_t>(row) +
              static_cast<std::size_t>(previous_first)] = *gradient;
  }
  for (int row = 0; row < visible; ++row) {
    if (!ShowAbilityRow(
            host, rows, row, false,
            gradients[static_cast<std::size_t>(row) +
                      static_cast<std::size_t>(rows.cursor.first)])) {
      return false;
    }
  }
  return true;
}

bool AssignPressedButton(const AbilityRowsState& rows, AssignmentHost& host,
                         int selected) {
  const auto input = host.Input();
  if (input.circle) {
    return ToggleButtonAssignment(host, rows.type, AssignmentButton::kCircle,
                                  selected);
  }
  if (input.triangle) {
    return ToggleButtonAssignment(host, rows.type, AssignmentButton::kTriangle,
                                  selected);
  }
  if (input.square) {
    return ToggleButtonAssignment(host, rows.type, AssignmentButton::kSquare,
                                  selected);
  }
  return true;
}

int UpdateActiveAssignments(AbilityRowsState& rows, AssignmentState& state,
                            AssignmentHost& host) {
  host.HideActionCost();
  const int selected = rows.cursor.highlighted + rows.cursor.first;
  const int previous_first = rows.cursor.first;
  if (!DrawAssignments(rows, host) ||
      !AssignPressedButton(rows, host, selected) ||
      !host.SelectRow(rows.cursor.highlighted + kFirstAbilityRow, false)) {
    return 0;
  }
  const auto input = host.Input();
  if (input.cancel) {
    host.PlaySound(AssignmentSound::kLeave);
    host.ReleaseAbilityText();
    host.RememberCursor(rows.type, rows.cursor);
    return 1;
  }
  if (input.repeat_up) MoveUp(rows, input);
  if (input.repeat_down) MoveDown(rows, input);
  if (selected != rows.cursor.highlighted + rows.cursor.first) {
    host.PlaySound(AssignmentSound::kChange);
    if (previous_first != rows.cursor.first &&
        !RefreshRows(rows, previous_first, host)) {
      return 0;
    }
  }
  if (!host.SelectRow(rows.cursor.highlighted + kFirstAbilityRow, true)) {
    return 0;
  }
  state.cursor_animation = host.DrawCursor(
      state.cursor_animation, rows.cursor.highlighted + kCursorRowOffset);
  const auto action = host.LearnedAction(
      rows.type, rows.cursor.highlighted + rows.cursor.first);
  if (!action) {
    host.Refuse("selected ability has no learned action");
    return 0;
  }
  host.SetRiskCost(*action);
  return 0;
}
}  // namespace

// decomp: _mapAbility (src/MENU/MENU2.PRG/64.c:166; 0x80102B5C).
bool ToggleButtonAssignment(AssignmentHost& host, AbilityMenuType type,
                            AssignmentButton button, int ability_index) {
  const auto slot = static_cast<std::size_t>(button);
  if (!ValidType(type) || slot >= kAssignmentButtonCount || ability_index < 0 ||
      ability_index >= static_cast<int>(kAbilityCapacity)) {
    host.Refuse("invalid ability assignment type, button or learned index");
    return false;
  }
  const auto action = host.LearnedAction(type, ability_index);
  const auto mappings = host.Mappings(type);
  if (!action || mappings.size() < kAssignmentButtonCount) {
    host.Refuse("ability assignment has no learned action or button mapping");
    return false;
  }
  if (mappings[slot] == *action) {
    host.PlaySound(AssignmentSound::kLeave);
    mappings[slot] = 0;
    return true;
  }
  host.PlaySound(AssignmentSound::kSet);
  for (auto& mapped : mappings.first(kAssignmentButtonCount)) {
    if (mapped == *action) mapped = 0;
  }
  mappings[slot] = *action;
  return true;
}

// decomp: _abilityMenu (src/MENU/MENU2.PRG/64.c:217; 0x80102CAC).
int UpdateAssignments(AbilityRowsState& rows, AssignmentState& state,
                      AssignmentHost& host) {
  if (!ValidateRows(rows, host)) return 0;
  if (rows.menu_state < kEntryPhaseLimit) {
    if (rows.menu_state >=
        static_cast<std::uint32_t>(std::min(rows.count, kVisibleAbilityRows))) {
      host.Refuse("ability entry phase exceeds the visible row count");
      return 0;
    }
    if (!ShowAbilityRow(host, rows, static_cast<int>(rows.menu_state), true)) {
      return 0;
    }
    ++rows.menu_state;
    if (rows.menu_state == static_cast<std::uint32_t>(rows.count) ||
        rows.menu_state == kVisibleAbilityRows) {
      rows.menu_state = kWaitReady;
    }
    return 0;
  }
  if (!host.RowGradient(rows.cursor.highlighted + kFirstAbilityRow)) {
    host.Refuse("highlighted ability menu row is missing");
    return 0;
  }
  const auto selected = static_cast<std::size_t>(rows.cursor.highlighted) +
                        static_cast<std::size_t>(rows.cursor.first);
  host.ShowDescription(host.AbilityText().subspan(
      selected * kAbilityRowWords + kAbilityNameWords + 2,
      kAbilityDescriptionWords + 2));
  if (rows.menu_state == kWaitReady) {
    if (host.RowsIdle()) rows.menu_state = kAssignments;
  } else if (rows.menu_state == kAssignments) {
    return UpdateActiveAssignments(rows, state, host);
  }
  return 0;
}
}  // namespace VS::Menu::BattleAbilities
