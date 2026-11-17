#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/MagicMenusLive.hpp"
#include "Menu/MagicMenusTestWorld.hpp"
#include "Menu/MagicTerminal.hpp"
#include "Menu/MagicTerminalLive.hpp"

namespace {
namespace magic = VS::Menu::Magic;
using magic::SpellMenuState;
using magic::SpellMenuStep;
using magic::test::World;
using Entry = std::optional<int> (*)(unsigned int, SpellMenuState&,
                                     magic::MenuHost&);
constexpr std::array<Entry, 4> kEntries = {
    magic::UpdateWarlockMenu,
    magic::UpdateShamanMenu,
    magic::UpdateSorcererMenu,
    magic::UpdateEnchanterMenu,
};

SpellMenuState InputState(std::uint8_t action = 140) {
  SpellMenuState state;
  state.step = SpellMenuStep::kInput;
  state.available.count = 1;
  state.available.spells[0] = {0, action};
  return state;
}
SpellMenuState RankState() {
  SpellMenuState state;
  state.step = SpellMenuStep::kRanks;
  state.selection = 106;
  return state;
}
}  // namespace

TEST_CASE("F02604 reinitialization preserves unmodified school statics",
          "[magic-terminal][F02604]") {
  World world;
  auto state = RankState();
  state.blink = 15;
  CHECK(magic::UpdateWarlockMenu(2, state, world) == 0);
  CHECK(state.step == SpellMenuStep::kInit);
  CHECK(state.remember_cursor);
  CHECK(state.selection == 106);
  CHECK(state.blink == 15);
  CHECK((world.calls == std::vector<std::vector<std::int64_t>>{{1, 0, 1}}));
}

TEST_CASE("F02604 restricted partial rows keep original text and action order",
          "[magic-terminal][F02604]") {
  World world;
  SpellMenuState state;
  world.unlocked.fill(false);
  world.unlocked[world.ids[1]] = true;
  world.unlocked[world.ids[17]] = true;
  world.action_restrictions = 0x80;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  REQUIRE(world.rows.size() == 2);
  CHECK(world.rows[0].name.index == world.strings[63]);
  CHECK(world.rows[1].description.index == world.strings[29]);
  CHECK(world.rows[0].type == 1);
  CHECK(world.rows[1].type == 1);
  CHECK(state.available.spells[1].action_id == world.ids[17]);
}

TEST_CASE(
    "F02605 partial Shaman rows use action names and populated restrictions",
    "[magic-terminal][F02605]") {
  World world;
  SpellMenuState state;
  world.unlocked.fill(false);
  world.unlocked[world.ids[5]] = true;
  world.action_restrictions = 0xB7;
  CHECK(magic::UpdateShamanMenu(0, state, world) == 0);
  REQUIRE(world.rows.size() == 1);
  CHECK(world.style == 0x208);
  CHECK(world.rows[0].name.table == magic::TextTable::kActionName);
  CHECK(world.rows[0].name.index == world.ids[5]);
  CHECK(world.rows[0].description.index == world.strings[35]);
  CHECK(world.rows[0].type == 1);
}

TEST_CASE(
    "F02606 Sorcerer rows combine action flags without reserved global bits",
    "[magic-terminal][F02606]") {
  World world;
  SpellMenuState state;
  world.action_restrictions = 0x48;
  world.restrictions[world.ids[17]] = 0x80000000U;
  CHECK(magic::UpdateSorcererMenu(0, state, world) == 0);
  REQUIRE(world.rows.size() == 18);
  CHECK(world.style == 0x209);
  CHECK(world.rows.front().type == 0);
  CHECK(world.rows.back().type == 1);
  CHECK(world.rows.back().description.index == world.strings[53]);
}

TEST_CASE(
    "F02606 partial Sorcerer population applies global restrictions only to "
    "visible rows",
    "[magic-terminal][F02606]") {
  World world;
  SpellMenuState state;
  world.unlocked.fill(false);
  world.unlocked[world.ids[17]] = true;
  world.action_restrictions = 0xB7;
  CHECK(magic::UpdateSorcererMenu(0, state, world) == 0);
  REQUIRE(world.rows.size() == 1);
  CHECK(world.rows.front().type == 1);
  CHECK(world.rows.front().description.index == world.strings[53]);
  CHECK(state.available.count == 1);
  CHECK(state.available.spells.front().action_id == world.ids[17]);
}

TEST_CASE(
    "F02607 restricted Enchanter lists handle partial and empty populations",
    "[magic-terminal][F02607]") {
  World world;
  SpellMenuState state;
  world.unlocked.fill(false);
  const bool any = GENERATE(false, true);
  world.unlocked[world.ids[7]] = any;
  world.action_restrictions = 0xB7;
  CHECK(magic::UpdateEnchanterMenu(0, state, world) == 0);
  CHECK(world.rows.size() == (any ? 1U : 0U));
  CHECK(world.style == 0x20A);
  if (any) {
    CHECK(world.rows.front().type == 1);
    CHECK(world.rows.front().description.index == world.strings[61]);
  } else {
    CHECK_FALSE(magic::UpdateEnchanterMenu(0, state, world));
    CHECK(world.refusals.size() == 1);
  }
}

TEST_CASE("Magic schools gate animation and restore an unsigned cursor setting",
          "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  World world;
  SpellMenuState state;
  world.idle = false;
  world.cursor_setting = 255;
  CHECK(kEntries[index](2, state, world) == 0);
  CHECK(kEntries[index](0, state, world) == 0);
  CHECK(state.step == SpellMenuStep::kInit);
  world.shortcut = 255;
  CHECK(kEntries[index](0, state, world) == 0);
  CHECK(world.rows.size() == magic::kSpellCounts[index]);
  CHECK(world.cursor_at_create == 1);
  CHECK(world.cursor_setting == 255);
}

TEST_CASE("Magic schools refuse missing sources without publishing a list",
          "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  const auto failure = GENERATE(0, 1, 2, 3);
  World world;
  SpellMenuState state;
  world.id_count = failure == 0 ? 0 : world.ids.size();
  world.string_count = failure == 1 ? 0 : world.strings.size();
  world.refuse_action = failure == 2;
  world.refuse_flags = failure == 3;
  CHECK_FALSE(kEntries[index](0, state, world));
  CHECK(state.step == SpellMenuStep::kInit);
  CHECK(world.rows.empty());
  CHECK(world.refusals.size() == 1);
}

TEST_CASE("Magic schools restore cursor on a refused row publication",
          "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  World world;
  SpellMenuState state;
  state.remember_cursor = true;
  world.cursor_setting = 254;
  world.refuse_rows = true;
  CHECK_FALSE(kEntries[index](0, state, world));
  CHECK(world.cursor_setting == 254);
  CHECK(state.step == SpellMenuStep::kInit);
  CHECK(state.available.count == 0);
}

TEST_CASE(
    "Magic schools select unsigned high action IDs and wait before returning",
    "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  World world;
  auto state = InputState(255);
  world.selected_row = 0;
  CHECK(kEntries[index](0, state, world) == 0);
  CHECK(state.selection == 255);
  CHECK_FALSE(world.display_cost);
  world.idle = false;
  CHECK(kEntries[index](0, state, world) == 0);
  world.idle = true;
  CHECK(kEntries[index](0, state, world) == 255);
}

TEST_CASE("Magic schools preserve back and shortcut quit sentinels",
          "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  const bool shortcut = GENERATE(false, true);
  World world;
  auto state = InputState();
  world.selected_row = -2;
  world.shortcut = shortcut ? 255 : 0;
  CHECK(kEntries[index](0, state, world) == 0);
  CHECK(state.selection == (shortcut ? -2 : -1));
  CHECK(state.step == SpellMenuStep::kReturn);
  CHECK((world.calls[1] == std::vector<std::int64_t>{7, shortcut ? 40 : 0}));
  CHECK(world.calls.size() == (shortcut ? 4U : 2U));
}

TEST_CASE("Magic schools wrap selected-row addition before the signed branch",
          "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  World world;
  auto state = InputState();
  world.selected_row = std::numeric_limits<int>::max();
  CHECK(kEntries[index](0, state, world) == 0);
  CHECK(state.selection == std::numeric_limits<int>::min());
  CHECK(state.step == SpellMenuStep::kReturn);
  CHECK(kEntries[index](0, state, world) == std::numeric_limits<int>::min());
}

TEST_CASE(
    "Magic schools refuse stale selections and highlights outside populated "
    "rows",
    "[magic-terminal][F02604][F02605][F02606][F02607]") {
  const auto index = GENERATE(0U, 1U, 2U, 3U);
  const bool highlight = GENERATE(false, true);
  World world;
  auto state = InputState();
  world.selected_row = highlight ? -1 : 1;
  world.highlighted_row = -1;
  CHECK_FALSE(kEntries[index](0, state, world));
  CHECK(state.step == SpellMenuStep::kInput);
  CHECK(world.display_cost);
  CHECK(world.refusals.size() == 1);
}

TEST_CASE(
    "F02604 ranked spell preview and selection preserve variable cost flow",
    "[magic-terminal][F02604]") {
  World world;
  auto state = InputState(130);
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK((world.calls.back() == std::vector<std::int64_t>{6, -130}));
  world.cursor.scroll = 255;
  world.selected_row = 0;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(state.selection == 130);
  CHECK(state.step == SpellMenuStep::kInitRanks);
  CHECK(world.display_cost);
  CHECK((world.calls.back() == std::vector<std::int64_t>{1, 255, 2}));
}

TEST_CASE(
    "F02604 rank initialization clamps unsigned cursor to contiguous learned "
    "ranks",
    "[magic-terminal][F02604]") {
  const int count = GENERATE(2, 3, 4);
  World world;
  auto state = RankState();
  state.step = SpellMenuStep::kInitRanks;
  state.blink = 15;
  world.cursor.selected = 255;
  world.unlocked[108] = count >= 3;
  world.unlocked[109] = count != 3;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(world.cursor.count == count);
  CHECK(world.cursor.selected == count - 1);
  CHECK(state.blink == 0);
  CHECK(state.step == SpellMenuStep::kRanks);
}

TEST_CASE(
    "F02604 ranks draw styled glyphs and complete arrow geometry without "
    "backpatches",
    "[magic-terminal][F02604]") {
  World world;
  auto state = RankState();
  state.blink = 15;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(state.blink == 0);
  REQUIRE(world.text.records.size() == 14);
  CHECK(world.text.records.front()[1] == 0x66808080);
  CHECK(world.text.records.front()[4] == 0x000A0005);
  REQUIRE(world.icons.records.size() == 6);
  CHECK(world.icons.records[2][1] == 0x64602000);
  CHECK(world.icons.records.back()[5] == 0x000C304F);
  CHECK(world.icons.records.back()[9] == 0x404F);
}

TEST_CASE(
    "F02604 rank confirm denial still permits bounded repeated navigation",
    "[magic-terminal][F02604]") {
  World world;
  auto state = RankState();
  world.input = {.confirm = true, .right = true, .left = true};
  world.restrictions[106] = 1;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(world.cursor.selected == 0);
  CHECK(state.step == SpellMenuStep::kRanks);
  CHECK((world.calls[1] == std::vector<std::int64_t>{10, 2}));
  CHECK((world.calls.back() == std::vector<std::int64_t>{6, 106}));
}

TEST_CASE("F02604 rank quit has priority and clears the cost before returning",
          "[magic-terminal][F02604]") {
  World world;
  auto state = RankState();
  world.input = {.quit = true, .back = true, .confirm = true};
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(state.selection == -2);
  CHECK(state.step == SpellMenuStep::kReturn);
  CHECK_FALSE(world.display_cost);
  CHECK(world.icons.records.empty());
}

TEST_CASE(
    "F02604 rank back flies out exactly the ten rank rows before rebuilding",
    "[magic-terminal][F02604]") {
  World world;
  auto state = RankState();
  world.input.back = true;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(state.step == SpellMenuStep::kInit);
  REQUIRE(world.calls.size() == 11);
  CHECK((world.calls[1] == std::vector<std::int64_t>{11, 20}));
  CHECK((world.calls.back() == std::vector<std::int64_t>{11, 29}));
}

TEST_CASE("F02604 rank confirmation returns each actual learned action",
          "[magic-terminal][F02604]") {
  const auto rank = GENERATE(0U, 1U, 2U, 3U);
  World world;
  auto state = RankState();
  world.cursor = {static_cast<std::uint8_t>(rank), 4, 0};
  world.input.confirm = true;
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 0);
  CHECK(state.selection == 106 + rank);
  CHECK(state.step == SpellMenuStep::kReturn);
  CHECK(magic::UpdateWarlockMenu(0, state, world) == 106 + rank);
}

TEST_CASE(
    "F02604 ranks explicitly refuse invalid state and unavailable rendering",
    "[magic-terminal][F02604]") {
  const int failure = GENERATE(0, 1, 2, 3, 4, 5);
  World world;
  auto state = RankState();
  world.cursor.selected = failure == 0 ? 255 : 0;
  world.cursor.count = failure == 1 ? 0 : 2;
  state.blink = failure == 2 ? 16 : 0;
  world.refuse_drawing = failure == 3;
  world.width_count = failure == 4 ? 0 : world.widths.size();
  world.icons.capacity = failure == 5 ? 1 : 256;
  CHECK_FALSE(magic::UpdateWarlockMenu(0, state, world));
  CHECK(world.refusals.size() == 1);
  CHECK(state.step == SpellMenuStep::kRanks);
  CHECK(world.icons.records.size() <= world.icons.capacity);
}

TEST_CASE("F02600 inactive and unknown state bytes do not dispatch",
          "[magic-terminal][F02600]") {
  std::uint8_t state = GENERATE(0, 1, 2, 18, 255);
  World world;
  const auto original = state;
  CHECK(magic::Update(state, world) == 0);
  CHECK(state == original);
  CHECK(world.calls.empty());
}

TEST_CASE(
    "F02600 submenu uses locked descriptions and teleport save-point "
    "restrictions",
    "[magic-terminal][F02600]") {
  std::uint8_t state = 4;
  World world;
  world.schools[2] = false;
  world.teleport_availability = GENERATE(1, 2, 255);
  world.save_points[47] = 255;
  world.cursor_setting = 255;
  CHECK(magic::Update(state, world) == 0);
  REQUIRE(world.rows.size() == 5);
  CHECK(world.rows[2].description.index == world.strings[8]);
  CHECK(world.rows[2].type == 1);
  CHECK(world.rows[4].type == (world.teleport_availability == 2 ? 1 : 0));
  CHECK(world.rows[4].description.index ==
        (world.teleport_availability == 2 ? 144 : 106));
  CHECK(world.cursor_setting == 255);
  CHECK(state == 5);
}

TEST_CASE("F02600 teleport description preserves repeated live-query ordering",
          "[magic-terminal][F02600]") {
  std::uint8_t state = 3;
  World world;
  world.teleport_availability = 2;
  world.at_save_point = {false, true, false};
  CHECK(magic::Update(state, world) == 0);
  REQUIRE(world.rows.size() == 5);
  CHECK(world.rows.back().description.index == 144);
  CHECK(world.save_point_query == 2);
  CHECK(world.cursor_at_create == 0);
}

TEST_CASE("F02600 shortcuts publish their state before nested school calls",
          "[magic-terminal][F02600]") {
  const auto school = GENERATE(0U, 1U, 2U, 3U);
  std::uint8_t state = 3;
  World world;
  world.shortcut = static_cast<std::uint8_t>(school + 1);
  world.caller_state = &state;
  world.callback = [&](int code) {
    if (code == 17) {
      state = 200;
      world.action = 999;
    }
  };
  CHECK(magic::Update(state, world) == 0);
  CHECK(state == 200);
  CHECK(world.action == 999);
  CHECK((world.calls.back() ==
         std::vector<std::int64_t>{17, school, 1, 7 + school * 2}));
}

TEST_CASE(
    "F02600 resumed schools publish the return state after their init call",
    "[magic-terminal][F02600]") {
  const auto school = GENERATE(0U, 1U, 2U, 3U);
  auto state = static_cast<std::uint8_t>(6 + school * 2);
  World world;
  world.caller_state = &state;
  world.callback = [&](int code) {
    if (code == 17) {
      state = 200;
      world.current_menu = 211;
    }
  };
  CHECK(magic::Update(state, world) == 0);
  CHECK(state == 7 + school * 2);
  CHECK(world.current_menu == 211);
  CHECK((world.calls.back() ==
         std::vector<std::int64_t>{17, school, 2, 6 + school * 2}));
}

TEST_CASE("F02600 submenu selection routes all schools and teleport",
          "[magic-terminal][F02600]") {
  const auto row = GENERATE(0, 1, 2, 3, 4);
  std::uint8_t state = 5;
  World world;
  world.selected_row = row;
  world.caller_state = &state;
  CHECK(magic::Update(state, world) == 0);
  CHECK(state == (row == 4 ? 14 : 7 + row * 2));
  CHECK(world.calls.back()[0] == (row == 4 ? 18 : 17));
}

TEST_CASE(
    "F02600 cancellation and wrapped selections preserve distinct exit flows",
    "[magic-terminal][F02600]") {
  const int selected = GENERATE(-1, -2, -3, std::numeric_limits<int>::max());
  std::uint8_t state = 5;
  World world;
  world.selected_row = selected;
  CHECK(magic::Update(state, world) == 0);
  CHECK(state == (selected == -1 ? 5 : (selected == -3 ? 17 : 15)));
}

TEST_CASE(
    "F02600 confirmed spells keep nested writes and publish the school ability "
    "type",
    "[magic-terminal][F02600]") {
  const auto school = GENERATE(0U, 1U, 2U, 3U);
  auto state = static_cast<std::uint8_t>(7 + school * 2);
  World world;
  world.child_result = 255;
  world.callback = [&](int code) {
    if (code == 17) {
      world.return_menu = 211;
      world.selection_kind = 123;
    }
  };
  CHECK(magic::Update(state, world) == 0);
  CHECK(world.action == 255);
  CHECK(world.ability_type == 6 + school * 2);
  CHECK(world.return_menu == 211);
  CHECK(world.selection_kind == 123);
  CHECK(state == 16);
}

TEST_CASE("F02600 school pending back and quit results do not execute actions",
          "[magic-terminal][F02600]") {
  std::uint8_t state = GENERATE(7, 9, 11, 13);
  const auto before = state;
  World world;
  world.child_result = GENERATE(0, -1, -2, std::numeric_limits<int>::min());
  CHECK(magic::Update(state, world) == 0);
  CHECK(
      state ==
      (world.child_result == 0 ? before : (world.child_result == -2 ? 17 : 4)));
  CHECK(world.action == 0);
}

TEST_CASE("F02600 execution reads the menu after the readiness callback",
          "[magic-terminal][F02600]") {
  std::uint8_t state = 16;
  World world;
  world.current_menu = 9;
  world.callback = [&](int code) {
    if (code == 0) {
      world.current_menu = 255;
    }
  };
  CHECK(magic::Update(state, world) == 1);
  CHECK(state == 0);
  CHECK(world.return_menu == 255);
  CHECK(world.current_menu == 1);
  CHECK(world.selection_kind == 7);
}

TEST_CASE(
    "F02600 teleport keeps packed return bits and distinct selection kind",
    "[magic-terminal][F02600]") {
  std::uint8_t state = 14;
  World world;
  world.teleport_result = std::numeric_limits<int>::max();
  world.ability_type = 255;
  CHECK(magic::Update(state, world) == 0);
  CHECK(world.action == std::numeric_limits<int>::max() - 1);
  CHECK(world.selection_kind == 10);
  CHECK(world.ability_type == 255);
  CHECK(state == 17);
}

TEST_CASE("F02600 exit gates retain state until rows are idle",
          "[magic-terminal][F02600]") {
  std::uint8_t state = GENERATE(15, 16, 17);
  const auto before = state;
  World world;
  world.idle = false;
  CHECK(magic::Update(state, world) == 0);
  CHECK(state == before);
  world.idle = true;
  CHECK(magic::Update(state, world) == 1);
  CHECK(state == 0);
  CHECK(world.current_menu == (before == 15 ? 0 : 1));
}

TEST_CASE("F02600 refuses malformed sources and does not mask child failures",
          "[magic-terminal][F02600]") {
  const int failure = GENERATE(0, 1, 2, 3, 4, 5);
  std::uint8_t state = failure == 0 ? 3 : (failure < 3 ? 4 : 6);
  World world;
  world.shortcut = failure == 0 ? 255 : 0;
  world.string_count = failure == 1 ? 0 : world.strings.size();
  world.teleport_availability = failure == 2 ? 1 : 0;
  world.save_point_count = failure == 2 ? 0 : world.save_points.size();
  world.refuse_header = failure == 3;
  world.refuse_title = failure == 4;
  world.refuse_child = failure == 5;
  const auto original = state;
  CHECK_FALSE(magic::Update(state, world));
  CHECK(state == original);
  CHECK(world.refusals.size() == 1);
}

TEST_CASE("F02600 live null state is reported rather than dereferenced",
          "[magic-terminal-live][F02600]") {
  const auto before = magic::RefusedMagicMenuCount();
  CHECK(magic::UpdateLive(nullptr) == 0);
  CHECK(magic::RefusedMagicMenuCount() == before + 1);
}
