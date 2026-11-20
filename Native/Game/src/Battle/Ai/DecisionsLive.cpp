// Hand-maintained (tools/port/owned.csv).
// Live binding for the decision behaviours and movement dispatcher
// (Decisions.cpp).
#include "Battle/Ai/DecisionsLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionExecutionLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Battle/Ai/CandidateMovementLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Core/Diagnostics.hpp"

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(VS::Battle::Ai::decisionCallbacks) ==
              kDecisionBehaviorCount);
constexpr int kFleeVisibleMode = 1;
constexpr unsigned kFirstNonLocomotionCommand = 9;
constexpr int kAllyRelationship = 2;

VS::Battle::Ai::ActorAiState& StateOf(VS::Battle::Ai::ActorAiState* state) {
  Require(state != nullptr, "AI decision behaviour requires an actor state");
  return *state;
}

VS::Battle::Ai::ActorAiState& StateOf(
    VS::Battle::Ai::DecisionDispatchView* view) {
  return StateOf(reinterpret_cast<VS::Battle::Ai::ActorAiState*>(view));
}

class LiveDecisionActor final : public DecisionActor {
 public:
  explicit LiveDecisionActor(VS::Battle::Ai::ActorAiState& state)
      : state_(state),
        settings_(*reinterpret_cast<VS::Battle::Ai::SettingsView*>(&state)) {}

  [[nodiscard]] bool Fleeing() const override {
    return state_.should_flee != 0;
  }
  [[nodiscard]] unsigned ActionStatus() const override {
    return state_.action_status;
  }
  int FleeVisibleEnemies() override {
    return FleeLiveVisibleEnemies(
        reinterpret_cast<VS::Battle::Ai::CandidateMovementView*>(&state_),
        kFleeVisibleMode);
  }
  void WanderAroundSavedCenter() override {
    WanderAroundLiveSavedCenter(&state_);
  }
  int UpdateCombatPursuit() override {
    return UpdateLiveCombatPursuit(&state_);
  }
  void PatrolWaypoints() override { PatrolLiveWaypoints(&state_); }
  [[nodiscard]] std::optional<PursuitCandidate> BestCandidate() const override {
    const VS::Battle::Ai::ActionCandidateRecord* candidate =
        state_.candidate_order[0];
    if (candidate == nullptr || state_.model == nullptr) {
      return std::nullopt;
    }
    return PursuitCandidate{.target = static_cast<int>(candidate->target_id),
                            .distance_squared = static_cast<std::uint32_t>(
                                candidate->distance_squared),
                            .extended_reach = state_.model->flies != 0};
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
                          std::size(state_.distances_squared)) {
      return std::nullopt;
    }
    return SupportTarget{
        .tile = tile,
        .distance_squared =
            state_.distances_squared[static_cast<std::size_t>(target)]};
  }
  void MoveToTile(unsigned tile) override {
    MoveLiveActorToTile(
        reinterpret_cast<VS::Battle::Ai::CandidateMovementView*>(&state_), tile,
        0);
  }
  void WanderAroundTile(unsigned tile, int radius) override {
    WanderAroundLiveTile(
        reinterpret_cast<VS::Battle::Ai::CandidateMovementView*>(&state_), tile,
        radius);
  }
  [[nodiscard]] bool EngagementReady() override {
    return IsLiveEngagementReady(&state_);
  }
  [[nodiscard]] int BehaviorMode() const override {
    return state_.behavior_mode;
  }
  void ClearBehaviorMode() override { state_.behavior_mode = 0; }
  [[nodiscard]] bool CommandIsLocomotion() const override {
    const auto* actor =
        reinterpret_cast<const VS::Battle::Actor*>(state_.actor);
    Require(actor != nullptr, "AI locomotion update requires a battle actor");
    return actor->command_opcode < kFirstNonLocomotionCommand;
  }
  void ReleaseReservations() override {
    ReleaseLiveActorAiReservations(&state_);
  }
  void IssueCommand(int command) override {
    IssueLiveActorCommand(&state_, command);
  }
  bool TryExecutePlannedActions() override {
    return TryExecuteLivePlannedActions(&state_) != 0;
  }
  bool TryRecoverBlockedMovement() override {
    return TryRecoverLiveBlockedMovement(
               reinterpret_cast<VS::Battle::Ai::BlockedRecoveryView*>(
                   &state_)) != 0;
  }
  bool RunDecisionBehavior() override {
    return settings_.decision_callback != nullptr &&
           settings_.decision_callback(&state_) != 0;
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
  VS::Battle::Ai::SettingsView& settings_;
};

template <typename Behavior>
int RunBehavior(VS::Battle::Ai::ActorAiState* state, Behavior behavior) {
  LiveDecisionActor actor(StateOf(state));
  return behavior(actor);
}
}  // namespace

int UpdateLivePatrolCombatBehavior(VS::Battle::Ai::ActorAiState* state) {
  return RunBehavior(state, UpdatePatrolCombatBehavior);
}

int UpdateLiveFleeOrWanderBehavior(VS::Battle::Ai::ActorAiState* state) {
  return RunBehavior(state, UpdateFleeOrWanderBehavior);
}

int UpdateLiveAdaptivePursuitBehavior(VS::Battle::Ai::ActorAiState* state) {
  return RunBehavior(state, UpdateAdaptivePursuitBehavior);
}

int UpdateLiveSupportFollowBehavior(VS::Battle::Ai::ActorAiState* state) {
  return RunBehavior(state, UpdateSupportFollowBehavior);
}

int UpdateLiveEngagementApproachBehavior(VS::Battle::Ai::ActorAiState* state) {
  return RunBehavior(state, UpdateEngagementApproachBehavior);
}

void UpdateLiveActorLocomotionMode(VS::Battle::Ai::DecisionDispatchView* view) {
  LiveDecisionActor actor(StateOf(view));
  UpdateActorLocomotionMode(actor);
}

void DispatchLiveMovementBehavior(VS::Battle::Ai::DecisionDispatchView* view) {
  LiveDecisionActor actor(StateOf(view));
  DispatchMovementBehavior(actor);
}

LiveDecisionBehavior SelectLiveDecisionBehavior(std::uint32_t spawn_settings) {
  const std::optional<unsigned> slot = DecisionBehaviorSlot(spawn_settings);
  if (!slot) {
    VS::Core::Warnf(
        "ai",
        "decision behaviour index %u is past the %u-entry table; the "
        "actor wanders instead (#272)",
        DecisionBehaviorIndex(spawn_settings), kDecisionBehaviorCount);
    return nullptr;
  }
  return VS::Battle::Ai::decisionCallbacks[*slot];
}
}  // namespace VS::Battle::Ai
