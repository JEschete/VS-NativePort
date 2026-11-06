#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/Options.hpp"
#include "Menu/OptionsLive.hpp"

extern "C" {
#include "Menu/Framework/FrameworkLiveState.h"
}

namespace {
namespace options = VS::Menu::Options;
namespace framework = VS::Menu::Framework;
using options::Step;

constexpr int kNoSelection = -1;
constexpr int kCancel = -2;
constexpr int kLeaveForBattle = -3;
constexpr int kRememberedRow = 3;
constexpr std::uint16_t kFirstTextOffset = 100;
constexpr std::size_t kTextIndexes = 60;

std::uint8_t StepByte(Step step) { return static_cast<std::uint8_t>(step); }

struct CreatedRows {
  int count = 0;
  int row_info = 0;
  std::vector<options::RowText> texts;
  std::vector<std::int32_t> row_types;
  std::uint8_t cursor_memory = 0;
};

class FakeHost final : public options::Host {
 public:
  std::vector<std::uint16_t> strings =
      std::vector<std::uint16_t>(options::kMenuStringHalfwords);
  bool rows_idle = true;
  int selected_row = kNoSelection;
  int highlighted_row = 0;
  bool puzzle_disabled = false;
  options::Settings settings{};

  std::vector<std::string> refusals;
  std::vector<CreatedRows> created;
  std::vector<int> cleared;
  std::vector<std::pair<int, int>> fly_outs;
  int dismissed = 0;
  int info_boxes = 0;
  std::vector<int> command_labels;
  std::vector<framework::InformationHeader> headers;
  std::vector<int> text_box_states;
  std::vector<options::Settings> stored;
  std::vector<bool> puzzle_modes;
  std::vector<int> mono_outputs;
  std::vector<int> vibrations;
  int battle_returns = 0;

  FakeHost() {
    for (std::size_t i = 0; i < kTextIndexes; ++i) {
      strings[i] = static_cast<std::uint16_t>(kFirstTextOffset + i);
    }
  }

  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  std::span<const std::uint16_t> MenuStrings() override { return strings; }
  bool RowsIdle() override { return rows_idle; }
  void CreateRows(int row_count, int row_info,
                  std::span<const options::RowText> texts,
                  std::span<const std::int32_t> row_types) override {
    created.push_back({.count = row_count,
                       .row_info = row_info,
                       .texts = {texts.begin(), texts.end()},
                       .row_types = {row_types.begin(), row_types.end()},
                       .cursor_memory = settings.cursor_memory});
  }
  int SelectedRow() override { return selected_row; }
  int HighlightedRow() override { return highlighted_row; }
  void ClearRowsExcept(int row) override { cleared.push_back(row); }
  void FlyOutRowsAndHoistSelection(int row, int offset) override {
    fly_outs.emplace_back(row, offset);
  }
  int RememberedRow() override { return kRememberedRow; }
  void DismissInformationBox() override { ++dismissed; }
  void InitializeInformationBox() override { ++info_boxes; }
  void SetCommandLabel(int action) override {
    command_labels.push_back(action);
  }
  void InitInformationTextBox(framework::InformationHeader header) override {
    headers.push_back(header);
  }
  void SetInformationTextBoxState(int state) override {
    text_box_states.push_back(state);
  }
  options::Settings LoadSettings() override { return settings; }
  void StoreSettings(const options::Settings& next) override {
    settings = next;
    stored.push_back(next);
  }
  bool PuzzleModeDisabled() override { return puzzle_disabled; }
  void SetPuzzleMode(bool enabled) override { puzzle_modes.push_back(enabled); }
  void SetMonoOutput(int mono) override { mono_outputs.push_back(mono); }
  void PlayVibrationPreset(int preset) override {
    vibrations.push_back(preset);
  }
  void ReturnToBattle() override { ++battle_returns; }
};

struct OptionsFixture {
  options::State state{};
  FakeHost host;
  options::Context ctx{.state = state, .host = host};
  std::uint8_t step = 0;

  int Run(Step at) {
    step = StepByte(at);
    return options::Update(ctx, step);
  }

  // Opens the choice from its settings row, lets its rows appear and picks
  // `row` (from 0).
  int Choose(Step choice, int row) {
    host.selected_row = StepByte(choice) - StepByte(Step::kSimpleMap);
    Run(Step::kHandleInput);
    const std::uint8_t opened = step;
    host.selected_row = kNoSelection;
    options::Update(ctx, step);
    host.selected_row = row;
    options::Update(ctx, step);
    host.selected_row = kNoSelection;
    const int result = options::Update(ctx, step);
    CHECK(opened == StepByte(choice));
    return result;
  }
};
}  // namespace

TEST_CASE("Options opens nine settings rows",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.puzzle_disabled = true;
  fixture.host.settings.cursor_memory = 0;
  CHECK(fixture.Run(Step::kInit) == 0);
  CHECK(fixture.step == StepByte(Step::kHandleInput));
  REQUIRE(fixture.host.created.size() == 1);
  const CreatedRows& rows = fixture.host.created[0];
  CHECK(rows.count == 9);
  CHECK(rows.row_info == 0x145);
  CHECK(rows.texts[0].name == kFirstTextOffset);
  CHECK(rows.texts[8].description == kFirstTextOffset + 17);
}

TEST_CASE("Options settings rows grey a disabled Puzzle Mode",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.puzzle_disabled = true;
  fixture.host.settings.cursor_memory = 0;
  fixture.Run(Step::kInit);
  REQUIRE(fixture.host.created.size() == 1);
  const CreatedRows& rows = fixture.host.created[0];
  CHECK(rows.texts[6].description == 999);
  CHECK(rows.row_types[6] == 1);
  CHECK(rows.row_types[5] == 0);
  // Reopening the list remembers the cursor while the rows are made.
  CHECK(rows.cursor_memory == 1);
  CHECK(fixture.host.settings.cursor_memory == 0);
}

TEST_CASE("Options waits for idle rows before its first settings list",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.rows_idle = false;
  CHECK(fixture.Run(Step::kWaitReady) == 0);
  CHECK(fixture.step == StepByte(Step::kWaitReady));
  CHECK(fixture.host.created.empty());
  fixture.host.rows_idle = true;
  CHECK(options::Update(fixture.ctx, fixture.step) == 0);
  CHECK(fixture.host.info_boxes == 1);
  CHECK(fixture.step == StepByte(Step::kHandleInput));
}

TEST_CASE("Options first settings list stores the settings it opens with",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.rows_idle = false;
  fixture.Run(Step::kWaitReady);
  fixture.host.rows_idle = true;
  options::Update(fixture.ctx, fixture.step);
  REQUIRE(fixture.host.created.size() == 1);
  CHECK(fixture.host.created[0].cursor_memory == 0);
  CHECK(fixture.host.stored.size() == 1);
  CHECK(fixture.host.created[0].row_types[6] == 0);
}

TEST_CASE("Options choice flies its settings row out",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.settings.ability_timing = true;
  fixture.host.selected_row = 1;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.step == StepByte(Step::kAbilityTiming));
  REQUIRE(fixture.host.fly_outs.size() == 1);
  CHECK(fixture.host.fly_outs[0] == std::pair{kRememberedRow, 1});
}

TEST_CASE("Options choice reads its rows and marks the current value",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.settings.ability_timing = true;
  fixture.host.selected_row = 1;
  fixture.Run(Step::kHandleInput);
  fixture.host.selected_row = kNoSelection;
  CHECK(options::Update(fixture.ctx, fixture.step) == 0);
  REQUIRE(fixture.host.created.size() == 1);
  const CreatedRows& rows = fixture.host.created[0];
  CHECK(rows.count == 2);
  CHECK(rows.row_info == 0x247);
  CHECK(rows.texts[0].name == kFirstTextOffset + 26);
  CHECK(rows.texts[1].description == kFirstTextOffset + 29);
  CHECK(rows.row_types == std::vector<std::int32_t>{4, 0});
}

TEST_CASE("Options applies a two-row choice and reopens the settings list",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.settings.ability_timing = true;
  fixture.host.settings.weapon_status = false;
  CHECK(fixture.Choose(Step::kAbilityTiming, 1) == 0);
  CHECK(fixture.step == StepByte(Step::kInit));
  CHECK_FALSE(fixture.host.settings.ability_timing);
  CHECK(fixture.host.cleared == std::vector<int>{7});
  CHECK(fixture.Choose(Step::kWeaponStatus, 0) == 0);
  CHECK(fixture.host.settings.weapon_status);
  CHECK(fixture.Choose(Step::kCursorMemory, 0) == 0);
  CHECK(fixture.host.settings.cursor_memory == 1);
  CHECK(fixture.host.vibrations.empty());
}

TEST_CASE("Options simple map choice stores the value its row selects",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.settings.simple_map = 1;
  fixture.host.selected_row = 0;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.step == StepByte(Step::kSimpleMap));
  fixture.host.selected_row = kNoSelection;
  options::Update(fixture.ctx, fixture.step);
  REQUIRE(fixture.host.created.size() == 1);
  CHECK(fixture.host.created[0].count == 3);
  CHECK(fixture.host.created[0].row_info == 0x246);
  CHECK(fixture.host.created[0].row_types ==
        std::vector<std::int32_t>{0, 4, 0});
  CHECK(fixture.Choose(Step::kSimpleMap, 2) == 0);
  CHECK(fixture.host.settings.simple_map == 0);
  CHECK(fixture.Choose(Step::kSimpleMap, 0) == 0);
  CHECK(fixture.host.settings.simple_map == 3);
}

TEST_CASE("Options sound and puzzle choices drive their outputs",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  CHECK(fixture.Choose(Step::kSound, 1) == 0);
  CHECK(fixture.host.mono_outputs == std::vector<int>{1});
  CHECK(fixture.host.settings.mono_sound == 1);
  CHECK(fixture.Choose(Step::kPuzzleMode, 1) == 0);
  CHECK(fixture.host.settings.puzzle_mode_off);
  CHECK(fixture.host.puzzle_modes == std::vector<bool>{false});
}

TEST_CASE("Options vibration and information choices drive their outputs",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  CHECK(fixture.Choose(Step::kVibration, 0) == 0);
  CHECK(fixture.host.vibrations == std::vector<int>{0});
  CHECK(fixture.host.settings.vibration_on == 1);
  fixture.host.settings.information = 1;
  CHECK(fixture.Choose(Step::kInformation, 1) == 0);
  CHECK(fixture.host.settings.information == 0);
  CHECK(fixture.host.info_boxes == 1);
}

TEST_CASE("Options cancel closes the screen back to the menu",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.selected_row = kCancel;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.step == StepByte(Step::kExitToMenu));
  CHECK(fixture.host.cleared == std::vector<int>{framework::kNoRow});
  CHECK(fixture.Run(Step::kExitToMenu) == 1);
  CHECK(fixture.host.battle_returns == 0);
}

TEST_CASE("Options battle request closes the screen once its rows are idle",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.selected_row = kLeaveForBattle;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.step == StepByte(Step::kExitToBattle));
  fixture.host.rows_idle = false;
  CHECK(options::Update(fixture.ctx, fixture.step) == 0);
  CHECK(fixture.host.battle_returns == 0);
  fixture.host.rows_idle = true;
  CHECK(options::Update(fixture.ctx, fixture.step) == 1);
  CHECK(fixture.step == StepByte(Step::kNone));
  CHECK(fixture.host.battle_returns == 1);
  CHECK(fixture.Run(Step::kExitToMenu) == 1);
  CHECK(fixture.host.battle_returns == 1);
}

TEST_CASE("Options choice leaves for the battle without applying a setting",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.settings.armor_status = true;
  fixture.host.selected_row = 3;
  fixture.Run(Step::kHandleInput);
  fixture.host.selected_row = kNoSelection;
  options::Update(fixture.ctx, fixture.step);
  fixture.host.selected_row = kLeaveForBattle;
  options::Update(fixture.ctx, fixture.step);
  CHECK(fixture.host.cleared == std::vector<int>{framework::kNoRow});
  CHECK(fixture.host.dismissed == 1);
  CHECK(fixture.host.command_labels == std::vector<int>{0});
  fixture.host.selected_row = kNoSelection;
  CHECK(options::Update(fixture.ctx, fixture.step) == 0);
  CHECK(fixture.step == StepByte(Step::kExitToBattle));
  CHECK(fixture.host.settings.armor_status);
  CHECK(fixture.host.stored.empty());
}

TEST_CASE("Options shows the caution box over a disabled Puzzle Mode row",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.host.headers.empty());
  fixture.host.puzzle_disabled = true;
  fixture.host.highlighted_row = 6;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.host.headers ==
        std::vector{framework::InformationHeader::kCaution});
  fixture.host.highlighted_row = 2;
  fixture.host.settings.information = 1;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.host.headers.back() ==
        framework::InformationHeader::kInformation);
  fixture.host.settings.information = 0;
  fixture.Run(Step::kHandleInput);
  CHECK(fixture.host.text_box_states == std::vector<int>{0xB, 0xB, 0});
  CHECK(fixture.step == StepByte(Step::kHandleInput));
}

TEST_CASE("Options refuses row texts outside its string table",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  fixture.host.strings.resize(12);
  CHECK(fixture.Run(Step::kInit) == 0);
  CHECK(fixture.step == StepByte(Step::kInit));
  CHECK(fixture.host.created.empty());
  REQUIRE(fixture.host.refusals.size() == 1);
  CHECK(fixture.host.refusals[0].find("outside the 12-halfword") !=
        std::string::npos);
  fixture.host.strings.assign(options::kMenuStringHalfwords, 0);
  fixture.host.strings[20] = 2000;
  fixture.host.selected_row = 0;
  fixture.Run(Step::kHandleInput);
  fixture.host.selected_row = kNoSelection;
  options::Update(fixture.ctx, fixture.step);
  CHECK(fixture.host.created.empty());
  CHECK(fixture.host.refusals.size() == 2);
  CHECK(fixture.step == StepByte(Step::kSimpleMap));
}

TEST_CASE("Options choice entry points share one choice state each",
          "[menu-files][menu][menu-options]") {
  OptionsFixture fixture;
  using Choice = int (*)(options::Context&, int);
  constexpr std::array<Choice, options::kChoiceCount> kChoices{
      options::UpdateSimpleMapChoice,    options::UpdateAbilityTimingChoice,
      options::UpdateWeaponStatusChoice, options::UpdateArmorStatusChoice,
      options::UpdateCursorMemoryChoice, options::UpdateInformationChoice,
      options::UpdatePuzzleModeChoice,   options::UpdateSoundChoice,
      options::UpdateVibrationChoice};
  for (std::size_t i = 0; i < kChoices.size(); ++i) {
    CAPTURE(i);
    fixture.state.choices[i].step = 2;
    CHECK(kChoices[i](fixture.ctx, 1) == 0);
    CHECK(fixture.state.choices[i].step == 0);
    CHECK(kChoices[i](fixture.ctx, 0) == 0);
    CHECK(fixture.state.choices[i].step == 1);
  }
  CHECK(fixture.host.created.size() == options::kChoiceCount);
  CHECK(fixture.host.refusals.empty());
}

TEST_CASE("Options live binding returns to the battle through the menu state",
          "[menu-files][menu][menu-options]") {
  const auto saved_state = Menu_Framework_menuState;
  Menu_Framework_MenuRow* const saved_items = Menu_Framework_menuItems;
  Menu_Framework_menuItems = nullptr;
  Menu_Framework_menuState.currentMenu = 0;
  options::ResetLiveOptionsStatics();
  std::uint8_t step = StepByte(Step::kExitToBattle);
  CHECK(options::UpdateLiveOptions(step) == 1);
  CHECK(step == StepByte(Step::kNone));
  CHECK(Menu_Framework_menuState.currentMenu == 8);
  Menu_Framework_menuState.currentMenu = 0;
  step = StepByte(Step::kExitToMenu);
  CHECK(options::UpdateLiveOptions(step) == 1);
  CHECK(step == StepByte(Step::kNone));
  CHECK(Menu_Framework_menuState.currentMenu == 0);
  CHECK(options::UpdateLiveOptions(step) == 0);
  Menu_Framework_menuItems = saved_items;
  Menu_Framework_menuState = saved_state;
}

TEST_CASE("Options retires the MENU8.PRG options module symbols",
          "[menu-files][menu][menu-options]") {
  // MENU8.PRG 88.c: _simpleMapOptionMenu .. _vibrationOptionMenu and
  // vs_menu8_exec. No module image stores them, so their entries are retired
  // with the generated code.
  constexpr auto kAddresses = std::to_array<unsigned>(
      {0x80102888, 0x80102A5C, 0x80102C0C, 0x80102DBC, 0x80102F68, 0x80103110,
       0x801032B8, 0x8010345C, 0x801035FC, 0x801037B4});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const unsigned address : kAddresses) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU8.PRG",
                                            address) == nullptr);
  }
}
