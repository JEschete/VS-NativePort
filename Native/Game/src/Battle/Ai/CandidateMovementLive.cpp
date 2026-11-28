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

#include "Actors/ActorRecord.hpp"
#include "Actors/ActorSpawningLive.hpp"
#include "Actors/ModelInstance.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionExecutionLive.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/CandidateMovement.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"

namespace VS::Battle::Ai {
namespace {
using State = VS::Battle::Ai::ActorAiState;
using Movement = VS::Battle::Ai::CandidateMovementView;
using Escort = VS::Battle::Ai::EscortView;
static_assert(sizeof(VS::Battle::Ai::ActionCandidateRecord) ==
              sizeof(ActionCandidate::Words));
static_assert(std::tuple_size_v<decltype(State::candidate_order)> ==
              kCandidateCapacity);
static_assert(std::size(VS::Battle::Ai::actorAiStates) ==
              kCandidateTargetSlots);
static_assert(kFrameActorSlots == kCandidateTargetSlots);
static_assert(offsetof(Movement, opaque1) == offsetof(State, run_requested));
static_assert(offsetof(Movement, position) == offsetof(State, position));
static_assert(offsetof(Movement, point_height) ==
              offsetof(State, point_height));
static_assert(offsetof(Movement, max_range) == offsetof(State, max_range));
static_assert(offsetof(Movement, plan_mode) == offsetof(State, plan_mode));
static_assert(offsetof(Movement, planned_tile) ==
              offsetof(State, planned_tile));
static_assert(offsetof(Movement, movement_target) ==
              offsetof(State, movement_target));
static_assert(offsetof(Movement, movement_flags) ==
              offsetof(State, movement_flags));
static_assert(offsetof(Movement, destination) == offsetof(State, destination));
static_assert(offsetof(Movement, relations) == offsetof(State, relations));
static_assert(offsetof(Movement, distances_squared) ==
              offsetof(State, distances_squared));
static_assert(offsetof(Movement, candidate_count) ==
              offsetof(State, candidate_count));
static_assert(offsetof(Escort, opaque_e) == offsetof(State, should_flee));
static_assert(offsetof(Escort, opaque_f) == offsetof(State, guarding));
static_assert(offsetof(Escort, linked_actor) == offsetof(State, linked_actor));
static_assert(offsetof(Escort, opaque18) == offsetof(State, opaque18));
static_assert(offsetof(Escort, opaque44) == offsetof(State, guard_delay));
static_assert(offsetof(Escort, x) == offsetof(State, position));
static_assert(offsetof(Escort, actor_id) == offsetof(State, actor_id));
static_assert(offsetof(Escort, opaque_e9) == offsetof(State, range_band));
static_assert(offsetof(Escort, action_status) ==
              offsetof(State, action_status));
static_assert(offsetof(Escort, movement_target) ==
              offsetof(State, movement_target));
static_assert(offsetof(Escort, relations) == offsetof(State, relations));
static_assert(offsetof(Escort, candidate_order) ==
              offsetof(State, candidate_order));

State& StateOf(State* state) {
  Require(state != nullptr, "AI candidate movement requires an actor state");
  return *state;
}

template <typename T>
T RequireValue(const std::optional<T>& value, const char* message) {
  if (!value) {
    VS::Core::Errorf("ai", "%s", message);
    std::abort();
  }
  return value.value();
}

bool ValidTarget(int target) {
  return target >= 0 && target < kCandidateTargetSlots;
}

CandidateMover Load(const State& state) {
  CandidateMover mover{.gait = state.run_requested,
                       .fleeing = state.should_flee,
                       .guarding = state.guarding,
                       .linked_actor = state.linked_actor,
                       .alerted = state.opaque18,
                       .guard_delay = state.guard_delay,
                       .position = Core::Math::PositionOf(state.position),
                       .point_height = state.point_height,
                       .actor_id = state.actor_id,
                       .max_range = state.max_range,
                       .range_mode = state.range_band,
                       .plan_mode = state.plan_mode,
                       .action_status = state.action_status,
                       .planned_tile = state.planned_tile.word,
                       .movement_target = state.movement_target,
                       .movement_flags = state.movement_flags,
                       .destination = state.destination.word,
                       .candidate_count = state.candidate_count};
  for (int id = 0; id < kCandidateTargetSlots; ++id) {
    mover.relations.at(id) = std::bit_cast<std::uint32_t>(state.relations[id]);
    mover.distances_squared.at(id) = state.distances_squared[id];
  }
  return mover;
}

void Store(const CandidateMover& mover, State& state) {
  state.run_requested = mover.gait;
  state.guarding = mover.guarding;
  state.opaque18 = mover.alerted;
  state.guard_delay = mover.guard_delay;
  state.movement_target = mover.movement_target;
  for (int id = 0; id < kCandidateTargetSlots; ++id) {
    state.relations[id] =
        std::bit_cast<VS::Battle::Ai::RelationWord>(mover.relations.at(id));
  }
}

class LiveCandidateMovementWorld final
    : public LiveDecisionList<CandidateMovementWorld> {
 public:
  explicit LiveCandidateMovementWorld(State& state) : state_(state) {}

  std::optional<std::size_t> Handle(
      VS::Battle::Ai::ActionCandidateRecord* candidate) const {
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
    if (rank >= std::size(state_.candidate_order)) return std::nullopt;
    return Handle(state_.candidate_order[rank]);
  }
  [[nodiscard]] std::optional<ActionCandidate> Candidate(
      std::size_t handle) const override {
    if (handle >= candidates_.size()) return std::nullopt;
    return ActionCandidate(
        std::bit_cast<ActionCandidate::Words>(*candidates_.at(handle)));
  }
  [[nodiscard]] bool HasFrameActor(int target) const override {
    return ValidTarget(target) && LiveFrameContext().actors[target] != nullptr;
  }
  [[nodiscard]] std::optional<PursuitTarget> TargetModel(
      int target) const override {
    if (!ValidTarget(target) || VS::Actors::actorRecords[target] == nullptr) {
      return std::nullopt;
    }
    const auto& model = VS::Actors::actorRecords[target]->instance;
    constexpr int kLayerShift = 8;
    constexpr int kZShift = 16;
    constexpr int kStatusShift = 24;
    return PursuitTarget{
        .position = Core::Math::PositionOf(model.position),
        .tile = static_cast<std::uint32_t>(model.current_tile_x) |
                (static_cast<std::uint32_t>(model.current_tile_layer)
                 << kLayerShift) |
                (static_cast<std::uint32_t>(model.current_tile_z) << kZShift) |
                (static_cast<std::uint32_t>(model.current_tile_spare)
                 << kStatusShift)};
  }
  [[nodiscard]] std::optional<std::int16_t> TargetHeight(
      int target) const override {
    if (!ValidTarget(target) ||
        VS::Battle::Ai::actorAiStates[target] == nullptr) {
      return std::nullopt;
    }
    return VS::Battle::Ai::actorAiStates[target]->point_height;
  }
  [[nodiscard]] std::optional<std::uint32_t> FrameDistance(
      int observer, int target) const override {
    if (!HasFrameActor(observer) || !ValidTarget(target)) return std::nullopt;
    return LiveFrameContext().actors[observer]->distances_squared[target];
  }
  [[nodiscard]] std::optional<ActorRoster> BattleActors() const override {
    ActorRoster roster;
    std::array<bool, kCandidateTargetSlots> seen{};
    for (auto* actor = VS::Battle::actors[0]; actor != nullptr;
         actor = VS::Actors::ResolveLiveActorInstance(actor->next)) {
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
    Require(VS::Battle::Ai::roomAiState != nullptr,
            "AI pursuit requires room readiness");
    return VS::Battle::Ai::roomAiState->readiness_level;
  }
  void CountApproachMove() override {
    Require(VS::Battle::Ai::roomAiState != nullptr,
            "AI pursuit requires the room approach counter");
    auto& count = VS::Battle::Ai::roomAiState->approach_moves;
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
      VS::Battle::Ai::TilePosition source{};
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
  mutable std::vector<VS::Battle::Ai::ActionCandidateRecord*> candidates_;
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

int MoveLiveForActionCandidate(VS::Battle::Ai::CandidateMovementView* state,
                               VS::Battle::Ai::CandidateEntryView* candidate) {
  return Run(reinterpret_cast<State*>(state), [&](auto& world, auto& mover) {
    if (mover.candidate_count == 0) return std::optional<int>{0};
    const auto handle = world.Handle(
        reinterpret_cast<VS::Battle::Ai::ActionCandidateRecord*>(candidate));
    return MoveForActionCandidate(
        world, mover,
        RequireValue(handle, "AI movement requires an action candidate"));
  });
}

int UpdateLiveCombatPursuit(State* state) {
  return Run(state, UpdateCombatPursuit);
}

int TryApproachLivePlannedTarget(
    State* state, VS::Battle::Ai::ActionCandidateRecord* candidate) {
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

int UpdateLiveEscortCombatBehavior(Escort* state) {
  return Run(reinterpret_cast<State*>(state), UpdateEscortCombatBehavior);
}
}  // namespace VS::Battle::Ai
