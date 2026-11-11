// Hand-maintained (tools/port/owned.csv).
// Live binding for the radial movement (RadialMovement.cpp).
#include "Battle/Ai/RadialMovementLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/RadialMovement.hpp"
#include "Legacy/Psx.h"
extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Navigation.h"
#include "Core/Diagnostics.h"
}

namespace VS::Battle::Ai {
namespace {
using AiState = Battle_Ai_ActorAiState;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Errorf("ai", "%s", message);
    std::abort();
  }
}

// The model's fields read as zero without a model; the entries that reach
// them require one.
RadialMover Load(const AiState& state) {
  const auto* model = state.model;
  return {.toward_point = state.opaque9,
          .approach_directly = state.opaqueD,
          .tile = state.tile.word,
          .saved_destination = state.savedDestination.word,
          .x = state.position.vx,
          .z = state.position.vz,
          .flies = model != nullptr && model->opaqueABit5 != 0,
          .model_look_yaw =
              model != nullptr ? model->opaque14 : static_cast<std::int16_t>(0),
          .model_facing =
              model != nullptr ? model->opaque26 : static_cast<std::int16_t>(0),
          .move_attempts = state.moveAttempts,
          .move_distance = state.moveDistance,
          .turn_timer = state.opaque13A,
          .turn_period = state.opaque13C,
          .target_x = state.targetX,
          .target_z = state.targetZ,
          .movement_target = state.movementTarget,
          .movement_flags = state.movementFlags,
          .destination = state.destination.word,
          .next_tile = state.nextTile.word,
          .move_x = state.moveX,
          .move_z = state.moveZ,
          .heading = state.heading,
          .angle = state.angle,
          .stall_counter = state.stallCounter};
}

// The model's fields are the model's; they are not written back.
void Store(const RadialMover& mover, AiState& state) {
  state.opaque9 = mover.toward_point;
  state.opaqueD = mover.approach_directly;
  state.tile.word = mover.tile;
  state.savedDestination.word = mover.saved_destination;
  state.position.vx = mover.x;
  state.position.vz = mover.z;
  state.moveAttempts = mover.move_attempts;
  state.moveDistance = mover.move_distance;
  state.opaque13A = mover.turn_timer;
  state.opaque13C = mover.turn_period;
  state.targetX = mover.target_x;
  state.targetZ = mover.target_z;
  state.movementTarget = mover.movement_target;
  state.movementFlags = mover.movement_flags;
  state.destination.word = mover.destination;
  state.nextTile.word = mover.next_tile;
  state.moveX = mover.move_x;
  state.moveZ = mover.move_z;
  state.heading = mover.heading;
  state.angle = mover.angle;
  state.stallCounter = mover.stall_counter;
}

// The room's AI frame, the battle actors and the actor's movement code. Each
// call taking the mover stores it into the AI state first and reloads it
// after.
class LiveRadialWorld final : public RadialMovementWorld {
 public:
  explicit LiveRadialWorld(AiState& state) : state_(state) {}

  [[nodiscard]] NavigationDirectionCodes DirectionCodes() const override {
    return LiveDirectionCodes();
  }
  [[nodiscard]] std::optional<RoomTileBounds> RoomBounds() const override {
    return LiveRoomTileBounds();
  }
  [[nodiscard]] std::uint32_t GridBounds() const override {
    return Battle_Ai_FrameState()->roomBounds;
  }
  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      RadialMover& mover, std::uint32_t tile) override {
    int occupancy = 0;
    Run(mover, [&] {
      occupancy = LiveDestinationOccupancy(
          reinterpret_cast<Battle_Ai_MoveTargetView*>(&state_), tile);
    });
    return occupancy;
  }
  [[nodiscard]] std::optional<RadialPoint> ActorPosition(
      int actor_id) const override {
    if (actor_id < 0 ||
        static_cast<std::size_t>(actor_id) >= std::size(Actors_actorRecords)) {
      return std::nullopt;
    }
    const auto* record = Actors_actorRecords[actor_id];
    if (record == nullptr) return std::nullopt;
    return RadialPoint{record->instance.position.vx,
                       record->instance.position.vz};
  }
  void IssueCommand(RadialMover& mover, CommandIntent intent) override {
    Run(mover,
        [&] { IssueLiveActorCommand(&state_, static_cast<int>(intent)); });
  }
  void MoveTowardDestinationTile(RadialMover& mover, int angle) override {
    Run(mover, [&] { MoveLiveActorTowardDestinationTile(&state_, angle); });
  }
  void ReleaseReservations(RadialMover& mover) override {
    Run(mover, [&] { ReleaseLiveActorAiReservations(&state_); });
  }
  void BeginTerrainMovement(RadialMover& mover) override {
    Run(mover, [&] { BeginLiveAiTerrainMovement(&state_); });
  }

 private:
  template <typename Call>
  void Run(RadialMover& mover, Call&& call) {
    Store(mover, state_);
    call();
    mover = Load(state_);
  }

  AiState& state_;
};

// Runs a typed routine over the state's mover, stores it back and aborts when
// the routine refused.
template <typename Routine>
void Update(AiState& state, const char* refusal, Routine routine) {
  LiveRadialWorld world(state);
  auto mover = Load(state);
  const bool done = routine(world, mover);
  Store(mover, state);
  Require(done, refusal);
}

AiState& StateWithModel(AiState* state) {
  Require(state != nullptr && state->model != nullptr,
          "AI radial movement requires an AI state and a model");
  return *state;
}
}  // namespace

void ProjectLiveClearRadialDestination(Battle_Ai_ActorAiState* state,
                                       Battle_Ai_TilePosition source, int angle,
                                       unsigned int distance) {
  Require(state != nullptr, "AI radial projection requires an AI state");
  Update(*state, "AI radial projection refused its room or actors",
         [&](RadialMovementWorld& world, RadialMover& mover) {
           return ProjectClearRadialDestination(world, mover, source.word,
                                                angle, distance);
         });
}

void MoveLiveToRadialDestination(Battle_Ai_ActorAiState* state, int unused) {
  Update(StateWithModel(state), "AI radial movement refused its room or actors",
         [&](RadialMovementWorld& world, RadialMover& mover) {
           return MoveToRadialDestination(world, mover, unused);
         });
}

void MoveLiveAwayFromReference(Battle_Ai_ActorAiState* state, int angle) {
  Update(StateWithModel(state),
         "AI retreat movement refused its room or actors",
         [&](RadialMovementWorld& world, RadialMover& mover) {
           return MoveAwayFromReference(world, mover, angle);
         });
}

void RecoverLiveMovement(Battle_Ai_ActorAiState* state) {
  Update(StateWithModel(state),
         "AI movement recovery refused its target or terrain",
         [](RadialMovementWorld& world, RadialMover& mover) {
           return RecoverMovement(world, mover);
         });
}
}  // namespace VS::Battle::Ai
