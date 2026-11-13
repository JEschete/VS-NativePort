#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "TestSupport/ActionAnimationsFixture.hpp"

namespace {
namespace t = VS::test_support::action_animations;
namespace b = VS::Battle;
using t::Kind;
using t::Result;

TEST_CASE("F00819 keeps full signed modes and uses the instance enemy slot",
          "[action-reactions][F00819]") {
  for (const auto [mode, animation] :
       {std::pair{std::numeric_limits<int>::min(), 0},
        {-1, 0},
        {0, 6},
        {1, 0xC6},
        {2, 0xC8},
        {4, 0},
        {0x10001, 0}}) {
    t::World world;
    auto& actor = world.actors.at(0);
    std::iota(actor.pose.begin(), actor.pose.end(), 100U);
    const auto old_blend = actor.blend_pose;
    REQUIRE(b::SetEnemyCombatPose(0, mode, world) == Result::kComplete);
    CHECK(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{
              {Kind::kPlay, 0, static_cast<std::uint32_t>(animation), 0, 0}});
    CHECK(actor.blend_pose == (mode < 3 ? actor.pose : old_blend));
    CHECK(world.queries == 0);
    CHECK(world.target_reads == 0);
  }
}

TEST_CASE("F00819 player and unrelated current animations return before lookup",
          "[action-reactions][F00819]") {
  t::World world;
  auto& actor = world.actors.at(0);
  actor.slot = 0;
  actor.animation = 0xC6;
  REQUIRE(b::SetEnemyCombatPose(16, 3, world) == Result::kComplete);
  CHECK(world.events.empty());
  actor.slot = 1;
  actor.animation = 0xC7;
  REQUIRE(b::SetEnemyCombatPose(16, 3, world) == Result::kComplete);
  CHECK(world.events.empty());
  world.table_owner = -1;
  CHECK(b::SetEnemyCombatPose(16, -1, world) == Result::kMissingActor);
}

TEST_CASE("F00819 finish lookup retains its chosen clip and original owner",
          "[action-reactions][F00819]") {
  for (const int animation : {0xC6, 0xC8}) {
    for (const bool present : {false, true}) {
      t::World world;
      auto& actor = world.actors.at(0);
      actor.animation = static_cast<std::uint8_t>(animation);
      actor.blend_pose.fill(0xF1234567U);
      const auto old_blend = actor.blend_pose;
      world.headers.at(0) = present;
      world.callback = [&](const t::Event& event) {
        if (event.kind == Kind::kHeader) {
          actor.animation = 0;
          world.table_owner = 1;
        }
      };
      REQUIRE(b::SetEnemyCombatPose(16, 3, world) == Result::kComplete);
      CHECK(world.Calls(Kind::kHeader) ==
            std::vector<t::Event>{
                {Kind::kHeader, 0, static_cast<std::uint32_t>(animation + 1)}});
      CHECK(world.Calls(Kind::kPlay) ==
            std::vector<t::Event>{
                {Kind::kPlay, 16,
                 static_cast<std::uint32_t>(present ? animation + 1 : 6), 0,
                 present ? 8U : 16U}});
      CHECK(actor.blend_pose == old_blend);
      CHECK(world.actor_reads == 1);
    }
  }
}

TEST_CASE(
    "F00819 copies the whole current pose after playback on its cached owner",
    "[action-reactions][F00819]") {
  t::World world;
  auto& first = world.actors.at(0);
  std::iota(first.pose.begin(), first.pose.end(), 0x12345600U);
  const auto before = first.pose;
  world.actors.at(1).blend_pose.fill(0xFFFFFFFFU);
  const auto other_blend = world.actors.at(1).blend_pose;
  world.preset = 12;
  REQUIRE(b::SetEnemyCombatPose(16, 1, world) == Result::kComplete);
  CHECK(first.pose != before);
  CHECK(first.blend_pose == first.pose);
  CHECK(world.actors.at(1).blend_pose == other_blend);
  CHECK(world.table_owner == 1);
  CHECK(world.actor_reads == 1);
}

TEST_CASE("F00832 still queues impact effects when the entire prefix is gated",
          "[action-reactions][F00832]") {
  for (const std::uint32_t flags :
       {0U, 0x00A00000U, 0x00C00000U, 0x00820000U, 0x00830000U}) {
    t::World world;
    world.actors.at(0).flags = flags;
    REQUIRE(b::PlayDefenseAbilityAnimation(0, world) == Result::kComplete);
    CHECK(world.events == std::vector<t::Event>{{Kind::kImpact, 0}});
    CHECK(world.target_reads == 0);
    CHECK(world.actors.at(0).jump == 99);
  }
}

TEST_CASE(
    "F00832 executes the real defense prefix before its unconditional tail",
    "[action-reactions][F00832]") {
  std::vector<std::array<std::uint32_t, 4>> outcomes;
  for (const auto [radius, pitch] :
       {std::pair{127, -229}, {128, -228}, {129, 171}, {127, 172}}) {
    t::World world;
    world.actors.at(0).radius = static_cast<std::uint16_t>(radius);
    world.helper_pitch = pitch;
    REQUIRE(b::PlayDefenseAbilityAnimation(16, world) == Result::kComplete);
    const auto play = world.Calls(Kind::kPlay).at(0);
    outcomes.push_back({play.first, play.third, world.actors.at(0).turn,
                        static_cast<std::uint16_t>(world.actors.at(0).delta)});
    CHECK(world.events.at(world.events.size() - 2).kind == Kind::kReset);
    CHECK(world.events.back() == t::Event{Kind::kImpact, 16});
  }
  CHECK(outcomes ==
        std::vector<std::array<std::uint32_t, 4>>{{0x4F, 8, 4, 123},
                                                  {0x50, 4, 777, 333},
                                                  {0x50, 4, 777, 333},
                                                  {0x51, 8, 4, 123}});
}

TEST_CASE("F00832 tail observes cleanup on the callback-current cached actor",
          "[action-reactions][F00832]") {
  for (const int preset : {7, 8}) {
    t::World world;
    world.actors.at(0).flags = 0xA589FDFFU;
    world.preset = preset;
    std::uint32_t tail_flags = 0;
    std::uint8_t tail_jump = 99;
    world.callback = [&](const t::Event& event) {
      if (event.kind == Kind::kImpact) {
        tail_flags = world.actors.at(0).flags;
        tail_jump = world.actors.at(0).jump;
      }
    };
    REQUIRE(b::PlayDefenseAbilityAnimation(16, world) == Result::kComplete);
    CHECK(tail_flags == (preset == 7 ? 0xA588FDFFU : 0xA58BFDFFU));
    CHECK(tail_jump == 0);
    CHECK(world.actors.at(2).jump == 99);
    CHECK(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 0}});
    CHECK(world.Calls(Kind::kImpact) ==
          std::vector<t::Event>{{Kind::kImpact, 16}});
    CHECK(world.actor_reads == 1);
  }
}

TEST_CASE("F00832 cleans an undrawn player without using its target",
          "[action-reactions][F00832]") {
  t::World world;
  auto& actor = world.actors.at(0);
  actor.slot = 0;
  actor.flags = 0xE0621234U;
  REQUIRE(b::PlayDefenseAbilityAnimation(16, world) == Result::kComplete);
  CHECK(actor.flags == 0xE0601234U);
  CHECK(actor.jump == 0);
  CHECK(world.target_reads == 0);
  CHECK(world.events == std::vector<t::Event>{{Kind::kImpact, 16}});
  world.table_owner = -1;
  world.events.clear();
  CHECK(b::PlayDefenseAbilityAnimation(16, world) == Result::kMissingActor);
  CHECK(world.events.empty());
}

TEST_CASE(
    "F00891 narrows facing to signed sixteen bits and preserves other flags",
    "[action-reactions][F00891]") {
  for (const auto [input, narrowed] : {std::pair{0xFFFF, -1},
                                       {0x18000, -32768},
                                       {std::numeric_limits<int>::max(), -1},
                                       {std::numeric_limits<int>::min(), 0}}) {
    t::World world;
    auto& actor = world.actors.at(0);
    actor.slot = 0;
    actor.flags = 0x9ABCDEF0U;
    REQUIRE(b::FinishEnemyCombatPose(16, input, world) == Result::kComplete);
    CHECK(world.events ==
          std::vector<t::Event>{
              {Kind::kTurn, 16, static_cast<std::uint32_t>(narrowed), 8}});
    CHECK(actor.flags == 0xEABCDEF0U);
    CHECK(world.queries == 0);
    CHECK(world.target_reads == 0);
  }
}

TEST_CASE(
    "F00891 finishes the original owner after turn and header replacements",
    "[action-reactions][F00891]") {
  t::World world;
  auto& first = world.actors.at(0);
  auto& second = world.actors.at(1);
  second.flags = 0x12345678U;
  const auto other_flags = world.actors.at(2).flags;
  world.callback = [&](const t::Event& event) {
    if (event.kind == Kind::kTurn) {
      world.table_owner = 1;
      second.animation = 0xC8;
    } else if (event.kind == Kind::kHeader) {
      world.table_owner = 2;
      second.animation = 0;
    } else if (event.kind == Kind::kPlay) {
      first.flags = 0xBA987654U;
    }
  };
  REQUIRE(b::FinishEnemyCombatPose(16, 0x18000, world) == Result::kComplete);
  CHECK(world.events == std::vector<t::Event>{{Kind::kTurn, 16, 0xFFFF8000U, 8},
                                              {Kind::kHeader, 1, 0xC9},
                                              {Kind::kPlay, 16, 0xC9, 0, 8}});
  CHECK(first.flags == 0xEA987654U);
  CHECK(second.flags == 0x12345678U);
  CHECK(world.actors.at(2).flags == other_flags);
  CHECK(world.actor_reads == 2);
}

TEST_CASE("F00891 still finishes when its real combat-pose policy does nothing",
          "[action-reactions][F00891]") {
  t::World world;
  world.actors.at(0).animation = 0xC7;
  REQUIRE(b::FinishEnemyCombatPose(0, -1, world) == Result::kComplete);
  CHECK(world.actors.at(0).flags == 0x60800000U);
  CHECK(world.Calls(Kind::kPlay).empty());
  CHECK(world.queries == 0);
  CHECK(world.Calls(Kind::kTurn) ==
        std::vector<t::Event>{{Kind::kTurn, 0, 0xFFFFFFFFU, 8}});
}

TEST_CASE(
    "F00891 diagnoses missing used owners instead of finishing a new entry",
    "[action-reactions][F00891]") {
  t::World world;
  world.table_owner = -1;
  CHECK(b::FinishEnemyCombatPose(16, 0, world) == Result::kMissingActor);
  CHECK(world.events.empty());
  world.table_owner = 0;
  const auto original_flags = world.actors.at(0).flags;
  world.callback = [&](const t::Event& event) {
    if (event.kind == Kind::kTurn) world.table_owner = -1;
  };
  CHECK(b::FinishEnemyCombatPose(16, 0, world) == Result::kMissingActor);
  CHECK(world.actors.at(0).flags == original_flags);
  CHECK(world.queries == 0);
}
}  // namespace
