#include "Menu/BattleAbilitiesTerminalLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <iterator>

#include "Audio/SfxLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.hpp"
#include "Battle/ActionMenu.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/LearningLive.hpp"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/BattleAbilitiesLiveState.hpp"
#include "Menu/BattleAbilitiesTerminal.hpp"
#include "Menu/BattleAbilitiesUnlockLive.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Sounds.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Progress.hpp"

namespace menu2_battle_abilities =
    VS::Content::text::menu2_prg::battle_abilities;

namespace VS::Menu::BattleAbilities {
namespace {
constexpr int kSetSound = 35;
constexpr int kCategoryRowInfo = 0x117;
constexpr int kPreservedCategory = 2;
constexpr int kBattleMenu = 3;
constexpr std::size_t kPointersPerRow = 2;
constexpr std::size_t kCategoryCount = 2;
constexpr std::size_t kAssignmentLayerByteOffset = 8;
constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kTriangle = 0x10;
constexpr std::uint32_t kSquare = 0x80;
constexpr std::uint32_t kCancel = 0x40;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
AssignmentState assignments;
AbilityListState chainAbilityList;
AbilityListState defenseAbilityList;
AbilityScreenAnimation screenAnimation;

void Refuse(std::string_view reason) {
  VS::Core::LogWarn("menu.battle-abilities", "Ability terminal refused: {}",
                    reason);
}

std::span<const std::uint8_t> TextBytes(std::span<const std::uint16_t> words) {
  return {reinterpret_cast<const std::uint8_t*>(words.data()),
          words.size_bytes()};
}

std::span<std::uint8_t> LearnedActions(AbilityKind kind) {
  return kind == AbilityKind::kChain ? std::span(unlockedChainAbilities)
                                     : std::span(unlockedDefenseAbilities);
}

char* MenuText(std::size_t offset) {
  if (offset >= std::size(battleAbilityStrings)) {
    Refuse("ability terminal text offset outside its table");
    return nullptr;
  }
  return reinterpret_cast<char*>(battleAbilityStrings + offset);
}

class LiveAssignmentHost final : public AssignmentHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  std::optional<std::uint8_t> LearnedAction(AbilityMenuType type,
                                            int index) const override {
    if (index < 0 || index >= static_cast<int>(kAbilityCapacity) ||
        (type != AbilityMenuType::kChain &&
         type != AbilityMenuType::kDefense)) {
      return std::nullopt;
    }
    return LearnedActions(
        type == AbilityMenuType::kChain
            ? AbilityKind::kChain
            : AbilityKind::kDefense)[static_cast<std::size_t>(index)];
  }
  std::span<std::uint8_t> Mappings(AbilityMenuType type) override {
    if (type == AbilityMenuType::kChain) {
      return {reinterpret_cast<std::uint8_t*>(
                  VS::Input::settings.mapped_chain_abilities),
              kAssignmentButtonCount};
    }
    if (type == AbilityMenuType::kDefense) {
      return {reinterpret_cast<std::uint8_t*>(
                  VS::Input::settings.mapped_defense_abilities),
              kAssignmentButtonCount};
    }
    return {};
  }
  void PlaySound(AssignmentSound sound) override {
    switch (sound) {
      case AssignmentSound::kLeave:
        PlayLeaveSound();
        break;
      case AssignmentSound::kSet:
        Audio::LivePlaySfxCentered(kMenuSoundChannelGroup, kSetSound);
        break;
      case AssignmentSound::kChange:
        PlayChangeSound();
        break;
    }
  }
  std::span<const std::uint16_t> AbilityText() const override {
    if (abilityText == nullptr) return {};
    const auto capacity = Core::Memory::LivePayloadCapacity(abilityText);
    if (!capacity) return {};
    return {abilityText, *capacity / sizeof(std::uint16_t)};
  }
  bool ShowRow(const AssignmentRow& row,
               std::span<const std::uint16_t> name) override {
    auto& item = Framework::SetLiveMenuItemText({.row = row.row,
                                                 .x = row.x,
                                                 .y = row.y,
                                                 .w = row.width,
                                                 .background_width = 0},
                                                TextBytes(name));
    if (row.entering) {
      item.state = Framework::RowState::kSlideX;
      item.target0 = static_cast<std::int16_t>(row.target_x);
    }
    item.is_scrollable = static_cast<std::uint8_t>(row.scroll);
    if (row.gradient) item.gradient_state = *row.gradient;
    return true;
  }
  std::optional<std::uint8_t> RowGradient(int row) override {
    const auto* item = Framework::LiveMenuItem(row);
    return item != nullptr ? std::optional(item->gradient_state) : std::nullopt;
  }
  bool SelectRow(int row, bool selected) override {
    auto* item = Framework::LiveMenuItem(row);
    if (item == nullptr) {
      Refuse("selected ability row is missing");
      return false;
    }
    item->selected = selected ? 1 : 0;
    return true;
  }
  void ShowDescription(std::span<const std::uint16_t> text) override {
    Framework::SetLiveInformationMessageText(TextBytes(text));
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void HideActionCost() override { VS::Menu::Framework::displayActionCost = 0; }
  void DrawButton(AssignmentButton button, int x, int y) override {
    const std::array<Framework::ButtonIcon, kAssignmentButtonCount> icons{
        Framework::ButtonIcon::kCircle, Framework::ButtonIcon::kTriangle,
        Framework::ButtonIcon::kSquare};
    auto* table = static_cast<std::byte*>(
        VS::Render::FrameScratchpad().overlay_ordering_table);
    if (table == nullptr) {
      Refuse("ability button draw has no overlay ordering table");
      return;
    }
    Framework::DrawLiveButtonIcon(
        icons[static_cast<std::size_t>(button)], x, y,
        reinterpret_cast<std::uint32_t*>(table + kAssignmentLayerByteOffset));
  }
  AssignmentInput Input() const override {
    const auto pressed = static_cast<std::uint32_t>(VS::Input::buttonsPressed);
    const auto repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat);
    return {.circle = (pressed & kCircle) != 0,
            .triangle = (pressed & kTriangle) != 0,
            .square = (pressed & kSquare) != 0,
            .cancel = (pressed & kCancel) != 0,
            .repeat_up = (repeat & kUp) != 0,
            .repeat_down = (repeat & kDown) != 0,
            .pressed_up = (pressed & kUp) != 0,
            .pressed_down = (pressed & kDown) != 0};
  }
  void ReleaseAbilityText() override {
    Core::Memory::FreeLiveFromHighEnd(abilityText);
    abilityText = nullptr;
  }
  void RememberCursor(AbilityMenuType type, AbilityCursor cursor) override {
    const auto slot = static_cast<std::size_t>(type) * kPointersPerRow;
    VS::Menu::Framework::menuMemory.cursor_memories[slot] =
        static_cast<std::uint8_t>(cursor.highlighted);
    VS::Menu::Framework::menuMemory.cursor_memories[slot + 1] =
        static_cast<std::uint8_t>(cursor.first);
  }
  int DrawCursor(int animation, int position) override {
    return Framework::DrawLiveCursor(static_cast<std::uint32_t>(animation),
                                     position);
  }
  void SetRiskCost(std::uint8_t action) override { LiveSetRiskCost(action); }
};

class LiveListHost final : public AbilityListHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  void FlyOutCategory(AbilityKind kind) override {
    Framework::FlyOutLiveRowsAndHoistSelection(
        kind == AbilityKind::kChain ? 0 : 1, 1);
  }
  bool ShortcutInvoked() const override {
    return VS::Menu::Framework::shortcutInvoked != 0;
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  std::span<const std::uint8_t> ActionOrder(AbilityKind kind) const override {
    const auto actions = kind == AbilityKind::kChain
                             ? std::span(VS::Battle::chainAbilityActionIds)
                             : std::span(VS::Battle::defenseAbilityActionIds);
    return {reinterpret_cast<const std::uint8_t*>(actions.data()),
            actions.size()};
  }
  bool ActionUnlocked(std::uint8_t action) const override {
    return VS::Battle::actionDefinitions[action].unlocked != 0;
  }
  void StoreLearnedAction(AbilityKind kind, int index,
                          std::uint8_t action) override {
    LearnedActions(kind)[static_cast<std::size_t>(index)] = action;
  }
  void CreateRows(AbilityKind kind,
                  std::span<const LearnedAbility> rows) override {
    std::array<std::uint16_t*, kLearnedAbilitySlots * kPointersPerRow> text{};
    const auto description_base =
        kind == AbilityKind::kChain
            ? menu2_battle_abilities::index::kChainAbilityEffect0
            : menu2_battle_abilities::index::kDefenseAbilityEffect0;
    for (std::size_t row = 0; row < rows.size(); ++row) {
      const auto description = static_cast<std::size_t>(description_base) +
                               rows[row].description_slot;
      const auto offset = battleAbilityStrings[description];
      if (offset >= std::size(battleAbilityStrings)) {
        Refuse("learned ability description offset outside its owner");
        return;
      }
      text[row * kPointersPerRow] = reinterpret_cast<std::uint16_t*>(
          VS::Battle::actionDefinitions[rows[row].action].name.data());
      text[row * kPointersPerRow + 1] = battleAbilityStrings + offset;
    }
    LiveCreateAbilityRows(static_cast<int>(rows.size()),
                          static_cast<int>(kind == AbilityKind::kChain
                                               ? AbilityMenuType::kChain
                                               : AbilityMenuType::kDefense),
                          text);
  }
  int UpdateAssignments() override { return LiveUpdateAssignments(); }
  void ClearAbilityRows() override {
    Framework::ClearLiveMenuExcept(kPreservedCategory);
  }
};

class LivePointsHost final : public UnlockPointsHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  int UnlockCount() const override {
    return VS::State::artsStatus.kills.battle_abilities_unlocked;
  }
  std::uint16_t TotalKills() const override {
    return VS::State::artsStatus.kills.total_kills;
  }
  std::optional<int> Requirement(int count) const override {
    return VS::Battle::LiveBattleAbilityRequirement(count);
  }
  void DrawCompactText(std::string_view text, std::uint32_t position) override {
    std::array<char, kUnlockPointsTextCapacity> terminated{};
    if (text.size() >= terminated.size()) {
      Refuse("ability points text exceeds its compact buffer");
      return;
    }
    std::ranges::copy(text, terminated.begin());
    Render::DrawLiveCompactText(terminated.data(), std::bit_cast<int>(position),
                                nullptr);
  }
};

class LiveScreenHost final : public AbilityScreenHost {
 public:
  void Refuse(std::string_view reason) override {
    BattleAbilities::Refuse(reason);
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  int Shortcut() const override {
    return static_cast<std::uint8_t>(VS::Menu::Framework::shortcutInvoked);
  }
  bool ShowShortcutCategory(const AbilityShortcutRow& row) override {
    const auto index = row.kind == AbilityKind::kChain
                           ? menu2_battle_abilities::index::kChainAbilities
                           : menu2_battle_abilities::index::kDefenseAbilities;
    const auto offset = battleAbilityStrings[index];
    if (offset >= std::size(battleAbilityStrings)) {
      Refuse("ability shortcut category text is outside its table");
      return false;
    }
    auto& item = Framework::SetLiveMenuItemText(
        {.row = row.row,
         .x = row.x,
         .y = row.y,
         .w = row.width,
         .background_width = row.background_width},
        TextBytes(std::span(battleAbilityStrings).subspan(offset)));
    item.selected = 1;
    return true;
  }
  bool HasLearnedAbility(AbilityKind kind) override {
    return Battle::LiveHasUnlockedBattleAbility(
               kind == AbilityKind::kDefense ? 1 : 0) != 0;
  }
  void ShowCategories(bool chain_available, bool defense_available,
                      bool force_cursor_memory) override {
    auto* chain_name =
        MenuText(menu2_battle_abilities::offset::kChainAbilities);
    // Original 0x801039FC shifts the absent-chain label by a byte, not its
    // type. Preserve this source quirk separately from the disabled defense
    // row.
    if (!chain_available && chain_name != nullptr) ++chain_name;
    std::array<char*, kCategoryCount * kPointersPerRow> text{
        chain_name,
        MenuText(chain_available
                     ? menu2_battle_abilities::offset::kChainAbilitiesDesc
                     : menu2_battle_abilities::offset::kNoChainAbilities),
        MenuText(menu2_battle_abilities::offset::kDefenseAbilities),
        MenuText(defense_available
                     ? menu2_battle_abilities::offset::kDefenseAbilitiesDesc
                     : menu2_battle_abilities::offset::kNoDefenseAbilities)};
    std::array<int, kCategoryCount> types{0, defense_available ? 0 : 1};
    const auto remembered = VS::Input::settings.cursor_memory;
    if (force_cursor_memory) VS::Input::settings.cursor_memory = 1;
    Framework::CreateLiveMenuRows(static_cast<int>(kCategoryCount),
                                  kCategoryRowInfo, text.data(), types.data());
    VS::Input::settings.cursor_memory = remembered;
  }
  int SelectedCategory() override { return Framework::LiveSelectedRow(); }
  int UpdateAbilityList(AbilityKind kind, bool initialize) override {
    return kind == AbilityKind::kChain
               ? LiveUpdateChainAbilityMenu(initialize ? 1 : 0)
               : LiveUpdateDefenseAbilityMenu(initialize ? 1 : 0);
  }
  void ClearMenu() override {
    Framework::ClearLiveMenuExcept(Framework::kNoRow);
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void ClearCommand() override {
    Framework::SetLiveCommandLabel(Framework::MenuCommand::kNone);
  }
  void ReturnToBattleMenu() override {
    VS::Menu::Framework::menuState.control.current_menu = kBattleMenu;
  }
  std::optional<std::uint8_t> AnimationOffset(
      std::uint8_t step) const override {
    if (step >= std::size(VS::Menu::Framework::rowAnimationSteps)) {
      return std::nullopt;
    }
    return VS::Menu::Framework::rowAnimationSteps[step];
  }
  void DrawNextUnlockPoints(int x_offset) override {
    LiveDrawNextUnlockPoints(x_offset);
  }
};
}  // namespace

// decomp binding: mapAbility (src/MENU/MENU2.PRG/64.c:175; was _mapAbility).
void LiveToggleButtonAssignment(int type, int button, int ability_index) {
  if ((type != static_cast<int>(AbilityMenuType::kChain) &&
       type != static_cast<int>(AbilityMenuType::kDefense)) ||
      button < 0 || button >= static_cast<int>(kAssignmentButtonCount)) {
    Refuse("invalid live ability type or assignment button");
    return;
  }
  LiveAssignmentHost host;
  ToggleButtonAssignment(host, static_cast<AbilityMenuType>(type),
                         static_cast<AssignmentButton>(button), ability_index);
}

// decomp binding: abilityMenu (src/MENU/MENU2.PRG/64.c:226; was _abilityMenu).
int LiveUpdateAssignments() {
  AbilityRowsState rows{
      .count = menuAbilityCount,
      .menu_state = abilityMenuState,
      .type = static_cast<AbilityMenuType>(selectedAbilityType),
      .cursor = {.highlighted = highlightedAbility, .first = firstAbility}};
  LiveAssignmentHost host;
  const int result = UpdateAssignments(rows, assignments, host);
  abilityMenuState = rows.menu_state;
  highlightedAbility = rows.cursor.highlighted;
  firstAbility = static_cast<std::uint8_t>(rows.cursor.first);
  return result;
}

// decomp binding: chainAbilityMenu (src/MENU/MENU2.PRG/64.c:501; was
// _chainAbilityMenu).
int LiveUpdateChainAbilityMenu(int initialize) {
  LiveListHost host;
  return UpdateChainAbilityMenu(chainAbilityList, host, initialize != 0);
}

// decomp binding: defenseAbilityMenu (src/MENU/MENU2.PRG/64.c:562; was
// _defenseAbilityMenu).
int LiveUpdateDefenseAbilityMenu(int initialize) {
  LiveListHost host;
  return UpdateDefenseAbilityMenu(defenseAbilityList, host, initialize != 0);
}

// decomp binding: setNextAbilityUnlockPointsRemaining
// (src/MENU/MENU2.PRG/64.c:621; was _setNextAbilityUnlockPointsRemaining).
void LiveDrawNextUnlockPoints(int x_offset) {
  LivePointsHost host;
  DrawNextUnlockPoints(x_offset, host);
}

// decomp binding: vs_menu2_exec (src/MENU/MENU2.PRG/64.c:665).
int LiveUpdateAbilityScreen(std::uint8_t* phase) {
  if (phase == nullptr) {
    Refuse("ability screen has no state byte");
    return 1;
  }
  LiveScreenHost host;
  return UpdateAbilityScreen(*phase, screenAnimation, host);
}

// vs_battle_loadMenuPrg (src/BATTLE/BATTLE.PRG/5BF94.c) reads six sectors.
// Our 0x80104EC4..D3 and 0x801050C0..EF statics have zero bytes in that image,
// including the original C declarations without explicit initializers.
void ResetLiveAbilityTerminalModuleState() {
  assignments = {};
  chainAbilityList = {};
  defenseAbilityList = {};
  screenAnimation = {};
  std::ranges::fill(LearnedActions(AbilityKind::kChain), std::uint8_t{0});
  std::ranges::fill(LearnedActions(AbilityKind::kDefense), std::uint8_t{0});
}

void ResetLiveAbilityTerminalColdState() {
  ResetLiveAbilityTerminalModuleState();
}
}  // namespace VS::Menu::BattleAbilities
