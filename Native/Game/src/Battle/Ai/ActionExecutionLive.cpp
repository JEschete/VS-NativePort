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

#include "Battle/Ai/ActionExecution.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Core/FrameTiming.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace VS::Battle::Ai {
namespace {
using State = Battle_Ai_ActorAiState;
constexpr std::size_t kClearedHeaderBytes = 14;
static_assert(sizeof(Battle_Ai_ActionCandidate) ==
              sizeof(ActionCandidate::Words));
static_assert(offsetof(State, opaqueE) == kClearedHeaderBytes);
static_assert(std::is_same_v<decltype(State::currentCandidate),
                             Battle_Ai_ActionCandidate*>);
static_assert(std::size(Battle_Ai_actorAiStates) == kCandidateTargetSlots);

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

class LiveExecution final : public ActionExecutionWorld {
 public:
  explicit LiveExecution(State& state) : state_(state) {}
  std::optional<std::size_t> Handle(
      Battle_Ai_ActionCandidate* candidate) const {
    if (candidate == nullptr) return std::nullopt;
    const auto found = std::ranges::find(candidates_, candidate);
    if (found != candidates_.end()) return found - candidates_.begin();
    candidates_.push_back(candidate);
    return candidates_.size() - 1;
  }
  int CandidateCount() const override { return state_.candidateCount; }
  std::optional<std::size_t> RankedCandidate(std::size_t rank) const override {
    if (rank >= std::size(state_.candidateOrder)) return std::nullopt;
    return Handle(state_.candidateOrder[rank]);
  }
  std::size_t FallbackCandidate() const override {
    return RequireDecisionValue(Handle(&state_.currentChoice),
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
        std::bit_cast<Battle_Ai_ActionCandidate>(candidate.RawWords());
  }
  bool CanExecute(std::size_t handle) override {
    return CanExecuteLiveActionCandidate(&state_, candidates_.at(handle));
  }
  void TryEngage() override { TryEngageLiveActor(&state_); }
  bool EngagementReady() override { return IsLiveEngagementReady(&state_); }
  void SelectCandidate(std::size_t handle) override {
    state_.currentCandidate = candidates_.at(handle);
    Battle_Ai_candidateEngagedThisUpdate = 1;
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
    return state_.usesRoomActionDelay == 0;
  }
  std::optional<std::uint8_t> RoomDelay() const override {
    if (!HasRoom()) return std::nullopt;
    return Battle_Ai_roomAiState->defaultActionDelay;
  }
  void SetDelay(std::uint8_t delay) override { state_.minActionDelay = delay; }
  bool HasRoom() const override { return Battle_Ai_roomAiState != nullptr; }
  std::optional<int> SpecialActor(unsigned slot) const override {
    if (!HasRoom() || slot >= std::size(Battle_Ai_roomAiState->specialActors) ||
        Battle_Ai_roomAiState->specialActors[slot] == nullptr) {
      return std::nullopt;
    }
    return Battle_Ai_roomAiState->specialActors[slot]->id;
  }
  std::optional<bool> GimmickReady(int actor) const override {
    if (actor < 0 ||
        static_cast<std::size_t>(actor) >= std::size(Field_gimmickInstances) ||
        Field_gimmickInstances[actor] == nullptr) {
      return std::nullopt;
    }
    const auto& gimmick = *Field_gimmickInstances[actor];
    return gimmick.parent != 0 && gimmick.mode == 0;
  }

 private:
  State& state_;
  mutable std::vector<Battle_Ai_ActionCandidate*> candidates_;
};

class LiveDecisions final : public LiveDecisionList<DecisionUpdateWorld> {
 public:
  bool SelectActor(int id) override {
    if (id < 0 || id >= kCandidateTargetSlots) return false;
    state_ = Battle_Ai_FrameState()->actors[id];
    return state_ != nullptr;
  }
  DecisionDispatchState State() const override {
    return {.restricted = state_->reserved14[0] != 0,
            .partial = state_->reserved1D[0] != 0,
            .tile_x = state_->tile.tile.tileX,
            .tile_z = state_->tile.tile.tileZ,
            .position = Core::Math::PositionOf(state_->position),
            .point_height = state_->pointHeight};
  }
  int ElapsedTicks() override { return Core::ElapsedFrameCounterTicks(); }
  int TickBudget() const override { return Battle_Ai_frameTickBudget; }
  void CacheTick(int tick) override {
    Battle_Ai_FrameState()->lastBudgetTick = tick;
  }
  void IssueCommand(int command) override {
    IssueLiveActorCommand(state_, command);
  }
  std::optional<DecisionPlayer> Player() const override {
    const auto* ai = Battle_Ai_actorAiStates[0];
    if (ai == nullptr) return DecisionPlayer{};
    const auto* record = Actors_actorRecords[0];
    if (record == nullptr) return std::nullopt;
    return DecisionPlayer{.has_ai = true,
                          .moving = record->instance.locomotionState != 0,
                          .y = record->instance.position.vy,
                          .point_height = ai->pointHeight,
                          .body_height = ai->bodyHeight};
  }
  std::optional<Core::Math::Position3> PlayerBattlePosition() const override {
    if (Battle_actors[0] == nullptr || Battle_actors[0]->model == nullptr) {
      return std::nullopt;
    }
    return Core::Math::PositionOf(Battle_actors[0]->model->position);
  }
  void StorePlayerDistance(std::uint32_t distance) override {
    state_->distancesSquared[0] = distance;
  }
  std::optional<int> ExecutePlannedActions() override {
    return TryExecuteCheckedLivePlannedActions(state_);
  }
  std::optional<std::uint16_t> TileFlags(unsigned x,
                                         unsigned z) const override {
    const auto* flags = Battle_Ai_FrameState()->tileFlags;
    if (flags == nullptr || x >= kAiFrameGridSide || z >= kAiFrameGridSide) {
      return std::nullopt;
    }
    return flags[z][x];
  }
  void ResetMovement(std::uint8_t area) override {
    ResetLiveDecisionMovement(*state_, area);
  }
  void ClearFirstDirectionWord() override {
    Battle_Ai_FrameState()->directionCache[0] = 0;
  }
  void DispatchMovement() override {
    DispatchLiveMovementBehavior(
        reinterpret_cast<Battle_Ai_DecisionDispatchView*>(state_));
  }
  void UpdateLocomotion() override {
    UpdateLiveActorLocomotionMode(
        reinterpret_cast<Battle_Ai_DecisionDispatchView*>(state_));
  }

 private:
  Battle_Ai_ActorAiState* state_ = nullptr;
};
}  // namespace

[[noreturn]] void RefuseLiveDecisionInputs(const char* message) {
  Core_Errorf("ai", "%s", message);
  std::abort();
}

std::size_t LiveDecisionListHead() {
  return reinterpret_cast<std::size_t>(Battle_actors[0]);
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
  state.movementTarget = kCandidateTargetSlots;
  state.savedDestination.word = UINT32_MAX;
  state.ground_area = area;
  state.reserved154Area = area;
}

std::optional<int> TryExecuteCheckedLiveActionCandidate(
    State* state, Battle_Ai_ActionCandidate* candidate) {
  if (state == nullptr || candidate == nullptr) {
    Core_Debugf("ai", "AI action execution requires state and candidate");
    return std::nullopt;
  }
  LiveExecution world(*state);
  const auto handle = world.Handle(candidate);
  if (!handle) return std::nullopt;
  return TryExecuteActionCandidate(world, handle.value());
}

std::optional<int> TryExecuteCheckedLivePlannedActions(State* state) {
  if (state == nullptr) {
    Core_Debugf("ai", "AI planned execution requires an actor state");
    return std::nullopt;
  }
  LiveExecution world(*state);
  return TryExecutePlannedActions(world);
}

int TryExecuteLiveActionCandidate(State* state,
                                  Battle_Ai_ActionCandidate* candidate) {
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
