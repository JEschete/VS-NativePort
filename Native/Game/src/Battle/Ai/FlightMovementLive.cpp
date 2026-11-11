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
#include <vector>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/FlightMovement.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/RadialMovementLive.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Damage.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;

static_assert(kAiDirectionBytes == kFlightDirectionCount);
static_assert(sizeof(Battle_Ai_FrameContext::blocker_actor) ==
              kFlightDirectionCount);
static_assert(sizeof(AiState{}.reservedBC) == sizeof(std::int16_t));
// The approach target (0xE5) lies in the state's reserved bytes, which the
// target approach view names.
constexpr std::size_t kActorIdOffset = 0x88;
constexpr std::size_t kApproachTargetOffset = 0xE5;
static_assert(offsetof(Battle_Ai_TargetApproachView, targetId) ==
              offsetof(AiState, actorId) + kApproachTargetOffset -
                  kActorIdOffset);

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

Battle_Ai_FrameContext& Frame() { return *Battle_Ai_FrameState(); }

FlightMover Load(const AiState& state) {
  const auto* model = state.model;
  const auto& approach =
      *reinterpret_cast<const Battle_Ai_TargetApproachView*>(&state);
  std::int16_t ground_clearance = 0;
  std::memcpy(&ground_clearance, state.reservedBC, sizeof(ground_clearance));
  return {.close_approach = static_cast<std::uint8_t>(state.reserved0),
          .movement_disabled = state.movementDisabled,
          .tile = state.tile.word,
          .previous_tile = static_cast<std::uint32_t>(state.previousTile),
          .x = state.position.vx,
          .y = state.position.vy,
          .z = state.position.vz,
          .flies = model != nullptr
                       ? std::optional<bool>(model->opaqueABit5 != 0)
                       : std::nullopt,
          .actor_id = state.actorId,
          .radius = state.radius,
          .max_range = state.maxRange,
          .body_height = state.bodyHeight,
          .ground_clearance = ground_clearance,
          .flight_height = state.flightHeight,
          .approach_target = approach.targetId,
          .movement_flags = state.movementFlags,
          .destination = state.destination.word,
          .move_x = state.moveX,
          .move_z = state.moveZ,
          .move_y = state.moveY,
          .heading = state.heading,
          .stall_counter = state.stallCounter};
}

// Flight movement writes only these fields; the rest are read.
void Store(const FlightMover& mover, AiState& state) {
  state.movementDisabled = mover.movement_disabled;
  state.moveX = mover.move_x;
  state.moveZ = mover.move_z;
  state.moveY = mover.move_y;
  state.stallCounter = mover.stall_counter;
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
  void RewindActors() override { cursor_ = Battle_actors[0]; }
  [[nodiscard]] std::optional<int> NextActor() override {
    if (cursor_ == nullptr) return std::nullopt;
    const int id = cursor_->id;
    cursor_ = cursor_->next;
    return id;
  }
  [[nodiscard]] std::optional<FlightObstacle> Obstacle(
      int actor_id) const override {
    if (actor_id < 0 || actor_id >= kAiFrameActorSlots) return std::nullopt;
    const auto* other = Frame().actors[actor_id];
    if (other == nullptr) return std::nullopt;
    FlightObstacle obstacle{.actor_id = other->actorId,
                            .radius = other->radius,
                            .max_range = other->maxRange,
                            .body_height = other->bodyHeight,
                            .position = std::nullopt};
    if (other->model != nullptr) {
      const auto& position = other->model->opaque1C;
      obstacle.position =
          FlightPoint{.x = position.vx, .y = position.vy, .z = position.vz};
    }
    return obstacle;
  }
  [[nodiscard]] std::optional<FlightRoom> Room() const override {
    const auto bounds = LiveRoomTileBounds();
    if (!bounds) return std::nullopt;
    return FlightRoom{.bounds = *bounds,
                      .ceiling = Battle_Ai_roomAiState->flightCeiling};
  }
  [[nodiscard]] std::optional<std::uint16_t> TileFlags(int x,
                                                       int z) const override {
    const auto& frame = Frame();
    if (frame.tileFlags == nullptr || x < 0 || z < 0 || x >= kAiFrameGridSide ||
        z >= kAiFrameGridSide) {
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
          reinterpret_cast<Battle_Ai_MoveTargetView*>(&state_), tile);
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

int IsLiveFlightDirectionObstructed(Battle_Ai_ActorAiState* state,
                                    int direction, int ahead) {
  auto& live = RequireState(state);
  LiveFlightWorld world(live);
  const auto answer =
      FlightDirectionObstruction(world, Load(live), direction, ahead != 0);
  Require(answer.has_value(),
          "AI flight obstruction refused its direction or actors");
  return *answer;
}

int CanLiveActorFlyInDirection(Battle_Ai_ActorAiState* state, int direction) {
  auto& live = RequireState(state);
  LiveFlightWorld world(live);
  const auto open = CanFlyInDirection(world, Load(live), direction);
  Require(open.has_value(), "AI flight test refused its direction or room");
  return *open ? 1 : 0;
}

int UpdateLiveFlightGroundTransition(Battle_Ai_ActorAiState* state, int angle,
                                     int mode) {
  const auto changed = Update(
      RequireState(state), [&](FlightMovementWorld& world, FlightMover& mover) {
        return UpdateFlightGroundTransition(world, mover, angle, mode != 0);
      });
  Require(changed.has_value(),
          "AI flight transition refused its room, terrain or model");
  return *changed ? 1 : 0;
}

void UpdateLiveFlightMovement(Battle_Ai_ActorAiState* state, int angle) {
  const bool done = Update(RequireState(state),
                           [&](FlightMovementWorld& world, FlightMover& mover) {
                             return UpdateFlightMovement(world, mover, angle);
                           });
  Require(done, "AI flight movement refused its room, actors or model");
}

void UpdateLiveFlightDirectionalMovement(Battle_Ai_ActorAiState* state,
                                         int angle) {
  const bool done = Update(
      RequireState(state), [&](FlightMovementWorld& world, FlightMover& mover) {
        return UpdateFlightDirectionalMovement(world, mover, angle);
      });
  Require(done, "AI flight movement refused its room, actors or model");
}
}  // namespace VS::Battle::Ai
