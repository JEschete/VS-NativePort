// Hand-maintained (tools/port/owned.csv).
// Live binding for the decision behaviours and movement dispatcher
// (Decisions.cpp).
#include "Battle/Ai/DecisionsLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Movement.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(Battle_Ai_decisionCallbacks) == kDecisionBehaviorCount);
constexpr int kFleeVisibleMode = 1;
constexpr unsigned kFirstNonLocomotionCommand = 9;
constexpr int kAllyRelationship = 2;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

Battle_Ai_ActorAiState& StateOf(Battle_Ai_ActorAiState* state) {
  Require(state != nullptr, "AI decision behaviour requires an actor state");
  return *state;
}

Battle_Ai_ActorAiState& StateOf(Battle_Ai_DecisionDispatchView* view) {
  return StateOf(reinterpret_cast<Battle_Ai_ActorAiState*>(view));
}

class LiveDecisionActor final : public DecisionActor {
 public:
  explicit LiveDecisionActor(Battle_Ai_ActorAiState& state)
      : state_(state),
        settings_(*reinterpret_cast<Battle_Ai_SettingsView*>(&state)) {}

  [[nodiscard]] bool Fleeing() const override { return state_.opaqueE != 0; }
  [[nodiscard]] unsigned ActionStatus() const override {
    return state_.actionStatus;
  }
  int FleeVisibleEnemies() override {
    return FleeLiveVisibleEnemies(
        reinterpret_cast<Battle_Ai_CandidateMovementView*>(&state_),
        kFleeVisibleMode);
  }
  void WanderAroundSavedCenter() override {
    WanderAroundLiveSavedCenter(&state_);
  }
  int UpdateCombatPursuit() override {
    return Battle_Ai_UpdateCombatPursuit(&state_);
  }
  void PatrolWaypoints() override { PatrolLiveWaypoints(&state_); }
  [[nodiscard]] std::optional<PursuitCandidate> BestCandidate() const override {
    const Battle_Ai_ActionCandidate* candidate = state_.candidateOrder[0];
    if (candidate == nullptr || state_.model == nullptr) {
      return std::nullopt;
    }
    return PursuitCandidate{.target = static_cast<int>(candidate->targetId),
                            .distance_squared = static_cast<std::uint32_t>(
                                candidate->distanceSquared),
                            .extended_reach = state_.model->opaqueABit5 != 0};
  }
  int ApproachActor(int target, int distance) override {
    return ApproachLiveActor(&state_, target, distance, 0);
  }
  int TrackActorPosition(int target) override {
    return TrackLiveActorPosition(&state_, target);
  }
  std::optional<SupportTarget> FindSupportTarget() override {
    std::uint32_t tile = 0;
    const int target =
        FindLiveActorByRelationship(&state_, &tile, kAllyRelationship);
    if (target < 0 || static_cast<std::size_t>(target) >=
                          std::size(state_.distancesSquared)) {
      return std::nullopt;
    }
    return SupportTarget{
        .tile = tile,
        .distance_squared =
            state_.distancesSquared[static_cast<std::size_t>(target)]};
  }
  void MoveToTile(unsigned tile) override {
    MoveLiveActorToTile(
        reinterpret_cast<Battle_Ai_CandidateMovementView*>(&state_), tile, 0);
  }
  void WanderAroundTile(unsigned tile, int radius) override {
    WanderAroundLiveTile(
        reinterpret_cast<Battle_Ai_CandidateMovementView*>(&state_), tile,
        radius);
  }
  [[nodiscard]] bool EngagementReady() override {
    return IsLiveEngagementReady(&state_);
  }
  [[nodiscard]] int BehaviorMode() const override {
    return state_.behaviorMode;
  }
  void ClearBehaviorMode() override { state_.behaviorMode = 0; }
  [[nodiscard]] bool CommandIsLocomotion() const override {
    const auto* actor = reinterpret_cast<const Battle_Actor*>(state_.actor);
    Require(actor != nullptr, "AI locomotion update requires a battle actor");
    return actor->commandOpcode < kFirstNonLocomotionCommand;
  }
  void ReleaseReservations() override {
    ReleaseLiveActorAiReservations(&state_);
  }
  void IssueCommand(int command) override {
    IssueLiveActorCommand(&state_, command);
  }
  bool TryExecutePlannedActions() override {
    return Battle_Ai_TryExecutePlannedActions(&state_) != 0;
  }
  bool TryRecoverBlockedMovement() override {
    return Battle_Ai_TryRecoverBlockedMovement(
               reinterpret_cast<Battle_Ai_BlockedRecoveryView*>(&state_)) != 0;
  }
  bool RunDecisionBehavior() override {
    return settings_.decisionCallback != nullptr &&
           settings_.decisionCallback(&state_) != 0;
  }

 private:
  Battle_Ai_ActorAiState& state_;
  Battle_Ai_SettingsView& settings_;
};

template <typename Behavior>
int RunBehavior(Battle_Ai_ActorAiState* state, Behavior behavior) {
  LiveDecisionActor actor(StateOf(state));
  return behavior(actor);
}
}  // namespace

int UpdateLivePatrolCombatBehavior(Battle_Ai_ActorAiState* state) {
  return RunBehavior(state, UpdatePatrolCombatBehavior);
}

int UpdateLiveFleeOrWanderBehavior(Battle_Ai_ActorAiState* state) {
  return RunBehavior(state, UpdateFleeOrWanderBehavior);
}

int UpdateLiveAdaptivePursuitBehavior(Battle_Ai_ActorAiState* state) {
  return RunBehavior(state, UpdateAdaptivePursuitBehavior);
}

int UpdateLiveSupportFollowBehavior(Battle_Ai_ActorAiState* state) {
  return RunBehavior(state, UpdateSupportFollowBehavior);
}

int UpdateLiveEngagementApproachBehavior(Battle_Ai_ActorAiState* state) {
  return RunBehavior(state, UpdateEngagementApproachBehavior);
}

void UpdateLiveActorLocomotionMode(Battle_Ai_DecisionDispatchView* view) {
  LiveDecisionActor actor(StateOf(view));
  UpdateActorLocomotionMode(actor);
}

void DispatchLiveMovementBehavior(Battle_Ai_DecisionDispatchView* view) {
  LiveDecisionActor actor(StateOf(view));
  DispatchMovementBehavior(actor);
}

LiveDecisionBehavior SelectLiveDecisionBehavior(std::uint32_t spawn_settings) {
  const std::optional<unsigned> slot = DecisionBehaviorSlot(spawn_settings);
  if (!slot) {
    Core_Warnf("ai",
               "decision behaviour index %u is past the %u-entry table; the "
               "actor wanders instead (#272)",
               DecisionBehaviorIndex(spawn_settings), kDecisionBehaviorCount);
    return nullptr;
  }
  return Battle_Ai_decisionCallbacks[*slot];
}
}  // namespace VS::Battle::Ai
