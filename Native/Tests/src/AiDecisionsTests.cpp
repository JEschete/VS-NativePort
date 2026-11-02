#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Ai/Decisions.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Legacy/Psx.h"

extern "C" {
#include "Battle/Ai/ActorState.h"
}

namespace {
namespace Ai = VS::Battle::Ai;

// Records each primitive a behaviour issues, in order, with its arguments.
class FakeDecisionActor final : public Ai::DecisionActor {
 public:
  bool fleeing = false;
  unsigned status = 0;
  int flee_result = 0;
  int pursuit_result = 0;
  int approach_result = 0;
  int track_result = 0;
  std::optional<Ai::PursuitCandidate> candidate;
  std::optional<Ai::SupportTarget> ally;
  bool engagement_ready = false;
  int mode = 0;
  bool walking_command = false;
  bool planned = false;
  bool recovered = false;
  bool behavior = false;
  std::vector<std::string> calls;

  [[nodiscard]] bool Fleeing() const override { return fleeing; }
  [[nodiscard]] unsigned ActionStatus() const override { return status; }
  int FleeVisibleEnemies() override {
    calls.emplace_back("flee");
    return flee_result;
  }
  void WanderAroundSavedCenter() override { calls.emplace_back("wander"); }
  int UpdateCombatPursuit() override {
    calls.emplace_back("pursue");
    return pursuit_result;
  }
  void PatrolWaypoints() override { calls.emplace_back("patrol"); }
  [[nodiscard]] std::optional<Ai::PursuitCandidate> BestCandidate()
      const override {
    return candidate;
  }
  int ApproachActor(int target, int distance) override {
    calls.push_back("approach " + std::to_string(target) + " " +
                    std::to_string(distance));
    return approach_result;
  }
  int TrackActorPosition(int target) override {
    calls.push_back("track " + std::to_string(target));
    return track_result;
  }
  std::optional<Ai::SupportTarget> FindSupportTarget() override {
    calls.emplace_back("find ally");
    return ally;
  }
  void MoveToTile(unsigned tile) override {
    calls.push_back("move " + std::to_string(tile));
  }
  void WanderAroundTile(unsigned tile, int radius) override {
    calls.push_back("wander " + std::to_string(tile) + " " +
                    std::to_string(radius));
  }
  [[nodiscard]] bool EngagementReady() override { return engagement_ready; }
  [[nodiscard]] int BehaviorMode() const override { return mode; }
  void ClearBehaviorMode() override {
    calls.emplace_back("clear mode");
    mode = 0;
  }
  [[nodiscard]] bool CommandIsLocomotion() const override {
    return walking_command;
  }
  void ReleaseReservations() override { calls.emplace_back("release"); }
  void IssueCommand(int command) override {
    calls.push_back("command " + std::to_string(command));
  }
  bool TryExecutePlannedActions() override {
    calls.emplace_back("planned");
    return planned;
  }
  bool TryRecoverBlockedMovement() override {
    calls.emplace_back("recover");
    return recovered;
  }
  bool RunDecisionBehavior() override {
    calls.emplace_back("behaviour");
    return behavior;
  }
};

using Calls = std::vector<std::string>;
}  // namespace

TEST_CASE("AI patrol combat flees or wanders while fleeing",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.fleeing = true;
  CHECK(Ai::UpdatePatrolCombatBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"flee", "wander"});
  actor.calls.clear();
  actor.flee_result = 1;
  CHECK(Ai::UpdatePatrolCombatBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"flee"});
}

TEST_CASE("AI patrol combat pursues and otherwise patrols", "[ai][decisions]") {
  FakeDecisionActor actor;
  CHECK(Ai::UpdatePatrolCombatBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"pursue", "patrol"});
  actor.calls.clear();
  actor.pursuit_result = 7;
  CHECK(Ai::UpdatePatrolCombatBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"pursue"});
}

TEST_CASE("AI flee-or-wander returns the pursuit result when not fleeing",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.pursuit_result = 5;
  CHECK(Ai::UpdateFleeOrWanderBehavior(actor) == 5);
  CHECK(actor.calls == Calls{"pursue"});
  actor.calls.clear();
  actor.fleeing = true;
  CHECK(Ai::UpdateFleeOrWanderBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"flee", "wander"});
}

TEST_CASE("AI adaptive pursuit flees or pursues outside approach statuses",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.fleeing = true;
  actor.flee_result = 3;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 3);
  CHECK(actor.calls == Calls{"flee"});

  actor = FakeDecisionActor{};
  actor.status = 2;
  actor.pursuit_result = 4;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 4);
  CHECK(actor.calls == Calls{"pursue"});

  // Only the low five status bits select the approach mode.
  actor = FakeDecisionActor{};
  actor.status = 32 + 3;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 0);
  CHECK(actor.calls == Calls{"pursue"});
}

TEST_CASE("AI adaptive pursuit needs a candidate to approach",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.status = 1;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 0);
  CHECK(actor.calls.empty());
}

TEST_CASE("AI adaptive pursuit approaches within the model reach",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.status = 1;
  actor.candidate = Ai::PursuitCandidate{
      .target = 3, .distance_squared = 100, .extended_reach = false};
  actor.approach_result = 9;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 9);
  CHECK(actor.calls == Calls{"approach 3 256"});

  actor.calls.clear();
  actor.candidate->extended_reach = true;
  actor.approach_result = 0;
  actor.track_result = 6;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 6);
  CHECK(actor.calls == Calls{"approach 3 320", "track 3"});
}

TEST_CASE("AI adaptive pursuit status 0 caps the approach at the distance",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.candidate = Ai::PursuitCandidate{
      .target = 2, .distance_squared = 100 * 100, .extended_reach = true};
  actor.approach_result = 1;
  // The original square-root table resolves 100 squared to 99.
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"approach 2 99"});

  actor.calls.clear();
  actor.candidate->distance_squared = 400 * 400;
  CHECK(Ai::UpdateAdaptivePursuitBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"approach 2 320"});
}

TEST_CASE("AI support follow walks to a distant ally and wanders near it",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.ally = Ai::SupportTarget{
      .tile = 0x1234, .distance_squared = Ai::kSupportDistanceSquared};
  CHECK(Ai::UpdateSupportFollowBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"pursue", "find ally", "wander 4660 5"});

  actor.calls.clear();
  actor.ally->distance_squared = Ai::kSupportDistanceSquared + 1;
  CHECK(Ai::UpdateSupportFollowBehavior(actor) == 1);
  CHECK(actor.calls == Calls{"pursue", "find ally", "move 4660"});
}

TEST_CASE("AI support follow keeps a pursuit and refuses a missing ally",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.pursuit_result = 2;
  CHECK(Ai::UpdateSupportFollowBehavior(actor) == 2);
  CHECK(actor.calls == Calls{"pursue"});

  actor = FakeDecisionActor{};
  CHECK(Ai::UpdateSupportFollowBehavior(actor) == 0);
  CHECK(actor.calls == Calls{"pursue", "find ally"});
}

TEST_CASE("AI engagement approach pursues once ready, else nears Ashley",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.track_result = 4;
  CHECK(Ai::UpdateEngagementApproachBehavior(actor) == 4);
  CHECK(actor.calls == Calls{"approach 0 256", "track 0"});

  actor = FakeDecisionActor{};
  actor.engagement_ready = true;
  actor.pursuit_result = 8;
  CHECK(Ai::UpdateEngagementApproachBehavior(actor) == 8);
  CHECK(actor.calls == Calls{"pursue"});

  actor = FakeDecisionActor{};
  actor.engagement_ready = true;
  actor.approach_result = 3;
  CHECK(Ai::UpdateEngagementApproachBehavior(actor) == 3);
  CHECK(actor.calls == Calls{"pursue", "approach 0 256"});
}

TEST_CASE("AI locomotion keeps a walking actor moving", "[ai][decisions]") {
  FakeDecisionActor actor;
  Ai::UpdateActorLocomotionMode(actor);
  CHECK(actor.calls.empty());

  actor.mode = 1;
  actor.walking_command = true;
  Ai::UpdateActorLocomotionMode(actor);
  CHECK(actor.calls == Calls{"command 1"});
  CHECK(actor.mode == 1);
}

TEST_CASE("AI locomotion stops an actor whose behaviour mode ended",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.mode = 1;
  Ai::UpdateActorLocomotionMode(actor);
  CHECK(actor.calls == Calls{"release", "clear mode", "command 2"});
  CHECK(actor.mode == 0);

  actor = FakeDecisionActor{};
  actor.mode = 2;
  actor.walking_command = true;
  Ai::UpdateActorLocomotionMode(actor);
  CHECK(actor.calls == Calls{"release", "clear mode", "command 2"});
}

TEST_CASE("AI movement dispatch stops at the first handled stage",
          "[ai][decisions]") {
  FakeDecisionActor actor;
  actor.planned = true;
  Ai::DispatchMovementBehavior(actor);
  CHECK(actor.calls == Calls{"planned"});

  actor = FakeDecisionActor{};
  actor.recovered = true;
  Ai::DispatchMovementBehavior(actor);
  CHECK(actor.calls == Calls{"planned", "recover"});

  actor = FakeDecisionActor{};
  actor.behavior = true;
  Ai::DispatchMovementBehavior(actor);
  CHECK(actor.calls == Calls{"planned", "recover", "behaviour"});

  actor = FakeDecisionActor{};
  Ai::DispatchMovementBehavior(actor);
  CHECK(actor.calls == Calls{"planned", "recover", "behaviour", "wander"});
}

TEST_CASE("AI decision behaviour slots stay within the nine-entry table",
          "[ai][decisions]") {
  // Shipped spawns select slots 0, 1, 3 and 4 (#272).
  STATIC_REQUIRE(Ai::DecisionBehaviorSlot(0x00000000U) == 0U);
  STATIC_REQUIRE(Ai::DecisionBehaviorSlot(0x00000030U) == 3U);
  STATIC_REQUIRE(Ai::DecisionBehaviorSlot(0xFFFFFE8FU) == 8U);
  STATIC_REQUIRE(Ai::DecisionBehaviorIndex(0x000001F0U) == 31U);
  for (std::uint32_t index = 9; index < 32; ++index) {
    CHECK_FALSE(Ai::DecisionBehaviorSlot(index << 4).has_value());
  }
}

namespace {
int TestDecisionBehavior(Battle_Ai_ActorAiState* /*state*/) { return 1; }
}  // namespace

TEST_CASE("AI live behaviour selection refuses indices past the table",
          "[ai][decisions]") {
  std::array<Ai::LiveDecisionBehavior, Ai::kDecisionBehaviorCount> saved{};
  std::copy(std::begin(Battle_Ai_decisionCallbacks),
            std::end(Battle_Ai_decisionCallbacks), saved.begin());
  std::fill(std::begin(Battle_Ai_decisionCallbacks),
            std::end(Battle_Ai_decisionCallbacks), nullptr);
  Battle_Ai_decisionCallbacks[8] = TestDecisionBehavior;
  CHECK(Ai::SelectLiveDecisionBehavior(8U << 4) == TestDecisionBehavior);
  CHECK(Ai::SelectLiveDecisionBehavior(9U << 4) == nullptr);
  CHECK(Ai::SelectLiveDecisionBehavior(31U << 4) == nullptr);
  std::copy(saved.begin(), saved.end(),
            std::begin(Battle_Ai_decisionCallbacks));
}
