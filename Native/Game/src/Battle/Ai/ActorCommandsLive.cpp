// Hand-maintained (tools/port/owned.csv).
// Live binding for VS::Battle::Ai::IssueActorCommand (ActorCommands.cpp).
#include "Battle/Ai/ActorCommandsLive.hpp"

#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>

#include "Actors/ActorRecord.hpp"
#include "Actors/Movement.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/AiState.hpp"
#include "Battle/Ai/CurrentTerrain.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/NavigationPrimitivesLive.hpp"
#include "Battle/Damage.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/TerrainHeight.hpp"

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

int DirectionComponent(int value) {
  return value > 1 ? value - kComponentModulus : value;
}

unsigned DirectionForOffset(int dx, int dz) {
  const auto& frame = LiveFrameContext();
  for (unsigned index = 0; index < std::size(frame.directions); ++index) {
    const auto packed = frame.directions[index];
    if (DirectionComponent(packed & kComponentMask) == dx &&
        DirectionComponent((packed >> kDirectionZShift) & kComponentMask) ==
            dz) {
      return index;
    }
  }
  Core::Errorf("ai", "AI movement corner has unsupported offset (%d,%d)", dx,
               dz);
  std::abort();
}

class LiveActorCommandRoom final : public ActorCommandRoom {
 public:
  LiveActorCommandRoom(const VS::Battle::Ai::ActorAiState& state,
                       const VS::Battle::Actor& actor)
      : state_(state), actor_(actor) {}

  [[nodiscard]] Field::TerrainGrid Terrain() const override {
    return CurrentTerrain();
  }

  [[nodiscard]] bool IsCornerBlocked(std::uint32_t tile, int dx,
                                     int dz) const override {
    const unsigned direction = DirectionForOffset(dx, dz);
    const auto& frame = LiveFrameContext();
    if (frame.roomData == nullptr) {
      return IsLiveAdjacentTileStepBlocked(tile, static_cast<int>(direction));
    }
    Require(frame.terrainRegions != nullptr && frame.heightMap != nullptr,
            "AI regional movement requires terrain grids");
    const int x = static_cast<int>(tile & kTileMask);
    const int z = static_cast<int>((tile >> kTileZShift) & kTileMask);
    const auto width = frame.roomBounds & kTileMask;
    Require(width > 0 && width <= kNavigationGridSide &&
                x < kNavigationGridSide && z < kNavigationGridSide,
            "AI regional movement coordinates are out of bounds");
    if (x + dx < 0 || z + dz < 0 || x + dx >= kNavigationGridSide ||
        z + dz >= kNavigationGridSide) {
      return true;
    }
    const auto current =
        (frame.heightMap[z][x] >> kTerrainLayerShift) & kTerrainLayerMask;
    const auto next = (frame.heightMap[z + dz][x + dx] >> kTerrainLayerShift) &
                      kTerrainLayerMask;
    const std::uint8_t* cell = FindTerrainRegionCell(x, z);
    Require(cell != nullptr, "AI regional movement leaves the terrain regions");
    return ((*cell >> direction) & 1u) != 0 || current < next;
  }

  [[nodiscard]] ActorFacing Facing() const override {
    Require(actor_.model != nullptr, "AI turning requires an actor model");
    return ActorFacing{actor_.model->facing,
                       std::bit_cast<std::int16_t>(actor_.model->look_yaw)};
  }

  [[nodiscard]] AttackSelection SelectedAttack() const override {
    const auto* candidate = state_.current_candidate;
    Require(candidate != nullptr && candidate->row < kLimbCount &&
                candidate->column < kActionsPerLimb,
            "AI attack requires a valid selected action slot");
    return AttackSelection{
        candidate->row,
        candidate->column,
        candidate->target_id,
        candidate->target_limb,
        static_cast<int>(candidate->height_offset),
        state_.action_slots[candidate->row][candidate->column].kind};
  }

  [[nodiscard]] CommandPosition TargetPosition(
      unsigned target_id) const override {
    Require(target_id < std::size(VS::Actors::actorRecords),
            "AI positional attack target is out of bounds");
    const auto* target = VS::Actors::actorRecords[target_id];
    Require(target != nullptr, "AI positional attack requires a target model");
    return CommandPosition{target->instance.position.vx,
                           target->instance.position.vy,
                           target->instance.position.vz};
  }

  void ReserveTile(int x, int z) override {
    ReserveLiveAiDestinationTile(x, z);
  }

 private:
  const VS::Battle::Ai::ActorAiState& state_;
  const VS::Battle::Actor& actor_;
};

ActorCommandState Snapshot(const VS::Battle::Ai::ActorAiState& state,
                           const VS::Battle::Actor& actor) {
  ActorCommandState snapshot{};
  snapshot.actor_id = actor.id;
  snapshot.tile = state.tile.word;
  snapshot.next_tile = state.next_tile.word;
  snapshot.destination = state.destination.word;
  snapshot.position_x = state.position.vx;
  snapshot.position_z = state.position.vz;
  snapshot.radius = state.radius;
  snapshot.heading = state.heading;
  snapshot.angle = static_cast<std::int16_t>(state.angle);
  snapshot.facing_delay = state.facing_delay;
  snapshot.turn_rate = state.opaque_13a;
  // PS1 `char` is unsigned; bit 0 marks a model whose upper body turns alone.
  snapshot.looks_with_upper_body =
      (static_cast<unsigned char>(state.has_tagged_bone3) & 1u) != 0;
  snapshot.move_x = state.move_x;
  snapshot.move_y = state.move_y;
  snapshot.move_z = state.move_z;
  snapshot.may_run = state.opaque1 != 0;
  snapshot.locomotion_flags = state.locomotion_flags;
  snapshot.walk_step = state.walk_step;
  snapshot.run_step = state.run_step;
  snapshot.movement_flags = state.movement_flags;
  std::memcpy(&snapshot.step_height, state.step_height_bytes.data(),
              sizeof(snapshot.step_height));
  snapshot.flight_height = state.flight_height;
  std::memcpy(&snapshot.last_slide_tile, state.last_slide_tile_bytes.data(),
              sizeof(snapshot.last_slide_tile));
  snapshot.next_position = CommandPosition{
      state.next_position.vx, state.next_position.vy, state.next_position.vz};
  return snapshot;
}

void StoreBack(const ActorCommandState& snapshot,
               VS::Battle::Ai::ActorAiState& state) {
  std::memcpy(state.last_slide_tile_bytes.data(), &snapshot.last_slide_tile,
              sizeof(snapshot.last_slide_tile));
  state.next_position.vx = snapshot.next_position.x;
  state.next_position.vy = snapshot.next_position.y;
  state.next_position.vz = snapshot.next_position.z;
}
}  // namespace

// decomp: func_800DEEFC (src/BATTLE/BATTLE.PRG/766FC.s:14); the typed
// dispatcher is VS::Battle::Ai::IssueActorCommand.
void IssueLiveActorCommand(VS::Battle::Ai::ActorAiState* state, int action) {
  Require(state != nullptr && action >= 0 && action < kCommandIntentCount,
          "AI actor command intent is out of bounds");
  Require(state->actor_id < std::size(VS::Battle::actors),
          "AI actor command ID is out of bounds");
  auto* actor = VS::Battle::actors[state->actor_id];
  Require(actor != nullptr &&
              static_cast<void*>(actor) == static_cast<void*>(state->actor),
          "AI actor command identity does not match the actor table");
  LiveFrameContext().movementResult = action;
  actor->command_state = kCommandPending;
  actor->command_target = state->movement_target;
  ActorCommandPacket packet{static_cast<CommandOpcode>(actor->command_opcode),
                            actor->command_arg0, actor->command_arg1.word,
                            actor->command_arg2};
  ActorCommandState snapshot = Snapshot(*state, *actor);
  LiveActorCommandRoom room(*state, *actor);
  IssueActorCommand(room, snapshot, packet, static_cast<CommandIntent>(action));
  StoreBack(snapshot, *state);
  actor->command_opcode = static_cast<std::uint8_t>(packet.opcode);
  actor->command_arg0 = packet.arg0;
  actor->command_arg1.word = packet.arg1;
  actor->command_arg2 = packet.arg2;
}
}  // namespace VS::Battle::Ai
