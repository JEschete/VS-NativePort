#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "TestSupport/ActionAnimationsFixture.hpp"

namespace {
namespace t = VS::test_support::action_animations;
namespace b = VS::Battle;
using t::Kind;
using t::Result;

TEST_CASE("F00817 selects melee bands and the full-word special parameter",
          "[action-animations][F00817]") {
  std::vector<std::uint32_t> animations;
  for (const int pitch : {-229, -228, 171, 172, 768, 769}) {
    t::World world;
    world.helper_pitch = pitch;
    REQUIRE(b::PlayMeleeAttackAnimation(16, 0, world) == Result::kComplete);
    animations.push_back(world.Calls(Kind::kPlay).at(0).first);
  }
  REQUIRE(animations ==
          std::vector<std::uint32_t>{0x48, 0x49, 0x49, 0x4A, 0x4A, 0x4E});
  t::World world;
  world.helper_pitch = 769;
  REQUIRE(b::PlayMeleeAttackAnimation(16, 0x10000, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 0x4A, 0, 6}});
}

TEST_CASE("F00817 keeps its owner but rereads radius and model after helpers",
          "[action-animations][F00817]") {
  t::World world;
  world.actors.at(0).model = 2;
  world.helper_facing = 0x1FFFF;
  world.helper_pitch = 769;
  world.preset = 1;
  REQUIRE(b::PlayMeleeAttackAnimation(16, 0, world) == Result::kComplete);
  REQUIRE(world.actors.at(0).delta == -1);
  REQUIRE(world.actors.at(0).turn == 20);
  REQUIRE(world.Calls(Kind::kPlay).at(0).first == 0x4E);
  REQUIRE(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 0}});
  REQUIRE(world.actors.at(1).delta == 333);
  REQUIRE((world.actors.at(0).flags & 0x100U) != 0);
}

TEST_CASE("F00817 ignores an absent actor and faces a null target by zero",
          "[action-animations][F00817]") {
  t::World world;
  world.table_owner = -1;
  REQUIRE(b::PlayMeleeAttackAnimation(0, std::numeric_limits<int>::min(),
                                      world) == Result::kComplete);
  REQUIRE(world.events.empty());
  REQUIRE(world.target_reads == 0);
  world.table_owner = 0;
  world.target.reset();
  REQUIRE(b::PlayMeleeAttackAnimation(16, 0, world) == Result::kComplete);
  REQUIRE(world.actors.at(0).delta == 0);
  REQUIRE(world.Calls(Kind::kPlay).at(0).first == 0x49);
}

TEST_CASE("F00818 turns only below radius 128 and uses the defense pitch bands",
          "[action-animations][F00818]") {
  std::vector<std::array<std::uint32_t, 4>> outcomes;
  for (const auto [radius, pitch] :
       {std::pair{127, -229}, {128, -228}, {129, 172}}) {
    t::World world;
    world.actors.at(0).radius = static_cast<std::uint16_t>(radius);
    world.helper_pitch = pitch;
    REQUIRE(b::PlayDefenseAnimation(16, world) == Result::kComplete);
    const auto play = world.Calls(Kind::kPlay).at(0);
    outcomes.push_back({play.first, play.third, world.actors.at(0).turn,
                        static_cast<std::uint16_t>(world.actors.at(0).delta)});
  }
  REQUIRE(outcomes ==
          std::vector<std::array<std::uint32_t, 4>>{
              {0x4F, 8, 4, 123}, {0x50, 4, 777, 333}, {0x51, 4, 777, 333}});
}

TEST_CASE("F00818 gates on the instance slot rather than the input slot",
          "[action-animations][F00818]") {
  t::World world;
  world.actors.at(0).flags |= 0x00620000;
  REQUIRE(b::PlayDefenseAnimation(0, world) == Result::kComplete);
  REQUIRE(world.events.empty());
  REQUIRE(world.actors.at(0).jump == 99);
  world.actors.at(0).slot = 0;
  REQUIRE(b::PlayDefenseAnimation(16, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kPlay).size() == 1);
  REQUIRE(world.actors.at(0).flags == 0x00E00000U);
  REQUIRE(world.actors.at(0).jump == 0);
}

TEST_CASE(
    "F00818 cleanup observes post-reset fields and preserves unrelated bits",
    "[action-animations][F00818]") {
  std::vector<std::pair<std::uint32_t, int>> outcomes;
  for (const int preset : {7, 8}) {
    t::World world;
    world.actors.at(0).flags = 0xA589FDFFU;
    world.preset = preset;
    REQUIRE(b::PlayDefenseAnimation(16, world) == Result::kComplete);
    outcomes.emplace_back(world.actors.at(0).flags, world.actors.at(0).jump);
    REQUIRE(world.Calls(Kind::kReset).at(0).owner == 0);
  }
  REQUIRE(outcomes == std::vector<std::pair<std::uint32_t, int>>{
                          {0xA588FDFFU, 0}, {0xA58BFDFFU, 0}});
  t::World unused;
  unused.actors.at(0).slot = 0;
  unused.actors.at(0).flags = 0x00620000;
  REQUIRE(b::PlayDefenseAnimation(16, unused) == Result::kComplete);
  REQUIRE(unused.target_reads == 0);
  REQUIRE(unused.actors.at(0).flags == 0x00600000U);
  REQUIRE(unused.actors.at(0).jump == 0);
}

TEST_CASE(
    "F00823 only initial casting turns, with radius 129 as the wide boundary",
    "[action-animations][F00823]") {
  std::vector<std::uint16_t> turns;
  for (const auto [phase, radius] :
       {std::pair{0, 127}, {0, 128}, {0, 129}, {1, 129}}) {
    t::World world;
    world.actors.at(0).radius = static_cast<std::uint16_t>(radius);
    REQUIRE(b::PlaySpellCastAnimation(16, phase, world) == Result::kComplete);
    turns.push_back(world.actors.at(0).turn);
    if (phase == 1) {
      REQUIRE(world.target_reads == 0);
      REQUIRE(world.actors.at(0).delta == 333);
    }
  }
  REQUIRE(turns == std::vector<std::uint16_t>{10, 10, 20, 777});
}

TEST_CASE("F00823 fallback reads fresh drawn state without requerying",
          "[action-animations][F00823]") {
  t::World world;
  world.headers.at(0) = false;
  world.preset = 3;
  REQUIRE(b::PlaySpellCastAnimation(16, 1, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kHeader) ==
          std::vector<t::Event>{{Kind::kHeader, 0, 0x58 + 100}});
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 0x59, 0, 6}});
  REQUIRE(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 0}});
}

TEST_CASE("F00823 validates a used phase only after its null-actor return",
          "[action-animations][F00823]") {
  t::World world;
  world.table_owner = -1;
  REQUIRE(b::PlaySpellCastAnimation(16, 0x10000, world) == Result::kComplete);
  REQUIRE(world.events.empty());
  world.table_owner = 0;
  REQUIRE(b::PlaySpellCastAnimation(16, 0x10000, world) ==
          Result::kInvalidCastPhase);
  REQUIRE(world.events.empty());
  REQUIRE(world.target_reads == 0);
}

TEST_CASE(
    "F00831 toggles and adds the entire side word with wrapping arithmetic",
    "[action-animations][F00831]") {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> results;
  for (const auto [seed, sequence] :
       {std::pair{0x80000000U, 0}, {0xFFFFFFFFU, 0x10001}, {0xA5A50000U, 1}}) {
    t::World world;
    world.target.reset();
    world.group = 2;
    world.side = seed;
    REQUIRE(b::PlayChainAbilityAnimation(16, 17, sequence, world) ==
            Result::kComplete);
    results.emplace_back(world.side, world.Calls(Kind::kPlay).at(0).first);
    REQUIRE(world.Calls(Kind::kReset).empty());
  }
  REQUIRE(results ==
          std::vector<std::pair<std::uint32_t, std::uint32_t>>{
              {0x80000001U, 0x800000D9U}, {0xFFFFFFFEU, 214U}, {0U, 216U}});
}

TEST_CASE(
    "F00831 observes bone callback changes for pitch Y and subsequent yaw",
    "[action-animations][F00831]") {
  t::World world;
  world.controlled_helpers = false;
  world.preset = 9;
  REQUIRE(b::PlayChainAbilityAnimation(16, 0, 0, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kBone) == std::vector<t::Event>{{Kind::kBone, 1}});
  REQUIRE(world.target.has_value());
  if (world.target) REQUIRE(world.target.value().y == 1200);
  REQUIRE(world.actors.at(0).turn == 10);
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 207, 0, 8}});
  REQUIRE(world.actors.at(0).delta == -24036);
  REQUIRE(world.Calls(Kind::kPitch).empty());
  REQUIRE(world.Calls(Kind::kReset).empty());
}

TEST_CASE(
    "F00831 distinguishes unused inputs, invalid abilities and failed pitch "
    "domains",
    "[action-animations][F00831]") {
  t::World world;
  world.table_owner = -1;
  REQUIRE(b::PlayChainAbilityAnimation(16, -100, 1, world) ==
          Result::kComplete);
  REQUIRE(world.group_reads == 0);
  world.table_owner = 0;
  REQUIRE(b::PlayChainAbilityAnimation(16, 18, 1, world) ==
          Result::kInvalidChainAbility);
  REQUIRE(world.group_reads == 0);
  world.actors.at(0).position = {-32768, 0, -32768, 0};
  world.target = VS::Actors::MotionVector{0, 0, 0, 0};
  REQUIRE(b::PlayChainAbilityAnimation(16, 0, 1, world) ==
          Result::kNegativeSquaredDistance);
  REQUIRE(world.events.empty());
  world.actors.at(0).position = {};
  world.bone_success = false;
  REQUIRE(b::PlayChainAbilityAnimation(16, 0, 1, world) ==
          Result::kMissingRootBone);
  REQUIRE(world.Calls(Kind::kPlay).empty());
  REQUIRE(world.side == 0);
}

TEST_CASE(
    "F00831 refuses a host that loses its retained target during a bone query",
    "[action-animations][F00831]") {
  t::World world;
  world.callback = [&](const t::Event& event) {
    if (event.kind == Kind::kBone) world.target.reset();
  };
  REQUIRE(b::PlayChainAbilityAnimation(16, 0, 1, world) ==
          Result::kMissingTarget);
  REQUIRE(world.Calls(Kind::kPlay).empty());
  REQUIRE(world.side == 0);
}

TEST_CASE(
    "F00833 mode zero has its distinct 128 radius threshold and retained owner",
    "[action-animations][F00833]") {
  t::World world;
  world.actors.at(0).radius = 128;
  world.headers.at(0) = false;
  world.preset = 3;
  REQUIRE(b::PlaySpecialActionAnimation(16, 0, world) == Result::kComplete);
  REQUIRE(world.actors.at(0).turn == 20);
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 0xC1, 0, 8}});
  REQUIRE(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 0}});
}

TEST_CASE("F00833 missing FF reloads the table before the fallback helpers",
          "[action-animations][F00833]") {
  t::World world;
  world.headers.at(0) = false;
  world.preset = 4;
  world.actors.at(1).radius = 128;
  world.helper_pitch = 769;
  REQUIRE(b::PlaySpecialActionAnimation(16, 1, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kFacing).at(0).owner == 1);
  REQUIRE(world.Calls(Kind::kPitch).at(0).owner == 1);
  REQUIRE(world.actors.at(1).turn == 10);
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 0x4A, 0, 6}});
  REQUIRE(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 1}});
  REQUIRE(world.special_pitch == 77);
}

TEST_CASE(
    "F00833 present FF retains its original actor and unknown full-word modes "
    "do nothing",
    "[action-animations][F00833]") {
  t::World world;
  world.preset = 4;
  REQUIRE(b::PlaySpecialActionAnimation(16, 1, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kBone) == std::vector<t::Event>{{Kind::kBone, 5}});
  REQUIRE(world.special_pitch == 2);
  REQUIRE(world.Calls(Kind::kReset).empty());
  std::vector<std::array<unsigned, 3>> outcomes;
  for (const int mode : {-1, 2, 0x10000, 0x10001}) {
    t::World unused;
    unused.table_owner = -1;
    outcomes.push_back(
        {static_cast<unsigned>(b::PlaySpecialActionAnimation(16, mode, unused)),
         static_cast<unsigned>(unused.events.size()), unused.target_reads});
  }
  REQUIRE(outcomes == std::vector<std::array<unsigned, 3>>(4, {0, 0, 0}));
}

TEST_CASE("F00834 executes special mode zero without any FE follow-up",
          "[action-animations][F00834]") {
  t::World world;
  REQUIRE(b::PlayBreakArtAnimation(16, 0, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kHeader) ==
          std::vector<t::Event>{{Kind::kHeader, 0, 0xBF}});
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 0xBF, 0, 8}});
  REQUIRE(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 0}});
  REQUIRE(world.Calls(Kind::kQueue).empty());
}

TEST_CASE("F00834 reloads the follow-up owner after the real FF playback",
          "[action-animations][F00834]") {
  t::World world;
  world.preset = 6;
  REQUIRE(b::PlayBreakArtAnimation(16, 1, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kHeader) ==
          std::vector<t::Event>{{Kind::kHeader, 0, 0xFF},
                                {Kind::kHeader, 2, 0xFE}});
  REQUIRE(world.Calls(Kind::kPlay) ==
          std::vector<t::Event>{{Kind::kPlay, 16, 0xFF, 0, 8}});
  REQUIRE(world.Calls(Kind::kQueue) ==
          std::vector<t::Event>{{Kind::kQueue, 16, 0xFE, 8, 0}});
  REQUIRE(world.Calls(Kind::kReset).empty());
}

TEST_CASE(
    "F00834 shares the real missing-FF fallback before conditional FE queuing",
    "[action-animations][F00834]") {
  t::World world;
  world.headers = {false, false};
  world.preset = 4;
  REQUIRE(b::PlayBreakArtAnimation(16, 1, world) == Result::kComplete);
  REQUIRE(world.Calls(Kind::kFacing).at(0).owner == 1);
  REQUIRE(world.Calls(Kind::kReset) ==
          std::vector<t::Event>{{Kind::kReset, 1}});
  REQUIRE(world.Calls(Kind::kHeader) ==
          std::vector<t::Event>{{Kind::kHeader, 0, 0xFF},
                                {Kind::kHeader, 1, 0xFE}});
  REQUIRE(world.Calls(Kind::kQueue).empty());
  t::World unused;
  unused.table_owner = -1;
  REQUIRE(b::PlayBreakArtAnimation(16, 0x10001, unused) == Result::kComplete);
  REQUIRE(unused.events.empty());
}
}  // namespace
