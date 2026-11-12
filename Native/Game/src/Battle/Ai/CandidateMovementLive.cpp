// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/CandidateMovementLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <type_traits>
#include <vector>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/CandidateMovement.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using State = Battle_Ai_ActorAiState;
using Movement = Battle_Ai_CandidateMovementView;
using Escort = Battle_Ai_EscortView;
static_assert(sizeof(Battle_Ai_ActionCandidate) ==
              sizeof(ActionCandidate::Words));
static_assert(std::extent_v<decltype(State::candidateOrder)> ==
              kCandidateCapacity);
static_assert(std::size(Battle_Ai_actorAiStates) == kCandidateTargetSlots);
static_assert(kAiFrameActorSlots == kCandidateTargetSlots);
static_assert(offsetof(Movement, opaque1) == offsetof(State, opaque1));
static_assert(offsetof(Movement, position) == offsetof(State, position));
static_assert(offsetof(Movement, pointHeight) == offsetof(State, pointHeight));
static_assert(offsetof(Movement, maxRange) == offsetof(State, maxRange));
static_assert(offsetof(Movement, planMode) == offsetof(State, planMode));
static_assert(offsetof(Movement, plannedTile) == offsetof(State, plannedTile));
static_assert(offsetof(Movement, movementTarget) ==
              offsetof(State, movementTarget));
static_assert(offsetof(Movement, movementFlags) ==
              offsetof(State, movementFlags));
static_assert(offsetof(Movement, destination) == offsetof(State, destination));
static_assert(offsetof(Movement, relations) == offsetof(State, relations));
static_assert(offsetof(Movement, distancesSquared) ==
              offsetof(State, distancesSquared));
static_assert(offsetof(Movement, candidateCount) ==
              offsetof(State, candidateCount));
static_assert(offsetof(Escort, opaqueE) == offsetof(State, opaqueE));
static_assert(offsetof(Escort, opaqueF) == offsetof(State, opaqueF));
static_assert(offsetof(Escort, linkedActor) == offsetof(State, linkedActor));
static_assert(offsetof(Escort, opaque18) == offsetof(State, opaque18));
static_assert(offsetof(Escort, opaque44) == offsetof(State, opaque44));
static_assert(offsetof(Escort, x) == offsetof(State, position));
static_assert(offsetof(Escort, actorId) == offsetof(State, actorId));
static_assert(offsetof(Escort, opaqueE9) == offsetof(State, opaqueE9));
static_assert(offsetof(Escort, actionStatus) == offsetof(State, actionStatus));
static_assert(offsetof(Escort, movementTarget) ==
              offsetof(State, movementTarget));
static_assert(offsetof(Escort, relations) == offsetof(State, relations));
static_assert(offsetof(Escort, candidateOrder) ==
              offsetof(State, candidateOrder));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

State& StateOf(State* state) {
  Require(state != nullptr, "AI candidate movement requires an actor state");
  return *state;
}

template <typename T>
T RequireValue(const std::optional<T>& value, const char* message) {
  if (!value) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
  return value.value();
}

bool ValidTarget(int target) {
  return target >= 0 && target < kCandidateTargetSlots;
}

CandidateMover Load(const State& state) {
  CandidateMover mover{.gait = state.opaque1,
                       .fleeing = state.opaqueE,
                       .guarding = state.opaqueF,
                       .linked_actor = state.linkedActor,
                       .alerted = state.opaque18,
                       .guard_delay = state.opaque44,
                       .position = Core::Math::PositionOf(state.position),
                       .point_height = state.pointHeight,
                       .actor_id = state.actorId,
                       .max_range = state.maxRange,
                       .range_mode = state.opaqueE9,
                       .plan_mode = state.planMode,
                       .action_status = state.actionStatus,
                       .planned_tile = state.plannedTile.word,
                       .movement_target = state.movementTarget,
                       .movement_flags = state.movementFlags,
                       .destination = state.destination.word,
                       .candidate_count = state.candidateCount};
  for (int id = 0; id < kCandidateTargetSlots; ++id) {
    mover.relations.at(id) = std::bit_cast<std::uint32_t>(state.relations[id]);
    mover.distances_squared.at(id) = state.distancesSquared[id];
  }
  return mover;
}

void Store(const CandidateMover& mover, State& state) {
  state.opaque1 = mover.gait;
  state.opaqueF = mover.guarding;
  state.opaque18 = mover.alerted;
  state.opaque44 = mover.guard_delay;
  state.movementTarget = mover.movement_target;
}

class LiveCandidateMovementWorld final : public CandidateMovementWorld {
 public:
  explicit LiveCandidateMovementWorld(State& state) : state_(state) {}

  std::optional<std::size_t> Handle(
      Battle_Ai_ActionCandidate* candidate) const {
    if (candidate == nullptr) return std::nullopt;
    const auto found = std::ranges::find(candidates_, candidate);
    if (found != candidates_.end()) {
      return static_cast<std::size_t>(found - candidates_.begin());
    }
    candidates_.push_back(candidate);
    return candidates_.size() - 1;
  }
  [[nodiscard]] std::optional<std::size_t> RankedHandle(
      std::size_t rank) const override {
    if (rank >= std::size(state_.candidateOrder)) return std::nullopt;
    return Handle(state_.candidateOrder[rank]);
  }
  [[nodiscard]] std::optional<ActionCandidate> Candidate(
      std::size_t handle) const override {
    if (handle >= candidates_.size()) return std::nullopt;
    return ActionCandidate(
        std::bit_cast<ActionCandidate::Words>(*candidates_.at(handle)));
  }
  [[nodiscard]] bool HasFrameActor(int target) const override {
    return ValidTarget(target) &&
           Battle_Ai_FrameState()->actors[target] != nullptr;
  }
  [[nodiscard]] std::optional<PursuitTarget> TargetModel(
      int target) const override {
    if (!ValidTarget(target) || Actors_actorRecords[target] == nullptr) {
      return std::nullopt;
    }
    const auto& model = Actors_actorRecords[target]->instance;
    constexpr int kLayerShift = 8;
    constexpr int kZShift = 16;
    constexpr int kStatusShift = 24;
    return PursuitTarget{
        .position = Core::Math::PositionOf(model.position),
        .tile = static_cast<std::uint32_t>(model.currentTileX) |
                (static_cast<std::uint32_t>(model.currentTileLayer)
                 << kLayerShift) |
                (static_cast<std::uint32_t>(model.currentTileZ) << kZShift) |
                (static_cast<std::uint32_t>(model.reserved5F) << kStatusShift)};
  }
  [[nodiscard]] std::optional<std::int16_t> TargetHeight(
      int target) const override {
    if (!ValidTarget(target) || Battle_Ai_actorAiStates[target] == nullptr) {
      return std::nullopt;
    }
    return Battle_Ai_actorAiStates[target]->pointHeight;
  }
  [[nodiscard]] std::optional<std::uint32_t> FrameDistance(
      int observer, int target) const override {
    if (!HasFrameActor(observer) || !ValidTarget(target)) return std::nullopt;
    return Battle_Ai_FrameState()->actors[observer]->distancesSquared[target];
  }
  [[nodiscard]] std::optional<ActorRoster> BattleActors() const override {
    ActorRoster roster;
    std::array<bool, kCandidateTargetSlots> seen{};
    for (auto* actor = Battle_actors[0]; actor != nullptr;
         actor = actor->next) {
      if (!ValidTarget(actor->id) || seen.at(actor->id)) return std::nullopt;
      seen.at(actor->id) = true;
      roster.ids.at(roster.count++) = static_cast<std::uint8_t>(actor->id);
    }
    return roster;
  }
  [[nodiscard]] int GroundHeight(int x, int z) const override {
    return Field::GroundHeightAtWorldPosition(CurrentTerrain(), x, z);
  }
  [[nodiscard]] std::uint16_t ReadinessLevel() const override {
    Require(Battle_Ai_roomAiState != nullptr,
            "AI pursuit requires room readiness");
    return Battle_Ai_roomAiState->readinessLevel;
  }
  void CountApproachMove() override {
    Require(Battle_Ai_roomAiState != nullptr,
            "AI pursuit requires the room approach counter");
    auto& count = Battle_Ai_roomAiState->approachMoves;
    count = Core::Math::WrappingAdd(count, 1);
  }
  [[nodiscard]] bool CanApproach(CandidateMover& mover,
                                 std::size_t candidate) override {
    return Call(mover, [&] {
      return CanApproachLiveEngagedTarget(&state_, candidates_.at(candidate));
    });
  }
  [[nodiscard]] bool CooldownReady(CandidateMover& mover,
                                   std::size_t candidate) override {
    return Call(mover, [&] {
      return IsLiveActionCooldownReady(&state_, candidates_.at(candidate));
    });
  }
  void TryEngage(CandidateMover& mover) override {
    Call(mover, [&] { TryEngageLiveActor(&state_); });
  }
  void Disengage(CandidateMover& mover) override {
    Call(mover, [&] { DisengageLiveActor(&state_); });
  }
  int Approach(CandidateMover& mover, const ApproachRequest& request) override {
    return Call(mover, [&] {
      return ApproachLiveActor(&state_, request.target, request.distance,
                               request.option);
    });
  }
  int Track(CandidateMover& mover, int target) override {
    return Call(mover, [&] { return TrackLiveActorPosition(&state_, target); });
  }
  int Flee(CandidateMover& mover, int kind) override {
    return Call(mover, [&] {
      return FleeLiveVisibleEnemies(reinterpret_cast<Movement*>(&state_), kind);
    });
  }
  void Wander(CandidateMover& mover, std::uint32_t tile, int radius) override {
    Call(mover, [&] {
      WanderAroundLiveTile(reinterpret_cast<Movement*>(&state_), tile, radius);
    });
  }
  void MoveToTile(CandidateMover& mover, std::uint32_t tile,
                  int radius) override {
    Call(mover, [&] {
      MoveLiveActorToTile(reinterpret_cast<Movement*>(&state_), tile, radius);
    });
  }
  RelatedActor FindRelated(CandidateMover& mover, int relation,
                           bool want_tile) override {
    return Call(mover, [&] {
      RelatedActor found;
      found.actor_id = FindLiveActorByRelationship(
          &state_, want_tile ? &found.tile : nullptr, relation);
      return found;
    });
  }
  void ReleaseReservations(CandidateMover& mover) override {
    Call(mover, [&] { ReleaseLiveActorAiReservations(&state_); });
  }
  void Recover(CandidateMover& mover) override {
    Call(mover, [&] { RecoverLiveMovement(&state_); });
  }
  void ProjectRadial(CandidateMover& mover, std::uint32_t tile, int angle,
                     std::uint32_t distance) override {
    Call(mover, [&] {
      Battle_Ai_TilePosition source{};
      source.word = tile;
      ProjectLiveClearRadialDestination(&state_, source, angle, distance);
    });
  }
  void DispatchLocomotion(CandidateMover& mover, int kind, int angle) override {
    Call(mover, [&] { DispatchLiveAiLocomotionMode(&state_, kind, angle); });
  }
  [[nodiscard]] int RandomMasked(int mask) override {
    return NextLiveActorRandomMasked(mask);
  }
  void SetAwareness(CandidateMover& mover, int marker, int frames) override {
    Call(mover,
         [&] { SetLiveTemporaryAwarenessIndicator(&state_, marker, frames); });
  }

 private:
  template <typename Operation>
  auto Call(CandidateMover& mover, Operation operation)
      -> decltype(operation()) {
    Store(mover, state_);
    if constexpr (std::is_void_v<decltype(operation())>) {
      operation();
      mover = Load(state_);
    } else {
      auto result = operation();
      mover = Load(state_);
      return result;
    }
  }
  State& state_;
  mutable std::vector<Battle_Ai_ActionCandidate*> candidates_;
};

template <typename Operation>
int Run(State* state, Operation operation) {
  State& actor = StateOf(state);
  LiveCandidateMovementWorld world(actor);
  auto mover = Load(actor);
  const auto result = operation(world, mover);
  Store(mover, actor);
  return RequireValue(result, "AI candidate movement refused its inputs");
}
}  // namespace

int MoveLiveForActionCandidate(Battle_Ai_CandidateMovementView* state,
                               Battle_Ai_CandidateEntryView* candidate) {
  return Run(reinterpret_cast<State*>(state), [&](auto& world, auto& mover) {
    if (mover.candidate_count == 0) return std::optional<int>{0};
    const auto handle =
        world.Handle(reinterpret_cast<Battle_Ai_ActionCandidate*>(candidate));
    return MoveForActionCandidate(
        world, mover,
        RequireValue(handle, "AI movement requires an action candidate"));
  });
}

int UpdateLiveCombatPursuit(State* state) {
  return Run(state, UpdateCombatPursuit);
}

int TryApproachLivePlannedTarget(State* state,
                                 Battle_Ai_ActionCandidate* candidate) {
  return Run(state, [&](auto& world, auto& mover) {
    const auto handle = world.Handle(candidate);
    return TryApproachPlannedTarget(
        world, mover,
        RequireValue(handle, "AI approach requires an action candidate"));
  });
}

int UpdateLiveCloseFollowBehavior(Escort* state) {
  return Run(reinterpret_cast<State*>(state), UpdateCloseFollowBehavior);
}

int UpdateLiveEnemyPursuitBehavior(Escort* state) {
  return Run(reinterpret_cast<State*>(state), UpdateEnemyPursuitBehavior);
}

int UpdateLiveGuardMovementBehavior(Escort* state) {
  return Run(reinterpret_cast<State*>(state), UpdateGuardMovementBehavior);
}
}  // namespace VS::Battle::Ai
