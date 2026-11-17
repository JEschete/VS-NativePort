#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/AbilityTerminalWorld.hpp"

namespace {
namespace a = VS::Menu::BattleAbilities;
namespace fixture = VS::Test::ability_terminal;
using fixture::Call;

a::AbilityRowsState ActiveRows(int count, int highlighted = 0, int first = 0) {
  return {.count = count,
          .menu_state = 17,
          .type = a::AbilityMenuType::kChain,
          .cursor = {.highlighted = highlighted, .first = first}};
}
}  // namespace

TEST_CASE("F02620 moves an assignment without touching other kinds or tails",
          "[menu][battle-abilities-terminal][F02620]") {
  fixture::AssignmentWorld world;
  const auto type =
      GENERATE(a::AbilityMenuType::kChain, a::AbilityMenuType::kDefense);
  const auto button =
      GENERATE(a::AssignmentButton::kCircle, a::AssignmentButton::kTriangle,
               a::AssignmentButton::kSquare);
  const auto action = type == a::AbilityMenuType::kChain ? 29 : 45;
  auto& mapping = type == a::AbilityMenuType::kChain ? world.chain_mapping
                                                     : world.defense_mapping;
  mapping = {static_cast<std::uint8_t>(action),
             static_cast<std::uint8_t>(action),
             static_cast<std::uint8_t>(action), 0xFE};
  mapping.at(static_cast<std::size_t>(button)) = 7;
  CHECK(a::ToggleButtonAssignment(world, type, button, 5));
  CHECK(mapping.at(static_cast<std::size_t>(button)) == action);
  CHECK(std::ranges::count(mapping, action) == 1);
  CHECK(mapping.back() == 0xFE);
  CHECK(world.calls == (std::vector<Call>{{2}}));
  CHECK(world.refusals.empty());
}

TEST_CASE("F02620 removes only the pressed duplicate on a repeated assignment",
          "[menu][battle-abilities-terminal][F02620]") {
  fixture::AssignmentWorld world;
  world.chain_mapping = {24, 24, 0, 0xAB};
  CHECK(a::ToggleButtonAssignment(world, a::AbilityMenuType::kChain,
                                  a::AssignmentButton::kCircle, 0));
  CHECK(world.chain_mapping == (std::array<std::uint8_t, 4>{0, 24, 0, 0xAB}));
  CHECK(world.calls == (std::vector<Call>{{1}}));
  CHECK(world.defense_mapping == (std::array<std::uint8_t, 4>{}));
}

TEST_CASE("F02620 applies duplicate removal after synchronous sound effects",
          "[menu][battle-abilities-terminal][F02620]") {
  fixture::AssignmentWorld world;
  world.chain[0] = 0xE2;
  world.on_sound = [&](a::AssignmentSound sound) {
    if (sound == a::AssignmentSound::kSet) world.chain_mapping[2] = 0xE2;
  };
  CHECK(a::ToggleButtonAssignment(world, a::AbilityMenuType::kChain,
                                  a::AssignmentButton::kTriangle, 0));
  CHECK(world.chain_mapping == (std::array<std::uint8_t, 4>{0, 0xE2, 0, 0}));
}

TEST_CASE("F02620 refuses invalid mappings before playing or storing",
          "[menu][battle-abilities-terminal][F02620]") {
  fixture::AssignmentWorld world;
  const auto index = GENERATE(-1, 16, 0);
  if (index == 0) world.mapping_count = 2;
  CHECK_FALSE(a::ToggleButtonAssignment(world, a::AbilityMenuType::kChain,
                                        a::AssignmentButton::kSquare, index));
  CHECK(world.refusals.size() == 1);
  CHECK(world.calls.empty());
  CHECK_FALSE(a::ToggleButtonAssignment(world, a::AbilityMenuType::kNone,
                                        a::AssignmentButton::kCircle, 0));
  CHECK_FALSE(a::ToggleButtonAssignment(
      world, a::AbilityMenuType::kChain,
      std::bit_cast<a::AssignmentButton>(std::uint8_t{3}), 0));
  world.missing_action = 0;
  CHECK_FALSE(a::ToggleButtonAssignment(world, a::AbilityMenuType::kChain,
                                        a::AssignmentButton::kCircle, 0));
}

TEST_CASE("F02621 creates at most eight bounded rows before waiting",
          "[menu][battle-abilities-terminal][F02621]") {
  const int count = GENERATE(1, 7, 8, 9, 16);
  fixture::AssignmentWorld world;
  a::AssignmentState state{.cursor_animation = 19};
  auto rows = ActiveRows(count);
  rows.menu_state = 0;
  for (int row = 0; row < std::min(count, 8); ++row) {
    CHECK(a::UpdateAssignments(rows, state, world) == 0);
    CHECK(world.items.at(static_cast<std::size_t>(row)).text_offset ==
          row * 64);
    CHECK(world.items.at(static_cast<std::size_t>(row)).target == 194);
  }
  CHECK(rows.menu_state == 16);
  CHECK(state.cursor_animation == 19);
  CHECK(world.items[7].scroll == (count > 8 ? 2 : 0));
  CHECK(world.description_copy.empty());
}

TEST_CASE(
    "F02621 shows the selected description while waiting and at idle phases",
    "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  auto rows = ActiveRows(12, 2, 3);
  rows.menu_state = GENERATE(10U, 15U, 16U, 18U);
  const auto before = rows.menu_state;
  world.ready = false;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(world.description_copy.front() == 5 * 64 + 16);
  CHECK(world.description_copy.size() == 48);
  CHECK(rows.menu_state == before);
  world.ready = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.menu_state == (before == 16 ? 17 : before));
  CHECK(world.risk_action == -1);
}

TEST_CASE("F02621 assigns before cancel and releases only owned row text once",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state{.cursor_animation = 31};
  auto rows = ActiveRows(11, 3, 2);
  world.input = {.circle = true,
                 .triangle = true,
                 .square = true,
                 .cancel = true,
                 .repeat_up = true};
  CHECK(a::UpdateAssignments(rows, state, world) == 1);
  CHECK(world.chain_mapping[0] == 29);
  CHECK(world.chain_mapping[1] == 0);
  CHECK(world.memories[48] == 3);
  CHECK(world.memories[49] == 2);
  CHECK(world.free_count == 1);
  CHECK(state.cursor_animation == 31);
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(world.free_count == 1);
}

TEST_CASE("F02621 small lists wrap on repeat and process both directions",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  const int count = GENERATE(1, 2, 8);
  auto rows = ActiveRows(count);
  world.input.repeat_up = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.highlighted == count - 1);
  CHECK(world.risk_action == 24 + count - 1);
  world.input.repeat_down = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.highlighted == count - 1);
  world.input.repeat_up = false;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.highlighted == 0);
}

TEST_CASE("F02621 large-list upward wrap requires a fresh press",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  auto rows = ActiveRows(GENERATE(9, 16));
  world.input.repeat_up = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.highlighted == 0);
  world.input.pressed_up = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.first == rows.count - 8);
  CHECK(rows.cursor.highlighted == 7);
}

TEST_CASE("F02621 large-list downward wrap requires a fresh press",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  const int count = GENERATE(9, 16);
  auto rows = ActiveRows(count, 7, count - 8);
  world.input = {.repeat_down = true};
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.highlighted == 7);
  world.input.pressed_down = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.first == 0);
}

TEST_CASE("F02621 scrolling preserves gradients by absolute ability",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  auto rows = ActiveRows(12, 1, 1);
  for (std::size_t i = 0; i < world.items.size(); ++i) {
    world.items[i].gradient = static_cast<std::uint8_t>(90 + i);
  }
  world.input.repeat_up = true;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.first == 0);
  CHECK(world.items[0].gradient == 0);
  CHECK(world.items[1].gradient == 90);
  CHECK(world.items[7].gradient == 96);
  CHECK(world.items[7].scroll == 2);
  CHECK(world.items[1].selected == 1);
  CHECK(world.items[0].state == 1);
}

TEST_CASE("F02621 downward scrolling uses the penultimate selectable row",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  auto rows = ActiveRows(10, 6);
  world.input.repeat_down = true;
  world.cursor_result = 0x81234567;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(rows.cursor.first == 1);
  CHECK(rows.cursor.highlighted == 6);
  CHECK(world.items[0].scroll == 1);
  CHECK(world.items[7].scroll == 2);
  CHECK(std::bit_cast<std::uint32_t>(state.cursor_animation) == 0x81234567);
  CHECK(world.risk_action == 31);
  CHECK(world.calls.back() == (Call{11, 31}));
}

TEST_CASE("F02621 refreshes button input after assignment side effects",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  auto rows = ActiveRows(2);
  world.input.square = true;
  world.on_sound = [&](a::AssignmentSound sound) {
    if (sound == a::AssignmentSound::kSet) world.input.cancel = true;
  };
  CHECK(a::UpdateAssignments(rows, state, world) == 1);
  CHECK(world.chain_mapping[2] == 24);
  CHECK(world.free_count == 1);
  CHECK(world.risk_action == -1);
}

TEST_CASE("F02621 refuses missing row ownership and invalid paging",
          "[menu][battle-abilities-terminal][F02621]") {
  fixture::AssignmentWorld world;
  a::AssignmentState state;
  auto rows = ActiveRows(10);
  const int invalid = GENERATE(0, 1, 2, 3, 4);
  if (invalid == 0) world.has_text = false;
  if (invalid == 1) rows.count = 17;
  if (invalid == 2) rows.cursor.first = 3;
  if (invalid == 3) rows.cursor.highlighted = 7;
  if (invalid == 4) world.text_size = 10 * 64 - 1;
  CHECK(a::UpdateAssignments(rows, state, world) == 0);
  CHECK(world.refusals.size() == 1);
  CHECK(world.calls.empty());
}
