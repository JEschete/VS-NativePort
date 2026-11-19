// Hand-maintained (tools/port/owned.csv).
#include "Menu/OptionsLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Audio/SoundBanksLive.hpp"
#include "Core/Log.hpp"
#include "Input/ControllerLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Input/Settings.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/FrameworkLiveState.hpp"
#include "Menu/Options.hpp"
#include "Menu/OptionsLiveState.hpp"
#include "Menu/TextBox.h"
#include "Menu/TextBoxStateLive.hpp"
#include "State/StateFlags.hpp"

// decomp: _menuStrings (src/MENU/MENU8.PRG/88.c)
u_short VS::Menu::Options::menuStrings[VS::Menu::Options::kMenuStringCount];

namespace VS::Menu::Options {
namespace {
static_assert(sizeof(menuStrings) ==
              kMenuStringHalfwords * sizeof(std::uint16_t));

// The bits of Settings' first byte: mode_flags bit 3 and the display flags.
constexpr std::uint8_t kPuzzleModeOffBit = 0x08;
constexpr std::uint8_t kAbilityTimingBit = 0x20;
constexpr std::uint8_t kWeaponStatusBit = 0x40;
constexpr std::uint8_t kArmorStatusBit = 0x80;
// The information text box and vs_battle_menuState.currentMenu's battle value.
constexpr std::size_t kInformationTextBox = 7;
constexpr char kBattleMenu = 8;

std::uint8_t FlagByte() {
  std::uint8_t flags = 0;
  std::memcpy(&flags, &VS::Input::settings, sizeof flags);
  return flags;
}

void SetFlag(std::uint8_t& flags, std::uint8_t bit, bool on) {
  flags = static_cast<std::uint8_t>(on ? (flags | bit) : (flags & ~bit));
}

State g_state{};

class LiveHost final : public Host {
 public:
  void Refuse(std::string_view what) override {
    VS::Core::LogWarn("menu.options", "{}", what);
  }
  std::span<const std::uint16_t> MenuStrings() override { return menuStrings; }
  bool RowsIdle() override { return Framework::AreLiveMenuRowsIdle() != 0; }
  void CreateRows(int row_count, int row_info, std::span<const RowText> texts,
                  std::span<const std::int32_t> row_types) override {
    std::array<char*, kChoiceCount * 2> strings{};
    std::array<int, kChoiceCount> types{};
    for (std::size_t row = 0; row < texts.size() && row < types.size(); ++row) {
      strings[row * 2] = Text(texts[row].name);
      strings[(row * 2) + 1] = Text(texts[row].description);
      types[row] = row_types[row];
    }
    Framework::CreateLiveMenuRows(row_count, row_info, strings.data(),
                                  types.data());
  }
  int SelectedRow() override { return Framework::LiveSelectedRow(); }
  int HighlightedRow() override { return Framework::LiveHighlightedRow(); }
  void ClearRowsExcept(int row) override {
    Framework::ClearLiveMenuExcept(row);
  }
  void FlyOutRowsAndHoistSelection(int row, int offset) override {
    Framework::FlyOutLiveRowsAndHoistSelection(row, offset);
  }
  int RememberedRow() override {
    return VS::Menu::Framework::menuMemory.options_row;
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void SetCommandLabel(Framework::MenuCommand action) override {
    Framework::SetLiveCommandLabel(action);
  }
  void InitInformationTextBox(Framework::InformationHeader header) override {
    VS::Menu::InitLiveInformationTextBox(
        static_cast<enum Menu_InformationBoxHeader>(header));
  }
  void SetInformationTextBoxState(int state) override {
    Menu_textBoxes[kInformationTextBox].state =
        static_cast<std::int16_t>(state);
  }
  Settings LoadSettings() override {
    const std::uint8_t flags = FlagByte();
    return {.puzzle_mode_off = (flags & kPuzzleModeOffBit) != 0,
            .ability_timing = (flags & kAbilityTimingBit) != 0,
            .weapon_status = (flags & kWeaponStatusBit) != 0,
            .armor_status = (flags & kArmorStatusBit) != 0,
            .cursor_memory = VS::Input::settings.cursor_memory,
            .information = VS::Input::settings.information,
            .simple_map = VS::Input::settings.simple_map,
            .mono_sound = VS::Input::settings.mono_sound,
            .vibration_on = VS::Input::settings.vibration_on};
  }
  void StoreSettings(const Settings& settings) override {
    std::uint8_t flags = FlagByte();
    SetFlag(flags, kPuzzleModeOffBit, settings.puzzle_mode_off);
    SetFlag(flags, kAbilityTimingBit, settings.ability_timing);
    SetFlag(flags, kWeaponStatusBit, settings.weapon_status);
    SetFlag(flags, kArmorStatusBit, settings.armor_status);
    std::memcpy(&VS::Input::settings, &flags, sizeof flags);
    VS::Input::settings.cursor_memory = settings.cursor_memory;
    VS::Input::settings.information = settings.information;
    VS::Input::settings.simple_map = settings.simple_map;
    VS::Input::settings.mono_sound = settings.mono_sound;
    VS::Input::settings.vibration_on = settings.vibration_on;
  }
  bool PuzzleModeDisabled() override {
    return VS::State::stateFlags.puzzle_mode_disabled != 0;
  }
  void SetPuzzleMode(bool enabled) override {
    VS::State::stateFlags.puzzle_mode = enabled ? 1 : 0;
  }
  void SetMonoOutput(int mono) override { Audio::LiveSetMonoOutput(mono); }
  void PlayVibrationPreset(int preset) override {
    Input::PlayLiveVibrationPreset(preset);
  }
  void ReturnToBattle() override {
    VS::Menu::Framework::menuState.control.current_menu = kBattleMenu;
  }

 private:
  static char* Text(std::uint16_t offset) {
    // The row texts are byte strings inside the halfword table.
    return reinterpret_cast<char*>(&menuStrings[offset]);
  }
};
}  // namespace

int UpdateLiveOptions(std::uint8_t& step) {
  LiveHost host;
  Context ctx{.state = g_state, .host = host};
  return Update(ctx, step);
}

void ResetLiveOptionsStatics() { g_state = {}; }
}  // namespace VS::Menu::Options
