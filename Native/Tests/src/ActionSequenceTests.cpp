#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "TestSupport/ActionSequenceFixture.hpp"

namespace VS::test_support::sequence {
namespace {
namespace b = VS::Battle;

TEST_CASE("F00187 short circuits each effect readiness gate",
          "[action-sequence][F00187]") {
  const auto gate = GENERATE(0, 1, 2, 3, 4, 5);
  World world;
  world.pending = gate == 0 ? -1 : 0;
  world.flags[0] = gate == 1 ? 0x08000000 : gate == 2 ? 0x100 : 0x88000101;
  world.free_asset = gate == 3 ? -1 : 0;
  world.can_start = gate == 4 ? 0 : -65536;
  b::WaitForActionEffectReady(world);
  const std::array<std::vector<Word>, 6> expected{
      std::vector<Word>{kNotification},
      {kNotification, kFlags, 0},
      {kNotification, kFlags, 0},
      {kNotification, kFlags, 0, kAsset},
      {kNotification, kFlags, 0, kAsset, kCanStart},
      {kNotification, kFlags, 0, kAsset, kCanStart, kAdvance}};
  CHECK(world.events == expected.at(gate));
  CHECK(world.queued_reads == 1);
}
TEST_CASE("F00187 retains the queue owner before the notification callback",
          "[action-sequence][F00187]") {
  World world;
  world.Current().index = 0xFFFFFFFF;
  world.flags[41] = 0x08000100;
  world.callback = [&](Word address) {
    if (address == kNotification) {
      world.states[0].queue[3].data.primary.actor = 41;
      world.current = 1;
      world.Current().queue[0].data.primary.actor = 2;
    }
  };
  b::WaitForActionEffectReady(world);
  CHECK(world.events == std::vector<Word>{kNotification, kFlags, 41, kAsset,
                                          kCanStart, kAdvance});
  CHECK(world.current == 1);
}
TEST_CASE("F00188 reads busy bit two once and leaves unused owners alone",
          "[action-sequence][F00188]") {
  const auto flags = GENERATE(0U, 1U, 2U, 0xFFFFFFFDU, 0xFFFFFFFFU);
  World world;
  world.effect_flags = flags;
  world.callback = [&](Word address) {
    if (address == kAnimation) world.effect_flags = 2;
  };
  b::StartReadyActionAnimation(world);
  const auto expected = (flags & 2U) != 0
                            ? std::vector<Word>{}
                            : std::vector<Word>{kAnimation, kTrack, 0};
  CHECK(world.events == expected);
  CHECK(world.queued_reads == 0);
  CHECK(world.chain_reads == 0);
}
TEST_CASE(
    "F00192 tests the full planning result and uses current phase services",
    "[action-sequence][F00192]") {
  const int result = GENERATE(0, 1, -1, 65536, -65536);
  World world;
  world.Current().index = 0xFFFFFFFE;
  world.plan_result = result;
  world.callback = [&](Word address) {
    if (address == kPlans) world.current = 1;
    if (address == kRelease) world.Current().queue[0].data.primary.actor = 41;
  };
  b::WaitForActionResolution(world);
  const auto expected = result == 0
                            ? std::vector<Word>{kPlans, 2, kRelease, kFade, 41}
                            : std::vector<Word>{kPlans, 2};
  CHECK(world.events == expected);
  CHECK(world.queued_reads == 1);
}
TEST_CASE(
    "F00193 refuses an empty recovery phase without inventing a last owner",
    "[action-sequence][F00193]") {
  World world;
  CHECK(b::WaitForTargetRecovery(world) ==
        b::TargetRecoveryStatus::kInvalidSequence);
  CHECK(world.owners.empty());
  CHECK(world.events.empty());
}
TEST_CASE("F00193 readiness is unsigned and sticky but visits every actor",
          "[action-sequence][F00193]") {
  const int busy = GENERATE(-2, 1, std::numeric_limits<int>::min(),
                            std::numeric_limits<int>::max());
  World world;
  world.Current().count = 1;
  auto& action = world.Current().queue[0].data;
  action.count = 3;
  action.targets[0].actor = 1;
  action.targets[1].actor = 2;
  action.targets[2].actor = 3;
  world.busy[1] = busy;
  CHECK(b::WaitForTargetRecovery(world) == b::TargetRecoveryStatus::kWaiting);
  CHECK(world.events == std::vector<Word>{kBusy, 1, kBusy, 2, kBusy, 3});
}
TEST_CASE("F00193 repeats duplicate defeats and ignores the primary return",
          "[action-sequence][F00193]") {
  const int death = GENERATE(0, -65536);
  World world;
  world.Current().count = 1;
  auto& action = world.Current().queue[0].data;
  action.count = 3;
  action.targets[0].actor = 1;
  action.targets[1].actor = 1;
  action.targets[2].position_kind = -1;
  world.busy[1] = -1;
  world.defeats[0] = 17;
  world.defeats[1] = death;
  CHECK(b::WaitForTargetRecovery(world) == b::TargetRecoveryStatus::kStarted);
  CHECK(world.events == std::vector<Word>{kBusy, 1, kBusy, 1, kDefeat, 0,
                                          kDefeat, 1, kDefeat, 1, kRecovery,
                                          death == 0 ? 1U : 2U});
}
TEST_CASE(
    "F00193 rereads the sequence and inner count while retaining each owner",
    "[action-sequence][F00193]") {
  World world;
  world.Current().count = 5;
  auto& action = world.Current().queue[0].data;
  action.count = 2;
  action.targets[0].actor = 1;
  action.targets[1].actor = 2;
  action.targets[2].actor = 3;
  for (auto& queue : world.states[1].queue) {
    queue.data.primary.position_kind = 1;
  }
  world.states[1].count = 6;
  world.callback = [&](Word address) {
    if (address == kBusy) {
      world.current = 1;
      action.count = 3;
    }
  };
  CHECK(b::WaitForTargetRecovery(world) == b::TargetRecoveryStatus::kStarted);
  CHECK(world.owners == std::vector<Word>{0, 5, 6, 7, 4, 5, 4, 5, 6, 7, 4, 5});
  CHECK(world.events ==
        std::vector<Word>{kBusy, 1, kBusy, 2, kBusy, 3, kRecovery, 1});
}
TEST_CASE(
    "F00193 retains the first-pass last owner when callbacks empty the chain",
    "[action-sequence][F00193]") {
  const auto primary = GENERATE(0U, 7U);
  World world;
  world.Current().count = 1;
  auto& action = world.Current().queue[0].data;
  action.count = 1;
  action.primary.actor = static_cast<std::uint8_t>(primary);
  action.primary.position_kind = 1;
  world.callback = [&](Word address) {
    if (address == kBusy) world.current = 1;
  };
  CHECK(b::WaitForTargetRecovery(world) == b::TargetRecoveryStatus::kStarted);
  CHECK(world.owners == std::vector<Word>{0});
  CHECK(world.events ==
        std::vector<Word>{kBusy, 0, kRecovery, primary == 0 ? 1U : 0U});
}
TEST_CASE(
    "F00193 defeat callbacks change cached targets and the final world source "
    "byte",
    "[action-sequence][F00193]") {
  World world;
  world.Current().count = 1;
  auto& action = world.Current().queue[0].data;
  action.count = 1;
  action.targets[0].actor = 1;
  action.targets[1].actor = 1;
  action.targets[2].actor = 2;
  world.defeats[1] = -1;
  world.callback = [&](Word address) {
    if (address == kDefeat) {
      world.current = 1;
      action.count = 3;
      action.primary.actor = 9;
      action.primary.position_kind = 1;
    }
  };
  CHECK(b::WaitForTargetRecovery(world) == b::TargetRecoveryStatus::kStarted);
  CHECK(world.events == std::vector<Word>{kBusy, 1, kDefeat, 0, kDefeat, 1,
                                          kDefeat, 1, kDefeat, 2, kRecovery,
                                          0});
}
TEST_CASE("F00195 restores projection by sixty-four and signed distance by192",
          "[action-sequence][F00195]") {
  const auto pair =
      GENERATE(std::array{511, 512}, std::array{512, 512}, std::array{513, 512},
               std::array{577, 513}, std::array{-2147483647, -2147483583},
               std::array{2147483647, 2147483583});
  const auto mode = GENERATE(1, 3);
  World world;
  world.camera_mode = mode;
  world.projection = pair[0];
  world.distance = -32600;
  world.Current().preferred = -32768;
  world.look = 0;
  b::FinishActionSequence(world);
  CHECK(world.projection == pair[1]);
  CHECK(world.distance == -32768);
  CHECK(std::ranges::count(world.events, kProjection) ==
        (pair[0] == 512 ? 0 : 1));
}
TEST_CASE(
    "F00195 projection callbacks replace the current camera and preferred "
    "owner",
    "[action-sequence][F00195]") {
  World world;
  world.camera_mode = 1;
  world.projection = 513;
  world.states[1].preferred = 100;
  world.callback = [&](Word address) {
    if (address == kProjection) {
      world.current = 1;
      world.distance = 300;
      world.projection = 511;
      world.camera_mode = 0;
    }
  };
  b::FinishActionSequence(world);
  CHECK(world.events == std::vector<Word>{kProjection, 512});
  CHECK(world.distance == 108);
  CHECK(world.preferred_reads == 1);
}
TEST_CASE("F00195 other camera modes skip restoration but not settled cleanup",
          "[action-sequence][F00195]") {
  const auto mode = GENERATE(0, 2, -1, 4);
  World world;
  world.camera_mode = mode;
  world.projection = 511;
  b::FinishActionSequence(world);
  CHECK(world.preferred_reads == 0);
  CHECK(world.chain_reads == 0);
  world.projection = 512;
  b::FinishActionSequence(world);
  CHECK(world.events == std::vector<Word>{kLook, kResume, kHud, 0, kIdle, 0});
  CHECK(world.owners.empty());
}
TEST_CASE("F00195 waits for every target before any cleanup",
          "[action-sequence][F00195]") {
  World world;
  world.Current().count = 1;
  auto& action = world.Current().queue[0].data;
  action.count = 3;
  action.targets[0].actor = 1;
  action.targets[1].position_kind = 1;
  action.targets[2].actor = 2;
  world.busy[1] = -2;
  b::FinishActionSequence(world);
  CHECK(world.events == std::vector<Word>{kLook, kBusy, 1, kBusy, 2});
  CHECK(world.selected_reads == 0);
}
TEST_CASE(
    "F00195 cleanup rereads selection then runs separate target and source "
    "passes",
    "[action-sequence][F00195]") {
  World world;
  world.Current().count = 1;
  auto& action = world.Current().queue[0].data;
  action.primary.actor = 4;
  action.count = 2;
  action.targets[0].actor = 1;
  action.targets[1].actor = 1;
  action.targets[2].actor = 2;
  world.states[1].count = 1;
  world.states[1].queue[0].data.primary.actor = 7;
  world.callback = [&](Word address) {
    if (address == kHud) world.Current().selected = 41;
    if (address == kResolve) {
      world.current = 1;
      action.count = 3;
      action.primary.actor = 5;
    }
  };
  b::FinishActionSequence(world);
  CHECK(world.events ==
        std::vector<Word>{
            kLook,    kBusy,    1, kBusy, 1,        kResume,  kHud, 0, kIdle,
            41,       kResolve, 4, 1,     1,        kResolve, 5,    1, 1,
            kResolve, 5,        2, 1,     kResolve, 7,        7,    1});
  CHECK(world.owners == std::vector<Word>{0, 0, 4});
}
TEST_CASE(
    "F00195 field resume and HUD callbacks precede current selected actor "
    "lookup",
    "[action-sequence][F00195]") {
  World world;
  world.callback = [&](Word address) {
    if (address == kResume) world.current = 1;
    if (address == kHud) world.Current().selected = 41;
  };
  b::FinishActionSequence(world);
  CHECK(world.events == std::vector<Word>{kLook, kResume, kHud, 0, kIdle, 41});
  CHECK(world.selected_reads == 1);
}
TEST_CASE(
    "F00195 interprets the event result as signed low16 without a mode recheck",
    "[action-sequence][F00195]") {
  const auto result = GENERATE(1, 0x10001, -65535, 0, 2, -1, 65536);
  World world;
  world.control = 0;
  world.event_result = result;
  world.callback = [&](Word address) {
    if (address == kEvent) world.control = 3;
  };
  b::FinishActionSequence(world);
  const Word destination =
      static_cast<std::uint16_t>(result) == 1 ? kCutscene : kLoot;
  CHECK(world.events == std::vector<Word>{kLook, kResume, kHud, 0, kIdle, 0,
                                          kEvent, 12, 0, 0, 0, destination});
  CHECK(world.control == 3);
}
}  // namespace
}  // namespace VS::test_support::sequence
