// Hand-maintained (tools/port/owned.csv).
// Live binding for the movement behaviours (MovementBehavior.cpp).
#include "Battle/Ai/MovementBehaviorLive.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Core/Math/Distance.hpp"
#include "Field/PlatformPlacementLive.hpp"

extern "C" {
#include "Actors/Movement.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
#include "Field/Gimmicks.h"
}

namespace VS::Battle::Ai {
namespace {
// The room's auxiliary section holds four patrol rows before its waypoints.
constexpr std::size_t kLivePatrolRows = 4;

static_assert(std::size(Battle_Ai_actorAiStates) == kMovementActorSlots);
static_assert(std::extent_v<decltype(Battle_Ai_FrameContext::actors)> ==
              kMovementActorSlots);
static_assert(std::size(Actors_actorRecords) >= kMovementActorSlots);
static_assert(std::extent_v<decltype(Battle_Ai_ActorAiState::relations)> ==
              kMovementActorSlots);
static_assert(sizeof(Battle_Ai_RelationWord) == sizeof(std::uint32_t));
static_assert(sizeof(Battle_Ai_TilePosition) == sizeof(std::uint32_t));
static_assert(
    std::extent_v<decltype(Battle_Ai_ActorAiState::movementModeCounters)> ==
    kMovementModeCounterBytes);
// Pursuit progress lives in the actor state's otherwise unnamed bytes
// 0xDC-0xF7, which the target approach view names.
constexpr std::size_t kActorIdOffset = 0x88;
constexpr std::size_t kApproachRangeOffset = 0xDC;
constexpr std::size_t kPursuitProgressOffset = 0xE4;
constexpr std::size_t kPreviousTargetOffset = 0xEB;
constexpr std::size_t kCachedDestinationOffset = 0xEC;
constexpr std::size_t kTargetTileXOffset = 0xF4;
constexpr std::size_t kTargetTileHighOffset = 0xF7;
constexpr std::size_t ApproachOffset(std::size_t ps1_offset) {
  return offsetof(Battle_Ai_ActorAiState, actorId) + ps1_offset -
         kActorIdOffset;
}
static_assert(offsetof(Battle_Ai_TargetApproachView, actorId) ==
              offsetof(Battle_Ai_ActorAiState, actorId));
static_assert(offsetof(Battle_Ai_TargetApproachView, range) ==
              ApproachOffset(kApproachRangeOffset));
static_assert(offsetof(Battle_Ai_TargetApproachView, opaqueE4) ==
              ApproachOffset(kPursuitProgressOffset));
static_assert(offsetof(Battle_Ai_TargetApproachView, opaqueE7) ==
              offsetof(Battle_Ai_ActorAiState, opaqueE7));
static_assert(offsetof(Battle_Ai_TargetApproachView, previousTargetId) ==
              ApproachOffset(kPreviousTargetOffset));
static_assert(offsetof(Battle_Ai_TargetApproachView, cachedDestination) ==
              ApproachOffset(kCachedDestinationOffset));
static_assert(offsetof(Battle_Ai_TargetApproachView, targetTileX) ==
              ApproachOffset(kTargetTileXOffset));
static_assert(offsetof(Battle_Ai_TargetApproachView, reservedF7) ==
              ApproachOffset(kTargetTileHighOffset));
static_assert(offsetof(Battle_Ai_TargetApproachView, destination) ==
              offsetof(Battle_Ai_ActorAiState, destination));
static_assert(offsetof(Battle_Ai_TargetApproachView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));
// The views callers hold alias the actor AI state.
static_assert(offsetof(Battle_Ai_TerrainMoveView, destination) ==
              offsetof(Battle_Ai_ActorAiState, destination));
static_assert(offsetof(Battle_Ai_TerrainMoveView, moveZ) ==
              offsetof(Battle_Ai_ActorAiState, moveZ));
static_assert(offsetof(Battle_Ai_CandidateMovementView, planMode) ==
              offsetof(Battle_Ai_ActorAiState, planMode));
static_assert(offsetof(Battle_Ai_CandidateMovementView, plannedTile) ==
              offsetof(Battle_Ai_ActorAiState, plannedTile));
static_assert(offsetof(Battle_Ai_CandidateMovementView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));
static_assert(offsetof(Battle_Ai_PerceptionUpdateView, actionStatus) ==
              offsetof(Battle_Ai_ActorAiState, actionStatus));
static_assert(offsetof(Battle_Ai_PerceptionUpdateView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));
static_assert(offsetof(Battle_Ai_CandidateReadinessView, candidateOrder) ==
              offsetof(Battle_Ai_ActorAiState, candidateOrder));
static_assert(offsetof(Battle_Ai_EscortView, actorId) ==
              offsetof(Battle_Ai_ActorAiState, actorId));
static_assert(offsetof(Battle_Ai_EscortView, relations) ==
              offsetof(Battle_Ai_ActorAiState, relations));
static_assert(offsetof(Battle_Ai_SettingsView, decisionCallback) ==
              offsetof(Battle_Ai_ActorAiState, nativePointer68));
static_assert(offsetof(Battle_Ai_SettingsView, opaque2D) ==
              offsetof(Battle_Ai_ActorAiState, reserved2D));
static_assert(sizeof(Battle_Ai_GuardTileWord) == sizeof(std::uint32_t));

[[noreturn]] void Fail(const char* message) {
  Core_Errorf("ai", "%s", message);
  std::abort();
}

void Require(bool valid, const char* message) {
  if (!valid) Fail(message);
}

template <typename View>
Battle_Ai_ActorAiState& StateOf(View* view) {
  Require(view != nullptr, "AI movement behaviour requires an actor state");
  return *reinterpret_cast<Battle_Ai_ActorAiState*>(view);
}

Battle_Ai_TargetApproachView& Approach(Battle_Ai_ActorAiState& state) {
  return *reinterpret_cast<Battle_Ai_TargetApproachView*>(&state);
}

const Battle_Ai_TargetApproachView& Approach(
    const Battle_Ai_ActorAiState& state) {
  return *reinterpret_cast<const Battle_Ai_TargetApproachView*>(&state);
}

constexpr unsigned kByteBits = 8;
constexpr unsigned kHighByte = 3;

std::uint32_t PackBytes(unsigned first, unsigned second, unsigned third,
                        unsigned fourth) {
  return first | (second << kByteBits) | (third << (2 * kByteBits)) |
         (fourth << (kHighByte * kByteBits));
}

std::uint8_t ByteOf(std::uint32_t word, unsigned index) {
  return static_cast<std::uint8_t>(word >> (index * kByteBits));
}

MovementState LoadMovementState(const Battle_Ai_ActorAiState& source) {
  const Battle_Ai_TargetApproachView& approach = Approach(source);
  MovementState state;
  state.actor_id = source.actorId;
  state.tile = source.tile.word;
  state.position = Core::Math::PositionOf(source.position);
  state.movement_enabled = source.movementEnabled != 0;
  state.follows_platform_targets = source.settingsBit14 != 0;
  state.movement_flags = source.movementFlags;
  state.requested_mode = source.requestedMovementMode;
  state.current_mode = source.currentMovementMode;
  std::copy(std::begin(source.movementModeCounters),
            std::end(source.movementModeCounters), state.mode_counters.begin());
  state.navigation_flags = source.navigationFlags;
  state.pursuit = {.opaque_e4 = approach.opaqueE4,
                   .target_id = approach.targetId,
                   .timeout = approach.timeout,
                   .opaque_e7 = approach.opaqueE7,
                   .opaque_e8 = approach.opaqueE8,
                   .range_mode = approach.rangeMode,
                   .previous_option = approach.previousOption,
                   .previous_target_id = approach.previousTargetId};
  state.approach_range = approach.range;
  state.cached_destination = approach.cachedDestination.word;
  state.target_half_tile = PackBytes(
      approach.targetTileX, static_cast<std::uint8_t>(approach.reservedF5[0]),
      approach.targetTileZ, static_cast<std::uint8_t>(approach.reservedF7[0]));
  state.wander_phase = source.wanderPhase;
  state.wander_cooldown = source.wanderCooldown;
  state.wander_center_radius = source.wander_center_radius;
  state.wander_center = source.wander_center.word;
  state.patrol = {.index = source.patrolIndex,
                  .row = source.patrolRow,
                  .cooldown = source.patrolCooldown};
  state.previous_facing_tile = source.previousFacingTile.word;
  state.facing_pending = source.facingPending;
  state.flee_tile = source.flee_tile.word;
  state.flee_timer = source.flee_timer;
  state.arrive_when_near = source.opaque4;
  state.destination = source.destination.word;
  state.move_x = source.moveX;
  state.move_z = source.moveZ;
  for (std::size_t id = 0; id < state.relations.size(); ++id) {
    state.relations.at(id) = std::bit_cast<std::uint32_t>(source.relations[id]);
  }
  state.plan_mode = source.planMode;
  return state;
}

// Writes back the fields the movement behaviours change.
void StoreMovementState(const MovementState& state,
                        Battle_Ai_ActorAiState& target) {
  Battle_Ai_TargetApproachView& approach = Approach(target);
  target.requestedMovementMode = state.requested_mode;
  target.currentMovementMode = state.current_mode;
  std::copy(state.mode_counters.begin(), state.mode_counters.end(),
            std::begin(target.movementModeCounters));
  target.navigationFlags = state.navigation_flags;
  approach.opaqueE4 = state.pursuit.opaque_e4;
  approach.targetId = state.pursuit.target_id;
  approach.timeout = state.pursuit.timeout;
  approach.opaqueE7 = state.pursuit.opaque_e7;
  approach.opaqueE8 = state.pursuit.opaque_e8;
  approach.rangeMode = state.pursuit.range_mode;
  approach.previousOption = state.pursuit.previous_option;
  approach.previousTargetId = state.pursuit.previous_target_id;
  approach.range = state.approach_range;
  approach.cachedDestination.word = state.cached_destination;
  approach.targetTileX = ByteOf(state.target_half_tile, 0);
  approach.reservedF5[0] = static_cast<char>(ByteOf(state.target_half_tile, 1));
  approach.targetTileZ = ByteOf(state.target_half_tile, 2);
  approach.reservedF7[0] =
      static_cast<char>(ByteOf(state.target_half_tile, kHighByte));
  target.wanderPhase = state.wander_phase;
  target.wanderCooldown = state.wander_cooldown;
  target.patrolIndex = state.patrol.index;
  target.patrolCooldown = state.patrol.cooldown;
  target.previousFacingTile.word = state.previous_facing_tile;
  target.facingPending = state.facing_pending;
  target.flee_tile.word = state.flee_tile;
  target.flee_timer = state.flee_timer;
  target.opaque4 = state.arrive_when_near;
  target.destination.word = state.destination;
  target.moveX = state.move_x;
  target.moveZ = state.move_z;
  target.planMode = state.plan_mode;
}

std::span<const std::uint16_t> LivePatrolRoutes() {
  if (Battle_Ai_patrolRoutes == nullptr) return {};
  return {&Battle_Ai_patrolRoutes[0][0],
          kLivePatrolRows * kPatrolWaypointsPerRow};
}

// The battle actor list; nothing when it holds an ID outside the actor slots
// or repeats one.
std::optional<ActorRoster> LiveBattleActors() {
  ActorRoster roster;
  unsigned visited = 0;
  for (const Battle_Actor* actor = Battle_actors[0]; actor != nullptr;
       actor = actor->next) {
    if (actor->id < 0 || actor->id >= kMovementActorSlots) return std::nullopt;
    const unsigned bit = 1u << static_cast<unsigned>(actor->id);
    if ((visited & bit) != 0) return std::nullopt;
    visited |= bit;
    roster.ids.at(roster.count++) = static_cast<std::uint8_t>(actor->id);
  }
  return roster;
}

class LiveMovementHost final : public MovementHost {
 public:
  explicit LiveMovementHost(Battle_Ai_ActorAiState& state) : state_(state) {}

  [[nodiscard]] std::span<const std::uint16_t> PatrolRoutes() const override {
    return LivePatrolRoutes();
  }
  [[nodiscard]] bool HasFrameActor(int actor_id) const override {
    const Battle_Ai_FrameContext* frame = Battle_Ai_FrameState();
    Require(frame != nullptr, "AI movement requires the frame state");
    return frame->actors[static_cast<std::size_t>(actor_id)] != nullptr;
  }
  [[nodiscard]] std::optional<TargetPlacement> Placement(
      int actor_id) const override {
    const Actors_ActorRecord* record =
        Actors_actorRecords[static_cast<std::size_t>(actor_id)];
    if (record == nullptr) return std::nullopt;
    const Actors_ModelInstance& model = record->instance;
    return TargetPlacement{
        .planned_move = model.movementMode != 0 || model.locomotionState != 0,
        .support_layer = model.supportLayer,
        .x = model.position.vx,
        .z = model.position.vz,
        .planned_x = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(model.supportOffset)),
        .planned_z = model.reserved50,
        .current_tile = PackBytes(model.currentTileX, model.currentTileLayer,
                                  model.currentTileZ, model.reserved5F),
        .planned_tile = PackBytes(model.previousTileX, model.previousTileLayer,
                                  model.previousTileZ, model.reserved63)};
  }
  [[nodiscard]] std::optional<RouteGate> PlatformRouteGate(
      int group) const override {
    const auto waypoint = Field::LiveMovingPlatformTargetWaypoint(group);
    if (!waypoint) return std::nullopt;
    const auto end =
        FindLiveNearestRouteGateEnd(group, waypoint->x, waypoint->z);
    if (!end) return std::nullopt;
    return RouteGate{.x = end->x, .z = end->z};
  }
  [[nodiscard]] std::optional<ActorRoster> BattleActors() const override {
    return LiveBattleActors();
  }
  [[nodiscard]] std::int32_t TileHeight(std::uint32_t tile) const override {
    return Field::GroundHeightAtPackedTile(
        CurrentTerrain(), Core::Math::UnpackTileCoordinate(tile));
  }
  [[nodiscard]] bool RunsPatrolCombatBehavior() const override {
    return Settings().decisionCallback == UpdateLivePatrolCombatBehavior;
  }
  [[nodiscard]] std::uint8_t ActionMovementMask() const override {
    return Settings().opaque2D;
  }
  [[nodiscard]] std::optional<std::uint32_t> BestCandidateFlags()
      const override {
    const Battle_Ai_ActionCandidate* best = state_.candidateOrder[0];
    if (best == nullptr) return std::nullopt;
    return std::bit_cast<std::uint32_t>(best->opaque10);
  }

  int UpdateTargetApproach(MovementState& state, int option) override {
    StoreMovementState(state, state_);
    const int moved =
        Battle_Ai_UpdateTargetApproachMovement(&Approach(state_), option);
    state = LoadMovementState(state_);
    return moved;
  }
  void UpdateWander(MovementState& state, std::uint32_t tile,
                    std::uint32_t range) override {
    StoreMovementState(state, state_);
    Battle_Ai_UpdateWanderMovement(
        reinterpret_cast<Battle_Ai_WanderView*>(&state_), tile, range);
    state = LoadMovementState(state_);
  }
  void UpdateWaypointPatrol(MovementState& state) override {
    StoreMovementState(state, state_);
    Battle_Ai_UpdateWaypointPatrol(
        reinterpret_cast<Battle_Ai_PatrolView*>(&state_));
    state = LoadMovementState(state_);
  }
  int UpdateTargetFacing(MovementState& state, int target) override {
    StoreMovementState(state, state_);
    const int turned = Battle_Ai_UpdateTargetFacing(
        reinterpret_cast<Battle_Ai_MovementDecisionView*>(&state_), target);
    state = LoadMovementState(state_);
    return turned;
  }
  void BeginMovementTowardPoint(MovementState& state, int kind) override {
    StoreMovementState(state, state_);
    BeginLiveAiMovementTowardPoint(&state_, kind);
    state = LoadMovementState(state_);
  }
  void IssueCommand(MovementState& state, int command) override {
    StoreMovementState(state, state_);
    IssueLiveActorCommand(&state_, command);
    state = LoadMovementState(state_);
  }
  void DispatchLocomotion(MovementState& state) override {
    StoreMovementState(state, state_);
    DispatchLiveAiLocomotionMode(&state_, 0, 0);
    state = LoadMovementState(state_);
  }
  void FindNearestActiveWaypoint(MovementState& state) override {
    StoreMovementState(state, state_);
    Battle_Ai_FindNearestActiveWaypoint(
        reinterpret_cast<Battle_Ai_WaypointSearchView*>(&state_),
        reinterpret_cast<Battle_Ai_WaypointTile*>(&state_.plannedTile));
    state = LoadMovementState(state_);
  }

 private:
  [[nodiscard]] const Battle_Ai_SettingsView& Settings() const {
    return *reinterpret_cast<const Battle_Ai_SettingsView*>(&state_);
  }

  Battle_Ai_ActorAiState& state_;
};

class LiveRelationshipChanges final : public RelationshipChangeHost {
 public:
  explicit LiveRelationshipChanges(Battle_Ai_ReactionRecord* record)
      : record_(record) {}

  [[nodiscard]] std::optional<ActorRoster> BattleActors() const override {
    return LiveBattleActors();
  }
  [[nodiscard]] bool HasAiState(int actor_id) const override {
    return Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  void ApplyActionReaction(int actor_id) override {
    Battle_Ai_ApplyActorActionReaction(
        ReactionState(actor_id),
        reinterpret_cast<Battle_Ai_StatApplicationView*>(record_));
  }
  void PropagateRelationships(int actor_id) override {
    Battle_Ai_PropagateActorRelationships(ReactionState(actor_id));
  }

 private:
  static Battle_Ai_ReactionView* ReactionState(int actor_id) {
    return reinterpret_cast<Battle_Ai_ReactionView*>(
        Battle_Ai_actorAiStates[static_cast<std::size_t>(actor_id)]);
  }

  Battle_Ai_ReactionRecord* record_;
};

// Runs a movement behaviour on a live actor state and stores its changes.
template <typename Behaviour>
auto RunMovement(Battle_Ai_ActorAiState& live, Behaviour behaviour) {
  MovementState state = LoadMovementState(live);
  LiveMovementHost host(live);
  auto result = behaviour(state, host);
  StoreMovementState(state, live);
  return result;
}

int RequireResult(const std::optional<int>& result, const char* message) {
  if (!result.has_value()) Fail(message);
  return *result;
}

int FindRelatedActor(Battle_Ai_ActorAiState& live, std::uint32_t* tile,
                     int relation) {
  const MovementState state = LoadMovementState(live);
  const LiveMovementHost host(live);
  const std::optional<RelatedActor> related =
      FindActorByRelationship(state, host, relation, tile != nullptr);
  if (!related.has_value()) {
    Fail(
        "AI relationship query requires a valid battle actor list and the "
        "matched actor's model");
  }
  if (tile != nullptr && related->actor_id >= 0) *tile = related->tile;
  return related->actor_id;
}
}  // namespace

int SetLiveMovementBehavior(Battle_Ai_ActorAiState* state, int mode) {
  return RunMovement(
      StateOf(state), [mode](MovementState& movement, LiveMovementHost& host) {
        return RequireResult(
            SetMovementBehavior(movement, host.PatrolRoutes(), mode),
            "AI movement requires a valid behaviour mode and patrol row");
      });
}

void RefreshLiveAiMovementCooldown(Battle_Ai_TerrainMoveView* state) {
  Battle_Ai_ActorAiState& live = StateOf(state);
  live.navigationFlags = RefreshAiMovementCooldown(live.navigationFlags,
                                                   live.movementEnabled != 0);
}

int ApproachLiveActor(Battle_Ai_ActorAiState* state, int target, int distance,
                      int option) {
  return RunMovement(StateOf(state), [=](MovementState& movement,
                                         LiveMovementHost& host) {
    return RequireResult(
        ApproachActor(
            movement, host,
            {.target = target, .distance = distance, .option = option}),
        "AI approach requires a target in the actor slots, its model and its "
        "platform's route gate");
  });
}

void RefreshLiveAiTargetTile(Battle_Ai_TargetApproachView* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(RefreshAiTargetTile(movement, host),
                "AI target refresh requires a target in the actor slots, its "
                "model and its platform's route gate");
        return 0;
      });
}

void RefreshLiveAiTargetTileWhenTracking(
    Battle_Ai_PerceptionUpdateView* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(RefreshAiTargetTileWhenTracking(movement, host),
                "AI target refresh requires a target in the actor slots, its "
                "model and its platform's route gate");
        return 0;
      });
}

void WanderAroundLiveSavedCenter(Battle_Ai_ActorAiState* state) {
  RunMovement(StateOf(state),
              [](MovementState& movement, LiveMovementHost& host) {
                WanderAroundSavedCenter(movement, host);
                return 0;
              });
}

void WanderAroundLiveTile(Battle_Ai_CandidateMovementView* state,
                          std::uint32_t tile, int radius) {
  RunMovement(StateOf(state),
              [=](MovementState& movement, LiveMovementHost& host) {
                WanderAroundTile(movement, host, tile, radius);
                return 0;
              });
}

void MoveLiveActorToTile(Battle_Ai_CandidateMovementView* state,
                         std::uint32_t tile, int radius) {
  RunMovement(StateOf(state),
              [=](MovementState& movement, LiveMovementHost& host) {
                MoveToTile(movement, host, tile, radius);
                return 0;
              });
}

void PatrolLiveWaypoints(Battle_Ai_ActorAiState* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(PatrolWaypoints(movement, host),
                "AI patrol requires a patrol row inside the room's routes");
        return 0;
      });
}

int FleeLiveVisibleEnemies(Battle_Ai_CandidateMovementView* state, int kind) {
  return RunMovement(
      StateOf(state), [kind](MovementState& movement, LiveMovementHost& host) {
        return RequireResult(
            FleeVisibleEnemies(movement, host, kind),
            "AI flight requires a valid battle actor list and each visible "
            "hostile's model");
      });
}

int TrackLiveActorPosition(Battle_Ai_ActorAiState* state, int target) {
  return RunMovement(StateOf(state), [target](MovementState& movement,
                                              LiveMovementHost& host) {
    return RequireResult(
        TrackActorPosition(movement, host, target),
        "AI facing requires a target in the actor slots and its model");
  });
}

int FindLiveActorByRelationship(Battle_Ai_EscortView* state,
                                Battle_Ai_GuardTileWord* tile, int relation) {
  return FindRelatedActor(StateOf(state),
                          tile != nullptr ? &tile->word : nullptr, relation);
}

int FindLiveActorByRelationship(Battle_Ai_ActorAiState* state,
                                std::uint32_t* tile, int relation) {
  return FindRelatedActor(StateOf(state), tile, relation);
}

int LiveTargetFacingCosine(const SVECTOR* from, const SVECTOR* to, int angle,
                           std::uint32_t distance) {
  Require(from != nullptr && to != nullptr,
          "AI facing cosine requires both positions");
  return TargetFacingCosine(Core::Math::PositionOf(*from),
                            Core::Math::PositionOf(*to), angle, distance);
}

void SelectLiveActionMovementMode(Battle_Ai_CandidateReadinessView* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(SelectActionMovementMode(movement, host),
                "AI action movement selection requires its best candidate");
        return 0;
      });
}

void ApplyLiveActionRelationshipChanges(Battle_Ai_ReactionRecord* record) {
  LiveRelationshipChanges host(record);
  Require(ApplyActionRelationshipChanges(host),
          "AI relationship changes require a valid battle actor list");
}
}  // namespace VS::Battle::Ai
