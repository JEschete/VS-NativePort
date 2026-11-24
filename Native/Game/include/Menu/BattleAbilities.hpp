#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Menu/Framework/Framework.hpp"

namespace VS::Menu::BattleAbilities {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kUnlockableAbilityCount = 11;
inline constexpr std::size_t kAbilityCapacity = 16;
inline constexpr std::size_t kAbilityNameWords = 14;
inline constexpr std::size_t kAbilityDescriptionWords = 46;
inline constexpr std::size_t kAbilityRowWords = 64;
inline constexpr std::size_t kRiskTextCapacity = 16;

enum class AbilityKind : std::uint8_t { kChain, kDefense };
enum class AbilityMenuType : std::uint8_t { kNone = 0, kChain = 24, kDefense };

class UnlockProgress {
 public:
  virtual ~UnlockProgress() = default;
  [[nodiscard]] virtual int EquippedWeaponCategory() const = 0;
  [[nodiscard]] virtual int ArtsLearned(int category) const = 0;
  virtual void SetArtsLearned(int category, int count) = 0;
  [[nodiscard]] virtual int WeaponCategoryKills(int category) const = 0;
  [[nodiscard]] virtual std::optional<int> BreakArtRequirement(
      int category, int tier) const = 0;
  [[nodiscard]] virtual int BattleAbilitiesUnlocked() const = 0;
  virtual void SetBattleAbilitiesUnlocked(int count) = 0;
  [[nodiscard]] virtual std::optional<int> BattleAbilityRequirement(
      int tier) const = 0;
  [[nodiscard]] virtual int TotalKills() const = 0;
  [[nodiscard]] virtual bool ActionUnlocked(std::uint8_t action) const = 0;
  virtual void UnlockAction(std::uint8_t action) = 0;
};

struct BreakArtUnlockState {
  std::uint8_t timeout = 0;
};

class BreakArtUnlockHost {
 public:
  virtual ~BreakArtUnlockHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  virtual void InitializeInformationBox() = 0;
  virtual void SetBreakArtMessage(int weapon_category, std::uint8_t action) = 0;
  [[nodiscard]] virtual bool InformationBoxDone() const = 0;
  [[nodiscard]] virtual bool ConfirmPressed() const = 0;
};

int UpdateBreakArtUnlock(BreakArtUnlockState& state, UnlockProgress& progress,
                         BreakArtUnlockHost& host, bool initialize);

[[nodiscard]] std::span<const std::uint16_t> UnlockDescription(
    int ability, std::span<const std::uint16_t> ability_strings,
    std::span<const std::uint16_t> menu_strings, std::size_t fallback_offset);

enum class AbilityUnlockPhase : std::uint8_t {
  kInit,
  kBothRemaining,
  kChainOnly,
  kDefenseOnly,
  kUnlockChain,
  kUnlockDefense,
  kShowMessage,
  kReinit,
};

struct AbilityUnlockState {
  std::array<std::uint8_t, kAbilityCapacity> remaining_chain{};
  std::array<std::uint8_t, kUnlockableAbilityCount> remaining_defense{};
  std::uint8_t chain_count = 0;
  std::uint8_t defense_count = 0;
  AbilityUnlockPhase phase = AbilityUnlockPhase::kInit;
  std::uint8_t timeout = 0;
  std::uint8_t first_visit = 0;
};

struct AbilityCategoryRow {
  AbilityKind kind{};
  int x = 0;
  int y = 0;
  int width = 0;
  int background_width = 0;
  int target_x = 0;
  Framework::RowState state = Framework::RowState::kInactive;
  bool selected = false;
};

class AbilityUnlockHost {
 public:
  virtual ~AbilityUnlockHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  virtual void ResetUnlockMenuMemory() = 0;
  [[nodiscard]] virtual int SelectedRow() const = 0;
  virtual bool ShowCategory(const AbilityCategoryRow& row) = 0;
  virtual void ShowBothCategories() = 0;
  virtual bool ShowAbilityList(AbilityKind kind,
                               std::span<const std::uint8_t> actions,
                               int visible_rows, bool force_cursor) = 0;
  virtual void ClearMenu() = 0;
  virtual void FlyOutRowsAndHoistSelection(int row) = 0;
  [[nodiscard]] virtual bool MenuRowsIdle() const = 0;
  virtual void SetAbilityMessage(bool all_unlocked, std::uint8_t action) = 0;
  virtual void InitializeInformationBox() = 0;
  [[nodiscard]] virtual bool InformationBoxDone() const = 0;
  [[nodiscard]] virtual bool ConfirmPressed() const = 0;
};

int UpdateAbilityUnlock(AbilityUnlockState& state, UnlockProgress& progress,
                        AbilityUnlockHost& host, bool initialize);

class UnlockNotificationHost {
 public:
  virtual ~UnlockNotificationHost() = default;
  virtual void InitializeInformationBox() = 0;
  [[nodiscard]] virtual bool BreakArtPending() const = 0;
  [[nodiscard]] virtual bool AbilityPending() const = 0;
  virtual int UpdateBreakArt(bool initialize) = 0;
  virtual int UpdateAbility(bool initialize) = 0;
  virtual int DismissInformationBox() = 0;
};

int UpdateUnlockNotifications(std::uint8_t& state,
                              UnlockNotificationHost& host);

struct RiskCostState {
  std::array<char, kRiskTextCapacity> text{};
};

class RiskCostHost {
 public:
  virtual ~RiskCostHost() = default;
  virtual void SetActionCostText(int index, std::string_view text, int x,
                                 bool disabled) = 0;
};

void SetRiskCost(RiskCostState& state, std::uint8_t cost, RiskCostHost& host);

struct AbilityCursor {
  int highlighted = 0;
  int first = 0;
};

struct AbilityRowsState {
  int count = 0;
  std::uint32_t menu_state = 0;
  AbilityMenuType type = AbilityMenuType::kChain;
  AbilityCursor cursor{};
};

struct AbilityRowText {
  std::span<const std::uint16_t> name;
  std::span<const std::uint16_t> description;
};

class AbilityRowsHost {
 public:
  virtual ~AbilityRowsHost() = default;
  virtual void Refuse(std::string_view reason) = 0;
  [[nodiscard]] virtual bool HasAbilityText() const = 0;
  [[nodiscard]] virtual bool CursorMemoryEnabled() const = 0;
  [[nodiscard]] virtual AbilityCursor SavedCursor(
      AbilityMenuType type) const = 0;
  [[nodiscard]] virtual std::optional<std::uint8_t> ActionAt(
      AbilityMenuType type, int index) const = 0;
  virtual void SetRiskCost(std::uint8_t action) = 0;
  [[nodiscard]] virtual std::span<std::uint16_t> AllocateAbilityText(
      std::size_t words) = 0;
  virtual void PublishRows(const AbilityRowsState& state,
                           std::span<std::uint16_t> text) = 0;
};

int CreateAbilityRows(AbilityRowsState& state, AbilityRowsHost& host, int count,
                      AbilityMenuType type,
                      std::span<const AbilityRowText> rows);
}  // namespace VS::Menu::BattleAbilities
