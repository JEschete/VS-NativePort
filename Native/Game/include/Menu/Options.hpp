#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Menu/Framework/Framework.hpp"

// MENU8.PRG's options screen (88.c): the nine-row settings list and the
// choice list each row opens. Every function works on a Context: the
// screen's State and a Host (OptionsLive.hpp binds it to the menu framework,
// vs_main_settings, the state flags, the sound output and the vibration).
namespace VS::Menu::Options {
inline constexpr std::size_t kChoiceCount = 9;
// _menuStrings (88.c): halfword offsets, then the strings.
inline constexpr std::size_t kMenuStringHalfwords = 1070;
// The rows the longest choice (the simple map) offers.
inline constexpr std::size_t kMaxChoiceRows = 3;

// The settings rows in their list order; each opens its choice.
enum class Choice : std::uint8_t {
  kSimpleMap,
  kAbilityTiming,
  kWeaponStatus,
  kArmorStatus,
  kCursorMemory,
  kInformation,
  kPuzzleMode,
  kSound,
  kVibration,
};

// vs_menu8_exec's states (88.c:551); the framework opens the screen at
// kWaitReady or kInit and the choices run at kSimpleMap + Choice.
enum class Step : std::uint8_t {
  kNone = 0,
  kWaitReady = 3,
  kInit,
  kHandleInput,
  kSimpleMap,
  kAbilityTiming,
  kWeaponStatus,
  kArmorStatus,
  kCursorMemory,
  kInformation,
  kPuzzleMode,
  kSound,
  kVibration,
  kExitToMenu,
  kExitToBattle,
};

// The vs_main_settings_t fields the screen reads and writes. The four flags
// are bits 3, 5, 6 and 7 of the record's first byte.
struct Settings {
  bool puzzle_mode_off = false;
  bool ability_timing = false;
  bool weapon_status = false;
  bool armor_status = false;
  std::uint8_t cursor_memory = 0;
  std::uint8_t information = 0;
  std::uint8_t simple_map = 0;
  std::uint8_t mono_sound = 0;
  std::uint8_t vibration_on = 0;
};

// One choice's local statics (_simpleMapOptionMenu and its siblings).
struct ChoiceState {
  std::int32_t step = 0;
  std::int32_t selected_row = 0;
};

struct State {
  std::array<ChoiceState, kChoiceCount> choices{};
};

// A row's name and description as halfword offsets into _menuStrings.
struct RowText {
  std::uint16_t name = 0;
  std::uint16_t description = 0;
};

class Host {
 public:
  virtual ~Host() = default;
  virtual void Refuse(std::string_view what) = 0;
  // _menuStrings.
  [[nodiscard]] virtual std::span<const std::uint16_t> MenuStrings() = 0;
  [[nodiscard]] virtual bool RowsIdle() = 0;
  virtual void CreateRows(int row_count, int row_info,
                          std::span<const RowText> texts,
                          std::span<const std::int32_t> row_types) = 0;
  [[nodiscard]] virtual int SelectedRow() = 0;
  [[nodiscard]] virtual int HighlightedRow() = 0;
  virtual void ClearRowsExcept(int row) = 0;
  virtual void FlyOutRowsAndHoistSelection(int row, int offset) = 0;
  // The options screen's remembered cursor row in the menu memory.
  [[nodiscard]] virtual int RememberedRow() = 0;
  virtual void DismissInformationBox() = 0;
  virtual void InitializeInformationBox() = 0;
  virtual void SetCommandLabel(int action) = 0;
  virtual void InitInformationTextBox(Framework::InformationHeader header) = 0;
  virtual void SetInformationTextBoxState(int state) = 0;
  [[nodiscard]] virtual Settings LoadSettings() = 0;
  virtual void StoreSettings(const Settings& settings) = 0;
  [[nodiscard]] virtual bool PuzzleModeDisabled() = 0;
  virtual void SetPuzzleMode(bool enabled) = 0;
  virtual void SetMonoOutput(int mono) = 0;
  virtual void PlayVibrationPreset(int preset) = 0;
  // vs_battle_menuState.currentMenu = 8: leave the menus for the battle.
  virtual void ReturnToBattle() = 0;
};

struct Context {
  State& state;
  Host& host;
};

// MENU8.PRG _simpleMapOptionMenu .. _vibrationOptionMenu (88.c:20-486): the
// choice under a settings row. 0 while it runs, then the chosen row from 1,
// -2 to leave for the battle or -1 to go back.
int UpdateSimpleMapChoice(Context& ctx, int init);
int UpdateAbilityTimingChoice(Context& ctx, int init);
int UpdateWeaponStatusChoice(Context& ctx, int init);
int UpdateArmorStatusChoice(Context& ctx, int init);
int UpdateCursorMemoryChoice(Context& ctx, int init);
int UpdateInformationChoice(Context& ctx, int init);
int UpdatePuzzleModeChoice(Context& ctx, int init);
int UpdateSoundChoice(Context& ctx, int init);
int UpdateVibrationChoice(Context& ctx, int init);

// MENU8.PRG vs_menu8_exec (88.c:551): one frame of the options screen; 1
// once it closes.
int Update(Context& ctx, std::uint8_t& step);
}  // namespace VS::Menu::Options
