// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/ActionExecutionLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <type_traits>
#include <vector>

#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionExecution.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/FrameTiming.hpp"
#include "Field/Gimmicks.hpp"

namespace VS::Battle::Ai {
namespace {
using State = VS::Battle::Ai::ActorAiState;
constexpr std::size_t kClearedHeaderBytes = 14;
static_assert(sizeof(VS::Battle::Ai::ActionCandidateRecord) ==
              sizeof(ActionCandidate::Words));
static_assert(offsetof(State, opaque_e) == kClearedHeaderBytes);
static_assert(std::is_same_v<decltype(State::current_candidate),
                             VS::Battle::Ai::ActionCandidateRecord*>);
static_assert(std::size(VS::Battle::Ai::actorAiStates) ==
              kCandidateTargetSlots);

class LiveExecution final : public ActionExecutionWorld {
 public:
  explicit LiveExecution(State& state) : state_(state) {}
  std::optional<std::size_t> Handle(
      VS::Battle::Ai::ActionCandidateRecord* candidate) const {
    if (candidate == nullptr) return std::nullopt;
    const auto found = std::ranges::find(candidates_, candidate);
    if (found != candidates_.end()) return found - candidates_.begin();
    candidates_.push_back(candidate);
    return candidates_.size() - 1;
  }
  int CandidateCount() const override { return state_.candidate_count; }
  std::optional<std::size_t> RankedCandidate(std::size_t rank) const override {
    if (rank >= std::size(state_.candidate_order)) return std::nullopt;
    return Handle(state_.candidate_order[rank]);
  }
  std::size_t FallbackCandidate() const override {
    return RequireDecisionValue(Handle(&state_.current_choice),
                                "AI fallback candidate is missing");
  }
  std::optional<ActionCandidate> Candidate(std::size_t handle) const override {
    if (handle >= candidates_.size()) return std::nullopt;
    return ActionCandidate(
        std::bit_cast<ActionCandidate::Words>(*candidates_.at(handle)));
  }
  void StoreCandidate(std::size_t handle,
                      const ActionCandidate& candidate) override {
    *candidates_.at(handle) =
        std::bit_cast<VS::Battle::Ai::ActionCandidateRecord>(
            candidate.RawWords());
  }
  bool CanExecute(std::size_t handle) override {
    return CanExecuteLiveActionCandidate(&state_, candidates_.at(handle));
  }
  void TryEngage() override { TryEngageLiveActor(&state_); }
  bool EngagementReady() override { return IsLiveEngagementReady(&state_); }
  void SelectCandidate(std::size_t handle) override {
    state_.current_candidate = candidates_.at(handle);
    VS::Battle::Ai::candidateEngagedThisUpdate = 1;
  }
  bool TargetMoving(unsigned target) const override {
    return target < std::size(Actors_actorRecords) &&
           Actors_actorRecords[target] != nullptr &&
           Actors_actorRecords[target]->instance.locomotionState != 0;
  }
  int RandomMasked(int mask) override {
    return NextLiveActorRandomMasked(mask);
  }
  void IssueCommand(int command) override {
    IssueLiveActorCommand(&state_, command);
  }
  bool RequiresCooldown(std::size_t handle) override {
    return DoesLiveActionRequireCooldown(&state_, candidates_.at(handle));
  }
  bool UsesRoomDelay() const override {
    return state_.uses_room_action_delay == 0;
  }
  std::optional<std::uint8_t> RoomDelay() const override {
    if (!HasRoom()) return std::nullopt;
    return VS::Battle::Ai::roomAiState->default_action_delay;
  }
  void SetDelay(std::uint8_t delay) override {
    state_.min_action_delay = delay;
  }
  bool HasRoom() const override {
    return VS::Battle::Ai::roomAiState != nullptr;
  }
  std::optional<int> SpecialActor(unsigned slot) const override {
    if (!HasRoom() ||
        slot >= std::size(VS::Battle::Ai::roomAiState->special_actors) ||
        VS::Battle::Ai::roomAiState->special_actors[slot] == nullptr) {
      return std::nullopt;
    }
    return VS::Battle::Ai::roomAiState->special_actors[slot]->id;
  }
  std::optional<bool> GimmickReady(int actor) const override {
    if (actor < 0 ||
        static_cast<std::size_t>(actor) >=
            std::size(VS::Field::gimmickInstances) ||
        VS::Field::gimmickInstances[actor] == nullptr) {
      return std::nullopt;
    }
    const auto& gimmick = *VS::Field::gimmickInstances[actor];
    return gimmick.parent != 0 && gimmick.mode == 0;
  }

 private:
  State& state_;
  mutable std::vector<VS::Battle::Ai::ActionCandidateRecord*> candidates_;
};

class LiveDecisions final : public LiveDecisionList<DecisionUpdateWorld> {
 public:
  bool SelectActor(int id) override {
    if (id < 0 || id >= kCandidateTargetSlots) return false;
    state_ = LiveFrameContext().actors[id];
    return state_ != nullptr;
  }
  DecisionDispatchState State() const override {
    return {.restricted = state_->reserved14[0] != 0,
            .partial = state_->reserved_1d[0] != 0,
            .tile_x = state_->tile.tile.tile_x,
            .tile_z = state_->tile.tile.tile_z,
            .position = Core::Math::PositionOf(state_->position),
            .point_height = state_->point_height};
  }
  int ElapsedTicks() override { return Core::ElapsedFrameCounterTicks(); }
  int TickBudget() const override { return VS::Battle::Ai::frameTickBudget; }
  void CacheTick(int tick) override {
    LiveFrameContext().lastBudgetTick = tick;
  }
  void IssueCommand(int command) override {
    IssueLiveActorCommand(state_, command);
  }
  std::optional<DecisionPlayer> Player() const override {
    const auto* ai = VS::Battle::Ai::actorAiStates[0];
    if (ai == nullptr) return DecisionPlayer{};
    const auto* record = Actors_actorRecords[0];
    if (record == nullptr) return std::nullopt;
    return DecisionPlayer{.has_ai = true,
                          .moving = record->instance.locomotionState != 0,
                          .y = record->instance.position.vy,
                          .point_height = ai->point_height,
                          .body_height = ai->body_height};
  }
  std::optional<Core::Math::Position3> PlayerBattlePosition() const override {
    if (VS::Battle::actors[0] == nullptr ||
        VS::Battle::actors[0]->model == nullptr) {
      return std::nullopt;
    }
    return Core::Math::PositionOf(VS::Battle::actors[0]->model->position);
  }
  void StorePlayerDistance(std::uint32_t distance) override {
    state_->distances_squared[0] = distance;
  }
  std::optional<int> ExecutePlannedActions() override {
    return TryExecuteCheckedLivePlannedActions(state_);
  }
  std::optional<std::uint16_t> TileFlags(unsigned x,
                                         unsigned z) const override {
    const auto* flags = LiveFrameContext().tileFlags;
    if (flags == nullptr || x >= kNavigationGridSide ||
        z >= kNavigationGridSide) {
      return std::nullopt;
    }
    return flags[z][x];
  }
  void ResetMovement(std::uint8_t area) override {
    ResetLiveDecisionMovement(*state_, area);
  }
  void ClearFirstDirectionWord() override {
    LiveFrameContext().ClearDirectionTests();
  }
  void DispatchMovement() override {
    DispatchLiveMovementBehavior(
        reinterpret_cast<VS::Battle::Ai::DecisionDispatchView*>(state_));
  }
  void UpdateLocomotion() override {
    UpdateLiveActorLocomotionMode(
        reinterpret_cast<VS::Battle::Ai::DecisionDispatchView*>(state_));
  }

 private:
  VS::Battle::Ai::ActorAiState* state_ = nullptr;
};
}  // namespace

[[noreturn]] void RefuseLiveDecisionInputs(const char* message) {
  VS::Core::Errorf("ai", "%s", message);
  std::abort();
}

std::size_t LiveDecisionListHead() {
  return reinterpret_cast<std::size_t>(VS::Battle::actors[0]);
}

std::optional<int> LiveDecisionListId(std::size_t node) {
  if (node == 0) return std::nullopt;
  return reinterpret_cast<const Battle_Actor*>(node)->id;
}

std::size_t LiveDecisionListNext(std::size_t node) {
  Require(node != 0, "AI decision successor requires a list node");
  return reinterpret_cast<std::size_t>(
      reinterpret_cast<const Battle_Actor*>(node)->next);
}

void ResetLiveDecisionMovement(State& state, std::uint8_t area) {
  std::memset(&state, 0, kClearedHeaderBytes);
  state.movement_target = kCandidateTargetSlots;
  state.saved_destination.word = UINT32_MAX;
  state.ground_area = area;
  state.reserved154_area = area;
}

std::optional<int> TryExecuteCheckedLiveActionCandidate(
    State* state, VS::Battle::Ai::ActionCandidateRecord* candidate) {
  if (state == nullptr || candidate == nullptr) {
    VS::Core::Debugf("ai", "AI action execution requires state and candidate");
    return std::nullopt;
  }
  LiveExecution world(*state);
  const auto handle = world.Handle(candidate);
  if (!handle) return std::nullopt;
  return TryExecuteActionCandidate(world, handle.value());
}

std::optional<int> TryExecuteCheckedLivePlannedActions(State* state) {
  if (state == nullptr) {
    VS::Core::Debugf("ai", "AI planned execution requires an actor state");
    return std::nullopt;
  }
  LiveExecution world(*state);
  return TryExecutePlannedActions(world);
}

int TryExecuteLiveActionCandidate(
    State* state, VS::Battle::Ai::ActionCandidateRecord* candidate) {
  return RequireDecisionValue(
      TryExecuteCheckedLiveActionCandidate(state, candidate),
      "AI candidate execution refused its inputs");
}

int TryExecuteLivePlannedActions(State* state) {
  return RequireDecisionValue(TryExecuteCheckedLivePlannedActions(state),
                              "AI planned execution refused its inputs");
}

bool UpdateCheckedLiveActorDecisions() {
  LiveDecisions world;
  return UpdateActorDecisions(world);
}

void UpdateLiveActorDecisions() {
  Require(UpdateCheckedLiveActorDecisions(),
          "AI decision update refused its inputs");
}
}  // namespace VS::Battle::Ai
