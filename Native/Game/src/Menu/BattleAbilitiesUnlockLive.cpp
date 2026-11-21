#include "Menu/BattleAbilitiesUnlockLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <iterator>

#include "Battle/AbilityState.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/TargetingState.hpp"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/BattleAbilities.hpp"
#include "Menu/BattleAbilitiesLiveState.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/TextBox.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "State/Progress.hpp"

namespace menu2_battle_abilities_menu =
    VS::Content::text::menu2_prg::battle_abilities_menu;

namespace VS::Menu::BattleAbilities {
namespace {
constexpr int kInformationBox = 7;
constexpr int kCategoryRowInfo = 0x1A;
constexpr int kAbilityCategoryCount = 2;
constexpr int kChainRowInfo = 0x11B;
constexpr int kDefenseRowInfo = 0x11C;
constexpr std::size_t kPointersPerRow = 2;
BreakArtUnlockState breakArtUnlock;
AbilityUnlockState abilityUnlock;
RiskCostState riskCost;

void Refuse(std::string_view reason) {
  VS::Core::LogWarn("menu.battle-abilities", "Ability menu refused: {}",
                    reason);
}

template <typename Array>
bool InRange(const Array& array, int index) {
  return index >= 0 && static_cast<std::size_t>(index) < std::size(array);
}

char LegacyByte(int value) {
  return std::bit_cast<char>(static_cast<std::uint8_t>(value));
}

char* MenuText(std::size_t offset) {
  return offset < std::size(battleAbilityMenuStrings)
             ? reinterpret_cast<char*>(battleAbilityMenuStrings + offset)
             : nullptr;
}

bool InformationDone() { return textBoxes[kInformationBox].flags.done != 0; }

bool ConfirmPressed() {
  return VS::Input::LowButtonByte(VS::Input::buttonsPressed) != 0;
}

class LiveProgress final : public UnlockProgress {
 public:
  int EquippedWeaponCategory() const override {
    return VS::Battle::characterState != nullptr
               ? VS::Battle::characterState->equipped_weapon_category
               : -1;
  }
  int ArtsLearned(int category) const override {
    return VS::State::artsStatus.arts_learned[category];
  }
  void SetArtsLearned(int category, int count) override {
    VS::State::artsStatus.arts_learned[category] =
        static_cast<std::uint8_t>(count);
  }
  int WeaponCategoryKills(int category) const override {
    return VS::State::artsStatus.kills.weapon_category_kills[category];
  }
  int BreakArtRequirement(int category, int tier) const override {
    return VS::Battle::breakArtPointRequirements[category][tier];
  }
  int BattleAbilitiesUnlocked() const override {
    return VS::State::artsStatus.kills.battle_abilities_unlocked;
  }
  void SetBattleAbilitiesUnlocked(int count) override {
    VS::State::artsStatus.kills.battle_abilities_unlocked =
        static_cast<std::uint16_t>(count);
  }
  int BattleAbilityRequirement(int tier) const override {
    return VS::Battle::battleAbilityPointRequirements[tier];
  }
  int TotalKills() const override {
    return VS::State::artsStatus.kills.total_kills;
  }
  bool ActionUnlocked(std::uint8_t action) const override {
    return VS::Battle::actionDefinitions[action].unlocked != 0;
  }
  void UnlockAction(std::uint8_t action) override {
    VS::Battle::actionDefinitions[action].unlocked = 1;
  }
};

class LiveBreakArtHost final : public BreakArtUnlockHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  void InitializeInformationBox() override {
    InitLiveInformationTextBox(InformationHeader::kCheck);
  }
  void SetBreakArtMessage(int weapon, std::uint8_t action) override {
    VS::Battle::stringContext.strings[0] =
        MenuText(battleAbilityMenuStrings[weapon]);
    VS::Battle::stringContext.strings[1] =
        VS::Battle::actionDefinitions[action].name.data();
    Framework::SetLiveInformationMessage(
        MenuText(menu2_battle_abilities_menu::offset::kBreakArtUnlock));
  }
  bool InformationBoxDone() const override { return InformationDone(); }
  bool ConfirmPressed() const override {
    return BattleAbilities::ConfirmPressed();
  }
};

class LiveRiskHost final : public RiskCostHost {
 public:
  void SetActionCostText(int index, std::string_view text, int x,
                         bool disabled) override {
    // The framework copies a full 16-byte legacy header, not just the text.
    std::array<char, kRiskTextCapacity> terminated{};
    std::ranges::copy(text, terminated.begin());
    Framework::SetLiveActionCostText(index, terminated.data(), x,
                                     disabled ? 1 : 0);
  }
};

class LiveAbilityHost final : public AbilityUnlockHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  void ResetUnlockMenuMemory() override {
    VS::Menu::Framework::menuMemory.ability_unlock_cursors = {};
  }
  int SelectedRow() const override { return Framework::LiveSelectedRow(); }
  bool ShowCategory(const AbilityCategoryRow& row) override {
    const bool chain = row.kind == AbilityKind::kChain;
    auto* item = Framework::SetLiveMenuItem(
        chain ? 0 : 1, row.x, row.y, row.width, row.background_width,
        MenuText(chain
                     ? menu2_battle_abilities_menu::offset::kChainAbilities
                     : menu2_battle_abilities_menu::offset::kDefenseAbilities));
    if (item == nullptr) {
      Refuse("missing category menu item");
      return false;
    }
    item->state = row.state;
    item->target0 = static_cast<std::int16_t>(row.target_x);
    item->selected = row.selected ? 1 : 0;
    return true;
  }
  void ShowBothCategories() override {
    std::array<char*, kAbilityCategoryCount * kPointersPerRow> strings{
        MenuText(menu2_battle_abilities_menu::offset::kChainAbilities),
        MenuText(menu2_battle_abilities_menu::offset::kSelect),
        MenuText(menu2_battle_abilities_menu::offset::kDefenseAbilities),
        MenuText(menu2_battle_abilities_menu::offset::kSelect)};
    std::array<int, kAbilityCategoryCount> types{};
    const auto saved_cursor = VS::Input::settings.cursor_memory;
    VS::Input::settings.cursor_memory = 1;
    Framework::CreateLiveMenuRows(kAbilityCategoryCount, kCategoryRowInfo,
                                  strings.data(), types.data());
    VS::Input::settings.cursor_memory = saved_cursor;
  }
  bool ShowAbilityList(AbilityKind kind, std::span<const std::uint8_t> actions,
                       int visible_rows, bool force_cursor) override {
    std::array<char*, kUnlockableAbilityCount * kPointersPerRow> strings{};
    std::array<int, kUnlockableAbilityCount> types{};
    for (std::size_t index = 0; index < actions.size(); ++index) {
      strings[index * kPointersPerRow] =
          VS::Battle::actionDefinitions[actions[index]].name.data();
      auto* description = LiveUnlockDescription(actions[index]);
      if (description == nullptr) return false;
      strings[index * kPointersPerRow + 1] = description;
    }
    const auto saved_cursor = VS::Input::settings.cursor_memory;
    if (force_cursor) VS::Input::settings.cursor_memory = 1;
    const int row_info =
        kind == AbilityKind::kChain ? kChainRowInfo : kDefenseRowInfo;
    Framework::CreateLiveMenuRows(visible_rows, row_info, strings.data(),
                                  types.data());
    VS::Input::settings.cursor_memory = saved_cursor;
    return true;
  }
  void ClearMenu() override {
    Framework::ClearLiveMenuExcept(Framework::kNoRow);
  }
  void FlyOutRowsAndHoistSelection(int row) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, 0);
  }
  bool MenuRowsIdle() const override {
    return Framework::AreLiveMenuRowsIdle() != 0;
  }
  void SetAbilityMessage(bool all_unlocked, std::uint8_t action) override {
    if (!all_unlocked) {
      VS::Battle::stringContext.strings[1] =
          VS::Battle::actionDefinitions[action].name.data();
    }
    Framework::SetLiveInformationMessage(MenuText(
        all_unlocked
            ? menu2_battle_abilities_menu::offset::kAllAbilitiesUnlocked
            : menu2_battle_abilities_menu::offset::kBattleAbilityUnlock));
  }
  void InitializeInformationBox() override {
    InitLiveInformationTextBox(InformationHeader::kCheck);
  }
  bool InformationBoxDone() const override { return InformationDone(); }
  bool ConfirmPressed() const override {
    return BattleAbilities::ConfirmPressed();
  }
};

class LiveNotificationHost final : public UnlockNotificationHost {
 public:
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  bool BreakArtPending() const override {
    return VS::Menu::Framework::unlockedBreakArt != 0;
  }
  bool AbilityPending() const override {
    return VS::Menu::Framework::unlockedBattleAbility != 0;
  }
  int UpdateBreakArt(bool initialize) override {
    return LiveUpdateBreakArtUnlock(initialize ? 1 : 0);
  }
  int UpdateAbility(bool initialize) override {
    return LiveUpdateAbilityUnlock(initialize ? 1 : 0);
  }
  int DismissInformationBox() override {
    return DismissLiveTextBox(kInformationBox);
  }
};

class LiveRowsHost final : public AbilityRowsHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  bool HasAbilityText() const override { return abilityText != nullptr; }
  bool CursorMemoryEnabled() const override {
    return VS::Input::settings.cursor_memory != 0;
  }
  AbilityCursor SavedCursor(AbilityMenuType type) const override {
    const auto index = static_cast<std::size_t>(type) * kPointersPerRow;
    return {VS::Menu::Framework::menuMemory.cursor_memories[index],
            VS::Menu::Framework::menuMemory.cursor_memories[index + 1]};
  }
  std::optional<std::uint8_t> ActionAt(AbilityMenuType type,
                                       int index) const override {
    if (!InRange(unlockedChainAbilities, index)) {
      return std::nullopt;
    }
    const auto* actions = type == AbilityMenuType::kChain
                              ? unlockedChainAbilities
                              : unlockedDefenseAbilities;
    return actions[index];
  }
  void SetRiskCost(std::uint8_t action) override { LiveSetRiskCost(action); }
  std::span<std::uint16_t> AllocateAbilityText(std::size_t words) override {
    auto* text = static_cast<std::uint16_t*>(
        Core::Memory::AllocateLiveFromHighEnd(words * sizeof(std::uint16_t)));
    return text != nullptr ? std::span(text, words)
                           : std::span<std::uint16_t>{};
  }
  void PublishRows(const AbilityRowsState& state,
                   std::span<std::uint16_t> text) override {
    menuAbilityCount = static_cast<std::uint8_t>(state.count);
    abilityMenuState = state.menu_state;
    selectedAbilityType = static_cast<std::uint8_t>(state.type);
    highlightedAbility = state.cursor.highlighted;
    firstAbility = static_cast<std::uint8_t>(state.cursor.first);
    abilityText = text.data();
  }
};

bool CopyOwnedWords(const std::uint16_t* source,
                    std::span<std::uint16_t> destination) {
  const std::array<std::span<const std::byte>, 3> owners{
      std::as_bytes(std::span(VS::Battle::actionDefinitions)),
      std::as_bytes(std::span(battleAbilityStrings)),
      std::as_bytes(std::span(battleAbilityMenuStrings))};
  const auto address = reinterpret_cast<std::uintptr_t>(source);
  for (const auto owner : owners) {
    const auto base = reinterpret_cast<std::uintptr_t>(owner.data());
    if (address < base || address - base > owner.size()) continue;
    const auto offset = static_cast<std::size_t>(address - base);
    if (destination.size_bytes() <= owner.size() - offset) {
      // The original 14-word name read includes four bytes after name[24].
      // Copy through the checked enclosing action array, not a uint16_t alias.
      std::memcpy(destination.data(), owner.data() + offset,
                  destination.size_bytes());
      return true;
    }
  }
  return false;
}

struct StagedRows {
  std::array<std::array<std::uint16_t, kAbilityNameWords>, kAbilityCapacity>
      names{};
  std::array<std::array<std::uint16_t, kAbilityDescriptionWords>,
             kAbilityCapacity>
      descriptions{};
  std::array<AbilityRowText, kAbilityCapacity> rows{};

  bool Read(std::span<std::uint16_t* const> text, std::size_t count) {
    for (std::size_t row = 0; row < count; ++row) {
      const auto* name = text[row * kPointersPerRow];
      const auto* description = text[row * kPointersPerRow + 1];
      if (name != nullptr) {
        if (!CopyOwnedWords(name, names[row])) return false;
        rows[row].name = names[row];
      }
      if (description != nullptr) {
        if (!CopyOwnedWords(description, descriptions[row])) return false;
        rows[row].description = descriptions[row];
      }
    }
    return true;
  }
};
}  // namespace

// decomp binding: breakArtUnlocked (src/MENU/MENU2.PRG/143C.c:31; was
// _breakArtUnlocked).
int LiveUpdateBreakArtUnlock(int initialize) {
  LiveProgress progress;
  if (initialize != 0) {
    const int weapon = progress.EquippedWeaponCategory();
    if (InRange(battleAbilityMenuStrings, weapon) &&
        MenuText(battleAbilityMenuStrings[weapon]) == nullptr) {
      Refuse("Break Art category text outside menu table");
      return 1;
    }
  }
  LiveBreakArtHost host;
  return UpdateBreakArtUnlock(breakArtUnlock, progress, host, initialize != 0);
}

// decomp binding: getAbilityDescription (src/MENU/MENU2.PRG/143C.c:85; was
// _getAbilityDescription).
char* LiveUnlockDescription(int ability) {
  const auto text =
      UnlockDescription(ability, battleAbilityStrings, battleAbilityMenuStrings,
                        menu2_battle_abilities_menu::offset::kUnknown1);
  if (text.empty()) {
    Refuse("ability description outside text table");
    return nullptr;
  }
  return reinterpret_cast<char*>(const_cast<std::uint16_t*>(text.data()));
}

// decomp binding: battleAbilityUnlocked (src/MENU/MENU2.PRG/143C.c:129; was
// _battleAbilityUnlocked).
int LiveUpdateAbilityUnlock(int initialize) {
  LiveProgress progress;
  LiveAbilityHost host;
  return UpdateAbilityUnlock(abilityUnlock, progress, host, initialize != 0);
}

// decomp binding: vs_menu2_actionUnlock (src/MENU/MENU2.PRG/143C.c:426).
int LiveUpdateUnlockNotifications(char* state) {
  if (state == nullptr) {
    Refuse("missing unlock notification state");
    return 1;
  }
  auto phase = static_cast<std::uint8_t>(*state);
  LiveNotificationHost host;
  const int result = UpdateUnlockNotifications(phase, host);
  *state = LegacyByte(phase);
  return result;
}

// decomp binding: setRiskCost (src/MENU/MENU2.PRG/64.c:22; was _setRiskCost).
void LiveSetRiskCost(int action) {
  if (!InRange(VS::Battle::actionDefinitions, action)) {
    Refuse("Risk action outside action table");
    return;
  }
  LiveRiskHost host;
  SetRiskCost(riskCost, VS::Battle::actionDefinitions[action].cost, host);
}

// decomp binding: initAbilityMenu (src/MENU/MENU2.PRG/64.c:73; was
// _initAbilityMenu).
int LiveCreateAbilityRows(int count, int type,
                          std::span<std::uint16_t* const> menu_text) {
  if (abilityText != nullptr) return 0;
  if (count <= 0 || count > static_cast<int>(kAbilityCapacity) ||
      menu_text.size() < static_cast<std::size_t>(count) * kPointersPerRow ||
      (type != static_cast<int>(AbilityMenuType::kChain) &&
       type != static_cast<int>(AbilityMenuType::kDefense))) {
    Refuse("invalid learned ability rows");
    return 0;
  }
  StagedRows staged;
  if (!staged.Read(menu_text, static_cast<std::size_t>(count))) {
    Refuse("ability row text outside its native owner");
    return 0;
  }
  LiveRowsHost host;
  AbilityRowsState state;
  return CreateAbilityRows(state, host, count,
                           static_cast<AbilityMenuType>(type), staged.rows);
}

// Original _initAbilityMenu ABI (src/MENU/MENU2.PRG/64.c:64, 0x80102928).
int LiveCreateAbilityRowsEntry(int count, int type, std::uint16_t** menu_text) {
  if (abilityText != nullptr || menu_text == nullptr || count <= 0 ||
      count > static_cast<int>(kAbilityCapacity)) {
    return LiveCreateAbilityRows(count, type, {});
  }
  return LiveCreateAbilityRows(
      count, type,
      {menu_text, static_cast<std::size_t>(count) * kPointersPerRow});
}

void ResetLiveAbilityFoundationState() {
  breakArtUnlock = {};
  abilityUnlock = {};
  riskCost = {};
}
}  // namespace VS::Menu::BattleAbilities
