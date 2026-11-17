#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Menu/AbilityTerminalWorld.hpp"

namespace {
namespace a = VS::Menu::BattleAbilities;
namespace fixture = VS::Test::ability_terminal;
using fixture::Call;

int RunList(a::AbilityKind kind, a::AbilityListState& state,
            fixture::ListWorld& world, bool initialize) {
  return kind == a::AbilityKind::kChain
             ? a::UpdateChainAbilityMenu(state, world, initialize)
             : a::UpdateDefenseAbilityMenu(state, world, initialize);
}

void CheckListCollection(a::AbilityKind kind) {
  fixture::ListWorld world;
  a::AbilityListState state;
  auto& order = kind == a::AbilityKind::kChain ? world.chain : world.defense;
  order[0] = 0xE8;
  world.unlocked[order[0]] = true;
  world.unlocked[order[4]] = true;
  world.unlocked[order[13]] = true;
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(world.listed == (std::vector<a::LearnedAbility>{
                            {order[0], 0}, {order[4], 4}, {order[13], 13}}));
  CHECK(world.listed_kind == kind);
  CHECK(state.phase == a::AbilityListPhase::kAssignments);
  const auto& learned = kind == a::AbilityKind::kChain ? world.learned_chain
                                                       : world.learned_defense;
  CHECK(learned[2] == order[13]);
  CHECK(learned[3] == (kind == a::AbilityKind::kChain ? 0xCD : 0xDE));
}

void CheckListLifecycle(a::AbilityKind kind) {
  fixture::ListWorld world;
  a::AbilityListState state{.phase = a::AbilityListPhase::kAssignments};
  world.ready = false;
  CHECK(RunList(kind, state, world, true) == 0);
  CHECK(state.phase == a::AbilityListPhase::kInit);
  CHECK(world.calls.front() == (Call{20, static_cast<int>(kind), 1}));
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(state.phase == a::AbilityListPhase::kInit);
  world.shortcut = true;
  world.calls.clear();
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(world.calls ==
        (std::vector<Call>{{21, 0, kind == a::AbilityKind::kChain ? 24 : 25}}));
  CHECK(state.phase == a::AbilityListPhase::kAssignments);
}

void CheckListExit(a::AbilityKind kind) {
  fixture::ListWorld world;
  a::AbilityListState state{.phase = a::AbilityListPhase::kAssignments};
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(world.calls == (std::vector<Call>{{22}}));
  world.assignment_result = 1;
  world.calls.clear();
  CHECK(RunList(kind, state, world, false) == 1);
  CHECK(world.calls == (std::vector<Call>{{22}, {23, 2}}));
  CHECK(state.phase == a::AbilityListPhase::kAssignments);
}

void CheckListRefusalAndRepeat(a::AbilityKind kind) {
  fixture::ListWorld world;
  a::AbilityListState state;
  world.order_size = 13;
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(world.refusals.size() == 1);
  CHECK(state.phase == a::AbilityListPhase::kInit);
  world.order_size = 16;
  world.on_create = [&] {
    state.phase = std::bit_cast<a::AbilityListPhase>(std::uint32_t{81});
  };
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(state.phase == a::AbilityListPhase::kAssignments);
  world.calls.clear();
  state.phase = std::bit_cast<a::AbilityListPhase>(std::uint32_t{99});
  CHECK(RunList(kind, state, world, false) == 0);
  CHECK(world.calls.empty());
}
}  // namespace

TEST_CASE("F02622 collects learned chain actions in producer order",
          "[menu][battle-abilities-terminal][F02622]") {
  CheckListCollection(a::AbilityKind::kChain);
}

TEST_CASE("F02623 collects learned defense actions in producer order",
          "[menu][battle-abilities-terminal][F02623]") {
  CheckListCollection(a::AbilityKind::kDefense);
}

TEST_CASE("F02622 preserves initialization waits and empty creation",
          "[menu][battle-abilities-terminal][F02622]") {
  CheckListLifecycle(a::AbilityKind::kChain);
}

TEST_CASE("F02623 preserves initialization waits and empty creation",
          "[menu][battle-abilities-terminal][F02623]") {
  CheckListLifecycle(a::AbilityKind::kDefense);
}

TEST_CASE("F02622 exits only after the nested assignment menu closes",
          "[menu][battle-abilities-terminal][F02622]") {
  CheckListExit(a::AbilityKind::kChain);
}

TEST_CASE("F02623 exits only after the nested assignment menu closes",
          "[menu][battle-abilities-terminal][F02623]") {
  CheckListExit(a::AbilityKind::kDefense);
}

TEST_CASE("F02622 checks producer bounds and advances after creation no-op",
          "[menu][battle-abilities-terminal][F02622]") {
  CheckListRefusalAndRepeat(a::AbilityKind::kChain);
}

TEST_CASE("F02623 checks producer bounds and advances after creation no-op",
          "[menu][battle-abilities-terminal][F02623]") {
  CheckListRefusalAndRepeat(a::AbilityKind::kDefense);
}

TEST_CASE("F02624 formats every decimal boundary and clamps surplus kills",
          "[menu][battle-abilities-terminal][F02624]") {
  fixture::PointsWorld world;
  const auto points =
      GENERATE(0, 1, 9, 10, 99, 100, 999, 1000, 9999, 10000, 65535);
  world.requirements[0] = static_cast<std::uint16_t>(points);
  a::DrawNextUnlockPoints(0, world);
  CHECK(world.texts == (std::vector<std::string>{
                           "NEXT", "#" + std::to_string(points) + "PT"}));
  CHECK(world.positions ==
        (std::vector<std::uint32_t>{0x004200CE, 0x0042012E}));
  world.kills = 65535;
  world.texts.clear();
  a::DrawNextUnlockPoints(0, world);
  CHECK(world.texts.back() == "#0PT");
}

TEST_CASE("F02624 preserves wrapping packed coordinates and last tier",
          "[menu][battle-abilities-terminal][F02624]") {
  fixture::PointsWorld world;
  world.count = 21;
  world.requirements[21] = 100;
  world.kills = 99;
  a::DrawNextUnlockPoints(-207, world);
  CHECK(world.texts.back() == "#1PT");
  CHECK(world.positions == (std::vector<std::uint32_t>{0xFFFFFFFF, 0x5F}));
  world.count = 22;
  world.texts.clear();
  a::DrawNextUnlockPoints(0, world);
  CHECK(world.texts.empty());
  CHECK(world.refusals.empty());
}

TEST_CASE("F02624 refuses absent or out-of-range unlock requirements",
          "[menu][battle-abilities-terminal][F02624]") {
  fixture::PointsWorld world;
  world.count = GENERATE(-1, 23, 65535, 0);
  world.missing_requirement = world.count == 0;
  a::DrawNextUnlockPoints(5, world);
  CHECK(world.texts.empty());
  CHECK(world.refusals.size() == 1);
}

TEST_CASE("F02625 initialization checks readiness twice without a shortcut",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = 3;
  a::AbilityScreenAnimation animation{true, 5};
  world.readiness = {true, false};
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == 3);
  CHECK(world.calls == (std::vector<Call>{{7}, {30}, {7}}));
  CHECK_FALSE(animation.menu_animating);
  CHECK(animation.points_step == 10);
  world.readiness = {true, true};
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == 5);
  CHECK(world.calls.back() == (Call{32, 1, 1, 0}));
}

TEST_CASE("F02625 rebuilds count-dependent category help with forced memory",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = 4;
  a::AbilityScreenAnimation animation{false, 10};
  world.chain_available = GENERATE(false, true);
  world.defense_available = GENERATE(false, true);
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == 5);
  CHECK(world.calls.back() == (Call{32, world.chain_available ? 1 : 0,
                                    world.defense_available ? 1 : 0, 1}));
  CHECK(world.calls.size() == 4);
}

TEST_CASE("F02625 shortcuts publish selection before nested initialization",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = 3;
  a::AbilityScreenAnimation animation;
  world.shortcut = GENERATE(7, 8);
  int observed_phase = 0;
  world.on_list = [&](a::AbilityKind, bool) { observed_phase = phase; };
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == world.shortcut - 1);
  CHECK(observed_phase == phase);
  REQUIRE(world.header.has_value());
  if (!world.header) return;
  CHECK(world.header->row == world.shortcut + 3);
  CHECK(world.header->background_width == 8);
  CHECK(world.header->y == 34);
  CHECK(animation.points_step == 10);
}

TEST_CASE("F02625 selection preserves no-input animation and exit destinations",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = 5;
  a::AbilityScreenAnimation animation{false, 5};
  world.selected = GENERATE(-3, -2, -1, 0, 1, 3);
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  const int expected = world.selected == -1   ? 5
                       : world.selected == 0  ? 6
                       : world.selected == 1  ? 7
                       : world.selected == -3 ? 9
                                              : 8;
  CHECK(phase == expected);
  CHECK(animation.menu_animating == (world.selected == -1));
  CHECK(animation.points_step == (world.selected == -1 ? 4 : 6));
  CHECK(world.calls.back() ==
        (Call{37, world.selected == -1 ? world.offsets[5] : 192}));
}

TEST_CASE("F02625 observes nested list changes rather than restoring snapshots",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = GENERATE(6, 7);
  a::AbilityScreenAnimation animation{false, 10};
  world.list_result = 1;
  world.on_list = [&](a::AbilityKind, bool) {
    world.shortcut = 8;
    world.current_menu = 97;
  };
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == 9);
  CHECK(world.current_menu == 97);
  CHECK(world.calls.back() == (Call{23, 40}));
  world.shortcut = 0;
  world.on_list = {};
  phase = 6;
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == 4);
}

TEST_CASE("F02625 completed exits skip animation and preserve main-menu owner",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = GENERATE(8, 9);
  const auto original_phase = phase;
  a::AbilityScreenAnimation animation{true, 7};
  world.readiness = {false, true};
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == original_phase);
  CHECK(animation.points_step == 6);
  world.calls.clear();
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 1);
  CHECK(phase == 0);
  CHECK(animation.points_step == 6);
  CHECK(world.current_menu == (original_phase == 9 ? 3 : 81));
  CHECK(world.calls == (std::vector<Call>{{35}, {36, 0}, {7}}));
}

TEST_CASE("F02625 rereads animation state after the point-drawing callback",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = 0;
  a::AbilityScreenAnimation animation{true, 4};
  world.on_points = [&] { animation.points_step = 2; };
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(animation.points_step == 1);
  CHECK(world.calls == (std::vector<Call>{{37, world.offsets[4]}}));
}

TEST_CASE("F02625 refuses invalid shortcut and animation references",
          "[menu][battle-abilities-terminal][F02625]") {
  fixture::ScreenWorld world;
  std::uint8_t phase = 3;
  a::AbilityScreenAnimation animation;
  world.shortcut = 6;
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(phase == 3);
  CHECK(world.refusals.size() == 1);
  CHECK_FALSE(world.header.has_value());
  phase = 0;
  animation = {true, 36};
  CHECK(a::UpdateAbilityScreen(phase, animation, world) == 0);
  CHECK(animation.points_step == 36);
  CHECK(world.refusals.size() == 2);
}
