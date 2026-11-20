// Hand-maintained (tools/port/owned.csv).
// Live binding for the ground navigation (GroundNavigation.cpp).
#include "Battle/Ai/GroundNavigationLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <utility>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/BlockedMovementLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FlightMovementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Battle/Damage.h"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle::Ai {
namespace {
constexpr std::size_t kHesitationKinds = 4;
constexpr int kRouteLinkEnds = 2;

static_assert(kFrameActorSlots == kNavigationActorSlots);
static_assert(
    std::tuple_size_v<
        decltype(VS::Battle::Ai::ActorAiState::movement_mode_counters)> ==
    kMovementModeCounterCount);
static_assert(sizeof(VS::Battle::Ai::ActorAiState{}.step_height_bytes) ==
              sizeof(std::uint16_t));
static_assert(sizeof(VS::Battle::Ai::ActorAiState{}.detour_tile_bytes) ==
              sizeof(std::uint32_t));
static_assert(sizeof(VS::Battle::Ai::ActorAiState{}.route_exit_bytes) ==
              sizeof(std::uint32_t));
static_assert(sizeof(VS::Battle::Ai::TilePosition) == sizeof(std::uint32_t));
// The movement views alias the actor AI state they are handed.
static_assert(offsetof(VS::Battle::Ai::MoveTargetView, movement_disabled) ==
              offsetof(VS::Battle::Ai::ActorAiState, movement_disabled));
static_assert(offsetof(VS::Battle::Ai::MoveTargetView, tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, tile));
static_assert(offsetof(VS::Battle::Ai::MoveTargetView, destination) ==
              offsetof(VS::Battle::Ai::ActorAiState, destination));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, tile));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, movement_flags) ==
              offsetof(VS::Battle::Ai::ActorAiState, movement_flags));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, destination) ==
              offsetof(VS::Battle::Ai::ActorAiState, destination));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, next_tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, next_tile));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, path) ==
              offsetof(VS::Battle::Ai::ActorAiState, navigation_flags));
static_assert(offsetof(VS::Battle::Ai::WanderView, is_stuck) ==
              offsetof(VS::Battle::Ai::ActorAiState, is_stuck));
static_assert(offsetof(VS::Battle::Ai::WanderView, tile) ==
              offsetof(VS::Battle::Ai::ActorAiState, tile));
// The approach plan's target and range lie in the state's reserved bytes.
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, range) >=
                  offsetof(VS::Battle::Ai::ActorAiState, reserved_d9) &&
              offsetof(VS::Battle::Ai::TargetApproachView, range) +
                      sizeof(int) <=
                  offsetof(VS::Battle::Ai::ActorAiState, reserved_d9) +
                      sizeof(VS::Battle::Ai::ActorAiState{}.reserved_d9));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, target_id) >=
                  offsetof(VS::Battle::Ai::ActorAiState, reserved_d9) &&
              offsetof(VS::Battle::Ai::TargetApproachView, target_id) <
                  offsetof(VS::Battle::Ai::ActorAiState, reserved_d9) +
                      sizeof(VS::Battle::Ai::ActorAiState{}.reserved_d9));
// The pursuit flags the detour clears: 0xE4 lies in the reserved bytes, 0xE7
// and 0xE8 are named in both layouts.
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, opaque_e4) >=
                  offsetof(VS::Battle::Ai::ActorAiState, reserved_d9) &&
              offsetof(VS::Battle::Ai::TargetApproachView, opaque_e4) <
                  offsetof(VS::Battle::Ai::ActorAiState, reserved_d9) +
                      sizeof(VS::Battle::Ai::ActorAiState{}.reserved_d9));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, opaque_e7) ==
              offsetof(VS::Battle::Ai::ActorAiState, opaque_e7));
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, opaque_e8) ==
              offsetof(VS::Battle::Ai::ActorAiState, opaque_e8));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, opaque_e4) ==
              offsetof(VS::Battle::Ai::TargetApproachView, opaque_e4));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, wait_timer) ==
              offsetof(VS::Battle::Ai::ActorAiState, opaque_19a));
static_assert(offsetof(VS::Battle::Ai::MovementPathView, opaque199) ==
              offsetof(VS::Battle::Ai::ActorAiState, detour_run));

VS::Battle::Ai::ActorAiState& State(void* view) {
  Require(view != nullptr, "AI ground navigation requires an AI state");
  return *static_cast<VS::Battle::Ai::ActorAiState*>(view);
}

std::uint32_t Word(const u_char* bytes) {
  std::uint32_t value = 0;
  std::memcpy(&value, bytes, sizeof(value));
  return value;
}

void LoadFlags(const VS::Battle::Ai::ActorAiState& state,
               GroundNavigator& nav) {
  nav.actor_id = state.actor_id;
  nav.gait = state.opaque1;
  nav.is_stuck = state.is_stuck;
  nav.height_filter = state.opaque3;
  nav.near_target_detour = state.opaque4;
  nav.target_distance_filter = state.opaque5;
  nav.ignore_floor = state.ignore_floor_mask;
  nav.movement_disabled = state.movement_disabled;
  nav.toward_point = state.opaque9;
  nav.facing_locked = static_cast<std::uint8_t>(state.reserved_a[2]);
  nav.height_filtered_movement = state.opaque_1a;
  nav.movement_enabled = state.movement_enabled;
  nav.area_lock = state.settings_bit14;
  nav.terrain_reject_mask = state.terrain_reject_mask;
  nav.requested_movement_mode = state.requested_movement_mode;
  nav.locomotion_mode = state.locomotion_mode;
  nav.ground_area = state.ground_area;
  nav.area_exception = state.reserved154_area;
  nav.movement_target = state.movement_target;
  nav.locomotion_flags = state.locomotion_flags;
  nav.walk_step = state.walk_step;
  nav.run_step = state.run_step;
  nav.reaction_pending = state.reaction_pending;
}

void LoadPlacement(const VS::Battle::Ai::ActorAiState& state,
                   GroundNavigator& nav) {
  nav.tile = state.tile.word;
  nav.previous_tile = state.previous_tile;
  nav.x = state.position.vx;
  nav.y = state.position.vy;
  nav.z = state.position.vz;
  nav.collision_radius = state.collision_radius;
  std::memcpy(&nav.step_height, state.step_height_bytes.data(),
              sizeof(nav.step_height));
  nav.tile_step_limit = state.reserved_b2;
  nav.max_range = state.max_range;
  nav.body_height = state.body_height;
  nav.fine_x = state.fine_x;
  nav.fine_z = state.fine_z;
  const auto& approach =
      *reinterpret_cast<const VS::Battle::Ai::TargetApproachView*>(&state);
  nav.approach_target_id = approach.target_id;
  nav.approach_range = approach.range;
  nav.pursuit_e4 = approach.opaque_e4;
  nav.pursuit_e7 = state.opaque_e7;
  nav.pursuit_e8 = state.opaque_e8;
}

void LoadMovement(const VS::Battle::Ai::ActorAiState& state,
                  GroundNavigator& nav) {
  nav.turn_timer = state.opaque_13a;
  nav.movement_wait = state.movement_wait;
  nav.target_x = state.target_x;
  nav.target_z = state.target_z;
  nav.movement_flags = state.movement_flags;
  nav.destination = state.destination.word;
  nav.next_tile = state.next_tile.word;
  nav.move_x = state.move_x;
  nav.move_z = state.move_z;
  nav.heading = state.heading;
  nav.navigation_flags = state.navigation_flags;
  nav.detour_tile = Word(state.detour_tile_bytes.data());
  nav.detour_repeats = state.detour_repeats;
  nav.detour_run = state.detour_run;
  nav.repeat_wait = state.opaque_19a;
  nav.route_gate = state.route_gate;
  nav.route_exit = Word(state.route_exit_bytes.data());
  nav.stall_counter = state.stall_counter;
  std::memcpy(nav.mode_counters.data(), state.movement_mode_counters.data(),
              nav.mode_counters.size());
}

GroundNavigator Load(const VS::Battle::Ai::ActorAiState& state) {
  GroundNavigator navigator;
  LoadFlags(state, navigator);
  LoadPlacement(state, navigator);
  LoadMovement(state, navigator);
  return navigator;
}

void StoreFlags(const GroundNavigator& nav,
                VS::Battle::Ai::ActorAiState& state) {
  state.actor_id = nav.actor_id;
  state.opaque1 = nav.gait;
  state.is_stuck = nav.is_stuck;
  state.opaque3 = nav.height_filter;
  state.opaque4 = nav.near_target_detour;
  state.opaque5 = nav.target_distance_filter;
  state.ignore_floor_mask = nav.ignore_floor;
  state.movement_disabled = nav.movement_disabled;
  state.opaque9 = nav.toward_point;
  state.reserved_a[2] = static_cast<char>(nav.facing_locked);
  state.opaque_1a = nav.height_filtered_movement;
  state.movement_enabled = nav.movement_enabled;
  state.settings_bit14 = nav.area_lock;
  state.terrain_reject_mask = nav.terrain_reject_mask;
  state.requested_movement_mode = nav.requested_movement_mode;
  state.locomotion_mode = nav.locomotion_mode;
  state.ground_area = nav.ground_area;
  state.reserved154_area = nav.area_exception;
  state.movement_target = nav.movement_target;
  state.locomotion_flags = nav.locomotion_flags;
  state.walk_step = nav.walk_step;
  state.run_step = nav.run_step;
  state.reaction_pending = nav.reaction_pending;
}

void StorePlacement(const GroundNavigator& nav,
                    VS::Battle::Ai::ActorAiState& state) {
  state.tile.word = nav.tile;
  state.previous_tile = nav.previous_tile;
  state.position.vx = nav.x;
  state.position.vy = nav.y;
  state.position.vz = nav.z;
  state.collision_radius = nav.collision_radius;
  std::memcpy(state.step_height_bytes.data(), &nav.step_height,
              sizeof(nav.step_height));
  state.reserved_b2 = nav.tile_step_limit;
  state.max_range = nav.max_range;
  state.body_height = nav.body_height;
  state.fine_x = nav.fine_x;
  state.fine_z = nav.fine_z;
  reinterpret_cast<VS::Battle::Ai::TargetApproachView&>(state).opaque_e4 =
      nav.pursuit_e4;
  state.opaque_e7 = nav.pursuit_e7;
  state.opaque_e8 = nav.pursuit_e8;
}

void StoreMovement(const GroundNavigator& nav,
                   VS::Battle::Ai::ActorAiState& state) {
  state.opaque_13a = nav.turn_timer;
  state.movement_wait = nav.movement_wait;
  state.target_x = nav.target_x;
  state.target_z = nav.target_z;
  state.movement_flags = nav.movement_flags;
  state.destination.word = nav.destination;
  state.next_tile.word = nav.next_tile;
  state.move_x = nav.move_x;
  state.move_z = nav.move_z;
  state.heading = nav.heading;
  state.navigation_flags = nav.navigation_flags;
  std::memcpy(state.detour_tile_bytes.data(), &nav.detour_tile,
              sizeof(nav.detour_tile));
  state.detour_repeats = nav.detour_repeats;
  state.detour_run = nav.detour_run;
  state.opaque_19a = nav.repeat_wait;
  state.route_gate = nav.route_gate;
  std::memcpy(state.route_exit_bytes.data(), &nav.route_exit,
              sizeof(nav.route_exit));
  state.stall_counter = nav.stall_counter;
  std::memcpy(state.movement_mode_counters.data(), nav.mode_counters.data(),
              nav.mode_counters.size());
}

void Store(const GroundNavigator& navigator,
           VS::Battle::Ai::ActorAiState& state) {
  StoreFlags(navigator, state);
  StorePlacement(navigator, state);
  StoreMovement(navigator, state);
}

VS::Battle::Ai::MovementPathView* PathView(
    VS::Battle::Ai::ActorAiState& state) {
  return reinterpret_cast<VS::Battle::Ai::MovementPathView*>(&state);
}

int FlightTest(VS::Battle::Ai::MovementPathView* view, int direction) {
  return CanLiveActorFlyInDirection(
      reinterpret_cast<VS::Battle::Ai::ActorAiState*>(view), direction);
}

const Actors_ActorRecord* Model(int actor_id) {
  if (actor_id < 0 || std::cmp_greater_equal(actor_id, kFrameActorSlots)) {
    return nullptr;
  }
  return Actors_actorRecords[actor_id];
}

// The room's AI frame, its maps and caches.
class LiveGroundRoom : public LiveRoomRegions<GroundNavigationWorld> {
 public:
  [[nodiscard]] std::uint32_t RoomBounds() const override {
    return Frame().roomBounds;
  }
  [[nodiscard]] std::optional<RoomTileBounds> RoomTileLimits() const override {
    return LiveRoomTileBounds();
  }
  [[nodiscard]] bool HasTileMaps() const override {
    return Frame().tileFlags != nullptr && Frame().heightMap != nullptr;
  }
  [[nodiscard]] std::uint16_t TileFlags(int x, int z) const override {
    return Frame().tileFlags[z][x];
  }
  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }

  [[nodiscard]] std::uint8_t WalkValid() const override {
    return Frame().walk_valid;
  }
  void SetWalkValid(std::uint8_t bits) override { Frame().walk_valid = bits; }
  [[nodiscard]] std::uint8_t WalkPassable() const override {
    return Frame().walk_passable;
  }
  void SetWalkPassable(std::uint8_t bits) override {
    Frame().walk_passable = bits;
  }
  [[nodiscard]] std::uint8_t BlockerValid() const override {
    return Frame().blocker_valid;
  }
  void SetBlockerValid(std::uint8_t bits) override {
    Frame().blocker_valid = bits;
  }
  [[nodiscard]] std::uint8_t BlockerActor(int direction) const override {
    return Frame().blocker_actor[direction];
  }
  void SetBlockerActor(int direction, std::uint8_t actor) override {
    Frame().blocker_actor[direction] = actor;
  }

  void RewindCollisionActors() override { cursor_ = Battle_actors[0]; }
  [[nodiscard]] std::optional<int> NextCollisionActor() override {
    if (cursor_ == nullptr) return std::nullopt;
    const int id = cursor_->id;
    cursor_ = cursor_->next;
    return id;
  }
  [[nodiscard]] std::optional<GroundCollisionActor> CollisionActor(
      int actor_id) const override {
    const auto* model = Model(actor_id);
    if (model == nullptr) return std::nullopt;
    const auto* ai = Frame().actors[actor_id];
    GroundCollisionActor actor{
        .has_ai_state = ai != nullptr,
        .model_moving = model->instance.movementMode != 0 ||
                        model->instance.locomotionState != 0,
        .model_x = model->instance.position.vx,
        .model_y = model->instance.position.vy,
        .model_z = model->instance.position.vz,
        .model_radius = model->occupancyRadius,
        .model_height = model->occupancyHeight};
    if (ai != nullptr) {
      actor.planned_x = ai->next_position.vx;
      actor.planned_y = ai->next_position.vy;
      actor.planned_z = ai->next_position.vz;
      actor.collision_radius = ai->collision_radius;
      actor.reach = ai->max_range;
      actor.body_height = ai->body_height;
    }
    return actor;
  }
  [[nodiscard]] std::optional<std::int16_t> ModelFacing(
      int actor_id) const override {
    const auto* model = Model(actor_id);
    if (model == nullptr) return std::nullopt;
    return model->instance.facing;
  }
  [[nodiscard]] std::optional<GroundPoint> ModelPosition(
      int actor_id) const override {
    const auto* model = Model(actor_id);
    if (model == nullptr) return std::nullopt;
    return GroundPoint{.x = model->instance.position.vx,
                       .z = model->instance.position.vz};
  }
  [[nodiscard]] std::optional<ActorDetourWait> OtherActorDetourWait(
      int actor_id) const override {
    const auto* other = OtherState(actor_id);
    if (other == nullptr) return std::nullopt;
    return ActorDetourWait{.movement_flags = other->movement_flags,
                           .repeat_wait = other->opaque_19a};
  }
  void SetOtherActorRepeatWait(int actor_id, std::uint8_t wait) override {
    auto* other = OtherState(actor_id);
    Require(other != nullptr, "AI detour nudge requires the actor's AI state");
    other->opaque_19a = wait;
  }

  int NextRandomMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
  [[nodiscard]] std::uint8_t HesitationLength(int kind) const override {
    if (kind < 0 || kind >= static_cast<int>(kHesitationKinds)) return 0;
    return std::bit_cast<std::array<std::uint8_t, kHesitationKinds>>(
        VS::Content::battleAiHesitationLengths[0])[static_cast<std::size_t>(
        kind)];
  }

  [[nodiscard]] bool HasRoomAiState() const override {
    return VS::Battle::Ai::roomAiState != nullptr;
  }
  [[nodiscard]] std::uint8_t ContactMask() const override {
    return VS::Battle::Ai::roomAiState->reserved_e_prefix[0];
  }
  void SetContactMask(std::uint8_t mask) override {
    VS::Battle::Ai::roomAiState->reserved_e_prefix[0] = mask;
  }

  [[nodiscard]] bool HasRouteGates() const override {
    return VS::Battle::Ai::routeGates != nullptr;
  }
  [[nodiscard]] RouteGateEnd RouteLinkStart(int link) const override {
    const auto& end =
        VS::Battle::Ai::routeGates[static_cast<std::size_t>(link) *
                                   kRouteLinkEnds];
    return {.x = end.tile_x, .z = end.tile_z};
  }

 protected:
  static FrameContext& Frame() { return LiveFrameContext(); }

 private:
  static VS::Battle::Ai::ActorAiState* OtherState(int actor_id) {
    if (actor_id < 0 || std::cmp_greater_equal(actor_id, kFrameActorSlots)) {
      return nullptr;
    }
    return Frame().actors[actor_id];
  }

  const Battle_Actor* cursor_ = nullptr;
};

// The room plus the actor AI state the navigator mirrors; each movement
// operation stores the navigator, runs the movement code and reloads it.
class LiveGroundWorld final : public LiveGroundRoom {
 public:
  explicit LiveGroundWorld(VS::Battle::Ai::ActorAiState* state)
      : state_(state) {}

  // The original passed the actor state, which the lookup ignores.
  int FindRouteGateAtTile(GroundNavigator& /*navigator*/,
                          std::uint32_t tile) override {
    return Required(FindLiveRouteGateLink(tile, false),
                    "AI route link lookup refused its room or platforms");
  }

  void Recover(GroundNavigator& navigator) override {
    Run(navigator, [](VS::Battle::Ai::ActorAiState& state) {
      RecoverLiveMovement(&state);
    });
  }
  void IssueCommand(GroundNavigator& navigator, int intent) override {
    Run(navigator, [intent](VS::Battle::Ai::ActorAiState& state) {
      IssueLiveActorCommand(&state, intent);
    });
  }
  void UpdateNavigationAreaMovement(GroundNavigator& navigator) override {
    Run(navigator, [](VS::Battle::Ai::ActorAiState& state) {
      UpdateLiveNavigationAreaMovement(TerrainView(state));
    });
  }
  void MoveWithHeightFilteredDirections(GroundNavigator& navigator) override {
    Run(navigator, [](VS::Battle::Ai::ActorAiState& state) {
      MoveLiveWithHeightFilteredDirections(TerrainView(state));
    });
  }
  void UpdateFlightMovement(GroundNavigator& navigator, int angle) override {
    Run(navigator, [angle](VS::Battle::Ai::ActorAiState& state) {
      UpdateLiveFlightMovement(&state, angle);
    });
  }
  void UpdateFlightDirectionalMovement(GroundNavigator& navigator,
                                       int angle) override {
    Run(navigator, [angle](VS::Battle::Ai::ActorAiState& state) {
      UpdateLiveFlightDirectionalMovement(&state, angle);
    });
  }
  void MoveToRadialDestination(GroundNavigator& navigator, int angle) override {
    Run(navigator, [angle](VS::Battle::Ai::ActorAiState& state) {
      MoveLiveToRadialDestination(&state, angle);
    });
  }
  void MoveAwayFromReference(GroundNavigator& navigator, int angle) override {
    Run(navigator, [angle](VS::Battle::Ai::ActorAiState& state) {
      MoveLiveAwayFromReference(&state, angle);
    });
  }
  void RefreshMovementCooldown(GroundNavigator& navigator) override {
    Run(navigator, [](VS::Battle::Ai::ActorAiState& state) {
      VS::Battle::Ai::RefreshLiveAiMovementCooldown(TerrainView(state));
    });
  }
  std::uint32_t FindAdjacentMovementDestination(GroundNavigator& navigator,
                                                int first, int second,
                                                std::uint32_t angle) override {
    std::uint32_t tile = 0;
    Run(navigator, [&](VS::Battle::Ai::ActorAiState& state) {
      tile = FindLiveAdjacentMovementDestination(
          reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&state), first,
          second, angle);
    });
    return tile;
  }
  int CanFlyInDirection(GroundNavigator& navigator, int direction) override {
    int passable = 0;
    Run(navigator, [&](VS::Battle::Ai::ActorAiState& state) {
      passable = FlightTest(PathView(state), direction);
    });
    return passable;
  }

 private:
  static VS::Battle::Ai::TerrainMoveView* TerrainView(
      VS::Battle::Ai::ActorAiState& state) {
    return reinterpret_cast<VS::Battle::Ai::TerrainMoveView*>(&state);
  }

  template <typename Operation>
  void Run(GroundNavigator& navigator, Operation operation) {
    Require(state_ != nullptr, "AI movement code requires an AI state");
    Store(navigator, *state_);
    operation(*state_);
    navigator = Load(*state_);
  }

  VS::Battle::Ai::ActorAiState* state_;
};

// A C path test over the actor AI state, run as a direction probe.
class LivePathProbe final : public GroundDirectionProbe {
 public:
  LivePathProbe(VS::Battle::Ai::ActorAiState& state, PathTest test)
      : state_(state), test_(test) {
    Require(test != nullptr, "AI path probe requires a native callback");
  }
  [[nodiscard]] std::optional<int> Probe(GroundNavigator& navigator,
                                         int direction) override {
    Store(navigator, state_);
    const int result = test_(PathView(state_), direction);
    navigator = Load(state_);
    return result;
  }

 private:
  VS::Battle::Ai::ActorAiState& state_;
  PathTest test_;
};

// Runs a mutating core routine over the state's navigator and stores it back.
template <typename Routine>
auto Update(VS::Battle::Ai::ActorAiState& state, Routine routine) {
  LiveGroundWorld world(&state);
  auto navigator = Load(state);
  auto result = routine(world, navigator);
  Store(navigator, state);
  return result;
}
}  // namespace

int LiveDestinationOccupancy(VS::Battle::Ai::MoveTargetView* view,
                             unsigned int tile) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(DestinationOccupancy(world, Load(state), tile),
                  "AI destination occupancy refused its room or actors");
}

int CanTraverseLiveTileEdge(VS::Battle::Ai::MoveTargetView* view,
                            unsigned int from, unsigned int to, int direction) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(CanTraverseTileEdge(world, Load(state), from, to, direction),
                  "AI tile edge query refused its direction or room");
}

int LiveDirectionalBlockingActor(VS::Battle::Ai::MovementPathView* view,
                                 int direction) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(DirectionalBlockingActor(world, Load(state), direction),
                  "AI blocker query refused its direction or room");
}

int CanLiveActorWalkInDirection(VS::Battle::Ai::WanderView* view,
                                int direction) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(CanWalkInDirection(world, Load(state), direction),
                  "AI walk probe refused its direction or room")
             ? 1
             : 0;
}

unsigned int FindLiveGapLandingTile(VS::Battle::Ai::MoveTargetView* view,
                                    unsigned int tile, int direction,
                                    unsigned int from) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(FindGapLandingTile(world, Load(state), tile, direction, from),
                  "AI gap landing query refused its direction or room");
}

void SetLiveDetourBoundary(VS::Battle::Ai::ActorAiState* view) {
  auto& state = State(view);
  auto navigator = Load(state);
  SetDetourBoundary(navigator);
  Store(navigator, state);
}

unsigned int RefreshLiveDetourProgress(VS::Battle::Ai::ActorAiState* view,
                                       unsigned int flags) {
  return Update(State(view), [flags](auto& world, auto& navigator) {
    return RefreshDetourProgress(world, navigator, flags);
  });
}

int LiveCardinalPassabilityMask(VS::Battle::Ai::MovementPathView* view,
                                PathTest test) {
  auto& state = State(view);
  LivePathProbe probe(state, test);
  auto navigator = Load(state);
  const auto mask = CardinalPassabilityMask(navigator, probe);
  Store(navigator, state);
  return Required(mask, "AI cardinal passability probe refused");
}

unsigned int InitializeLiveObstacleDetour(VS::Battle::Ai::ActorAiState* view,
                                          unsigned int flags, PathTest test) {
  auto& state = State(view);
  LivePathProbe probe(state, test);
  return Required(Update(state,
                         [&](auto& world, auto& navigator) {
                           return InitializeObstacleDetour(world, navigator,
                                                           flags, probe);
                         }),
                  "AI obstacle detour refused its probe or hesitation");
}

int FollowLiveObstacleDetour(VS::Battle::Ai::MovementPathView* view,
                             PathTest test, int blocked_flag_bit) {
  auto& state = State(view);
  LivePathProbe probe(state, test);
  return Required(Update(state,
                         [&](auto& world, auto& navigator) {
                           return FollowObstacleDetour(world, navigator, probe,
                                                       blocked_flag_bit);
                         }),
                  "AI obstacle detour refused its probe, room or blocker");
}

int UpdateLiveHesitationTurn(void* view, int chance, int kind, int slot) {
  return Required(Update(State(view),
                         [&](auto& world, auto& navigator) {
                           return UpdateHesitationTurn(world, navigator, chance,
                                                       kind, slot);
                         }),
                  "AI hesitation turn refused its slot, kind or length table");
}

int SelectLiveActorVerticalMovementStep(VS::Battle::Ai::ActorAiState* view,
                                        int* value, int* code) {
  Require(value != nullptr && code != nullptr,
          "AI vertical movement requires output storage");
  const auto step =
      Required(SelectActorVerticalMovementStep(Load(State(view))),
               "AI running vertical movement has no configured gait");
  if (!step.selected) return 0;
  *value = step.value;
  *code = step.code;
  return 1;
}

int IsTileBehindLiveActorTarget(VS::Battle::Ai::ActorAiState* view,
                                unsigned int tile) {
  auto& state = State(view);
  const LiveGroundWorld world(&state);
  return Required(IsTileBehindActorTarget(world, Load(state), tile),
                  "AI facing query requires actor and target models")
             ? 1
             : 0;
}

unsigned int FindLiveAiDescentTile(VS::Battle::Ai::ActorAiState* view,
                                   unsigned int tile, int direction,
                                   int steps) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(FindAiDescentTile(world, Load(state), tile, direction, steps),
                  "AI descent scan refused its direction or room");
}

void ClearLiveAiContactOccupancy(int bit) {
  LiveGroundWorld world(nullptr);
  ClearAiContactOccupancy(world, bit);
}

void SetLiveAiContactOccupancy(int bit) {
  LiveGroundWorld world(nullptr);
  SetAiContactOccupancy(world, bit);
}

void BeginLiveAiTerrainMovement(void* view) {
  Update(State(view), [](auto& world, auto& navigator) {
    BeginAiTerrainMovement(world, navigator);
    return true;
  });
}

void BeginLiveAiAlternateTerrainMovement(VS::Battle::Ai::ActorAiState* view) {
  Update(State(view), [](auto& world, auto& navigator) {
    BeginAiAlternateTerrainMovement(world, navigator);
    return true;
  });
}

void MoveLiveActorTowardDestinationTile(void* view, int /*angle*/) {
  Require(Update(State(view),
                 [](auto& world, auto& navigator) {
                   return MoveActorTowardDestinationTile(world, navigator);
                 }),
          "AI destination move refused its route link or room");
}

void BeginLiveAiGroundMovement(void* view, int angle) {
  Require(Update(State(view),
                 [angle](auto& world, auto& navigator) {
                   return BeginAiGroundMovement(world, navigator, angle);
                 }),
          "AI ground movement refused its route link, room or probe");
}

void BeginLiveAiMovementTowardPoint(VS::Battle::Ai::ActorAiState* view,
                                    int kind) {
  Require(Update(State(view),
                 [kind](auto& world, auto& navigator) {
                   return BeginAiMovementTowardPoint(world, navigator, kind);
                 }),
          "AI point movement refused its locomotion mode");
}

void DispatchLiveAiLocomotionMode(void* view, int kind, int angle) {
  Require(Update(State(view),
                 [kind, angle](auto& world, auto& navigator) {
                   return DispatchAiLocomotionMode(world, navigator, kind,
                                                   angle);
                 }),
          "AI locomotion dispatch has an invalid mode or request");
}

void ResolveLiveDirectionalMovement(VS::Battle::Ai::ActorAiState* view,
                                    int angle, int mode) {
  Require(Update(State(view),
                 [angle, mode](auto& world, auto& navigator) {
                   return ResolveDirectionalMovement(world, navigator, angle,
                                                     mode);
                 }),
          "AI directional movement refused its mode, probe or descent");
}
}  // namespace VS::Battle::Ai
