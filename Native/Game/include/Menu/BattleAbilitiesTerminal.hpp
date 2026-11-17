#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/BattleAbilities.hpp"

namespace VS::Menu::BattleAbilities {
inline constexpr int kVisibleAbilityRows = 8;
inline constexpr std::size_t kLearnedAbilitySlots = 14;
inline constexpr std::size_t kAssignmentButtonCount = 3;

enum class AssignmentButton : std::uint8_t { kCircle, kTriangle, kSquare };
enum class AssignmentSound : std::uint8_t { kLeave, kSet, kChange };
enum class ScrollMarker : std::uint8_t { kNone, kAbove, kBelow };

struct AssignmentInput {
  bool circle = false;
  bool triangle = false;
  bool square = false;
  bool cancel = false;
  bool repeat_up = false;
  bool repeat_down = false;
  bool pressed_up = false;
  bool pressed_down = false;
};

struct AssignmentRow {
  int row = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  bool entering = false;
  int target_x = 0;
  ScrollMarker scroll = ScrollMarker::kNone;
  std::optional<std::uint8_t> gradient;
};

struct AssignmentState {
  int cursor_animation = 0;
};

class AssignmentHost {
 public:
  virtual ~AssignmentHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> LearnedAction(
      AbilityMenuType type, int index) const = 0;
  [[nodiscard]] virtual std::span<std::uint8_t> Mappings(
      AbilityMenuType type) = 0;
  virtual void PlaySound(AssignmentSound sound) = 0;
  // Borrowed from the row owner until ReleaseAbilityText; UI calls copy text.
  [[nodiscard]] virtual std::span<const std::uint16_t> AbilityText() const = 0;
  virtual bool ShowRow(const AssignmentRow& row,
                       std::span<const std::uint16_t> name) = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> RowGradient(int row) = 0;
  virtual bool SelectRow(int row, bool selected) = 0;
  virtual void ShowDescription(std::span<const std::uint16_t> text) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void HideActionCost() = 0;
  virtual void DrawButton(AssignmentButton button, int x, int y) = 0;
  [[nodiscard]] virtual AssignmentInput Input() const = 0;
  virtual void ReleaseAbilityText() = 0;
  virtual void RememberCursor(AbilityMenuType type, AbilityCursor cursor) = 0;
  [[nodiscard]] virtual int DrawCursor(int animation, int position) = 0;
  virtual void SetRiskCost(std::uint8_t action) = 0;
};

bool ToggleButtonAssignment(AssignmentHost& host, AbilityMenuType type,
                            AssignmentButton button, int ability_index);
int UpdateAssignments(AbilityRowsState& rows, AssignmentState& state,
                      AssignmentHost& host);

enum class AbilityListPhase : std::uint32_t { kInit, kAssignments };
struct AbilityListState {
  AbilityListPhase phase = AbilityListPhase::kInit;
};

struct LearnedAbility {
  std::uint8_t action = 0;
  std::uint8_t description_slot = 0;
  friend bool operator==(const LearnedAbility&,
                         const LearnedAbility&) = default;
};

class AbilityListHost {
 public:
  virtual ~AbilityListHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  virtual void FlyOutCategory(AbilityKind kind) = 0;
  [[nodiscard]] virtual bool ShortcutInvoked() const = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  [[nodiscard]] virtual std::span<const std::uint8_t> ActionOrder(
      AbilityKind kind) const = 0;
  [[nodiscard]] virtual bool ActionUnlocked(std::uint8_t action) const = 0;
  virtual void StoreLearnedAction(AbilityKind kind, int index,
                                  std::uint8_t action) = 0;
  virtual void CreateRows(AbilityKind kind,
                          std::span<const LearnedAbility> rows) = 0;
  [[nodiscard]] virtual int UpdateAssignments() = 0;
  virtual void ClearAbilityRows() = 0;
};

int UpdateChainAbilityMenu(AbilityListState& state, AbilityListHost& host,
                           bool initialize);
int UpdateDefenseAbilityMenu(AbilityListState& state, AbilityListHost& host,
                             bool initialize);

class UnlockPointsHost {
 public:
  virtual ~UnlockPointsHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  [[nodiscard]] virtual int UnlockCount() const = 0;
  [[nodiscard]] virtual std::uint16_t TotalKills() const = 0;
  [[nodiscard]] virtual std::optional<std::uint16_t> Requirement(
      int unlock_count) const = 0;
  virtual void DrawCompactText(std::string_view text,
                               std::uint32_t position) = 0;
};

void DrawNextUnlockPoints(int x_offset, UnlockPointsHost& host);

enum class AbilityScreenPhase : std::uint8_t {
  kNone,
  kInit = 3,
  kSelectTypeInit,
  kSelectType,
  kChain,
  kDefense,
  kExitToMainMenu,
  kExitToBattle,
};

struct AbilityScreenAnimation {
  bool menu_animating = false;
  std::uint8_t points_step = 0;
};

struct AbilityShortcutRow {
  AbilityKind kind{};
  int row = 0;
  int x = 0;
  int y = 0;
  int width = 0;
  int background_width = 0;
};

class AbilityScreenHost {
 public:
  virtual ~AbilityScreenHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void InitializeInformationBox() = 0;
  [[nodiscard]] virtual int Shortcut() const = 0;
  virtual bool ShowShortcutCategory(const AbilityShortcutRow& row) = 0;
  [[nodiscard]] virtual bool HasLearnedAbility(AbilityKind kind) = 0;
  virtual void ShowCategories(bool chain_available, bool defense_available,
                              bool force_cursor_memory) = 0;
  [[nodiscard]] virtual int SelectedCategory() = 0;
  [[nodiscard]] virtual int UpdateAbilityList(AbilityKind kind,
                                              bool initialize) = 0;
  virtual void ClearMenu() = 0;
  virtual void DismissInformationBox() = 0;
  virtual void ClearCommand() = 0;
  virtual void ReturnToBattleMenu() = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> AnimationOffset(
      std::uint8_t step) const = 0;
  virtual void DrawNextUnlockPoints(int x_offset) = 0;
};

int UpdateAbilityScreen(std::uint8_t& phase, AbilityScreenAnimation& animation,
                        AbilityScreenHost& host);
}  // namespace VS::Menu::BattleAbilities
