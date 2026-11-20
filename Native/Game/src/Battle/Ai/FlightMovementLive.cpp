// Hand-maintained (tools/port/owned.csv).
// Live binding for the flight movement (FlightMovement.cpp).
#include "Battle/Ai/FlightMovementLive.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/FlightMovement.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;

static_assert(kNavigationDirections == kFlightDirectionCount);
static_assert(sizeof(FrameContext::blocker_actor) == kFlightDirectionCount);
static_assert(sizeof(AiState{}.reserved_bc) == sizeof(std::int16_t));
// The approach target (0xE5) lies in the state's reserved bytes, which the
// target approach view names.
constexpr std::size_t kActorIdOffset = 0x88;
constexpr std::size_t kApproachTargetOffset = 0xE5;
static_assert(offsetof(VS::Battle::Ai::TargetApproachView, target_id) ==
              offsetof(AiState, actor_id) + kApproachTargetOffset -
                  kActorIdOffset);

FrameContext& Frame() { return LiveFrameContext(); }

FlightMover Load(const AiState& state) {
  const auto* model = state.model;
  const auto& approach =
      *reinterpret_cast<const VS::Battle::Ai::TargetApproachView*>(&state);
  std::int16_t ground_clearance = 0;
  std::memcpy(&ground_clearance, state.reserved_bc.data(),
              sizeof(ground_clearance));
  return {.close_approach = static_cast<std::uint8_t>(state.reserved0),
          .movement_disabled = state.movement_disabled,
          .tile = state.tile.word,
          .previous_tile = static_cast<std::uint32_t>(state.previous_tile),
          .x = state.position.vx,
          .y = state.position.vy,
          .z = state.position.vz,
          .flies = model != nullptr
                       ? std::optional<bool>(model->opaque_a_bit5 != 0)
                       : std::nullopt,
          .actor_id = state.actor_id,
          .radius = state.radius,
          .max_range = state.max_range,
          .body_height = state.body_height,
          .ground_clearance = ground_clearance,
          .flight_height = state.flight_height,
          .approach_target = approach.target_id,
          .movement_flags = state.movement_flags,
          .destination = state.destination.word,
          .move_x = state.move_x,
          .move_z = state.move_z,
          .move_y = state.move_y,
          .heading = state.heading,
          .stall_counter = state.stall_counter};
}

// Flight movement writes only these fields; the rest are read.
void Store(const FlightMover& mover, AiState& state) {
  state.movement_disabled = mover.movement_disabled;
  state.move_x = mover.move_x;
  state.move_z = mover.move_z;
  state.move_y = mover.move_y;
  state.stall_counter = mover.stall_counter;
}

// The room's AI frame, the battle actors, the moving platforms and the
// actor's movement code. Each call taking the mover stores it into the AI
// state first and reloads it after.
class LiveFlightWorld final : public FlightMovementWorld {
 public:
  explicit LiveFlightWorld(AiState& state) : state_(state) {}

  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] FlightDirectionCache DirectionCache() const override {
    const auto& frame = Frame();
    FlightDirectionCache cache{.fly_tested = frame.walk_valid,
                               .fly_open = frame.walk_passable,
                               .obstruction_tested = frame.blocker_valid};
    std::copy(std::begin(frame.blocker_actor), std::end(frame.blocker_actor),
              cache.obstruction.begin());
    return cache;
  }
  void SetDirectionCache(const FlightDirectionCache& cache) override {
    auto& frame = Frame();
    frame.walk_valid = cache.fly_tested;
    frame.walk_passable = cache.fly_open;
    frame.blocker_valid = cache.obstruction_tested;
    std::copy(cache.obstruction.begin(), cache.obstruction.end(),
              std::begin(frame.blocker_actor));
  }
  [[nodiscard]] std::vector<Field::PlatformContact> PlatformPositions()
      const override {
    return Field::CollectLiveMovingPlatformPositions();
  }
  void RewindActors() override { cursor_ = VS::Battle::actors[0]; }
  [[nodiscard]] std::optional<int> NextActor() override {
    if (cursor_ == nullptr) return std::nullopt;
    const int id = cursor_->id;
    cursor_ = cursor_->next;
    return id;
  }
  [[nodiscard]] std::optional<FlightObstacle> Obstacle(
      int actor_id) const override {
    if (actor_id < 0 || std::cmp_greater_equal(actor_id, kFrameActorSlots)) {
      return std::nullopt;
    }
    const auto* other = Frame().actors[actor_id];
    if (other == nullptr) return std::nullopt;
    FlightObstacle obstacle{.actor_id = other->actor_id,
                            .radius = other->radius,
                            .max_range = other->max_range,
                            .body_height = other->body_height,
                            .position = std::nullopt};
    if (other->model != nullptr) {
      const auto& position =
          reinterpret_cast<const Actors_ActorRecord*>(other->model)
              ->instance.position;
      obstacle.position =
          FlightPoint{.x = position.vx, .y = position.vy, .z = position.vz};
    }
    return obstacle;
  }
  [[nodiscard]] std::optional<FlightRoom> Room() const override {
    const auto bounds = LiveRoomTileBounds();
    if (!bounds) return std::nullopt;
    return FlightRoom{.bounds = *bounds,
                      .ceiling = VS::Battle::Ai::roomAiState->flight_ceiling};
  }
  [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x,
                                                       int z) const override {
    const auto& frame = Frame();
    if (frame.tileFlags == nullptr || x < 0 || z < 0 ||
        x >= kNavigationGridSide || z >= kNavigationGridSide) {
      return std::nullopt;
    }
    return frame.tileFlags[z][x];
  }
  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }
  [[nodiscard]] std::optional<FlightTargetRecord> TargetRecord(
      int actor_id) const override {
    if (actor_id < 0 ||
        static_cast<std::size_t>(actor_id) >= std::size(Actors_actorRecords)) {
      return std::nullopt;
    }
    const auto* record = Actors_actorRecords[actor_id];
    if (record == nullptr) return std::nullopt;
    return FlightTargetRecord{.y = record->instance.position.vy,
                              .tile_x = record->instance.currentTileX,
                              .tile_z = record->instance.currentTileZ,
                              .collision_height = record->collisionHeight};
  }
  [[nodiscard]] std::optional<FlightStep> SelectVerticalStep(
      FlightMover& mover) override {
    int step = 0;
    int intent = 0;
    int selected = 0;
    Run(mover, [&] {
      selected = SelectLiveActorVerticalMovementStep(&state_, &step, &intent);
    });
    if (selected == 0) return std::nullopt;
    return FlightStep{.step = step, .intent = intent};
  }
  [[nodiscard]] int ResolveDirectionalMovement(FlightMover& mover, int angle,
                                               int mode) override {
    Run(mover, [&] { ResolveLiveDirectionalMovement(&state_, angle, mode); });
    return Frame().movementResult;
  }
  [[nodiscard]] int DestinationOccupancy(FlightMover& mover,
                                         std::uint32_t tile) override {
    int occupancy = 0;
    Run(mover, [&] {
      occupancy = LiveDestinationOccupancy(
          reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&state_), tile);
    });
    return occupancy;
  }
  void IssueCommand(FlightMover& mover, int intent) override {
    Run(mover, [&] { IssueLiveActorCommand(&state_, intent); });
  }
  void Recover(FlightMover& mover) override {
    Run(mover, [&] { RecoverLiveMovement(&state_); });
  }
  void ReleaseReservations(FlightMover& mover) override {
    Run(mover, [&] { ReleaseLiveActorAiReservations(&state_); });
  }
  void BeginTerrainMovement(FlightMover& mover) override {
    Run(mover, [&] { BeginLiveAiTerrainMovement(&state_); });
  }
  void BeginAlternateTerrainMovement(FlightMover& mover) override {
    Run(mover, [&] { BeginLiveAiAlternateTerrainMovement(&state_); });
  }
  void MoveTowardDestinationTile(FlightMover& mover, int angle) override {
    Run(mover, [&] { MoveLiveActorTowardDestinationTile(&state_, angle); });
  }
  void BeginGroundMovement(FlightMover& mover, int angle) override {
    Run(mover, [&] { BeginLiveAiGroundMovement(&state_, angle); });
  }

 private:
  template <typename Call>
  void Run(FlightMover& mover, Call&& call) {
    Store(mover, state_);
    call();
    mover = Load(state_);
  }

  AiState& state_;
  const Battle_Actor* cursor_ = nullptr;
};

AiState& RequireState(AiState* state) {
  Require(state != nullptr, "AI flight movement requires an AI state");
  return *state;
}

// Runs a typed routine over the state's mover and stores it back.
template <typename Routine>
auto Update(AiState& state, Routine routine) {
  LiveFlightWorld world(state);
  auto mover = Load(state);
  const auto result = routine(world, mover);
  Store(mover, state);
  return result;
}
}  // namespace

int IsLiveFlightDirectionObstructed(VS::Battle::Ai::ActorAiState* state,
                                    int direction, int ahead) {
  auto& live = RequireState(state);
  LiveFlightWorld world(live);
  const auto answer =
      FlightDirectionObstruction(world, Load(live), direction, ahead != 0);
  Require(answer.has_value(),
          "AI flight obstruction refused its direction or actors");
  return *answer;
}

int CanLiveActorFlyInDirection(VS::Battle::Ai::ActorAiState* state,
                               int direction) {
  auto& live = RequireState(state);
  LiveFlightWorld world(live);
  const auto open = CanFlyInDirection(world, Load(live), direction);
  Require(open.has_value(), "AI flight test refused its direction or room");
  return *open ? 1 : 0;
}

int UpdateLiveFlightGroundTransition(VS::Battle::Ai::ActorAiState* state,
                                     int angle, int mode) {
  const auto changed = Update(
      RequireState(state), [&](FlightMovementWorld& world, FlightMover& mover) {
        return UpdateFlightGroundTransition(world, mover, angle, mode != 0);
      });
  Require(changed.has_value(),
          "AI flight transition refused its room, terrain or model");
  return *changed ? 1 : 0;
}

void UpdateLiveFlightMovement(VS::Battle::Ai::ActorAiState* state, int angle) {
  const bool done = Update(RequireState(state),
                           [&](FlightMovementWorld& world, FlightMover& mover) {
                             return UpdateFlightMovement(world, mover, angle);
                           });
  Require(done, "AI flight movement refused its room, actors or model");
}

void UpdateLiveFlightDirectionalMovement(VS::Battle::Ai::ActorAiState* state,
                                         int angle) {
  const bool done = Update(
      RequireState(state), [&](FlightMovementWorld& world, FlightMover& mover) {
        return UpdateFlightDirectionalMovement(world, mover, angle);
      });
  Require(done, "AI flight movement refused its room, actors or model");
}
}  // namespace VS::Battle::Ai
