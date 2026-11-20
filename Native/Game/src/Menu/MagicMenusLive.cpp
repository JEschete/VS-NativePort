#include "Menu/MagicMenusLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "Battle/AbilitiesLive.hpp"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionCostsLive.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Content/StringTables.hpp"
#include "Core/Log.hpp"
#include "Field/ChestInteractionsLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Framework/Menus.hpp"
#include "Menu/Magic.hpp"
#include "Menu/MagicCostsLive.hpp"
#include "Menu/MagicMenus.hpp"
#include "Menu/MagicTerminal.hpp"
#include "Menu/MagicTerminalLive.hpp"
#include "Menu/Sounds.hpp"
#include "Render/Text/CompactFont.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/StateFlags.hpp"

namespace battle_menu_strings = VS::Content::text::battle_prg::menu_strings;

namespace VS::Menu::Magic {
namespace {
constexpr std::uint64_t kLogFirst = 16;
constexpr std::uint64_t kLogInterval = 1024;
constexpr std::size_t kRankSelection = 12;
constexpr std::size_t kRankCount = 13;
constexpr std::size_t kRankScroll = 14;
constexpr std::size_t kBlinkSteps = 16;
constexpr std::size_t kRowStorageBytes = Framework::kItemDescriptionBytes;
constexpr int kTitleFirstRow = 10;
constexpr int kTitleX = 320;
constexpr int kTitleY = 34;
constexpr int kTitleBackgroundWidth = 8;
std::array<SpellMenuState, kSchoolCount> g_schools{};
std::uint64_t g_refusals = 0;

void Refuse(std::string_view reason) {
  ++g_refusals;
  if (g_refusals <= kLogFirst || g_refusals % kLogInterval == 0) {
    VS::Core::LogWarn("menu.magic", "{} (refusal {})", reason, g_refusals);
  }
}

char StorageByte(std::uint8_t value) { return std::bit_cast<char>(value); }

std::optional<std::span<const std::uint8_t>> Terminated(
    std::span<const std::uint8_t> bytes) {
  const auto end = std::ranges::find(bytes, std::uint8_t{0});
  if (end == bytes.end()) {
    return std::nullopt;
  }
  return bytes.first(static_cast<std::size_t>(end - bytes.begin()) + 1);
}

std::optional<std::span<const std::uint8_t>> TableText(
    std::span<const std::uint16_t> table, std::size_t offset) {
  if (offset >= table.size()) {
    return std::nullopt;
  }
  const auto tail = table.subspan(offset);
  return Terminated(
      {reinterpret_cast<const std::uint8_t*>(tail.data()), tail.size_bytes()});
}

class LiveHost final : public MenuHost {
 public:
  void Refuse(std::string_view reason) override { Magic::Refuse(reason); }
  std::span<const std::uint16_t> MagicStrings() override {
    return magicStrings;
  }
  std::span<const std::uint8_t> SpellIds(School school) override {
    switch (school) {
      case School::kWarlock:
        return warlockSpellIds;
      case School::kShaman:
        return shamanSpellIds;
      case School::kSorcerer:
        return sorcererSpellIds;
      case School::kEnchanter:
        return enchanterSpellIds;
    }
    return {};
  }
  std::optional<bool> SpellUnlocked(int action) override {
    if (!ValidAction(action)) {
      return std::nullopt;
    }
    return Battle_actionDefinitions[action].unlocked != 0;
  }
  std::optional<std::uint32_t> SpellRestrictions(int action) override {
    if (!ValidAction(action) || Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      return std::nullopt;
    }
    return static_cast<std::uint32_t>(
        VS::Battle::LiveActorActionRestrictionFlags(0, action));
  }
  std::uint32_t ActionRestrictions() override {
    return static_cast<std::uint32_t>(VS::Menu::Framework::actionRestrictions);
  }
  std::uint8_t ShortcutInvoked() override {
    return static_cast<std::uint8_t>(VS::Menu::Framework::shortcutInvoked);
  }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  int SelectedRow() override { return Framework::LiveSelectedRow(); }
  int HighlightedRow() override { return Framework::LiveHighlightedRow(); }
  std::uint8_t CursorMemory() override {
    return static_cast<std::uint8_t>(VS::Input::settings.cursor_memory);
  }
  void SetCursorMemory(std::uint8_t value) override {
    VS::Input::settings.cursor_memory = StorageByte(value);
  }
  bool CreateRows(int style, std::span<const SpellRow> rows) override {
    if (rows.size() > AvailableSpellList::kCapacity) {
      Refuse("Magic rows exceed the native row capacity");
      return false;
    }
    std::array<char*, AvailableSpellList::kCapacity * 2> pointers{};
    std::array<int, AvailableSpellList::kCapacity> types{};
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const auto name = Resolve(rows[i].name);
      const auto description = Resolve(rows[i].description);
      if (!name || !description) {
        Refuse("Magic row text is out of bounds or unterminated");
        return false;
      }
      CopyRowText(*name, row_text_[i * 2]);
      CopyRowText(*description, row_text_[i * 2 + 1]);
      pointers[i * 2] = row_text_[i * 2].data();
      pointers[i * 2 + 1] = row_text_[i * 2 + 1].data();
      types[i] = rows[i].type;
    }
    // The framework synchronously copies 26 name / 92 description bytes into
    // its owned item rows. No pointer into this call's storage is retained.
    Framework::CreateLiveMenuRows(static_cast<int>(rows.size()), style,
                                  pointers.data(), types.data());
    return true;
  }
  void FlyOutRows(int row, int offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }
  void FlyOutRow(int row) override { Framework::FlyOutLiveRowRight(row); }
  void DisplayCost(bool enabled) override {
    VS::Menu::Framework::displayActionCost = enabled ? 1 : 0;
  }
  bool SpellCost(int action) override {
    const auto id = action < 0 ? -static_cast<std::int64_t>(action) : action;
    if (id >= static_cast<std::int64_t>(std::size(Battle_actionDefinitions)) ||
        Battle_characterState == nullptr ||
        Battle_characterState->stats == nullptr) {
      Refuse("Magic MP cost action or player is unavailable");
      return false;
    }
    SetSpellMpCostLive(action);
    return true;
  }
  void ClearRows(int except) override {
    Framework::ClearLiveMenuExcept(except);
  }
  void DismissInformation() override { Framework::DismissLiveInformationBox(); }
  void InitializeInformation() override {
    Framework::InitializeLiveInformationBox();
  }
  void SetCommand(Framework::MenuCommand command) override {
    Framework::SetLiveCommandLabel(command);
  }
  RankCursor ReadRankCursor() override {
    const auto memory = Framework::LiveMenuCursorMemory();
    return {memory[kRankSelection], memory[kRankCount], memory[kRankScroll]};
  }
  void SetRankCount(std::uint8_t count) override {
    Framework::LiveMenuCursorMemory()[kRankCount] = count;
  }
  void SetRankSelection(std::uint8_t selected) override {
    Framework::LiveMenuCursorMemory()[kRankSelection] = selected;
  }
  RankInput ReadRankInput() override {
    const auto pressed = static_cast<std::uint32_t>(VS::Input::buttonsPressed);
    const auto repeat = static_cast<std::uint32_t>(VS::Input::buttonRepeat);
    return {.quit = (pressed & PADRup) != 0,
            .back = (pressed & PADRdown) != 0,
            .confirm = (pressed & PADRright) != 0,
            .right = (repeat & PADLright) != 0,
            .left = (repeat & PADLleft) != 0};
  }
  void PlaySound(MenuSound sound) override {
    switch (sound) {
      case MenuSound::kLeave:
        PlayLeaveSound();
        break;
      case MenuSound::kSelect:
        PlaySelectSound();
        break;
      case MenuSound::kInvalid:
        PlayInvalidSound();
        break;
      case MenuSound::kChange:
        PlayChangeSound();
        break;
    }
  }
  std::optional<RankTarget> RankDrawing() override {
    auto* text =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kOverlay, 0);
    auto* icons =
        Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, -1);
    if (text == nullptr || icons == nullptr) {
      return std::nullopt;
    }
    text_queue_ = Render::LivePacketQueue(text);
    icon_queue_ = Render::LivePacketQueue(icons);
    return RankTarget{text_queue_,
                      icon_queue_,
                      {.widths = VS::Render::compactGlyphWidths,
                       .left_trim = VS::Render::compactGlyphLeftTrim,
                       .indices = VS::Render::compactGlyphIndices},
                      Bytes(VS::Menu::Framework::cursorBrightnessAnimation)
                          .first(kBlinkSteps)};
  }
  bool SchoolUnlocked(School school) override {
    return VS::Battle::LiveHasUnlockedSpellClass(static_cast<int>(school)) != 0;
  }
  std::uint8_t TeleportAvailability() override {
    return static_cast<std::uint8_t>(VS::State::stateFlags.opaque_b5);
  }
  std::span<const std::uint8_t> SavePointStates() override {
    return Bytes(VS::State::stateFlags.save_point_states);
  }
  bool AtSavePoint() override {
    return VS::Field::CurrentSavePointIdEntry() != 0;
  }
  bool SetSchoolTitle(School school, TextRef text, int width) override {
    const auto index = static_cast<std::size_t>(school);
    const auto bytes = Resolve(text);
    if (index >= kSchoolCount || !bytes) {
      Refuse("Magic school title is invalid or unterminated");
      return false;
    }
    auto& row = Framework::SetLiveMenuItemText(
        {.row = static_cast<int>(index) + kTitleFirstRow,
         .x = kTitleX,
         .y = kTitleY,
         .w = width,
         .background_width = kTitleBackgroundWidth},
        *bytes);
    row.selected = 1;
    return true;
  }
  bool CreateHeader() override {
    constexpr auto kHeader = battle_menu_strings::index::kMagic;
    if (!TableText(VS::Menu::menuStrings, VS::Menu::menuStrings[kHeader])) {
      Refuse("Magic header text is invalid or unterminated");
      return false;
    }
    CreateHeaderLive();
    return true;
  }
  std::optional<int> RunSchool(School school, unsigned int init) override {
    switch (school) {
      case School::kWarlock:
        return UpdateWarlockMenu(init, g_schools[0], *this);
      case School::kShaman:
        return UpdateShamanMenu(init, g_schools[1], *this);
      case School::kSorcerer:
        return UpdateSorcererMenu(init, g_schools[2], *this);
      case School::kEnchanter:
        return UpdateEnchanterMenu(init, g_schools.back(), *this);
    }
    Refuse("Magic school dispatch is invalid");
    return std::nullopt;
  }
  int RunTeleport(int init) override { return UpdateTeleportMenuLive(init); }
  void SetActionSelection(std::int32_t action) override {
    VS::Menu::Framework::actionSelection.execute_ability = action;
  }
  void SetAbilityType(std::uint8_t type) override {
    VS::Menu::Framework::menuState.control.execute_ability_type = type;
  }
  void SetSelectionKind(std::int16_t kind) override {
    VS::Menu::Framework::actionSelection.selection_kind = kind;
  }
  std::uint8_t CurrentMenu() override {
    return VS::Menu::Framework::menuState.control.current_menu;
  }
  void SetCurrentMenu(std::uint8_t menu) override {
    VS::Menu::Framework::menuState.control.current_menu = menu;
  }
  void SetReturnMenu(std::uint8_t menu) override {
    VS::Menu::Framework::menuState.control.return_state = menu;
  }

 private:
  template <typename Byte, std::size_t N>
  static std::span<const std::uint8_t> Bytes(const Byte (&bytes)[N]) {
    static_assert(sizeof(Byte) == 1);
    return {reinterpret_cast<const std::uint8_t*>(bytes), N};
  }
  static bool ValidAction(int action) {
    return action >= 0 && static_cast<std::size_t>(action) <
                              std::size(Battle_actionDefinitions);
  }
  std::optional<std::span<const std::uint8_t>> Resolve(TextRef text) {
    switch (text.table) {
      case TextTable::kMagic:
        return TableText(magicStrings, text.index);
      case TextTable::kTeleport:
        return TableText(teleportationStrings, text.index);
      case TextTable::kActionName:
        if (ValidAction(text.index)) {
          return Terminated(Bytes(Battle_actionDefinitions[text.index].name));
        }
        break;
    }
    return std::nullopt;
  }
  static void CopyRowText(std::span<const std::uint8_t> source,
                          std::array<char, kRowStorageBytes>& destination) {
    destination.fill(0);
    std::copy_n(source.begin(), std::min(source.size(), destination.size()),
                destination.begin());
  }

  std::array<std::array<char, kRowStorageBytes>,
             AvailableSpellList::kCapacity * 2>
      row_text_{};
  Render::LivePacketQueue text_queue_{nullptr};
  Render::LivePacketQueue icon_queue_{nullptr};
};

int RunLiveSchool(School school, unsigned int init) {
  LiveHost host;
  const auto result = host.RunSchool(school, init);
  // The legacy ABI has no error result. Refusals have already been reported;
  // hold the menu pending rather than returning a success or cancel action.
  return result ? *result : 0;
}
}  // namespace

// decomp: _warlockMagicMenu (src/MENU/MENU0.PRG/84.c:164).
int UpdateWarlockMenuLive(unsigned int init_shortcut_invoked) {
  return RunLiveSchool(School::kWarlock, init_shortcut_invoked);
}
// decomp: _shamanMagicMenu (src/MENU/MENU0.PRG/84.c:512).
int UpdateShamanMenuLive(unsigned int init_shortcut_invoked) {
  return RunLiveSchool(School::kShaman, init_shortcut_invoked);
}
// decomp: _sorcererMagicMenu (src/MENU/MENU0.PRG/84.c:613).
int UpdateSorcererMenuLive(unsigned int init_shortcut_invoked) {
  return RunLiveSchool(School::kSorcerer, init_shortcut_invoked);
}
// decomp: _enchanterMagicMenu (src/MENU/MENU0.PRG/84.c:713).
int UpdateEnchanterMenuLive(unsigned int init_shortcut_invoked) {
  return RunLiveSchool(School::kEnchanter, init_shortcut_invoked);
}
// decomp: vs_menu0_exec (src/MENU/MENU0.PRG/15AC.c:166).
int UpdateLive(std::uint8_t* state) {
  if (state == nullptr) {
    Refuse("Magic terminal state pointer is null");
    return 0;
  }
  LiveHost host;
  const auto result = Update(*state, host);
  return result ? *result : 0;
}

void ResetMagicMenusLiveState() { g_schools = {}; }
std::uint64_t RefusedMagicMenuCount() { return g_refusals; }
}  // namespace VS::Menu::Magic
