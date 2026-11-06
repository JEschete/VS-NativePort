#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Menu/Rename.hpp"
#include "Menu/RenameLive.hpp"
#include "Render/EncodedText.hpp"

namespace {
namespace rename = VS::Menu::Rename;
namespace framework = VS::Menu::Framework;
using VS::Menu::MenuSound;

constexpr std::uint8_t kSpace = VS::Render::kTextSpaceGlyph;
constexpr std::uint8_t kEnd = VS::Render::kTextTerminator;
constexpr std::uint32_t kL1 = 0x4;
constexpr std::uint32_t kR1 = 0x8;
constexpr std::uint32_t kTriangle = 0x10;
constexpr std::uint32_t kCircle = 0x20;
constexpr std::uint32_t kCross = 0x40;
constexpr std::uint32_t kSquare = 0x80;
constexpr std::uint32_t kUp = 0x1000;
constexpr std::uint32_t kDown = 0x4000;
// _renderRenameMenuStrings: the option texts' offsets and the grid.
constexpr std::size_t kOptionTextOffset = 150;
constexpr std::size_t kGridByte = 108;
constexpr std::uint8_t kGridGlyph = 0x21;
constexpr int kWeapon = 4;

constexpr std::uint32_t Xy(int x, int y) {
  return static_cast<std::uint32_t>(x) | (static_cast<std::uint32_t>(y) << 16);
}

class FakeQueue final : public VS::Render::PacketQueue {
 public:
  std::vector<std::vector<std::uint32_t>> packets;
  std::uint32_t ChainLink() const override { return 0; }
  bool Emit(std::span<const std::uint32_t> packet) override {
    packets.emplace_back(packet.begin(), packet.end());
    return true;
  }
};

class FakeHost final : public rename::Host {
 public:
  std::vector<std::uint8_t> strings =
      std::vector<std::uint8_t>(rename::kMenuStringHalfwords * 2);
  framework::Buttons buttons{};
  bool rows_idle = true;
  std::array<std::uint8_t, 16> animation_steps{0,  4,  8,  12, 16, 20,
                                               24, 28, 32, 36, 40, 44};
  std::optional<rename::Request> request;
  std::optional<rename::Weapon> weapon;
  std::optional<std::uint16_t> blade_item;
  std::vector<std::uint8_t> item_name = std::vector<std::uint8_t>(24, 0x30);

  FakeQueue queue;
  std::array<framework::MenuRow, framework::kNoRow> rows{};
  std::vector<std::string> refusals;
  std::vector<MenuSound> sounds;
  std::vector<int> effects;
  int glyphs = 0;
  std::vector<int> button_icons;
  std::vector<std::pair<int, std::uint32_t>> cursors;
  std::vector<int> main_cursors;
  std::vector<framework::RowPlacement> placements;
  std::vector<int> flown_right;
  std::vector<int> flown_left;
  int dismissed = 0;
  int info_boxes = 0;
  std::vector<std::vector<std::uint8_t>> prompt_names;
  int prompts_dismissed = 0;
  int menu_returns = 0;
  std::vector<std::pair<int, std::vector<std::uint8_t>>> stored_names;
  std::vector<int> equipped;

  FakeHost() {
    // Every index entry points at the option texts; the three options
    // follow each other.
    for (std::size_t index = 0; index < 19; ++index) {
      SetHalfword(index, kOptionTextOffset);
    }
    for (std::size_t option = 0; option < 3; ++option) {
      SetHalfword(6 + option, kOptionTextOffset + (option * 4));
      strings[(kOptionTextOffset + (option * 4)) * 2] =
          static_cast<std::uint8_t>(0x10 + option);
      strings[((kOptionTextOffset + (option * 4)) * 2) + 1] = kEnd;
    }
    std::fill_n(strings.begin() + kGridByte, 9 * 15, kGridGlyph);
  }

  void SetHalfword(std::size_t index, std::size_t value) {
    strings[index * 2] = static_cast<std::uint8_t>(value);
    strings[(index * 2) + 1] = static_cast<std::uint8_t>(value >> 8);
  }

  void Refuse(std::string_view what) override { refusals.emplace_back(what); }
  std::span<const std::uint8_t> MenuStrings() override { return strings; }
  framework::Buttons Input() override { return buttons; }
  void PlaySound(MenuSound sound) override { sounds.push_back(sound); }
  void PlaySoundEffect(int effect) override { effects.push_back(effect); }
  VS::Render::PacketQueue& Queue(framework::Layer /*layer*/) override {
    return queue;
  }
  void DrawGlyph(std::uint8_t /*glyph*/, int /*x*/, int /*y*/,
                 framework::Layer /*layers*/) override {
    ++glyphs;
  }
  void SetFontStyle(int /*style*/) override {}
  void DrawCompactText(std::string_view /*text*/, std::uint32_t /*position*/,
                       std::uint32_t /*color*/) override {}
  void DrawButtonIcon(int button, int /*x*/, int /*y*/) override {
    button_icons.push_back(button);
  }
  std::uint32_t GradientStopColor(std::uint32_t /*gradient*/,
                                  std::uint32_t /*color_index*/,
                                  int /*brightness*/) override {
    return 0;
  }
  int DrawCursor(int step, std::uint32_t position) override {
    cursors.emplace_back(step, position);
    return step + 1;
  }
  int DrawMainMenuCursor(int step, int position) override {
    main_cursors.push_back(position);
    return step + 1;
  }
  framework::MenuRow& SetMenuItem(
      const framework::RowPlacement& placement,
      std::span<const std::uint8_t> /*text*/) override {
    placements.push_back(placement);
    framework::MenuRow& row = rows.at(static_cast<std::size_t>(placement.row));
    row = {};
    return row;
  }
  framework::MenuRow& MenuItem(int row) override {
    return rows.at(static_cast<std::size_t>(row));
  }
  bool RowsIdle() override { return rows_idle; }
  void FlyOutRowRight(int row) override { flown_right.push_back(row); }
  void FlyOutRowLeft(int row) override { flown_left.push_back(row); }
  std::span<const std::uint8_t> RowAnimationSteps() override {
    return animation_steps;
  }
  void DismissInformationBox() override { ++dismissed; }
  void InitializeInformationBox() override { ++info_boxes; }
  void ShowConfirmPrompt(std::span<const std::uint8_t> name,
                         std::span<const std::uint8_t> /*prompt*/) override {
    prompt_names.emplace_back(name.begin(), name.end());
  }
  void DismissConfirmPrompt() override { ++prompts_dismissed; }
  void ReturnToMenu() override { ++menu_returns; }
  std::optional<rename::Request> RenameRequest() override { return request; }
  std::optional<rename::Weapon> LoadWeapon(int /*weapon*/) override {
    return weapon;
  }
  std::optional<std::uint16_t> BladeItem(int /*blade*/) override {
    return blade_item;
  }
  std::span<const std::uint8_t> ItemName(int /*item*/) override {
    return item_name;
  }
  void StoreWeaponName(int target,
                       std::span<const std::uint8_t> name) override {
    stored_names.emplace_back(target, std::vector(name.begin(), name.end()));
  }
  void EquipWeapon(int target) override { equipped.push_back(target); }
};

struct RenameFixture {
  rename::State state{};
  FakeHost host;
  rename::Context ctx{.state = state, .host = host};

  RenameFixture() { state.name.fill(kSpace); }

  void SetName(std::initializer_list<std::uint8_t> glyphs) {
    std::ranges::copy(glyphs, state.name.begin());
  }

  int Edit(int position, std::uint32_t repeat, std::uint32_t pressed = 0,
           std::uint32_t held = 0) {
    host.buttons = {.pressed = pressed, .repeat = repeat, .held = held};
    return rename::EditNameAtCursor(ctx, position);
  }

  // Slides the three confirmation rows in and waits for them.
  void OpenConfirmationRows() {
    host.buttons = {};
    while (state.confirmation_step < 4) {
      rename::UpdateConfirmation(ctx, false);
    }
  }

  int Press(std::uint32_t pressed, std::uint32_t repeat = 0) {
    host.buttons = {.pressed = pressed, .repeat = repeat};
    return rename::UpdateNameEditor(ctx, false);
  }

  // From the slid-away frame to the confirmation's chosen option.
  int ConfirmName(bool yes) {
    state.editor_step = rename::EditorStep::kInitConfirmation;
    state.animation_step = 10;
    state.weapon = kWeapon;
    host.buttons = {};
    rename::UpdateNameEditor(ctx, false);
    OpenConfirmationRows();
    if (!yes) {
      Press(0, kDown);
    }
    return Press(kCircle);
  }
};
}  // namespace

TEST_CASE("Rename inserts a name column at the cursor",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.SetName({0x0A, 0x0B, 0x0C, 0x0D});
  CHECK(fixture.Edit(1, kTriangle) == 1);
  CHECK(fixture.state.name[1] == kSpace);
  CHECK(fixture.state.name[2] == 0x0B);
  CHECK(fixture.state.name[4] == 0x0D);
  CHECK(fixture.state.nav_icon_pulse);
  CHECK(fixture.host.sounds == std::vector{MenuSound::kChange});
  CHECK(fixture.Edit(25, kTriangle) == 25);
  CHECK(fixture.host.refusals.size() == 1);
}

TEST_CASE("Rename deletes the name column at the cursor",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.SetName({0x0A, 0x0B, 0x0C, 0x0D});
  fixture.Edit(1, kTriangle);
  CHECK(fixture.Edit(1, kSquare) == 1);
  CHECK(fixture.state.name[1] == 0x0B);
  CHECK(fixture.state.name[3] == 0x0D);
  CHECK(fixture.state.name[19] == kSpace);
}

TEST_CASE("Rename moves the name cursor with L1 and R1",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  CHECK(fixture.Edit(5, kL1) == 4);
  CHECK(fixture.Edit(0, kL1) == 0);
  CHECK(fixture.Edit(0, kL1, kL1) == 19);
  CHECK(fixture.Edit(19, kR1) == 19);
  CHECK(fixture.Edit(19, kR1, kR1) == 0);
  CHECK(fixture.Edit(7, kR1) == 8);
  CHECK(fixture.Edit(7, kR1, 0, kL1 | kR1) == 7);
  CHECK(fixture.host.sounds.size() == 4);
}

TEST_CASE("Rename trims a name's trailing spaces and zeroes its tail",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  std::array<std::uint8_t, rename::kNameBytes> out{};
  out.fill(0x55);
  fixture.SetName({0x0A, 0x0B, kSpace, kSpace, kEnd});
  CHECK_FALSE(rename::CopyTrimmedName(fixture.ctx, out));
  CHECK(out[0] == 0x0A);
  CHECK(out[1] == 0x0B);
  CHECK(out[2] == kEnd);
  CHECK(out[3] == 0);
  CHECK(out[23] == 0);
}

TEST_CASE("Rename trims a blank name and refuses one that starts at its end",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  std::array<std::uint8_t, rename::kNameBytes> out{};
  out.fill(0x55);
  CHECK(rename::CopyTrimmedName(fixture.ctx, out));
  CHECK(out[0] == kEnd);
  fixture.state.name[0] = kEnd;
  CHECK(rename::CopyTrimmedName(fixture.ctx, out));
  CHECK(fixture.host.refusals.size() == 1);
}

TEST_CASE("Rename confirmation slides in three rows",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  CHECK(rename::UpdateConfirmation(fixture.ctx, true) == 0);
  fixture.host.rows_idle = false;
  for (int frame = 0; frame < 4; ++frame) {
    CHECK(rename::UpdateConfirmation(fixture.ctx, false) == 0);
  }
  CHECK(fixture.state.confirmation_step == 3);
  fixture.host.rows_idle = true;
  fixture.OpenConfirmationRows();
  REQUIRE(fixture.host.placements.size() == 3);
  CHECK(fixture.host.placements[2].row == 22);
  CHECK(fixture.host.placements[2].y == 160);
  CHECK(fixture.host.rows[20].state == framework::RowState::kSlideX);
  CHECK(fixture.host.rows[21].target0 == 194);
}

TEST_CASE("Rename confirmation picks an option",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  rename::UpdateConfirmation(fixture.ctx, true);
  fixture.OpenConfirmationRows();
  fixture.host.buttons = {.repeat = kUp};
  CHECK(rename::UpdateConfirmation(fixture.ctx, false) == 0);
  CHECK(fixture.state.confirmation_row == 2);
  CHECK(fixture.host.main_cursors.back() == 9);
  fixture.host.buttons = {.pressed = kCircle};
  CHECK(rename::UpdateConfirmation(fixture.ctx, false) == 3);
  CHECK(fixture.host.flown_right == std::vector{20, 21, 22});
}

TEST_CASE("Rename confirmation treats Cross as No",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  rename::UpdateConfirmation(fixture.ctx, true);
  fixture.OpenConfirmationRows();
  fixture.host.buttons = {};
  CHECK(rename::UpdateConfirmation(fixture.ctx, false) == 0);
  CHECK(fixture.host.rows[20].selected == 1);
  CHECK(fixture.host.rows[21].selected == 0);
  CHECK(fixture.host.main_cursors.back() == 7);
  fixture.host.buttons = {.repeat = kDown};
  CHECK(rename::UpdateConfirmation(fixture.ctx, false) == 0);
  CHECK(fixture.host.sounds == std::vector{MenuSound::kChange});
  fixture.host.buttons = {.pressed = kCross};
  CHECK(rename::UpdateConfirmation(fixture.ctx, false) == 2);
  CHECK(fixture.state.confirmation_row == 1);
}

TEST_CASE("Rename command highlight selects one of the rows after the name",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.rows[33].selected = 1;
  rename::SetCommandHighlight(fixture.ctx, 2);
  CHECK(fixture.state.selected_action == 3);
  CHECK(fixture.host.rows[33].selected == 0);
  CHECK(fixture.host.rows[31].selected == 0);
  rename::SetCommandHighlight(fixture.ctx, 5);
  CHECK(fixture.state.selected_action == 3);
  CHECK(fixture.host.rows[34].selected == 1);
  CHECK(fixture.host.rows[35].selected == 0);
  CHECK(fixture.host.rows[37].selected == 0);
}

TEST_CASE("Rename selection boxes emit additive overlay quads",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  rename::DrawSelectionBox(fixture.ctx, {.x = 10, .y = 20, .w = 30, .h = 40});
  REQUIRE(fixture.host.queue.packets.size() == 1);
  const std::vector<std::uint32_t>& box = fixture.host.queue.packets[0];
  CHECK(box == std::vector<std::uint32_t>{9U << VS::Render::kPacketLengthShift,
                                          0xE1000220, 0x3AC0A080, Xy(10, 20), 0,
                                          Xy(40, 20), 0, Xy(10, 60), 0xC0A080,
                                          Xy(40, 60)});
  CHECK(rename::HighlightGridCharacter(fixture.ctx, 5,
                                       {.column = 2, .row = 1}) == 6);
  REQUIRE(fixture.host.queue.packets.size() == 2);
  CHECK(fixture.host.queue.packets[1][3] == Xy(152, 92));
  CHECK(fixture.host.queue.packets[1][9] == Xy(162, 110));
  CHECK(fixture.host.cursors == std::vector{std::pair{5, Xy(136, 82)}});
}

TEST_CASE("Rename editor waits out its delay before its grid",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  CHECK(rename::UpdateNameEditor(fixture.ctx, true) == 0);
  for (int frame = 0; frame < 8; ++frame) {
    fixture.Press(0);
  }
  CHECK(fixture.state.editor_step == rename::EditorStep::kDelay);
  CHECK(fixture.host.cursors.empty());
  fixture.Press(0);
  CHECK(fixture.state.editor_step == rename::EditorStep::kGrid);
  CHECK(fixture.host.cursors.size() == 1);
  CHECK(fixture.host.queue.packets.size() == 2);
}

TEST_CASE("Rename editor types and erases grid glyphs",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  rename::UpdateNameEditor(fixture.ctx, true);
  for (int frame = 0; frame < 9; ++frame) {
    fixture.Press(0);
  }
  REQUIRE(fixture.state.editor_step == rename::EditorStep::kGrid);
  fixture.Press(kCircle);
  CHECK(fixture.state.name[0] == kGridGlyph);
  CHECK(fixture.state.position == 1);
  CHECK(fixture.host.queue.packets.back()[3] == Xy(167, 48));
  fixture.Press(0, kCross);
  CHECK(fixture.state.position == 0);
  CHECK(fixture.state.name[0] == kSpace);
}

TEST_CASE("Rename confirmation prompt shows the trimmed name",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.weapon = rename::Weapon{.blade = 1, .equipped = true};
  fixture.SetName({0x0A, 0x0B});
  CHECK(fixture.ConfirmName(true) == 1);
  REQUIRE(fixture.host.prompt_names.size() == 1);
  CHECK(fixture.host.prompt_names[0][1] == 0x0B);
  CHECK(fixture.host.prompt_names[0][2] == kEnd);
  CHECK(fixture.host.prompts_dismissed == 1);
}

TEST_CASE("Rename stores the confirmed name and re-equips the weapon",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.weapon = rename::Weapon{.blade = 1, .equipped = true};
  fixture.SetName({0x0A, 0x0B});
  CHECK(fixture.ConfirmName(true) == 1);
  CHECK(fixture.host.effects == std::vector{0xD});
  REQUIRE(fixture.host.stored_names.size() == 1);
  CHECK(fixture.host.stored_names[0].first == kWeapon);
  CHECK(fixture.host.stored_names[0].second[0] == 0x0A);
  CHECK(fixture.host.equipped == std::vector{kWeapon});
}

TEST_CASE("Rename offers the blade's name for a blank name",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.weapon = rename::Weapon{.blade = 2};
  fixture.host.blade_item = 77;
  CHECK(fixture.ConfirmName(false) == 0);
  REQUIRE(fixture.host.prompt_names.size() == 1);
  CHECK(fixture.host.prompt_names[0][0] == 0x30);
  CHECK(fixture.host.prompt_names[0][23] == 0x30);
  CHECK(fixture.state.editor_step == rename::EditorStep::kReturnToGrid);
  CHECK(fixture.state.show_interface);
  CHECK(fixture.host.sounds.back() == MenuSound::kLeave);
  CHECK(fixture.host.stored_names.empty());
}

TEST_CASE("Rename refuses to name a weapon without a blade",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.blade_item = 77;
  fixture.host.weapon = rename::Weapon{.blade = 0};
  CHECK(fixture.ConfirmName(true) == 1);
  CHECK(fixture.host.refusals.front() == "renamed weapon has no blade");
}

TEST_CASE("Rename screen loads the weapon's name",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.request = rename::Request{.edit_existing = true, .weapon = 3};
  rename::Weapon weapon{};
  weapon.name = {0x0A, kSpace, 0x0B, VS::Render::kTextSpacing,
                 0x05, 0x0C,   kEnd};
  fixture.host.weapon = weapon;
  std::uint8_t step = 0;
  CHECK(rename::Update(fixture.ctx, step) == 0);
  CHECK(step == 1);
  CHECK(fixture.state.weapon == 3);
  CHECK(fixture.state.name[0] == 0x0A);
  CHECK(fixture.state.name[1] == kSpace);
  CHECK(fixture.state.name[3] == kSpace);
  CHECK(fixture.state.name[4] == 0x0C);
  CHECK(fixture.host.queue.packets.empty());
}

TEST_CASE("Rename screen slides its frame in after loading the name",
          "[menu-files][menu][menu-rename]") {
  RenameFixture fixture;
  fixture.host.request = rename::Request{.edit_existing = true, .weapon = 3};
  rename::Weapon weapon{};
  weapon.name = {0x0A, kSpace, 0x0B, VS::Render::kTextSpacing,
                 0x05, 0x0C,   kEnd};
  fixture.host.weapon = weapon;
  std::uint8_t step = 0;
  rename::Update(fixture.ctx, step);
  REQUIRE(step == 1);
  rename::Update(fixture.ctx, step);
  CHECK(step == 2);
  CHECK(fixture.state.animation_step == 9);
  CHECK_FALSE(fixture.host.queue.packets.empty());
  CHECK(fixture.host.refusals.empty());
}

TEST_CASE("Rename live statics reset keeps the edited name",
          "[menu-files][menu][menu-rename]") {
  rename::State& live = rename::LiveRenameState();
  const rename::State saved = live;
  live.name[0] = 5;
  live.selected_action = 3;
  live.nav_icon_pulse = true;
  live.confirmation_cursor = 2;
  live.confirmed_name[0] = 9;
  live.action_counters[1] = 4;
  live.editor_step = rename::EditorStep::kGrid;
  rename::ResetLiveRenameStatics();
  CHECK(live.name[0] == 5);
  CHECK(live.selected_action == 0);
  CHECK_FALSE(live.nav_icon_pulse);
  CHECK(live.confirmation_cursor == 0);
  CHECK(live.confirmed_name[0] == 0);
  CHECK(live.action_counters[1] == 0);
  CHECK(live.editor_step == rename::EditorStep::kGrid);
  CHECK(&rename::LiveRenameState() == &live);
  live = saved;
}

TEST_CASE("Rename retires the MENU8.PRG naming module symbols",
          "[menu-files][menu][menu-rename]") {
  // MENU8.PRG 21A0.c: _insertDeleteHandler .. vs_menu8_execRename. No module
  // image stores them, so their entries are retired with the generated code.
  constexpr auto kAddresses = std::to_array<unsigned>(
      {0x801049A0, 0x80104B34, 0x80104BF0, 0x80105268, 0x80105308, 0x80105374,
       0x80105554, 0x801055B8, 0x80105F2C});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const unsigned address : kAddresses) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "MENU/MENU8.PRG",
                                            address) == nullptr);
  }
}
