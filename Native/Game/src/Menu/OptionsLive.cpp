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
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Options.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Menu/Framework/FrameworkLiveState.h"
#include "Menu/OptionsLiveState.h"
#include "Menu/TextBox.h"
#include "State/Settings.h"
#include "State/StateFlags.h"
}

// decomp: _menuStrings (src/MENU/MENU8.PRG/88.c)
u_short Menu_Options_menuStrings[kMenuOptionsMenuStringCount];

namespace VS::Menu::Options {
namespace {
// vs_main_settings_t (main.h:163): the flag byte, then cursorMemory; the
// one-byte settings follow slotState.
constexpr std::size_t kSettingsBytes = 0x20;
constexpr std::size_t kInformationOffset = 8;
constexpr std::size_t kSimpleMapOffset = 9;
constexpr std::size_t kMonoSoundOffset = 10;
constexpr std::size_t kVibrationOnOffset = 11;
static_assert(sizeof(State_Settings) == kSettingsBytes);
static_assert(offsetof(State_Settings, cursorMemory) == 1);
static_assert(offsetof(State_Settings, information) == kInformationOffset);
static_assert(offsetof(State_Settings, simpleMap) == kSimpleMapOffset);
static_assert(offsetof(State_Settings, monoSound) == kMonoSoundOffset);
static_assert(offsetof(State_Settings, vibrationOn) == kVibrationOnOffset);
static_assert(sizeof(Menu_Options_menuStrings) ==
              kMenuStringHalfwords * sizeof(std::uint16_t));

// The flag byte's bits (the bitfields above cursorMemory).
constexpr std::uint8_t kPuzzleModeOffBit = 0x08;
constexpr std::uint8_t kAbilityTimingBit = 0x20;
constexpr std::uint8_t kWeaponStatusBit = 0x40;
constexpr std::uint8_t kArmorStatusBit = 0x80;
// The information text box, the options screen's slot in the menu memory and
// vs_battle_menuState.currentMenu's battle value.
constexpr std::size_t kInformationTextBox = 7;
constexpr std::size_t kRememberedRowSlot = 5;
constexpr char kBattleMenu = 8;

std::uint8_t FlagByte() {
  std::uint8_t flags = 0;
  std::memcpy(&flags, &Input_settings, sizeof flags);
  return flags;
}

void SetFlag(std::uint8_t& flags, std::uint8_t bit, bool on) {
  flags = static_cast<std::uint8_t>(on ? (flags | bit) : (flags & ~bit));
}

State g_state{};

class LiveHost final : public Host {
 public:
  void Refuse(std::string_view what) override {
    VS_LOG_WARN("menu.options", "{}", what);
  }
  std::span<const std::uint16_t> MenuStrings() override {
    return Menu_Options_menuStrings;
  }
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
    return static_cast<unsigned char>(
        Menu_Framework_menuMemory.opaque85[kRememberedRowSlot]);
  }
  void DismissInformationBox() override {
    Framework::DismissLiveInformationBox();
  }
  void InitializeInformationBox() override {
    Framework::InitializeLiveInformationBox();
  }
  void SetCommandLabel(int action) override {
    Framework::SetLiveCommandLabel(action);
  }
  void InitInformationTextBox(Framework::InformationHeader header) override {
    Menu_InitInformationTextBox(
        static_cast<enum Menu_InformationBoxHeader>(header));
  }
  void SetInformationTextBoxState(int state) override {
    Menu_textBoxes[kInformationTextBox].state =
        static_cast<std::int16_t>(state);
  }
  Settings LoadSettings() override {
    const std::uint8_t flags = FlagByte();
    return {
        .puzzle_mode_off = (flags & kPuzzleModeOffBit) != 0,
        .ability_timing = (flags & kAbilityTimingBit) != 0,
        .weapon_status = (flags & kWeaponStatusBit) != 0,
        .armor_status = (flags & kArmorStatusBit) != 0,
        .cursor_memory = static_cast<std::uint8_t>(Input_settings.cursorMemory),
        .information = static_cast<std::uint8_t>(Input_settings.information),
        .simple_map = static_cast<std::uint8_t>(Input_settings.simpleMap),
        .mono_sound = static_cast<std::uint8_t>(Input_settings.monoSound),
        .vibration_on = static_cast<std::uint8_t>(Input_settings.vibrationOn)};
  }
  void StoreSettings(const Settings& settings) override {
    std::uint8_t flags = FlagByte();
    SetFlag(flags, kPuzzleModeOffBit, settings.puzzle_mode_off);
    SetFlag(flags, kAbilityTimingBit, settings.ability_timing);
    SetFlag(flags, kWeaponStatusBit, settings.weapon_status);
    SetFlag(flags, kArmorStatusBit, settings.armor_status);
    std::memcpy(&Input_settings, &flags, sizeof flags);
    Input_settings.cursorMemory = static_cast<char>(settings.cursor_memory);
    Input_settings.information = static_cast<char>(settings.information);
    Input_settings.simpleMap = static_cast<char>(settings.simple_map);
    Input_settings.monoSound = static_cast<char>(settings.mono_sound);
    Input_settings.vibrationOn = static_cast<char>(settings.vibration_on);
  }
  bool PuzzleModeDisabled() override {
    return Audio_Seq_unusedStateFlags.puzzleModeDisabled != 0;
  }
  void SetPuzzleMode(bool enabled) override {
    Audio_Seq_unusedStateFlags.puzzleMode = enabled ? 1 : 0;
  }
  void SetMonoOutput(int mono) override { Audio::LiveSetMonoOutput(mono); }
  void PlayVibrationPreset(int preset) override {
    Input::PlayLiveVibrationPreset(preset);
  }
  void ReturnToBattle() override {
    Menu_Framework_menuState.currentMenu = kBattleMenu;
  }

 private:
  static char* Text(std::uint16_t offset) {
    // The row texts are byte strings inside the halfword table.
    return reinterpret_cast<char*>(&Menu_Options_menuStrings[offset]);
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
