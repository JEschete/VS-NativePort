// Hand-maintained (tools/port/owned.csv).
#include "Menu/BreakArtsLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "Battle/ActionCostsLive.hpp"
#include "Content/StringTables.h"
#include "Core/Log.hpp"
#include "Menu/BreakArts.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/GameFontLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Menu/BreakArts.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/Framework/Menus.h"
#include "State/Progress.h"
}

namespace VS::Menu::break_arts {
namespace {
constexpr std::size_t kActionCount = 256;
constexpr std::size_t kActionNameBytes = 24;
constexpr std::size_t kAnimationCount = 11;
constexpr std::size_t kPointRequirementCategoryCount = 15;
constexpr int kMenuItemTargetPosition = 180;
State g_state{};

char ToStorageChar(std::uint8_t value) { return std::bit_cast<char>(value); }

std::optional<std::span<const std::uint8_t>> NullTerminated(
    const std::uint8_t* bytes, std::size_t size) {
  if (bytes == nullptr) {
    return std::nullopt;
  }
  const auto* const end = std::find(bytes, bytes + size, std::uint8_t{0});
  if (end == bytes + size) {
    return std::nullopt;
  }
  return std::span<const std::uint8_t>(
      bytes, static_cast<std::size_t>(end - bytes) + 1);
}

class LiveHost final : public Host {
 public:
  void Refuse(std::string_view reason) override {
    VS_LOG_WARN("menu.break_arts", "{}", reason);
  }

  std::span<const std::uint16_t> MenuStrings() override {
    return Menu_BreakArts_strings;
  }
  std::span<const std::uint16_t> BattleMenuStrings() override {
    return Menu_menuStrings;
  }
  std::uint16_t DaggerArtDescriptionIndex() const override {
    return Text_Menu1_strings_Index_daggerArt1;
  }
  std::uint16_t ViewArtsNameOffset() const override {
    return Text_Menu1_strings_Offset_viewArts;
  }
  std::uint16_t ViewArtsDescriptionOffset() const override {
    return Text_Menu1_strings_Offset_viewArtsDesc;
  }
  std::uint16_t BreakArtsHeaderIndex() const override {
    return Text_Battle_menuStrings_Index_breakArts;
  }
  std::int32_t DaggerArtOneId() const override {
    return Battle_ActionId_DaggerArtOne;
  }

  std::optional<std::uint8_t> ActionCost(int action_id) override {
    if (!ValidAction(action_id)) {
      return std::nullopt;
    }
    return Battle_actionDefinitions[action_id].cost;
  }
  std::optional<std::uint32_t> ActionRestrictionFlags(int action_id) override {
    if (!ValidAction(action_id) || Battle_characterState == nullptr) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(
        VS::Battle::LiveActorActionRestrictionFlags(0, action_id));
  }
  std::optional<bool> ActionUnlocked(int action_id) override {
    if (!ValidAction(action_id)) {
      return std::nullopt;
    }
    return Battle_actionDefinitions[action_id].unlocked != 0;
  }
  bool ActionNameAvailable(int action_id) override {
    if (!ValidAction(action_id)) {
      return false;
    }
    return std::memchr(Battle_actionDefinitions[action_id].name, '\0',
                       kActionNameBytes) != nullptr;
  }
  std::optional<int> EquippedWeaponCategory() override {
    if (Battle_characterState == nullptr) {
      return std::nullopt;
    }
    return Battle_characterState->equippedWeaponCategory;
  }
  std::optional<int> ArtsLearned(int category) override {
    if (category < 0 || static_cast<std::size_t>(category) >=
                            std::size(Audio_Seq_unusedArtsStatus.artsLearned)) {
      return std::nullopt;
    }
    return Audio_Seq_unusedArtsStatus.artsLearned[category];
  }
  std::optional<int> CategoryKills(int category) override {
    if (category < 0 ||
        static_cast<std::size_t>(category) >=
            std::size(Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills)) {
      return std::nullopt;
    }
    return Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[category];
  }
  std::optional<int> PointRequirement(int category, int art_index) override {
    if (category < 0 ||
        static_cast<std::size_t>(category) >= kPointRequirementCategoryCount ||
        art_index < 0 ||
        art_index >=
            static_cast<int>(std::size(Battle_breakArtPointRequirements[0]))) {
      return std::nullopt;
    }
    return Battle_breakArtPointRequirements[category][art_index];
  }

  std::uint8_t CursorMemory() override {
    return static_cast<std::uint8_t>(Input_settings.cursorMemory);
  }
  void SetCursorMemory(std::uint8_t value) override {
    Input_settings.cursorMemory = ToStorageChar(value);
  }
  std::uint32_t ActionRestrictions() override {
    return static_cast<std::uint32_t>(Menu_Framework_actionRestrictions);
  }
  bool ShortcutInvoked() override {
    return Menu_Framework_shortcutInvoked != 0;
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  int SelectedRow() override { return Framework::LiveSelectedRow(); }
  int HighlightedRow() override { return Framework::LiveHighlightedRow(); }
  std::span<const std::uint8_t> AnimationIndices() override {
    return {
        reinterpret_cast<const std::uint8_t*>(Menu_Framework_animationIndices),
        kAnimationCount};
  }
  std::span<const std::int8_t> RowAnimationSteps() override {
    return {
        reinterpret_cast<const std::int8_t*>(Menu_Framework_rowAnimationSteps),
        sizeof Menu_Framework_rowAnimationSteps};
  }

  bool SetMenuItem(const RowPlacement& placement, TextRef text) override {
    const auto value = Resolve(text);
    if (!value) {
      Refuse("Break Arts menu item text is unavailable or unterminated");
      return false;
    }
    Framework::MenuRow& row = Framework::SetLiveMenuItemText(
        {.row = placement.row,
         .x = placement.x,
         .y = placement.y,
         .w = placement.width,
         .background_width = placement.background_width},
        *value);
    row.state = Framework::RowState::kSlideX;
    row.target0 = kMenuItemTargetPosition;
    row.selected = 1;
    if (placement.icon >= 0) {
      row.row_icon = static_cast<std::uint8_t>(placement.icon);
    }
    return true;
  }
  bool CreateRows(int row_info, std::span<const Row> rows) override {
    if (rows.size() > kCategoryCount) {
      Refuse("Break Arts row count exceeds the live row buffer");
      return false;
    }
    std::array<char*, kCategoryCount * 2> strings{};
    std::array<int, kCategoryCount> types{};
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const auto name = Resolve(rows[i].name);
      const auto description = Resolve(rows[i].description);
      if (!name || !description) {
        Refuse("Break Arts row text is unavailable or unterminated");
        return false;
      }
      strings[i * 2] =
          reinterpret_cast<char*>(const_cast<std::uint8_t*>(name->data()));
      strings[i * 2 + 1] = reinterpret_cast<char*>(
          const_cast<std::uint8_t*>(description->data()));
      types[i] = rows[i].type;
    }
    Framework::CreateLiveMenuRows(static_cast<int>(rows.size()), row_info,
                                  strings.data(), types.data());
    return true;
  }
  void SetActionCostText(int index, std::string_view text, int x,
                         int disabled) override {
    const std::string terminated(text);
    Framework::SetLiveActionCostText(index, terminated.c_str(), x, disabled);
  }
  void DrawCompactText(std::string_view text, std::uint32_t position) override {
    const std::string terminated(text);
    Render::DrawLiveCompactText(terminated.c_str(), static_cast<int>(position),
                                nullptr);
  }
  void SetDisplayActionCost(int enabled) override {
    Menu_Framework_displayActionCost = static_cast<char>(enabled);
  }
  void ClearRowsExcept(int row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void SetCommandLabel(int command) override {
    Framework::SetLiveCommandLabel(command);
  }
  void SetActionSelection(std::uint16_t action_id) override {
    Menu_Framework_actionSelection.executeAbility.word = action_id;
  }
  void SetExecuteAbilityType(std::uint8_t type) override {
    Menu_Framework_menuState.executeAbilityType = ToStorageChar(type);
  }
  std::uint8_t CurrentMenu() override {
    return Menu_Framework_menuState.currentMenu;
  }
  void SetCurrentMenu(std::uint8_t menu) override {
    Menu_Framework_menuState.currentMenu = ToStorageChar(menu);
  }
  void SetReturnState(std::uint8_t state) override {
    Menu_Framework_menuState.returnState = ToStorageChar(state);
  }
  void SetSelectionKind(std::int16_t kind) override {
    Menu_Framework_actionSelection.selectionKind = kind;
  }

 private:
  static bool ValidAction(int action_id) {
    return action_id >= 0 && static_cast<std::size_t>(action_id) < kActionCount;
  }

  std::optional<std::span<const std::uint8_t>> Resolve(TextRef text) {
    switch (text.kind) {
      case TextKind::kMenuOffset: {
        const auto size = std::size(Menu_BreakArts_strings);
        if (text.value >= size) {
          return std::nullopt;
        }
        const auto* const bytes = reinterpret_cast<const std::uint8_t*>(
            &Menu_BreakArts_strings[text.value]);
        const std::size_t remaining =
            (size - text.value) * sizeof(Menu_BreakArts_strings[0]);
        return NullTerminated(bytes, remaining);
      }
      case TextKind::kActionName:
        if (!ValidAction(text.value)) {
          return std::nullopt;
        }
        return NullTerminated(reinterpret_cast<const std::uint8_t*>(
                                  Battle_actionDefinitions[text.value].name),
                              kActionNameBytes);
      case TextKind::kBattleMenuIndex: {
        const auto size = std::size(Menu_menuStrings);
        if (text.value >= size || Menu_menuStrings[text.value] >= size) {
          return std::nullopt;
        }
        const std::uint16_t offset = Menu_menuStrings[text.value];
        const auto* const bytes =
            reinterpret_cast<const std::uint8_t*>(&Menu_menuStrings[offset]);
        const std::size_t remaining =
            (size - offset) * sizeof(Menu_menuStrings[0]);
        return NullTerminated(bytes, remaining);
      }
    }
    return std::nullopt;
  }
};

}  // namespace

// decomp: MENU1.PRG _setHPCost (src/MENU/MENU1.PRG/30.c:13).
void SetHpCostLive(int action_id) {
  LiveHost host;
  SetHpCost(action_id, host);
}

// decomp: MENU1.PRG _setNextArtUnlockPointsRemaining
// (src/MENU/MENU1.PRG/30.c:45).
void DrawNextUnlockPointsLive(int x_offset, int weapon_category, int row) {
  LiveHost host;
  DrawNextUnlockPoints(x_offset, weapon_category, row, host);
}

// decomp: MENU1.PRG _weaponArtsMenu (src/MENU/MENU1.PRG/30.c:104).
int UpdateArtListLive(int type_cursor_memory) {
  LiveHost host;
  Context context{.state = g_state, .host = host};
  return UpdateArtList(type_cursor_memory, context);
}

// decomp: MENU1.PRG _weaponCategoriesMenu
// (src/MENU/MENU1.PRG/30.c:266).
int UpdateWeaponCategoriesLive(int initialize) {
  LiveHost host;
  Context context{.state = g_state, .host = host};
  return UpdateWeaponCategories(initialize, context);
}

// decomp: MENU1.PRG _setMenuHeader (src/MENU/MENU1.PRG/30.c:344).
void CreateHeaderLive() {
  LiveHost host;
  CreateHeader(host);
}

// decomp: MENU1.PRG vs_menu1_exec (src/MENU/MENU1.PRG/30.c:358).
int UpdateLive(u_char* step) {
  if (step == nullptr) {
    LiveHost host;
    host.Refuse("Break Arts update state pointer is null");
    return 0;
  }
  if (*step == static_cast<u_char>(ScreenStep::kInit)) {
    g_state = {};
  }
  LiveHost host;
  Context context{.state = g_state, .host = host};
  return Update(context, *step);
}

void ResetLiveStatics() { g_state = {}; }
}  // namespace VS::Menu::break_arts
