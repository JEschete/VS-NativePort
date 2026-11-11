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

#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/AreaRouteLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
constexpr std::size_t kHesitationKinds = 4;
constexpr int kRouteLinkEnds = 2;

static_assert(kAiFrameActorSlots == kNavigationActorSlots);
static_assert(kAiDirectionBytes == kNavigationDirections);
static_assert(std::size(Battle_Ai_ActorAiState{}.movementModeCounters) ==
              kMovementModeCounterCount);
static_assert(sizeof(Battle_Ai_ActorAiState{}.step_height_bytes) ==
              sizeof(std::uint16_t));
static_assert(sizeof(Battle_Ai_ActorAiState{}.detour_tile_bytes) ==
              sizeof(std::uint32_t));
static_assert(sizeof(Battle_Ai_ActorAiState{}.route_exit_bytes) ==
              sizeof(std::uint32_t));
static_assert(sizeof(Battle_Ai_TilePosition) == sizeof(std::uint32_t));
// The movement views alias the actor AI state they are handed.
static_assert(offsetof(Battle_Ai_MoveTargetView, movementDisabled) ==
              offsetof(Battle_Ai_ActorAiState, movementDisabled));
static_assert(offsetof(Battle_Ai_MoveTargetView, tile) ==
              offsetof(Battle_Ai_ActorAiState, tile));
static_assert(offsetof(Battle_Ai_MoveTargetView, destination) ==
              offsetof(Battle_Ai_ActorAiState, destination));
static_assert(offsetof(Battle_Ai_MovementPathView, tile) ==
              offsetof(Battle_Ai_ActorAiState, tile));
static_assert(offsetof(Battle_Ai_MovementPathView, movementFlags) ==
              offsetof(Battle_Ai_ActorAiState, movementFlags));
static_assert(offsetof(Battle_Ai_MovementPathView, destination) ==
              offsetof(Battle_Ai_ActorAiState, destination));
static_assert(offsetof(Battle_Ai_MovementPathView, nextTile) ==
              offsetof(Battle_Ai_ActorAiState, nextTile));
static_assert(offsetof(Battle_Ai_MovementPathView, path) ==
              offsetof(Battle_Ai_ActorAiState, navigationFlags));
static_assert(offsetof(Battle_Ai_WanderView, isStuck) ==
              offsetof(Battle_Ai_ActorAiState, isStuck));
static_assert(offsetof(Battle_Ai_WanderView, tile) ==
              offsetof(Battle_Ai_ActorAiState, tile));
// The approach plan's target and range lie in the state's reserved bytes.
static_assert(offsetof(Battle_Ai_TargetApproachView, range) >=
                  offsetof(Battle_Ai_ActorAiState, reservedD9) &&
              offsetof(Battle_Ai_TargetApproachView, range) + sizeof(int) <=
                  offsetof(Battle_Ai_ActorAiState, reservedD9) +
                      sizeof(Battle_Ai_ActorAiState{}.reservedD9));
static_assert(offsetof(Battle_Ai_TargetApproachView, targetId) >=
                  offsetof(Battle_Ai_ActorAiState, reservedD9) &&
              offsetof(Battle_Ai_TargetApproachView, targetId) <
                  offsetof(Battle_Ai_ActorAiState, reservedD9) +
                      sizeof(Battle_Ai_ActorAiState{}.reservedD9));

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

template <typename T>
T Required(const std::optional<T>& value, const char* message) {
  Require(value.has_value(), message);
  return *value;
}

Battle_Ai_ActorAiState& State(void* view) {
  Require(view != nullptr, "AI ground navigation requires an AI state");
  return *static_cast<Battle_Ai_ActorAiState*>(view);
}

std::uint32_t Word(const u_char* bytes) {
  std::uint32_t value = 0;
  std::memcpy(&value, bytes, sizeof(value));
  return value;
}

void LoadFlags(const Battle_Ai_ActorAiState& state, GroundNavigator& nav) {
  nav.actor_id = state.actorId;
  nav.gait = state.opaque1;
  nav.is_stuck = state.isStuck;
  nav.height_filter = state.opaque3;
  nav.near_target_detour = state.opaque4;
  nav.target_distance_filter = state.opaque5;
  nav.ignore_floor = state.ignore_floor_mask;
  nav.movement_disabled = state.movementDisabled;
  nav.toward_point = state.opaque9;
  nav.facing_locked = static_cast<std::uint8_t>(state.reservedA[2]);
  nav.height_filtered_movement = state.opaque1A;
  nav.movement_enabled = state.movementEnabled;
  nav.area_lock = state.settingsBit14;
  nav.terrain_reject_mask = state.terrain_reject_mask;
  nav.requested_movement_mode = state.requestedMovementMode;
  nav.locomotion_mode = state.locomotionMode;
  nav.ground_area = state.ground_area;
  nav.area_exception = state.reserved154Area;
  nav.movement_target = state.movementTarget;
  nav.locomotion_flags = state.locomotionFlags;
  nav.walk_step = state.walkStep;
  nav.run_step = state.runStep;
  nav.reaction_pending = state.reactionPending;
}

void LoadPlacement(const Battle_Ai_ActorAiState& state, GroundNavigator& nav) {
  nav.tile = state.tile.word;
  nav.previous_tile = state.previousTile;
  nav.x = state.position.vx;
  nav.y = state.position.vy;
  nav.z = state.position.vz;
  nav.collision_radius = state.collision_radius;
  std::memcpy(&nav.step_height, state.step_height_bytes,
              sizeof(nav.step_height));
  nav.tile_step_limit = state.reservedB2;
  nav.max_range = state.maxRange;
  nav.body_height = state.bodyHeight;
  nav.fine_x = state.fine_x;
  nav.fine_z = state.fine_z;
  const auto& approach =
      *reinterpret_cast<const Battle_Ai_TargetApproachView*>(&state);
  nav.approach_target_id = approach.targetId;
  nav.approach_range = approach.range;
}

void LoadMovement(const Battle_Ai_ActorAiState& state, GroundNavigator& nav) {
  nav.turn_timer = state.opaque13A;
  nav.movement_wait = state.movement_wait;
  nav.target_x = state.targetX;
  nav.target_z = state.targetZ;
  nav.movement_flags = state.movementFlags;
  nav.destination = state.destination.word;
  nav.next_tile = state.nextTile.word;
  nav.move_x = state.moveX;
  nav.move_z = state.moveZ;
  nav.heading = state.heading;
  nav.navigation_flags = state.navigationFlags;
  nav.detour_tile = Word(state.detour_tile_bytes);
  nav.detour_repeats = state.detour_repeats;
  nav.detour_run = state.detour_run;
  nav.repeat_wait = state.opaque19A;
  nav.route_gate = state.route_gate;
  nav.route_exit = Word(state.route_exit_bytes);
  nav.stall_counter = state.stallCounter;
  std::memcpy(nav.mode_counters.data(), state.movementModeCounters,
              nav.mode_counters.size());
}

GroundNavigator Load(const Battle_Ai_ActorAiState& state) {
  GroundNavigator navigator;
  LoadFlags(state, navigator);
  LoadPlacement(state, navigator);
  LoadMovement(state, navigator);
  return navigator;
}

void StoreFlags(const GroundNavigator& nav, Battle_Ai_ActorAiState& state) {
  state.actorId = nav.actor_id;
  state.opaque1 = nav.gait;
  state.isStuck = nav.is_stuck;
  state.opaque3 = nav.height_filter;
  state.opaque4 = nav.near_target_detour;
  state.opaque5 = nav.target_distance_filter;
  state.ignore_floor_mask = nav.ignore_floor;
  state.movementDisabled = nav.movement_disabled;
  state.opaque9 = nav.toward_point;
  state.reservedA[2] = static_cast<char>(nav.facing_locked);
  state.opaque1A = nav.height_filtered_movement;
  state.movementEnabled = nav.movement_enabled;
  state.settingsBit14 = nav.area_lock;
  state.terrain_reject_mask = nav.terrain_reject_mask;
  state.requestedMovementMode = nav.requested_movement_mode;
  state.locomotionMode = nav.locomotion_mode;
  state.ground_area = nav.ground_area;
  state.reserved154Area = nav.area_exception;
  state.movementTarget = nav.movement_target;
  state.locomotionFlags = nav.locomotion_flags;
  state.walkStep = nav.walk_step;
  state.runStep = nav.run_step;
  state.reactionPending = nav.reaction_pending;
}

void StorePlacement(const GroundNavigator& nav, Battle_Ai_ActorAiState& state) {
  state.tile.word = nav.tile;
  state.previousTile = nav.previous_tile;
  state.position.vx = nav.x;
  state.position.vy = nav.y;
  state.position.vz = nav.z;
  state.collision_radius = nav.collision_radius;
  std::memcpy(state.step_height_bytes, &nav.step_height,
              sizeof(nav.step_height));
  state.reservedB2 = nav.tile_step_limit;
  state.maxRange = nav.max_range;
  state.bodyHeight = nav.body_height;
  state.fine_x = nav.fine_x;
  state.fine_z = nav.fine_z;
}

void StoreMovement(const GroundNavigator& nav, Battle_Ai_ActorAiState& state) {
  state.opaque13A = nav.turn_timer;
  state.movement_wait = nav.movement_wait;
  state.targetX = nav.target_x;
  state.targetZ = nav.target_z;
  state.movementFlags = nav.movement_flags;
  state.destination.word = nav.destination;
  state.nextTile.word = nav.next_tile;
  state.moveX = nav.move_x;
  state.moveZ = nav.move_z;
  state.heading = nav.heading;
  state.navigationFlags = nav.navigation_flags;
  std::memcpy(state.detour_tile_bytes, &nav.detour_tile,
              sizeof(nav.detour_tile));
  state.detour_repeats = nav.detour_repeats;
  state.detour_run = nav.detour_run;
  state.opaque19A = nav.repeat_wait;
  state.route_gate = nav.route_gate;
  std::memcpy(state.route_exit_bytes, &nav.route_exit, sizeof(nav.route_exit));
  state.stallCounter = nav.stall_counter;
  std::memcpy(state.movementModeCounters, nav.mode_counters.data(),
              nav.mode_counters.size());
}

void Store(const GroundNavigator& navigator, Battle_Ai_ActorAiState& state) {
  StoreFlags(navigator, state);
  StorePlacement(navigator, state);
  StoreMovement(navigator, state);
}

Battle_Ai_MovementPathView* PathView(Battle_Ai_ActorAiState& state) {
  return reinterpret_cast<Battle_Ai_MovementPathView*>(&state);
}

int GroundTest(Battle_Ai_MovementPathView* view, int direction) {
  return CanLiveActorWalkInDirection(
      reinterpret_cast<Battle_Ai_WanderView*>(view), direction);
}

int FlightTest(Battle_Ai_MovementPathView* view, int direction) {
  return Battle_Ai_CanFlyInDirection(
      reinterpret_cast<Battle_Ai_DirectionCacheView*>(view), direction);
}

const Actors_ActorRecord* Model(int actor_id) {
  if (actor_id < 0 || actor_id >= kAiFrameActorSlots) return nullptr;
  return Actors_actorRecords[actor_id];
}

// The room's AI frame, its maps and caches.
class LiveGroundRoom : public LiveRoomRegions<GroundNavigationWorld> {
 public:
  [[nodiscard]] std::uint32_t RoomBounds() const override {
    return Frame().roomBounds;
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
      actor.planned_x = ai->nextPosition.vx;
      actor.planned_y = ai->nextPosition.vy;
      actor.planned_z = ai->nextPosition.vz;
      actor.collision_radius = ai->collision_radius;
      actor.reach = ai->maxRange;
      actor.body_height = ai->bodyHeight;
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

  int NextRandomMasked(int range) override {
    return NextLiveActorRandomMasked(range);
  }
  [[nodiscard]] std::uint8_t HesitationLength(int kind) const override {
    if (kind < 0 || kind >= static_cast<int>(kHesitationKinds)) return 0;
    return std::bit_cast<std::array<std::uint8_t, kHesitationKinds>>(
        Content_battleAiHesitationLengths[0])[static_cast<std::size_t>(kind)];
  }

  [[nodiscard]] bool HasRoomAiState() const override {
    return Battle_Ai_roomAiState != nullptr;
  }
  [[nodiscard]] std::uint8_t ContactMask() const override {
    return Battle_Ai_roomAiState->reservedEPrefix[0];
  }
  void SetContactMask(std::uint8_t mask) override {
    Battle_Ai_roomAiState->reservedEPrefix[0] = mask;
  }

  [[nodiscard]] bool HasRouteGates() const override {
    return Battle_Ai_routeGates != nullptr;
  }
  [[nodiscard]] RouteGateEnd RouteLinkStart(int link) const override {
    const auto& end =
        Battle_Ai_routeGates[static_cast<std::size_t>(link) * kRouteLinkEnds];
    return {.x = end.tileX, .z = end.tileZ};
  }

 protected:
  static Battle_Ai_FrameContext& Frame() { return *Battle_Ai_FrameState(); }

 private:
  const Battle_Actor* cursor_ = nullptr;
};

// The room plus the actor AI state the navigator mirrors; each movement
// operation stores the navigator, runs the movement code and reloads it.
class LiveGroundWorld final : public LiveGroundRoom {
 public:
  explicit LiveGroundWorld(Battle_Ai_ActorAiState* state) : state_(state) {}

  // The original passed the actor state, which the lookup ignores.
  int FindRouteGateAtTile(GroundNavigator& /*navigator*/,
                          std::uint32_t tile) override {
    return Required(FindLiveRouteGateLink(tile, false),
                    "AI route link lookup refused its room or platforms");
  }

  void Recover(GroundNavigator& navigator) override {
    Run(navigator,
        [](Battle_Ai_ActorAiState& state) { RecoverLiveMovement(&state); });
  }
  void IssueCommand(GroundNavigator& navigator, int intent) override {
    Run(navigator, [intent](Battle_Ai_ActorAiState& state) {
      IssueLiveActorCommand(&state, intent);
    });
  }
  void UpdateNavigationAreaMovement(GroundNavigator& navigator) override {
    Run(navigator, [](Battle_Ai_ActorAiState& state) {
      UpdateLiveNavigationAreaMovement(TerrainView(state));
    });
  }
  void MoveWithHeightFilteredDirections(GroundNavigator& navigator) override {
    Run(navigator, [](Battle_Ai_ActorAiState& state) {
      MoveLiveWithHeightFilteredDirections(TerrainView(state));
    });
  }
  void UpdateFlightMovement(GroundNavigator& navigator, int angle) override {
    Run(navigator, [angle](Battle_Ai_ActorAiState& state) {
      Battle_Ai_UpdateFlightMovement(
          reinterpret_cast<Battle_Ai_MovementStepView*>(&state), angle);
    });
  }
  void UpdateFlightDirectionalMovement(GroundNavigator& navigator,
                                       int angle) override {
    Run(navigator, [angle](Battle_Ai_ActorAiState& state) {
      Battle_Ai_UpdateFlightDirectionalMovement(&state, angle);
    });
  }
  void MoveToRadialDestination(GroundNavigator& navigator, int angle) override {
    Run(navigator, [angle](Battle_Ai_ActorAiState& state) {
      MoveLiveToRadialDestination(&state, angle);
    });
  }
  void MoveAwayFromReference(GroundNavigator& navigator, int angle) override {
    Run(navigator, [angle](Battle_Ai_ActorAiState& state) {
      MoveLiveAwayFromReference(&state, angle);
    });
  }
  int FollowObstacleDetour(GroundNavigator& navigator, bool flight,
                           int intent) override {
    int action = -1;
    Run(navigator, [&](Battle_Ai_ActorAiState& state) {
      action = Battle_Ai_FollowObstacleDetour(
          PathView(state), flight ? FlightTest : GroundTest, intent);
    });
    return action;
  }
  void RefreshMovementCooldown(GroundNavigator& navigator) override {
    Run(navigator, [](Battle_Ai_ActorAiState& state) {
      VS::Battle::Ai::RefreshLiveAiMovementCooldown(TerrainView(state));
    });
  }
  std::uint32_t FindAdjacentMovementDestination(GroundNavigator& navigator,
                                                int first, int second,
                                                std::uint32_t angle) override {
    std::uint32_t tile = 0;
    Run(navigator, [&](Battle_Ai_ActorAiState& state) {
      tile = Battle_Ai_FindAdjacentMovementDestination(
          reinterpret_cast<Battle_Ai_MoveTargetView*>(&state), first, second,
          angle);
    });
    return tile;
  }
  int CanFlyInDirection(GroundNavigator& navigator, int direction) override {
    int passable = 0;
    Run(navigator, [&](Battle_Ai_ActorAiState& state) {
      passable = FlightTest(PathView(state), direction);
    });
    return passable;
  }

 private:
  static Battle_Ai_TerrainMoveView* TerrainView(Battle_Ai_ActorAiState& state) {
    return reinterpret_cast<Battle_Ai_TerrainMoveView*>(&state);
  }

  template <typename Operation>
  void Run(GroundNavigator& navigator, Operation operation) {
    Require(state_ != nullptr, "AI movement code requires an AI state");
    Store(navigator, *state_);
    operation(*state_);
    navigator = Load(*state_);
  }

  Battle_Ai_ActorAiState* state_;
};

// A C path test over the actor AI state, run as a direction probe.
class LivePathProbe final : public GroundDirectionProbe {
 public:
  LivePathProbe(Battle_Ai_ActorAiState& state, Battle_Ai_PathTest test)
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
  Battle_Ai_ActorAiState& state_;
  Battle_Ai_PathTest test_;
};

// Runs a mutating core routine over the state's navigator and stores it back.
template <typename Routine>
auto Update(Battle_Ai_ActorAiState& state, Routine routine) {
  LiveGroundWorld world(&state);
  auto navigator = Load(state);
  auto result = routine(world, navigator);
  Store(navigator, state);
  return result;
}
}  // namespace

int LiveDestinationOccupancy(Battle_Ai_MoveTargetView* view,
                             unsigned int tile) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(DestinationOccupancy(world, Load(state), tile),
                  "AI destination occupancy refused its room or actors");
}

int CanTraverseLiveTileEdge(Battle_Ai_MoveTargetView* view, unsigned int from,
                            unsigned int to, int direction) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(CanTraverseTileEdge(world, Load(state), from, to, direction),
                  "AI tile edge query refused its direction or room");
}

int LiveDirectionalBlockingActor(Battle_Ai_MovementPathView* view,
                                 int direction) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(DirectionalBlockingActor(world, Load(state), direction),
                  "AI blocker query refused its direction or room");
}

int CanLiveActorWalkInDirection(Battle_Ai_WanderView* view, int direction) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(CanWalkInDirection(world, Load(state), direction),
                  "AI walk probe refused its direction or room")
             ? 1
             : 0;
}

unsigned int FindLiveGapLandingTile(Battle_Ai_MoveTargetView* view,
                                    unsigned int tile, int direction,
                                    unsigned int from) {
  auto& state = State(view);
  LiveGroundWorld world(&state);
  return Required(FindGapLandingTile(world, Load(state), tile, direction, from),
                  "AI gap landing query refused its direction or room");
}

void SetLiveDetourBoundary(Battle_Ai_ActorAiState* view) {
  auto& state = State(view);
  auto navigator = Load(state);
  SetDetourBoundary(navigator);
  Store(navigator, state);
}

unsigned int RefreshLiveDetourProgress(Battle_Ai_ActorAiState* view,
                                       unsigned int flags) {
  return Update(State(view), [flags](auto& world, auto& navigator) {
    return RefreshDetourProgress(world, navigator, flags);
  });
}

int LiveCardinalPassabilityMask(Battle_Ai_MovementPathView* view,
                                Battle_Ai_PathTest test) {
  auto& state = State(view);
  LivePathProbe probe(state, test);
  auto navigator = Load(state);
  const auto mask = CardinalPassabilityMask(navigator, probe);
  Store(navigator, state);
  return Required(mask, "AI cardinal passability probe refused");
}

unsigned int InitializeLiveObstacleDetour(Battle_Ai_ActorAiState* view,
                                          unsigned int flags,
                                          Battle_Ai_PathTest test) {
  auto& state = State(view);
  LivePathProbe probe(state, test);
  return Required(Update(state,
                         [&](auto& world, auto& navigator) {
                           return InitializeObstacleDetour(world, navigator,
                                                           flags, probe);
                         }),
                  "AI obstacle detour refused its probe or hesitation");
}

int UpdateLiveHesitationTurn(void* view, int chance, int kind, int slot) {
  return Required(Update(State(view),
                         [&](auto& world, auto& navigator) {
                           return UpdateHesitationTurn(world, navigator, chance,
                                                       kind, slot);
                         }),
                  "AI hesitation turn refused its slot, kind or length table");
}

int SelectLiveActorVerticalMovementStep(Battle_Ai_ActorAiState* view,
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

int IsTileBehindLiveActorTarget(Battle_Ai_ActorAiState* view,
                                unsigned int tile) {
  auto& state = State(view);
  const LiveGroundWorld world(&state);
  return Required(IsTileBehindActorTarget(world, Load(state), tile),
                  "AI facing query requires actor and target models")
             ? 1
             : 0;
}

unsigned int FindLiveAiDescentTile(Battle_Ai_ActorAiState* view,
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

void BeginLiveAiAlternateTerrainMovement(Battle_Ai_ActorAiState* view) {
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

void BeginLiveAiMovementTowardPoint(Battle_Ai_ActorAiState* view, int kind) {
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

void ResolveLiveDirectionalMovement(Battle_Ai_ActorAiState* view, int angle,
                                    int mode) {
  Require(Update(State(view),
                 [angle, mode](auto& world, auto& navigator) {
                   return ResolveDirectionalMovement(world, navigator, angle,
                                                     mode);
                 }),
          "AI directional movement refused its mode, probe or descent");
}
}  // namespace VS::Battle::Ai
