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

#include "Actors/ActorRecord.hpp"
#include "Battle/Action.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Battle/Ai/TargetApproachLive.hpp"
#include "Battle/Ai/WanderLive.hpp"
#include "Battle/Ai/WaypointsLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/Distance.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/PlatformPlacementLive.hpp"

using VS::Core::Gte::SVECTOR;

using VS::Battle::ActionRequestRecord;

namespace VS::Battle::Ai {
namespace {
static_assert(std::size(VS::Battle::Ai::actorAiStates) == kMovementActorSlots);
static_assert(std::extent_v<decltype(FrameContext::actors)> ==
              kMovementActorSlots);
static_assert(std::size(VS::Actors::actorRecords) >= kMovementActorSlots);
static_assert(
    std::tuple_size_v<decltype(VS::Battle::Ai::ActorAiState::relations)> ==
    kMovementActorSlots);
static_assert(sizeof(VS::Battle::Ai::RelationWord) == sizeof(std::uint32_t));
static_assert(sizeof(VS::Battle::Ai::TilePosition) == sizeof(std::uint32_t));
static_assert(
    std::tuple_size_v<
        decltype(VS::Battle::Ai::ActorAiState::movement_mode_counters)> ==
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
  return offsetof(VS::Battle::Ai::ActorAiState, actor_id) + ps1_offset -
         kActorIdOffset;
}
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, actor_id) ==
              offsetof(VS::Battle::Ai::ActorAiState, actor_id));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, range) ==
              ApproachOffset(kApproachRangeOffset));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView,
                       chasing_lost_target) ==
              ApproachOffset(kPursuitProgressOffset));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, recover_in_range) ==
              offsetof(VS::Battle::Ai::ActorAiState, opaque_e7));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView,
                       previous_target_id) ==
              ApproachOffset(kPreviousTargetOffset));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView,
                       cached_destination) ==
              ApproachOffset(kCachedDestinationOffset));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, target_tile_x) ==
              ApproachOffset(kTargetTileXOffset));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, reserved_f7) ==
              ApproachOffset(kTargetTileHighOffset));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, destination) ==
              offsetof(VS::Battle::Ai::ActorAiState, destination));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));
// The views callers hold alias the actor AI state.
static_assert(offsetof(VS::Battle::Ai::TerrainMoveView, destination) ==
              offsetof(VS::Battle::Ai::ActorAiState, destination));
static_assert(offsetof(VS::Battle::Ai::TerrainMoveView, move_z) ==
              offsetof(VS::Battle::Ai::ActorAiState, move_z));
static_assert(offsetof(VS::Battle::Ai::CandidateMovementView, plan_mode) ==
              offsetof(VS::Battle::Ai::ActorAiState, plan_mode));
static_assert(offsetof(VS::Battle::Ai::CandidateMovementView, planned_tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, planned_tile));
static_assert(offsetof(VS::Battle::Ai::CandidateMovementView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));
static_assert(offsetof(VS::Battle::Ai::PerceptionUpdateView, action_status) ==
              offsetof(VS::Battle::Ai::ActorAiState, action_status));
static_assert(offsetof(VS::Battle::Ai::PerceptionUpdateView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));
static_assert(offsetof(VS::Battle::Ai::CandidateReadinessView,
                       candidate_order) ==
              offsetof(VS::Battle::Ai::ActorAiState, candidate_order));
static_assert(offsetof(VS::Battle::Ai::EscortView, actor_id) ==
              offsetof(VS::Battle::Ai::ActorAiState, actor_id));
static_assert(offsetof(VS::Battle::Ai::EscortView, relations) ==
              offsetof(VS::Battle::Ai::ActorAiState, relations));
static_assert(offsetof(VS::Battle::Ai::SettingsView, decision_callback) ==
              offsetof(VS::Battle::Ai::ActorAiState, native_pointer68));
static_assert(offsetof(VS::Battle::Ai::SettingsView, action_movement_mask) ==
              offsetof(VS::Battle::Ai::ActorAiState, reserved_2d));
static_assert(sizeof(VS::Battle::Ai::GuardTileWord) == sizeof(std::uint32_t));

[[noreturn]] void Fail(const char* message) {
  VS::Core::Errorf("ai", "%s", message);
  std::abort();
}

void Require(bool valid, const char* message) {
  if (!valid) Fail(message);
}

template <typename View>
VS::Battle::Ai::ActorAiState& StateOf(View* view) {
  Require(view != nullptr, "AI movement behaviour requires an actor state");
  return *reinterpret_cast<VS::Battle::Ai::ActorAiState*>(view);
}

VS::Battle::Ai::TargetApproachView& Approach(
    VS::Battle::Ai::ActorAiState& state) {
  return *reinterpret_cast<VS::Battle::Ai::TargetApproachView*>(&state);
}

const VS::Battle::Ai::TargetApproachView& Approach(
    const VS::Battle::Ai::ActorAiState& state) {
  return *reinterpret_cast<const VS::Battle::Ai::TargetApproachView*>(&state);
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

MovementState LoadMovementState(const VS::Battle::Ai::ActorAiState& source) {
  const VS::Battle::Ai::TargetApproachView& approach = Approach(source);
  MovementState state;
  state.actor_id = source.actor_id;
  state.tile = source.tile.word;
  state.position = Core::Math::PositionOf(source.position);
  state.movement_enabled = source.movement_enabled != 0;
  state.follows_platform_targets = source.settings_bit14 != 0;
  state.movement_flags = source.movement_flags;
  state.requested_mode = source.requested_movement_mode;
  state.current_mode = source.current_movement_mode;
  std::copy(std::begin(source.movement_mode_counters),
            std::end(source.movement_mode_counters),
            state.mode_counters.begin());
  state.navigation_flags = source.navigation_flags;
  state.pursuit = {.opaque_e4 = approach.chasing_lost_target,
                   .target_id = approach.target_id,
                   .timeout = approach.timeout,
                   .opaque_e7 = approach.recover_in_range,
                   .opaque_e8 = approach.walked_close,
                   .range_mode = approach.range_mode,
                   .previous_option = approach.previous_option,
                   .previous_target_id = approach.previous_target_id};
  state.approach_range = approach.range;
  state.cached_destination = approach.cached_destination.word;
  state.target_half_tile =
      PackBytes(approach.target_tile_x,
                static_cast<std::uint8_t>(approach.reserved_f5[0]),
                approach.target_tile_z,
                static_cast<std::uint8_t>(approach.reserved_f7[0]));
  state.wander_phase = source.wander_phase;
  state.wander_cooldown = source.wander_cooldown;
  state.wander_center_radius = source.wander_center_radius;
  state.wander_center = source.wander_center.word;
  state.patrol = {.index = source.patrol_index,
                  .row = source.patrol_row,
                  .cooldown = source.patrol_cooldown};
  state.previous_facing_tile = source.previous_facing_tile.word;
  state.facing_pending = source.facing_pending;
  state.flee_tile = source.flee_tile.word;
  state.flee_timer = source.flee_timer;
  state.arrive_when_near = source.opaque4;
  state.destination = source.destination.word;
  state.move_x = source.move_x;
  state.move_z = source.move_z;
  for (std::size_t id = 0; id < state.relations.size(); ++id) {
    state.relations.at(id) = std::bit_cast<std::uint32_t>(source.relations[id]);
  }
  state.plan_mode = source.plan_mode;
  return state;
}

// Writes back the fields the movement behaviours change.
void StoreMovementState(const MovementState& state,
                        VS::Battle::Ai::ActorAiState& target) {
  VS::Battle::Ai::TargetApproachView& approach = Approach(target);
  target.requested_movement_mode = state.requested_mode;
  target.current_movement_mode = state.current_mode;
  std::copy(state.mode_counters.begin(), state.mode_counters.end(),
            std::begin(target.movement_mode_counters));
  target.navigation_flags = state.navigation_flags;
  approach.chasing_lost_target = state.pursuit.opaque_e4;
  approach.target_id = state.pursuit.target_id;
  approach.timeout = state.pursuit.timeout;
  approach.recover_in_range = state.pursuit.opaque_e7;
  approach.walked_close = state.pursuit.opaque_e8;
  approach.range_mode = state.pursuit.range_mode;
  approach.previous_option = state.pursuit.previous_option;
  approach.previous_target_id = state.pursuit.previous_target_id;
  approach.range = state.approach_range;
  approach.cached_destination.word = state.cached_destination;
  approach.target_tile_x = ByteOf(state.target_half_tile, 0);
  approach.reserved_f5[0] =
      static_cast<char>(ByteOf(state.target_half_tile, 1));
  approach.target_tile_z = ByteOf(state.target_half_tile, 2);
  approach.reserved_f7[0] =
      static_cast<char>(ByteOf(state.target_half_tile, kHighByte));
  target.wander_phase = state.wander_phase;
  target.wander_cooldown = state.wander_cooldown;
  target.patrol_index = state.patrol.index;
  target.patrol_cooldown = state.patrol.cooldown;
  target.previous_facing_tile.word = state.previous_facing_tile;
  target.facing_pending = state.facing_pending;
  target.flee_tile.word = state.flee_tile;
  target.flee_timer = state.flee_timer;
  target.opaque4 = state.arrive_when_near;
  target.destination.word = state.destination;
  target.move_x = state.move_x;
  target.move_z = state.move_z;
  target.plan_mode = state.plan_mode;
}

// The battle actor list; nothing when it holds an ID outside the actor slots
// or repeats one.
std::optional<ActorRoster> LiveBattleActors() {
  ActorRoster roster;
  unsigned visited = 0;
  for (const VS::Battle::Actor* actor = VS::Battle::actors[0]; actor != nullptr;
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
  explicit LiveMovementHost(VS::Battle::Ai::ActorAiState& state)
      : state_(state) {}

  [[nodiscard]] std::span<const std::uint16_t> PatrolRoutes() const override {
    return LivePatrolRoutes();
  }
  [[nodiscard]] bool HasFrameActor(int actor_id) const override {
    const FrameContext* frame = &LiveFrameContext();
    Require(frame != nullptr, "AI movement requires the frame state");
    return frame->actors[static_cast<std::size_t>(actor_id)] != nullptr;
  }
  [[nodiscard]] std::optional<TargetPlacement> Placement(
      int actor_id) const override {
    const VS::Actors::ActorRecord* record =
        VS::Actors::actorRecords[static_cast<std::size_t>(actor_id)];
    if (record == nullptr) return std::nullopt;
    const VS::Actors::ModelInstance& model = record->instance;
    return TargetPlacement{
        .planned_move = model.movement_mode != 0 || model.locomotion_state != 0,
        .support_layer = model.support_layer,
        .x = model.position.vx,
        .z = model.position.vz,
        .planned_x = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(model.support_offset)),
        .planned_z = model.reserved50,
        .current_tile =
            PackBytes(model.current_tile_x, model.current_tile_layer,
                      model.current_tile_z, model.reserved5f),
        .planned_tile =
            PackBytes(model.previous_tile_x, model.previous_tile_layer,
                      model.previous_tile_z, model.reserved63)};
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
    return Settings().decision_callback == UpdateLivePatrolCombatBehavior;
  }
  [[nodiscard]] std::uint8_t ActionMovementMask() const override {
    return Settings().action_movement_mask;
  }
  [[nodiscard]] std::optional<std::uint32_t> BestCandidateFlags()
      const override {
    const VS::Battle::Ai::ActionCandidateRecord* best =
        state_.candidate_order[0];
    if (best == nullptr) return std::nullopt;
    return std::bit_cast<std::uint32_t>(best->options);
  }

  int UpdateTargetApproach(MovementState& state, int option) override {
    StoreMovementState(state, state_);
    const int moved =
        UpdateLiveTargetApproachMovement(&Approach(state_), option);
    state = LoadMovementState(state_);
    return moved;
  }
  void UpdateWander(MovementState& state, std::uint32_t tile,
                    std::uint32_t range) override {
    StoreMovementState(state, state_);
    UpdateLiveWanderMovement(
        reinterpret_cast<VS::Battle::Ai::WanderView*>(&state_), tile, range);
    state = LoadMovementState(state_);
  }
  void UpdateWaypointPatrol(MovementState& state) override {
    StoreMovementState(state, state_);
    UpdateLiveWaypointPatrol(
        reinterpret_cast<VS::Battle::Ai::PatrolView*>(&state_));
    state = LoadMovementState(state_);
  }
  int UpdateTargetFacing(MovementState& state, int target) override {
    StoreMovementState(state, state_);
    const int turned = UpdateLiveTargetFacing(
        reinterpret_cast<VS::Battle::Ai::MovementDecisionView*>(&state_),
        target);
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
    FindLiveNearestActiveWaypoint(
        reinterpret_cast<VS::Battle::Ai::WaypointSearchView*>(&state_),
        reinterpret_cast<VS::Battle::Ai::WaypointTileRecord*>(
            &state_.planned_tile));
    state = LoadMovementState(state_);
  }

 private:
  [[nodiscard]] const VS::Battle::Ai::SettingsView& Settings() const {
    return *reinterpret_cast<const VS::Battle::Ai::SettingsView*>(&state_);
  }

  VS::Battle::Ai::ActorAiState& state_;
};

class LiveRelationshipChanges final : public RelationshipChangeHost {
 public:
  explicit LiveRelationshipChanges(VS::Battle::Ai::ReactionRecord* record)
      : record_(record) {}

  [[nodiscard]] std::optional<ActorRoster> BattleActors() const override {
    return LiveBattleActors();
  }
  [[nodiscard]] bool HasAiState(int actor_id) const override {
    return VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)] !=
           nullptr;
  }
  void ApplyActionReaction(int actor_id) override {
    // The original passes the completed action as actionStatApplication.
    ApplyLiveActorActionReaction(
        ReactionState(actor_id),
        reinterpret_cast<const ActionRequestRecord*>(record_));
  }
  void PropagateRelationships(int actor_id) override {
    PropagateLiveActorRelationships(ReactionState(actor_id));
  }

 private:
  static VS::Battle::Ai::ReactionView* ReactionState(int actor_id) {
    return reinterpret_cast<VS::Battle::Ai::ReactionView*>(
        VS::Battle::Ai::actorAiStates[static_cast<std::size_t>(actor_id)]);
  }

  VS::Battle::Ai::ReactionRecord* record_;
};

// Runs a movement behaviour on a live actor state and stores its changes.
template <typename Behaviour>
auto RunMovement(VS::Battle::Ai::ActorAiState& live, Behaviour behaviour) {
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

int FindRelatedActor(VS::Battle::Ai::ActorAiState& live, std::uint32_t* tile,
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

int SetLiveMovementBehavior(VS::Battle::Ai::ActorAiState* state, int mode) {
  return RunMovement(
      StateOf(state), [mode](MovementState& movement, LiveMovementHost& host) {
        return RequireResult(
            SetMovementBehavior(movement, host.PatrolRoutes(), mode),
            "AI movement requires a valid behaviour mode and patrol row");
      });
}

void RefreshLiveAiMovementCooldown(VS::Battle::Ai::TerrainMoveView* state) {
  VS::Battle::Ai::ActorAiState& live = StateOf(state);
  live.navigation_flags = RefreshAiMovementCooldown(live.navigation_flags,
                                                    live.movement_enabled != 0);
}

int ApproachLiveActor(VS::Battle::Ai::ActorAiState* state, int target,
                      int distance, int option) {
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

void RefreshLiveAiTargetTile(VS::Battle::Ai::TargetApproachView* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(RefreshAiTargetTile(movement, host),
                "AI target refresh requires a target in the actor slots, its "
                "model and its platform's route gate");
        return 0;
      });
}

void RefreshLiveAiTargetTileWhenTracking(
    VS::Battle::Ai::PerceptionUpdateView* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(RefreshAiTargetTileWhenTracking(movement, host),
                "AI target refresh requires a target in the actor slots, its "
                "model and its platform's route gate");
        return 0;
      });
}

void WanderAroundLiveSavedCenter(VS::Battle::Ai::ActorAiState* state) {
  RunMovement(StateOf(state),
              [](MovementState& movement, LiveMovementHost& host) {
                WanderAroundSavedCenter(movement, host);
                return 0;
              });
}

void WanderAroundLiveTile(VS::Battle::Ai::CandidateMovementView* state,
                          std::uint32_t tile, int radius) {
  RunMovement(StateOf(state),
              [=](MovementState& movement, LiveMovementHost& host) {
                WanderAroundTile(movement, host, tile, radius);
                return 0;
              });
}

void MoveLiveActorToTile(VS::Battle::Ai::CandidateMovementView* state,
                         std::uint32_t tile, int radius) {
  RunMovement(StateOf(state),
              [=](MovementState& movement, LiveMovementHost& host) {
                MoveToTile(movement, host, tile, radius);
                return 0;
              });
}

void PatrolLiveWaypoints(VS::Battle::Ai::ActorAiState* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(PatrolWaypoints(movement, host),
                "AI patrol requires a patrol row inside the room's routes");
        return 0;
      });
}

int FleeLiveVisibleEnemies(VS::Battle::Ai::CandidateMovementView* state,
                           int kind) {
  return RunMovement(
      StateOf(state), [kind](MovementState& movement, LiveMovementHost& host) {
        return RequireResult(
            FleeVisibleEnemies(movement, host, kind),
            "AI flight requires a valid battle actor list and each visible "
            "hostile's model");
      });
}

int TrackLiveActorPosition(VS::Battle::Ai::ActorAiState* state, int target) {
  return RunMovement(StateOf(state), [target](MovementState& movement,
                                              LiveMovementHost& host) {
    return RequireResult(
        TrackActorPosition(movement, host, target),
        "AI facing requires a target in the actor slots and its model");
  });
}

int FindLiveActorByRelationship(VS::Battle::Ai::EscortView* state,
                                VS::Battle::Ai::GuardTileWord* tile,
                                int relation) {
  return FindRelatedActor(StateOf(state),
                          tile != nullptr ? &tile->word : nullptr, relation);
}

int FindLiveActorByRelationship(VS::Battle::Ai::ActorAiState* state,
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

void SelectLiveActionMovementMode(
    VS::Battle::Ai::CandidateReadinessView* state) {
  RunMovement(
      StateOf(state), [](MovementState& movement, LiveMovementHost& host) {
        Require(SelectActionMovementMode(movement, host),
                "AI action movement selection requires its best candidate");
        return 0;
      });
}

void ApplyLiveActionRelationshipChanges(
    VS::Battle::Ai::ReactionRecord* record) {
  LiveRelationshipChanges host(record);
  Require(ApplyActionRelationshipChanges(host),
          "AI relationship changes require a valid battle actor list");
}
}  // namespace VS::Battle::Ai
