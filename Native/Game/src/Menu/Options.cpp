// Hand-maintained (tools/port/owned.csv).
#include "Menu/Options.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <span>

namespace VS::Menu::Options {
namespace {
// A choice's rows, the _menuStrings index of its first row's name (the
// description follows it) and the row_info word its rows open with.
struct ChoiceSpec {
  int rows;
  int first_text;
  int row_info;
};
// decomp: MENU8.PRG _simpleMapOptionMenu .. _vibrationOptionMenu
// (src/MENU/MENU8.PRG/88.c:20-486), Text_Menu8_menu_Index_frame ..
// Text_Menu8_menu_Index_vibrationOn.
constexpr std::array<ChoiceSpec, kChoiceCount> kChoiceSpecs{{
    {.rows = 3, .first_text = 20, .row_info = 0x246},
    {.rows = 2, .first_text = 26, .row_info = 0x247},
    {.rows = 2, .first_text = 30, .row_info = 0x248},
    {.rows = 2, .first_text = 34, .row_info = 0x249},
    {.rows = 2, .first_text = 38, .row_info = 0x24A},
    {.rows = 2, .first_text = 42, .row_info = 0x24B},
    {.rows = 2, .first_text = 46, .row_info = 0x24C},
    {.rows = 2, .first_text = 50, .row_info = 0x24D},
    {.rows = 2, .first_text = 54, .row_info = 0x24E},
}};
// decomp: _simpleMapValues (src/MENU/MENU8.PRG/88.c): the simple map setting
// each of its rows selects.
constexpr std::array<std::uint8_t, kMaxChoiceRows> kSimpleMapValues{3, 1, 0};

// The settings list.
constexpr int kSettingsRowInfo = 0x145;
constexpr std::size_t kPuzzleModeRow = 6;
// Text_Menu8_menu_Offset_puzzleModeSettingDisabled.
constexpr std::uint16_t kPuzzleModeDisabledText = 999;
// The row type that greys a row out and the bit that marks a current value.
constexpr std::int32_t kDisabledRowType = 1;
constexpr std::int32_t kCurrentValueRowType = 4;
// The settings row a choice keeps while the others fly out.
constexpr int kChoiceTitleRow = 7;
// The selection results: -1 cancels, -2 leaves for the battle.
constexpr int kLeaveForBattle = -2;
// The information text box (Menu_textBoxes[7]) shown over a disabled row.
constexpr int kTextBoxShown = 0xB;
constexpr int kTextBoxHidden = 0;
constexpr int kNoCommand = 0;
constexpr int kVibrationTestPreset = 0;

enum ChoiceStep : std::uint8_t { kCreateRows, kAwaitSelection, kAwaitIdle };

constexpr std::size_t Index(Choice choice) {
  return static_cast<std::size_t>(choice);
}

// Reads `out.size()` rows' name and description offsets from _menuStrings,
// starting at index `first`; refuses an index or offset past the table.
bool ReadRowTexts(Context& ctx, std::size_t first, std::span<RowText> out) {
  const std::span<const std::uint16_t> strings = ctx.host.MenuStrings();
  for (std::size_t row = 0; row < out.size(); ++row) {
    const std::size_t index = first + (row * 2);
    if (index + 1 >= strings.size() || strings[index] >= strings.size() ||
        strings[index + 1] >= strings.size()) {
      ctx.host.Refuse(std::format(
          "options text index {} lies outside the {}-halfword string table",
          index, strings.size()));
      return false;
    }
    out[row] = {.name = strings[index], .description = strings[index + 1]};
  }
  return true;
}

// The row a two-row choice marks as the current setting.
int CurrentRow(Choice choice, const Settings& settings) {
  switch (choice) {
    case Choice::kAbilityTiming:
      return settings.ability_timing ? 0 : 1;
    case Choice::kWeaponStatus:
      return settings.weapon_status ? 0 : 1;
    case Choice::kArmorStatus:
      return settings.armor_status ? 0 : 1;
    case Choice::kCursorMemory:
      return 1 - settings.cursor_memory;
    case Choice::kInformation:
      return 1 - settings.information;
    case Choice::kPuzzleMode:
      return settings.puzzle_mode_off ? 1 : 0;
    case Choice::kSound:
      return settings.mono_sound;
    case Choice::kVibration:
      return 1 - settings.vibration_on;
    case Choice::kSimpleMap:
      break;
  }
  return -1;
}

void MarkCurrentRows(Context& ctx, Choice choice, int rows,
                     std::span<std::int32_t> row_types) {
  const Settings settings = ctx.host.LoadSettings();
  if (choice == Choice::kSimpleMap) {
    for (std::size_t row = 0; row < kSimpleMapValues.size(); ++row) {
      if (settings.simple_map == kSimpleMapValues[row]) {
        row_types[row] |= kCurrentValueRowType;
      }
    }
    return;
  }
  const int row = CurrentRow(choice, settings);
  if (row < 0 || row >= rows) {
    // The original indexes its stack array with the setting's value.
    ctx.host.Refuse(
        std::format("options choice {} setting row {} lies outside its {} rows",
                    Index(choice), row, rows));
    return;
  }
  row_types[static_cast<std::size_t>(row)] |= kCurrentValueRowType;
}

bool CreateChoiceRows(Context& ctx, Choice choice) {
  const ChoiceSpec& spec = kChoiceSpecs[Index(choice)];
  const auto rows = static_cast<std::size_t>(spec.rows);
  std::array<RowText, kMaxChoiceRows> texts{};
  std::array<std::int32_t, kMaxChoiceRows> row_types{};
  if (!ReadRowTexts(ctx, static_cast<std::size_t>(spec.first_text),
                    std::span(texts).first(rows))) {
    return false;
  }
  MarkCurrentRows(ctx, choice, spec.rows, row_types);
  ctx.host.CreateRows(spec.rows, spec.row_info, std::span(texts).first(rows),
                      std::span(row_types).first(rows));
  return true;
}

// decomp: MENU8.PRG _simpleMapOptionMenu (src/MENU/MENU8.PRG/88.c:20) and its
// eight siblings share this body; the vibration choice tests the motor.
int UpdateChoice(Context& ctx, Choice choice, int init) {
  ChoiceState& local = ctx.state.choices[Index(choice)];
  if (init != 0) {
    local.step = kCreateRows;
    return 0;
  }
  switch (local.step) {
    case kCreateRows:
      if (ctx.host.RowsIdle() && CreateChoiceRows(ctx, choice)) {
        local.step = kAwaitSelection;
      }
      break;
    case kAwaitSelection:
      local.selected_row = ctx.host.SelectedRow() + 1;
      if (local.selected_row == 0) {
        break;
      }
      if (local.selected_row == kLeaveForBattle) {
        ctx.host.ClearRowsExcept(Framework::kNoRow);
        ctx.host.DismissInformationBox();
        ctx.host.SetCommandLabel(kNoCommand);
      } else {
        if (choice == Choice::kVibration && local.selected_row == 1) {
          ctx.host.PlayVibrationPreset(kVibrationTestPreset);
        }
        ctx.host.ClearRowsExcept(kChoiceTitleRow);
      }
      local.step = kAwaitIdle;
      break;
    case kAwaitIdle:
      if (ctx.host.RowsIdle()) {
        return local.selected_row;
      }
      break;
    default:
      break;
  }
  return 0;
}

// decomp: vs_menu8_exec's Init state (src/MENU/MENU8.PRG/88.c:579).
void OpenSettingsRows(Context& ctx, std::uint8_t& step) {
  std::array<RowText, kChoiceCount> texts{};
  std::array<std::int32_t, kChoiceCount> row_types{};
  if (!ReadRowTexts(ctx, 0, texts)) {
    return;
  }
  if (ctx.host.PuzzleModeDisabled()) {
    texts[kPuzzleModeRow].description = kPuzzleModeDisabledText;
    row_types[kPuzzleModeRow] = kDisabledRowType;
  }
  // Reopening the list after a choice keeps the cursor on that row.
  Settings settings = ctx.host.LoadSettings();
  const std::uint8_t cursor_memory = settings.cursor_memory;
  if (step != static_cast<std::uint8_t>(Step::kWaitReady)) {
    settings.cursor_memory = 1;
    ctx.host.StoreSettings(settings);
  }
  ctx.host.CreateRows(static_cast<int>(kChoiceCount), kSettingsRowInfo, texts,
                      row_types);
  settings.cursor_memory = cursor_memory;
  ctx.host.StoreSettings(settings);
  step = static_cast<std::uint8_t>(Step::kHandleInput);
}

// decomp: vs_menu8_exec's HandleInput state (src/MENU/MENU8.PRG/88.c:605).
void HandleSettingsInput(Context& ctx, std::uint8_t& step) {
  const int selected = ctx.host.SelectedRow();
  const int result = selected + 1;
  if (result > 0) {
    ctx.host.FlyOutRowsAndHoistSelection(ctx.host.RememberedRow(), 1);
    if (static_cast<std::size_t>(selected) < kChoiceCount) {
      step = static_cast<std::uint8_t>(static_cast<int>(Step::kSimpleMap) +
                                       selected);
      UpdateChoice(ctx, static_cast<Choice>(selected), 1);
    }
    return;
  }
  if (result < 0) {
    ctx.host.ClearRowsExcept(Framework::kNoRow);
    step = static_cast<std::uint8_t>(
        result == kLeaveForBattle ? Step::kExitToBattle : Step::kExitToMenu);
    return;
  }
  if (!ctx.host.PuzzleModeDisabled()) {
    return;
  }
  if (ctx.host.HighlightedRow() == static_cast<int>(kPuzzleModeRow)) {
    ctx.host.InitInformationTextBox(Framework::InformationHeader::kCaution);
    ctx.host.SetInformationTextBoxState(kTextBoxShown);
  } else if (ctx.host.LoadSettings().information != 0) {
    ctx.host.InitInformationTextBox(Framework::InformationHeader::kInformation);
    ctx.host.SetInformationTextBoxState(kTextBoxShown);
  } else {
    ctx.host.SetInformationTextBoxState(kTextBoxHidden);
  }
}

// The chosen row of a two-row choice: the first row turns the setting on.
std::uint8_t OnForFirstRow(int result) {
  return static_cast<std::uint8_t>(2 - result);
}

// decomp: vs_menu8_exec's choice states (src/MENU/MENU8.PRG/88.c:671-805).
void ApplySetting(Context& ctx, Choice choice, int result) {
  Settings settings = ctx.host.LoadSettings();
  switch (choice) {
    case Choice::kSimpleMap:
      if (static_cast<std::size_t>(result - 1) >= kSimpleMapValues.size()) {
        // The original reads past the three-byte _simpleMapValues.
        ctx.host.Refuse(std::format(
            "options simple map row {} lies outside its 3 values", result));
        return;
      }
      settings.simple_map =
          kSimpleMapValues[static_cast<std::size_t>(result - 1)];
      break;
    case Choice::kAbilityTiming:
      settings.ability_timing = (OnForFirstRow(result) & 1U) != 0;
      break;
    case Choice::kWeaponStatus:
      settings.weapon_status = (OnForFirstRow(result) & 1U) != 0;
      break;
    case Choice::kArmorStatus:
      settings.armor_status = (OnForFirstRow(result) & 1U) != 0;
      break;
    case Choice::kCursorMemory:
      settings.cursor_memory = OnForFirstRow(result);
      break;
    case Choice::kInformation:
      settings.information = OnForFirstRow(result);
      ctx.host.StoreSettings(settings);
      ctx.host.InitializeInformationBox();
      return;
    case Choice::kPuzzleMode:
      settings.puzzle_mode_off = ((result - 1) & 1) != 0;
      ctx.host.StoreSettings(settings);
      ctx.host.SetPuzzleMode(!settings.puzzle_mode_off);
      return;
    case Choice::kSound:
      settings.mono_sound = static_cast<std::uint8_t>(result - 1);
      ctx.host.StoreSettings(settings);
      ctx.host.SetMonoOutput(settings.mono_sound);
      return;
    case Choice::kVibration:
      settings.vibration_on = OnForFirstRow(result);
      break;
  }
  ctx.host.StoreSettings(settings);
}

void RunChoice(Context& ctx, std::uint8_t& step) {
  const auto choice =
      static_cast<Choice>(step - static_cast<std::uint8_t>(Step::kSimpleMap));
  const int result = UpdateChoice(ctx, choice, 0);
  if (result == 0) {
    return;
  }
  if (result == kLeaveForBattle) {
    step = static_cast<std::uint8_t>(Step::kExitToBattle);
    return;
  }
  if (result > 0) {
    ApplySetting(ctx, choice, result);
  }
  step = static_cast<std::uint8_t>(Step::kInit);
}

// decomp: vs_menu8_exec's ExitToMenu and ExitToBattle states
// (src/MENU/MENU8.PRG/88.c:806-822).
int CloseScreen(Context& ctx, std::uint8_t& step, bool to_battle) {
  ctx.host.DismissInformationBox();
  ctx.host.SetCommandLabel(kNoCommand);
  if (!ctx.host.RowsIdle()) {
    return 0;
  }
  if (to_battle) {
    ctx.host.ReturnToBattle();
  }
  step = static_cast<std::uint8_t>(Step::kNone);
  return 1;
}
}  // namespace

// decomp: MENU8.PRG _simpleMapOptionMenu (src/MENU/MENU8.PRG/88.c:20)
int UpdateSimpleMapChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kSimpleMap, init);
}

// decomp: MENU8.PRG _abilityTimingOptionMenu (src/MENU/MENU8.PRG/88.c:80)
int UpdateAbilityTimingChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kAbilityTiming, init);
}

// decomp: MENU8.PRG _weaponStatusOptionMenu (src/MENU/MENU8.PRG/88.c:138)
int UpdateWeaponStatusChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kWeaponStatus, init);
}

// decomp: MENU8.PRG _armorStatusOptionMenu (src/MENU/MENU8.PRG/88.c:196)
int UpdateArmorStatusChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kArmorStatus, init);
}

// decomp: MENU8.PRG _cursorMemoryOptionMenu (src/MENU/MENU8.PRG/88.c:255)
int UpdateCursorMemoryChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kCursorMemory, init);
}

// decomp: MENU8.PRG _informationOptionMenu (src/MENU/MENU8.PRG/88.c:313)
int UpdateInformationChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kInformation, init);
}

// decomp: MENU8.PRG _puzzleModeOptionMenu (src/MENU/MENU8.PRG/88.c:370)
int UpdatePuzzleModeChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kPuzzleMode, init);
}

// decomp: MENU8.PRG _soundOptionMenu (src/MENU/MENU8.PRG/88.c:429)
int UpdateSoundChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kSound, init);
}

// decomp: MENU8.PRG _vibrationOptionMenu (src/MENU/MENU8.PRG/88.c:486)
int UpdateVibrationChoice(Context& ctx, int init) {
  return UpdateChoice(ctx, Choice::kVibration, init);
}

// decomp: MENU8.PRG vs_menu8_exec (src/MENU/MENU8.PRG/88.c:551)
int Update(Context& ctx, std::uint8_t& step) {
  switch (static_cast<Step>(step)) {
    case Step::kWaitReady:
      if (!ctx.host.RowsIdle()) {
        break;
      }
      ctx.host.InitializeInformationBox();
      [[fallthrough]];
    case Step::kInit:
      OpenSettingsRows(ctx, step);
      break;
    case Step::kHandleInput:
      HandleSettingsInput(ctx, step);
      break;
    case Step::kSimpleMap:
    case Step::kAbilityTiming:
    case Step::kWeaponStatus:
    case Step::kArmorStatus:
    case Step::kCursorMemory:
    case Step::kInformation:
    case Step::kPuzzleMode:
    case Step::kSound:
    case Step::kVibration:
      RunChoice(ctx, step);
      break;
    case Step::kExitToMenu:
      return CloseScreen(ctx, step, false);
    case Step::kExitToBattle:
      return CloseScreen(ctx, step, true);
    case Step::kNone:
      break;
  }
  return 0;
}
}  // namespace VS::Menu::Options
