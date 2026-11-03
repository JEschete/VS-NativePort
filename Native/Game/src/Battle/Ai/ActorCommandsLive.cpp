// Hand-maintained (tools/port/owned.csv).
// Live binding for VS::Battle::Ai::IssueActorCommand (ActorCommands.cpp).
#include "Battle/Ai/ActorCommandsLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Field/TerrainHeight.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Damage.h"
}

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kTileMask = 0xFF;
constexpr int kTileZShift = 16;
constexpr int kDirectionZShift = 4;
constexpr int kComponentMask = 3;
constexpr int kComponentModulus = 4;
constexpr unsigned kTerrainLayerMask = 7;
constexpr int kTerrainLayerShift = 10;
constexpr unsigned kCommandPending = 0x80;
constexpr unsigned kLimbCount = 6;
constexpr unsigned kActionsPerLimb = 4;

void Require(bool valid, const char* message) {
  if (!valid) {
    Core_Debugf("ai", "%s", message);
    std::abort();
  }
}

int DirectionComponent(int value) {
  return value > 1 ? value - kComponentModulus : value;
}

unsigned DirectionForOffset(int dx, int dz) {
  const auto& frame = *Battle_Ai_FrameState();
  for (unsigned index = 0; index < std::size(frame.directions); ++index) {
    const auto packed = frame.directions[index];
    if (DirectionComponent(packed & kComponentMask) == dx &&
        DirectionComponent((packed >> kDirectionZShift) & kComponentMask) ==
            dz) {
      return index;
    }
  }
  Core_Debugf("ai", "AI movement corner has unsupported offset (%d,%d)", dx,
              dz);
  std::abort();
}

class LiveActorCommandRoom final : public ActorCommandRoom {
 public:
  LiveActorCommandRoom(const Battle_Ai_ActorAiState& state,
                       const Battle_Actor& actor)
      : state_(state), actor_(actor) {}

  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }

  [[nodiscard]] bool IsCornerBlocked(std::uint32_t tile, int dx,
                                     int dz) const override {
    const unsigned direction = DirectionForOffset(dx, dz);
    const auto& frame = *Battle_Ai_FrameState();
    if (frame.roomData == nullptr) {
      return IsLiveAdjacentTileStepBlocked(tile, static_cast<int>(direction));
    }
    Require(frame.terrainRegions != nullptr && frame.heightMap != nullptr,
            "AI regional movement requires terrain grids");
    const int x = static_cast<int>(tile & kTileMask);
    const int z = static_cast<int>((tile >> kTileZShift) & kTileMask);
    const auto width = frame.roomBounds & kTileMask;
    Require(width > 0 && width <= kAiFrameGridSide && x < kAiFrameGridSide &&
                z < kAiFrameGridSide,
            "AI regional movement coordinates are out of bounds");
    if (x + dx < 0 || z + dz < 0 || x + dx >= kAiFrameGridSide ||
        z + dz >= kAiFrameGridSide) {
      return true;
    }
    const auto current =
        (frame.heightMap[z][x] >> kTerrainLayerShift) & kTerrainLayerMask;
    const auto next = (frame.heightMap[z + dz][x + dx] >> kTerrainLayerShift) &
                      kTerrainLayerMask;
    return ((*Battle_Ai_TerrainRegionCell(x, z) >> direction) & 1u) != 0 ||
           current < next;
  }

  [[nodiscard]] ActorFacing Facing() const override {
    Require(actor_.model != nullptr, "AI turning requires an actor model");
    return ActorFacing{actor_.model->facing,
                       std::bit_cast<std::int16_t>(actor_.model->lookYaw)};
  }

  [[nodiscard]] AttackSelection SelectedAttack() const override {
    const auto* candidate = state_.currentCandidate;
    Require(candidate != nullptr && candidate->row < kLimbCount &&
                candidate->column < kActionsPerLimb,
            "AI attack requires a valid selected action slot");
    return AttackSelection{
        candidate->row,
        candidate->column,
        candidate->targetId,
        candidate->targetLimb,
        static_cast<int>(candidate->opaqueCBit7),
        state_.actionSlots[candidate->row][candidate->column].kind};
  }

  [[nodiscard]] CommandPosition TargetPosition(
      unsigned target_id) const override {
    Require(target_id < std::size(Actors_actorRecords),
            "AI positional attack target is out of bounds");
    const auto* target = Actors_actorRecords[target_id];
    Require(target != nullptr, "AI positional attack requires a target model");
    return CommandPosition{target->instance.position.vx,
                           target->instance.position.vy,
                           target->instance.position.vz};
  }

  void ReserveTile(int x, int z) override {
    ReserveLiveAiDestinationTile(x, z);
  }

 private:
  const Battle_Ai_ActorAiState& state_;
  const Battle_Actor& actor_;
};

ActorCommandState Snapshot(const Battle_Ai_ActorAiState& state,
                           const Battle_Actor& actor) {
  ActorCommandState snapshot{};
  snapshot.actor_id = actor.id;
  snapshot.tile = state.tile.word;
  snapshot.next_tile = state.nextTile.word;
  snapshot.destination = state.destination.word;
  snapshot.position_x = state.position.vx;
  snapshot.position_z = state.position.vz;
  snapshot.radius = state.radius;
  snapshot.heading = state.heading;
  snapshot.angle = static_cast<std::int16_t>(state.angle);
  snapshot.facing_delay = state.facingDelay;
  snapshot.turn_rate = state.opaque13A;
  // PS1 `char` is unsigned; bit 0 marks a model whose upper body turns alone.
  snapshot.looks_with_upper_body =
      (static_cast<unsigned char>(state.hasTaggedBone3) & 1u) != 0;
  snapshot.move_x = state.moveX;
  snapshot.move_y = state.moveY;
  snapshot.move_z = state.moveZ;
  snapshot.may_run = state.opaque1 != 0;
  snapshot.locomotion_flags = state.locomotionFlags;
  snapshot.walk_step = state.walkStep;
  snapshot.run_step = state.runStep;
  snapshot.movement_flags = state.movementFlags;
  std::memcpy(&snapshot.step_height, state.step_height_bytes,
              sizeof(snapshot.step_height));
  snapshot.flight_height = state.flightHeight;
  std::memcpy(&snapshot.last_slide_tile, state.last_slide_tile_bytes,
              sizeof(snapshot.last_slide_tile));
  snapshot.next_position = CommandPosition{
      state.nextPosition.vx, state.nextPosition.vy, state.nextPosition.vz};
  return snapshot;
}

void StoreBack(const ActorCommandState& snapshot,
               Battle_Ai_ActorAiState& state) {
  std::memcpy(state.last_slide_tile_bytes, &snapshot.last_slide_tile,
              sizeof(snapshot.last_slide_tile));
  state.nextPosition.vx = snapshot.next_position.x;
  state.nextPosition.vy = snapshot.next_position.y;
  state.nextPosition.vz = snapshot.next_position.z;
}
}  // namespace

// decomp: func_800DEEFC (src/BATTLE/BATTLE.PRG/766FC.s:14); the typed
// dispatcher is VS::Battle::Ai::IssueActorCommand.
void IssueLiveActorCommand(Battle_Ai_ActorAiState* state, int action) {
  Require(state != nullptr && action >= 0 && action < kCommandIntentCount,
          "AI actor command intent is out of bounds");
  Require(state->actorId < std::size(Battle_actors),
          "AI actor command ID is out of bounds");
  auto* actor = Battle_actors[state->actorId];
  Require(actor != nullptr &&
              static_cast<void*>(actor) == static_cast<void*>(state->actor),
          "AI actor command identity does not match the actor table");
  Battle_Ai_FrameState()->movementResult = action;
  actor->commandState = kCommandPending;
  actor->commandTarget = state->movementTarget;
  ActorCommandPacket packet{static_cast<CommandOpcode>(actor->commandOpcode),
                            actor->commandArg0, actor->commandArg1.word,
                            actor->commandArg2};
  ActorCommandState snapshot = Snapshot(*state, *actor);
  LiveActorCommandRoom room(*state, *actor);
  IssueActorCommand(room, snapshot, packet, static_cast<CommandIntent>(action));
  StoreBack(snapshot, *state);
  actor->commandOpcode = static_cast<u_char>(packet.opcode);
  actor->commandArg0 = packet.arg0;
  actor->commandArg1.word = packet.arg1;
  actor->commandArg2 = packet.arg2;
}
}  // namespace VS::Battle::Ai
