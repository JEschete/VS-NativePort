// Hand-maintained (tools/port/owned.csv).
// Live binding for the radial movement (RadialMovement.cpp).
#include "Battle/Ai/RadialMovementLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <optional>

#include "Actors/ActorRecord.hpp"
#include "Battle/Ai/ActorCleanupLive.hpp"
#include "Battle/Ai/ActorCommandsLive.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/GroundNavigationLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/RadialMovement.hpp"
#include "Core/Diagnostics.hpp"
#include "Legacy/Psx.h"

namespace VS::Battle::Ai {
namespace {
using AiState = VS::Battle::Ai::ActorAiState;

// The model's fields read as zero without a model; the entries that reach
// them require one.
RadialMover Load(const AiState& state) {
  const auto* model = state.model;
  const auto* model_record =
      model != nullptr ? reinterpret_cast<const VS::Actors::ActorRecord*>(model)
                       : nullptr;
  return {.toward_point = state.opaque9,
          .approach_directly = state.opaque_d,
          .tile = state.tile.word,
          .saved_destination = state.saved_destination.word,
          .x = state.position.vx,
          .z = state.position.vz,
          .flies = model != nullptr && model->opaque_a_bit5 != 0,
          .model_look_yaw =
              model_record != nullptr
                  ? std::bit_cast<std::int16_t>(model_record->instance.look_yaw)
                  : static_cast<std::int16_t>(0),
          .model_facing = model_record != nullptr
                              ? model_record->instance.facing
                              : static_cast<std::int16_t>(0),
          .move_attempts = state.move_attempts,
          .move_distance = state.move_distance,
          .turn_timer = state.opaque_13a,
          .turn_period = state.opaque_13c,
          .target_x = state.target_x,
          .target_z = state.target_z,
          .movement_target = state.movement_target,
          .movement_flags = state.movement_flags,
          .destination = state.destination.word,
          .next_tile = state.next_tile.word,
          .move_x = state.move_x,
          .move_z = state.move_z,
          .heading = state.heading,
          .angle = state.angle,
          .stall_counter = state.stall_counter};
}

// The model's fields are the model's; they are not written back.
void Store(const RadialMover& mover, AiState& state) {
  state.opaque9 = mover.toward_point;
  state.opaque_d = mover.approach_directly;
  state.tile.word = mover.tile;
  state.saved_destination.word = mover.saved_destination;
  state.position.vx = mover.x;
  state.position.vz = mover.z;
  state.move_attempts = mover.move_attempts;
  state.move_distance = mover.move_distance;
  state.opaque_13a = mover.turn_timer;
  state.opaque_13c = mover.turn_period;
  state.target_x = mover.target_x;
  state.target_z = mover.target_z;
  state.movement_target = mover.movement_target;
  state.movement_flags = mover.movement_flags;
  state.destination.word = mover.destination;
  state.next_tile.word = mover.next_tile;
  state.move_x = mover.move_x;
  state.move_z = mover.move_z;
  state.heading = mover.heading;
  state.angle = mover.angle;
  state.stall_counter = mover.stall_counter;
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
    return LiveFrameContext().roomBounds;
  }
  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }
  [[nodiscard]] std::optional<int> DestinationOccupancy(
      RadialMover& mover, std::uint32_t tile) override {
    int occupancy = 0;
    Run(mover, [&] {
      occupancy = LiveDestinationOccupancy(
          reinterpret_cast<VS::Battle::Ai::MoveTargetView*>(&state_), tile);
    });
    return occupancy;
  }
  [[nodiscard]] std::optional<RadialPoint> ActorPosition(
      int actor_id) const override {
    if (actor_id < 0 || static_cast<std::size_t>(actor_id) >=
                            std::size(VS::Actors::actorRecords)) {
      return std::nullopt;
    }
    const auto* record = VS::Actors::actorRecords[actor_id];
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

void ProjectLiveClearRadialDestination(VS::Battle::Ai::ActorAiState* state,
                                       VS::Battle::Ai::TilePosition source,
                                       int angle, unsigned int distance) {
  Require(state != nullptr, "AI radial projection requires an AI state");
  Update(*state, "AI radial projection refused its room or actors",
         [&](RadialMovementWorld& world, RadialMover& mover) {
           return ProjectClearRadialDestination(world, mover, source.word,
                                                angle, distance);
         });
}

void MoveLiveToRadialDestination(VS::Battle::Ai::ActorAiState* state,
                                 int unused) {
  Update(StateWithModel(state), "AI radial movement refused its room or actors",
         [&](RadialMovementWorld& world, RadialMover& mover) {
           return MoveToRadialDestination(world, mover, unused);
         });
}

void MoveLiveAwayFromReference(VS::Battle::Ai::ActorAiState* state, int angle) {
  Update(StateWithModel(state),
         "AI retreat movement refused its room or actors",
         [&](RadialMovementWorld& world, RadialMover& mover) {
           return MoveAwayFromReference(world, mover, angle);
         });
}

void RecoverLiveMovement(VS::Battle::Ai::ActorAiState* state) {
  Update(StateWithModel(state),
         "AI movement recovery refused its target or terrain",
         [](RadialMovementWorld& world, RadialMover& mover) {
           return RecoverMovement(world, mover);
         });
}
}  // namespace VS::Battle::Ai
