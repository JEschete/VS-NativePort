#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Menu/BattleAbilities.hpp"

namespace VS::Test {  // NOLINT(readability-identifier-naming)
namespace ability_foundation {
namespace abilities = Menu::BattleAbilities;

struct CostText {
  int index;
  std::string text;
  int x;
  bool disabled;
};

struct World : abilities::UnlockProgress,
               abilities::BreakArtUnlockHost,
               abilities::AbilityUnlockHost,
               abilities::RiskCostHost,
               abilities::AbilityRowsHost {
  int weapon = 1;
  std::array<int, 12> learned{};
  std::array<int, 10> kills{};
  std::array<std::array<int, 4>, 10> art_requirements{};
  int unlocked = 0;
  int total_kills = 0;
  std::array<int, 22> ability_requirements{};
  std::array<bool, 256> actions{};
  std::vector<std::string> events;
  std::vector<CostText> costs;
  int message_weapon = -1;
  int message_action = -1;
  bool all_unlocked = false;
  bool done = false;
  bool confirm = false;
  bool ready = true;
  int selected = -1;
  bool category_available = true;
  bool list_available = true;
  std::optional<abilities::AbilityCategoryRow> category;
  std::vector<std::uint8_t> listed;
  abilities::AbilityKind list_kind{};
  int visible_rows = 0;
  bool forced_cursor = false;
  bool has_text = false;
  bool remember_cursor = false;
  abilities::AbilityCursor cursor{};
  bool missing_action = false;
  bool fail_allocation = false;
  bool short_allocation = false;
  std::size_t allocation_words = 0;
  int risk_action = -1;
  std::array<std::uint16_t,
             abilities::kAbilityCapacity * abilities::kAbilityRowWords + 1>
      text{};
  abilities::AbilityRowsState published{};

  World() {
    for (auto& requirements : art_requirements) requirements.fill(100);
    ability_requirements.fill(100);
    text.fill(0xCAFE);
  }

  int EquippedWeaponCategory() const override { return weapon; }
  int ArtsLearned(int category_index) const override {
    return learned.at(category_index);
  }
  void SetArtsLearned(int category_index, int count) override {
    learned.at(category_index) = count;
    events.emplace_back("learn-art");
  }
  int WeaponCategoryKills(int category_index) const override {
    return kills.at(category_index);
  }
  std::optional<int> BreakArtRequirement(int category_index,
                                         int tier) const override {
    return art_requirements.at(category_index).at(tier);
  }
  int BattleAbilitiesUnlocked() const override { return unlocked; }
  void SetBattleAbilitiesUnlocked(int count) override {
    unlocked = count;
    events.emplace_back("earn-ability");
  }
  std::optional<int> BattleAbilityRequirement(int tier) const override {
    return ability_requirements.at(tier);
  }
  int TotalKills() const override { return total_kills; }
  bool ActionUnlocked(std::uint8_t action) const override {
    return actions.at(action);
  }
  void UnlockAction(std::uint8_t action) override {
    actions.at(action) = true;
    events.emplace_back("unlock-action");
  }
  void Refuse(std::string_view) override { events.emplace_back("refuse"); }
  void InitializeInformationBox() override { events.emplace_back("box"); }
  void SetBreakArtMessage(int category_index, std::uint8_t action) override {
    message_weapon = category_index;
    message_action = action;
    events.emplace_back("art-message");
  }
  bool InformationBoxDone() const override { return done; }
  bool ConfirmPressed() const override { return confirm; }
  void ResetUnlockMenuMemory() override { events.emplace_back("reset-memory"); }
  int SelectedRow() const override { return selected; }
  bool ShowCategory(const abilities::AbilityCategoryRow& row) override {
    category = row;
    events.emplace_back("category");
    return category_available;
  }
  void ShowBothCategories() override { events.emplace_back("both"); }
  bool ShowAbilityList(abilities::AbilityKind kind,
                       std::span<const std::uint8_t> remaining, int visible,
                       bool force_cursor) override {
    list_kind = kind;
    listed.assign(remaining.begin(), remaining.end());
    visible_rows = visible;
    forced_cursor = force_cursor;
    events.emplace_back("list");
    return list_available;
  }
  void ClearMenu() override { events.emplace_back("clear"); }
  void FlyOutRowsAndHoistSelection(int row) override {
    events.push_back("fly-" + std::to_string(row));
  }
  bool MenuRowsIdle() const override { return ready; }
  void SetAbilityMessage(bool all, std::uint8_t action) override {
    all_unlocked = all;
    message_action = action;
    events.emplace_back("ability-message");
  }
  void SetActionCostText(int index, std::string_view value, int x,
                         bool disabled) override {
    costs.push_back({index, std::string(value), x, disabled});
  }
  bool HasAbilityText() const override { return has_text; }
  bool CursorMemoryEnabled() const override { return remember_cursor; }
  abilities::AbilityCursor SavedCursor(
      abilities::AbilityMenuType) const override {
    return cursor;
  }
  std::optional<std::uint8_t> ActionAt(abilities::AbilityMenuType type,
                                       int index) const override {
    if (missing_action) return std::nullopt;
    const int first = type == abilities::AbilityMenuType::kChain ? 24 : 40;
    return static_cast<std::uint8_t>(first + index);
  }
  void SetRiskCost(std::uint8_t action) override {
    risk_action = action;
    events.emplace_back("risk");
  }
  std::span<std::uint16_t> AllocateAbilityText(std::size_t words) override {
    allocation_words = words;
    events.emplace_back("allocate");
    if (fail_allocation) return {};
    return std::span(text).first(short_allocation ? words - 1 : words);
  }
  void PublishRows(const abilities::AbilityRowsState& state,
                   std::span<std::uint16_t>) override {
    published = state;
    has_text = true;
    events.emplace_back("publish");
  }
};

struct Rows {
  std::array<std::array<std::uint16_t, abilities::kAbilityNameWords>,
             abilities::kAbilityCapacity>
      names{};
  std::array<std::array<std::uint16_t, abilities::kAbilityDescriptionWords>,
             abilities::kAbilityCapacity>
      descriptions{};
  std::array<abilities::AbilityRowText, abilities::kAbilityCapacity> text{};

  Rows() {
    for (std::size_t row = 0; row < text.size(); ++row) {
      std::ranges::fill(names[row], static_cast<std::uint16_t>(row + 0x100));
      std::ranges::fill(descriptions[row],
                        static_cast<std::uint16_t>(row + 0x200));
      text[row] = {names[row], descriptions[row]};
    }
  }
};

struct Notifications final : abilities::UnlockNotificationHost {
  bool art_pending = false;
  bool ability_pending = false;
  int art_result = 0;
  int ability_result = 0;
  int dismiss_result = 0;
  std::vector<int> calls;

  void InitializeInformationBox() override { calls.push_back(0); }
  bool BreakArtPending() const override { return art_pending; }
  bool AbilityPending() const override { return ability_pending; }
  int UpdateBreakArt(bool initialize) override {
    calls.push_back(initialize ? 1 : 2);
    return art_result;
  }
  int UpdateAbility(bool initialize) override {
    calls.push_back(initialize ? 3 : 4);
    return ability_result;
  }
  int DismissInformationBox() override {
    calls.push_back(5);
    return dismiss_result;
  }
};
}  // namespace ability_foundation
}  // namespace VS::Test
