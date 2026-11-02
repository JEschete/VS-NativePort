#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/FrameUpdate.hpp"

namespace {
using VS::Battle::Ai::ActorAiFrame;
using VS::Battle::Ai::FrameUpdateResult;

class RecordingFrame final : public ActorAiFrame {
 public:
  bool tile_flags = true;
  bool room_and_actors = true;
  std::uint32_t mask = 0;
  std::uint32_t target = 0;
  std::vector<std::string> calls;

  [[nodiscard]] bool HasTileFlags() const override { return tile_flags; }
  [[nodiscard]] bool HasRoomAndActors() const override {
    return room_and_actors;
  }
  void CacheWorkingState() override { calls.emplace_back("cache"); }
  [[nodiscard]] std::uint32_t AttributeActorMask() const override {
    return mask;
  }
  [[nodiscard]] std::uint32_t PerceptionTarget() const override {
    return target;
  }
  void SetPerceptionTarget(std::uint32_t value) override {
    calls.push_back("target=" + std::to_string(value));
    target = value;
  }
  void UpdatePerception() override { calls.emplace_back("perception"); }
  void UpdateDecisions() override { calls.emplace_back("decisions"); }
  void AnimateEngagement() override { calls.emplace_back("engagement"); }
  void SaveRandomState() override { calls.emplace_back("random"); }
};
}  // namespace

TEST_CASE("Frame update ticks decision timers down to zero",
          "[ai][frame-update]") {
  VS::Battle::Ai::DecisionTimers timers{2, 0};
  VS::Battle::Ai::TickDecisionTimers(timers);
  CHECK(timers.decision_delay == 1);
  CHECK(timers.min_action_delay == 0);
  timers = {0, 1};
  VS::Battle::Ai::TickDecisionTimers(timers);
  CHECK(timers.decision_delay == 0);
  CHECK(timers.min_action_delay == 0);
}

TEST_CASE("Frame update selects locomotion from the folded flags",
          "[ai][frame-update]") {
  using VS::Battle::Ai::SelectLocomotion;
  CHECK(SelectLocomotion(0x0000).mode == 0);
  CHECK(SelectLocomotion(0x0010).mode == 1);
  CHECK(SelectLocomotion(0x2000).mode == 1);
  CHECK(SelectLocomotion(0x0800).mode == 2);
  CHECK(SelectLocomotion(0x0838).mode == 1);
  const auto folded = SelectLocomotion(0x1203);
  CHECK(folded.movement_flags == (0x1203 | 0x12));
  CHECK(folded.mode == 1);
}

TEST_CASE("Frame update packs vertical steps as the original byte pair",
          "[ai][frame-update]") {
  using VS::Battle::Ai::MakeVerticalMovementSteps;
  auto steps = MakeVerticalMovementSteps(8, 12);
  CHECK(steps.walk_step == 8);
  CHECK(steps.run_step == 12);
  CHECK(steps.max_range == 12);
  steps = MakeVerticalMovementSteps(16, 6);
  CHECK(steps.max_range == 16);
  // A walk step above a byte carries into the run byte, as the packed store.
  steps = MakeVerticalMovementSteps(0x1FF, 2);
  CHECK(steps.walk_step == 0xFF);
  CHECK(steps.run_step == 3);
  CHECK(steps.max_range == 0xFF);
  steps = MakeVerticalMovementSteps(-1, 4);
  CHECK(steps.max_range == 4);
}

TEST_CASE("Frame update rotates the second perception target",
          "[ai][frame-update]") {
  using VS::Battle::Ai::NextPerceptionTarget;
  CHECK(NextPerceptionTarget(0, 7) == 0);
  CHECK(NextPerceptionTarget(0x10000, 7) == 0);
  CHECK(NextPerceptionTarget(0b1010, 1) == 3);
  CHECK(NextPerceptionTarget(0b1010, 3) == 1);
  CHECK(NextPerceptionTarget(0b0100, 2) == 2);
  CHECK(NextPerceptionTarget(0x8001, 0x2F) == 0);
}

TEST_CASE("Frame update locates the terrain region bitmap",
          "[ai][frame-update]") {
  using VS::Battle::Ai::kTerrainRegionOffset;
  using VS::Battle::Ai::LocateTerrainRegions;
  using VS::Battle::Ai::TerrainRegionWindow;
  CHECK_FALSE(LocateTerrainRegions(kTerrainRegionOffset - 1).has_value());
  const auto empty = LocateTerrainRegions(kTerrainRegionOffset);
  REQUIRE(empty.has_value());
  CHECK(empty.value_or(TerrainRegionWindow{}).offset == kTerrainRegionOffset);
  CHECK(empty.value_or(TerrainRegionWindow{1, 1}).count == 0);
  const auto window = LocateTerrainRegions(kTerrainRegionOffset + 0x400);
  REQUIRE(window.has_value());
  CHECK(window.value_or(TerrainRegionWindow{}).count == 0x400);
}

TEST_CASE("Frame update packs the room extent little-endian",
          "[ai][frame-update]") {
  CHECK(VS::Battle::Ai::PackRoomBounds({0x20, 0x18, 0x00, 0x00}) == 0x1820u);
  CHECK(VS::Battle::Ai::PackRoomBounds({1, 2, 3, 4}) == 0x04030201u);
}

TEST_CASE("Frame update runs the AI stages in order", "[ai][frame-update]") {
  RecordingFrame frame;
  frame.mask = 0b1000'0010;
  frame.target = 1;
  CHECK(VS::Battle::Ai::RunActorAiUpdateStages(frame) ==
        FrameUpdateResult::kUpdated);
  CHECK(frame.calls == std::vector<std::string>{"cache", "target=7",
                                                "perception", "decisions",
                                                "engagement", "random"});
}

TEST_CASE(
    "Frame update skips a frame without a room map and refuses one "
    "without a room or actors",
    "[ai][frame-update]") {
  RecordingFrame frame;
  frame.tile_flags = false;
  frame.room_and_actors = false;
  CHECK(VS::Battle::Ai::RunActorAiUpdateStages(frame) ==
        FrameUpdateResult::kNoRoomMap);
  CHECK(frame.calls.empty());
  frame.tile_flags = true;
  CHECK(VS::Battle::Ai::RunActorAiUpdateStages(frame) ==
        FrameUpdateResult::kMissingRoomOrActors);
  CHECK(frame.calls.empty());
}
