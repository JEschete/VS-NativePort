#include "Menu/BattleAbilitiesUnlockLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <iterator>

#include "Battle/AbilityState.h"
#include "Battle/ActionTable.h"
#include "Battle/TargetingState.h"
#include "Content/StringTables.h"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/BattleAbilities.h"
#include "Menu/BattleAbilities.hpp"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/TextBoxes.h"
#include "Menu/TextBox.h"
#include "Menu/TextBoxStateLive.hpp"
#include "State/Progress.hpp"

namespace VS::Menu::BattleAbilities {
namespace {
constexpr int kInformationBox = 7;
constexpr int kCategoryRowInfo = 0x1A;
constexpr int kAbilityCategoryCount = 2;
constexpr int kChainRowInfo = 0x11B;
constexpr int kDefenseRowInfo = 0x11C;
constexpr std::size_t kPointersPerRow = 2;
BreakArtUnlockState g_break_art;
AbilityUnlockState g_ability_unlock;
RiskCostState g_risk;

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
  return offset < std::size(Menu_BattleAbilities_battleAbilityMenuStrings)
             ? reinterpret_cast<char*>(
                   Menu_BattleAbilities_battleAbilityMenuStrings + offset)
             : nullptr;
}

bool InformationDone() {
  return Menu_textBoxes[kInformationBox].flags.done != 0;
}

bool ConfirmPressed() {
  return VS::Input::LowButtonByte(VS::Input::buttonsPressed) != 0;
}

class LiveProgress final : public UnlockProgress {
 public:
  int EquippedWeaponCategory() const override {
    return Battle_characterState != nullptr
               ? Battle_characterState->equippedWeaponCategory
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
    return Battle_breakArtPointRequirements[category][tier];
  }
  int BattleAbilitiesUnlocked() const override {
    return VS::State::artsStatus.kills.battle_abilities_unlocked;
  }
  void SetBattleAbilitiesUnlocked(int count) override {
    VS::State::artsStatus.kills.battle_abilities_unlocked =
        static_cast<std::uint16_t>(count);
  }
  int BattleAbilityRequirement(int tier) const override {
    return Battle_battleAbilityPointRequirements[tier];
  }
  int TotalKills() const override {
    return VS::State::artsStatus.kills.total_kills;
  }
  bool ActionUnlocked(std::uint8_t action) const override {
    return Battle_actionDefinitions[action].unlocked != 0;
  }
  void UnlockAction(std::uint8_t action) override {
    Battle_actionDefinitions[action].unlocked = 1;
  }
};

class LiveBreakArtHost final : public BreakArtUnlockHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  void InitializeInformationBox() override {
    InitLiveInformationTextBox(Menu_InformationBoxHeader_Check);
  }
  void SetBreakArtMessage(int weapon, std::uint8_t action) override {
    Battle_stringContext.strings[0] =
        MenuText(Menu_BattleAbilities_battleAbilityMenuStrings[weapon]);
    Battle_stringContext.strings[1] = Battle_actionDefinitions[action].name;
    Framework::SetLiveInformationMessage(
        MenuText(Text_Menu2_battleAbilitiesMenu_Offset_breakArtUnlock));
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
    std::memset(VS::Menu::Framework::menuMemory.opaque34, 0,
                sizeof(VS::Menu::Framework::menuMemory.opaque34));
  }
  int SelectedRow() const override { return Framework::LiveSelectedRow(); }
  bool ShowCategory(const AbilityCategoryRow& row) override {
    const bool chain = row.kind == AbilityKind::kChain;
    auto* item = Framework::SetLiveMenuItem(
        chain ? 0 : 1, row.x, row.y, row.width, row.background_width,
        MenuText(chain
                     ? Text_Menu2_battleAbilitiesMenu_Offset_chainAbilities
                     : Text_Menu2_battleAbilitiesMenu_Offset_defenseAbilities));
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
        MenuText(Text_Menu2_battleAbilitiesMenu_Offset_chainAbilities),
        MenuText(Text_Menu2_battleAbilitiesMenu_Offset_select),
        MenuText(Text_Menu2_battleAbilitiesMenu_Offset_defenseAbilities),
        MenuText(Text_Menu2_battleAbilitiesMenu_Offset_select)};
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
          Battle_actionDefinitions[actions[index]].name;
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
    Framework::ClearLiveMenuExcept(Menu_Framework_MenuItemId_None);
  }
  void FlyOutRowsAndHoistSelection(int row) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, 0);
  }
  bool MenuRowsIdle() const override {
    return Framework::AreLiveMenuRowsIdle() != 0;
  }
  void SetAbilityMessage(bool all_unlocked, std::uint8_t action) override {
    if (!all_unlocked) {
      Battle_stringContext.strings[1] = Battle_actionDefinitions[action].name;
    }
    Framework::SetLiveInformationMessage(MenuText(
        all_unlocked
            ? Text_Menu2_battleAbilitiesMenu_Offset_allAbilitiesUnlocked
            : Text_Menu2_battleAbilitiesMenu_Offset_battleAbilityUnlock));
  }
  void InitializeInformationBox() override {
    InitLiveInformationTextBox(Menu_InformationBoxHeader_Check);
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
  bool HasAbilityText() const override {
    return Menu_BattleAbilities_abilityText != nullptr;
  }
  bool CursorMemoryEnabled() const override {
    return VS::Input::settings.cursor_memory != 0;
  }
  AbilityCursor SavedCursor(AbilityMenuType type) const override {
    const auto index = static_cast<std::size_t>(type) * kPointersPerRow;
    return {static_cast<std::uint8_t>(
                VS::Menu::Framework::menuMemory.cursorMemories[index]),
            static_cast<std::uint8_t>(
                VS::Menu::Framework::menuMemory.cursorMemories[index + 1])};
  }
  std::optional<std::uint8_t> ActionAt(AbilityMenuType type,
                                       int index) const override {
    if (!InRange(Menu_BattleAbilities_unlockedChainAbilities, index)) {
      return std::nullopt;
    }
    const auto* actions = type == AbilityMenuType::kChain
                              ? Menu_BattleAbilities_unlockedChainAbilities
                              : Menu_BattleAbilities_unlockedDefenseAbilities;
    return static_cast<std::uint8_t>(actions[index]);
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
    Menu_BattleAbilities_abilityCount = LegacyByte(state.count);
    Menu_BattleAbilities_abilityMenuState = state.menu_state;
    Menu_BattleAbilities_selectedAbilityType =
        LegacyByte(static_cast<int>(state.type));
    Menu_BattleAbilities_highlightedAbility = state.cursor.highlighted;
    Menu_BattleAbilities_firstAbility = LegacyByte(state.cursor.first);
    Menu_BattleAbilities_abilityText = text.data();
  }
};

bool CopyOwnedWords(const std::uint16_t* source,
                    std::span<std::uint16_t> destination) {
  const std::array<std::span<const std::byte>, 3> owners{
      std::as_bytes(std::span(Battle_actionDefinitions)),
      std::as_bytes(std::span(Menu_BattleAbilities_battleAbilityStrings)),
      std::as_bytes(std::span(Menu_BattleAbilities_battleAbilityMenuStrings))};
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

// decomp binding: _breakArtUnlocked (src/MENU/MENU2.PRG/143C.c:27).
int LiveUpdateBreakArtUnlock(int initialize) {
  LiveProgress progress;
  if (initialize != 0) {
    const int weapon = progress.EquippedWeaponCategory();
    if (InRange(Menu_BattleAbilities_battleAbilityMenuStrings, weapon) &&
        MenuText(Menu_BattleAbilities_battleAbilityMenuStrings[weapon]) ==
            nullptr) {
      Refuse("Break Art category text outside menu table");
      return 1;
    }
  }
  LiveBreakArtHost host;
  return UpdateBreakArtUnlock(g_break_art, progress, host, initialize != 0);
}

// decomp binding: _getAbilityDescription (src/MENU/MENU2.PRG/143C.c:81).
char* LiveUnlockDescription(int ability) {
  const auto text =
      UnlockDescription(ability, Menu_BattleAbilities_battleAbilityStrings,
                        Menu_BattleAbilities_battleAbilityMenuStrings,
                        Text_Menu2_battleAbilitiesMenu_Offset_unknown1);
  if (text.empty()) {
    Refuse("ability description outside text table");
    return nullptr;
  }
  return reinterpret_cast<char*>(const_cast<std::uint16_t*>(text.data()));
}

// decomp binding: _battleAbilityUnlocked (src/MENU/MENU2.PRG/143C.c:125).
int LiveUpdateAbilityUnlock(int initialize) {
  LiveProgress progress;
  LiveAbilityHost host;
  return UpdateAbilityUnlock(g_ability_unlock, progress, host, initialize != 0);
}

// decomp binding: vs_menu2_actionUnlock (src/MENU/MENU2.PRG/143C.c:422).
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

// decomp binding: _setRiskCost (src/MENU/MENU2.PRG/64.c:15).
void LiveSetRiskCost(int action) {
  if (!InRange(Battle_actionDefinitions, action)) {
    Refuse("Risk action outside action table");
    return;
  }
  LiveRiskHost host;
  SetRiskCost(g_risk, Battle_actionDefinitions[action].cost, host);
}

// decomp binding: _initAbilityMenu (src/MENU/MENU2.PRG/64.c:64).
int LiveCreateAbilityRows(int count, int type,
                          std::span<std::uint16_t* const> menu_text) {
  if (Menu_BattleAbilities_abilityText != nullptr) return 0;
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
  if (Menu_BattleAbilities_abilityText != nullptr || menu_text == nullptr ||
      count <= 0 || count > static_cast<int>(kAbilityCapacity)) {
    return LiveCreateAbilityRows(count, type, {});
  }
  return LiveCreateAbilityRows(
      count, type,
      {menu_text, static_cast<std::size_t>(count) * kPointersPerRow});
}

void ResetLiveAbilityFoundationState() {
  g_break_art = {};
  g_ability_unlock = {};
  g_risk = {};
}
}  // namespace VS::Menu::BattleAbilities
