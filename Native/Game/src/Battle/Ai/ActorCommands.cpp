// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:1247-1503,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/ActorCommands.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <optional>

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Movement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/NavigationPrimitives.h"
#include "Battle/Damage.h"
#include "Field/TerrainHeight.h"
}

namespace {
enum class Intent {
  kStand,
  kStop,
  kHold,
  kSetHeading,
  kTurn,
  kMoveDirection,
  kMoveDestination,
  kMoveHeading,
  kMoveVector,
  kLunge,
  kDash,
  kMoveTile,
  kHover,
  kWalkToTile,
  kStepToTile,
  kAttack,
  kTimedTurn,
  kCancel,
  kCount
};

enum class Opcode : u_char {
  kStop = 1,
  kStand = 2,
  kTurn = 3,
  kFace = 4,
  kTimedTurn = 6,
  kLook = 7,
  kCancel = 8,
  kWalk = 9,
  kRun = 10,
  kStep = 11,
  kSlide = 13,
  kDrop = 14,
  kLunge = 16,
  kDash = 17,
  kJump = 18,
  kMoveVector = 19,
  kHold = 20,
  kMoveTile = 21,
  kHover = 22,
  kWalkToTile = 23,
  kAttack = 24
};

constexpr unsigned kAngleMask = 0xFFF;
constexpr int kHalfTurn = 0x800;
constexpr int kFullTurn = 0x1000;
constexpr int kForwardAngle = 0xC00;
constexpr int kUpperBodyTurnLimit = 0x320;
constexpr int kTileShift = 7;
constexpr int kTileSize = 128;
constexpr int kTileCenter = 64;
constexpr unsigned kTileMask = 0xFF;
constexpr unsigned kTileCoordinatesMask = 0xFF00FF;
constexpr int kTileZShift = 16;
constexpr int kDirectionZShift = 4;
constexpr int kComponentMask = 3;
constexpr int kComponentModulus = 4;
constexpr int kFixedPointShift = 12;
constexpr int kFixedPointOne = 4096;
constexpr unsigned kWalkFlags = 0x42;
constexpr unsigned kRunFlags = 0x84;
constexpr unsigned kHighRunFlags = 0x8400;
constexpr unsigned kMoveWalkHighFlags = 0x420000;
constexpr unsigned kStepWalkHighFlags = 0x4200;
constexpr unsigned kWalkInsteadOfStep = 8;
constexpr unsigned kJumpFlags = 0x30;
constexpr unsigned kDropFlags = 0xC0;
constexpr unsigned kRouteLayerMask = 0xFF00;
constexpr int kHalfWordBits = 16;
constexpr int kPackedByteShift = 8;
constexpr int kPackedSubtileShift = 17;
constexpr int kRunSpeedShift = 24;
constexpr int kDirectionAngle = 512;
constexpr unsigned kHalfWordMask = 0xFFFF;
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

int16_t Half(int value) {
  return std::bit_cast<int16_t>(static_cast<std::uint16_t>(value));
}

int Packed(int low, int high) {
  return std::bit_cast<int>(
      (static_cast<std::uint32_t>(low) & kHalfWordMask) |
      (static_cast<std::uint32_t>(high) << kHalfWordBits));
}

void SetOpcode(Battle_Actor& actor, Opcode opcode) {
  actor.commandOpcode = static_cast<u_char>(opcode);
}

struct Gait {
  Opcode opcode;
  unsigned speed;
};

std::optional<Gait> SelectGait(const Battle_Ai_ActorAiState& state,
                               unsigned walk_high_flags) {
  const auto flags = state.locomotionFlags;
  if (state.opaque1 != 0) {
    if ((flags & kRunFlags) != 0) return Gait{Opcode::kRun, state.walkStep};
    if ((flags & kHighRunFlags) != 0) return Gait{Opcode::kRun, state.runStep};
  }
  if ((flags & kWalkFlags) != 0) return Gait{Opcode::kWalk, state.walkStep};
  const auto packed = flags |
                      (static_cast<unsigned>(state.walkStep) << kHalfWordBits) |
                      (static_cast<unsigned>(state.runStep) << kRunSpeedShift);
  if ((packed & walk_high_flags) != 0) {
    return Gait{Opcode::kWalk, state.runStep};
  }
  return std::nullopt;
}

int DirectionComponent(int value) {
  return value > 1 ? value - kComponentModulus : value;
}

std::optional<unsigned> DirectionForOffset(int dx, int dz) {
  if (dx == 0 && dz == 0) return std::nullopt;
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

bool CornerBlocked(const Battle_Ai_ActorAiState& state, int dx, int dz) {
  const auto direction = DirectionForOffset(dx, dz);
  if (!direction) return false;
  const auto& frame = *Battle_Ai_FrameState();
  if (frame.roomData == nullptr) {
    return Battle_Ai_IsAdjacentTileStepBlocked(state.tile.word,
                                               static_cast<int>(*direction));
  }
  Require(frame.terrainRegions != nullptr && frame.heightMap != nullptr,
          "AI regional movement requires terrain grids");
  const int x = static_cast<int>(state.tile.word & kTileMask);
  const int z = static_cast<int>((state.tile.word >> kTileZShift) & kTileMask);
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
  return ((*Battle_Ai_TerrainRegionCell(x, z) >> *direction) & 1u) != 0 ||
         current < next;
}

void SlideAtTileEdge(Battle_Ai_ActorAiState& state, Battle_Actor& actor,
                     int new_x, int new_z) {
  const int tile_x = static_cast<int>(state.tile.word & kTileMask);
  const int tile_z =
      static_cast<int>((state.tile.word >> kTileZShift) & kTileMask);
  const int centre_x = tile_x * kTileSize + kTileCenter;
  const int centre_z = tile_z * kTileSize + kTileCenter;
  const int x = static_cast<std::uint16_t>(state.position.vx);
  const int z = static_cast<std::uint16_t>(state.position.vz);
  const int slide_x = (static_cast<std::uint16_t>(new_x) < centre_x
                           ? centre_x - kTileCenter + state.radius
                           : centre_x + kTileCenter - 1 - state.radius) -
                      x;
  const int slide_z = (static_cast<std::uint16_t>(new_z) < centre_z
                           ? centre_z - kTileCenter + state.radius
                           : centre_z + kTileCenter - 1 - state.radius) -
                      z;
  const auto tile = state.tile.word & kTileCoordinatesMask;
  std::uint32_t previous = 0;
  std::memcpy(&previous, state.last_slide_tile_bytes, sizeof(previous));
  if ((slide_x == 0 && slide_z == 0) ||
      (previous & kTileCoordinatesMask) == tile) {
    return;
  }
  std::memcpy(state.last_slide_tile_bytes, &tile, sizeof(tile));
  SetOpcode(actor, Opcode::kSlide);
  actor.commandArg0 = slide_x * kFixedPointOne;
  actor.commandArg1.word = slide_z * kFixedPointOne;
}

void Move(Battle_Ai_ActorAiState& state, Battle_Actor& actor, int heading) {
  const auto angle = static_cast<unsigned>(heading) & kAngleMask;
  actor.commandArg0 =
      Packed(static_cast<int>(angle), actor.commandArg0 >> kHalfWordBits);
  const auto gait = SelectGait(state, kMoveWalkHighFlags);
  if (!gait) {
    SetOpcode(actor, Opcode::kStand);
    return;
  }
  SetOpcode(actor, gait->opcode);
  const auto turned = (kForwardAngle - static_cast<int>(angle)) & kAngleMask;
  const int new_x = static_cast<std::uint16_t>(state.position.vx) +
                    ((Core_Gte_Cos(static_cast<int>(turned)) *
                      static_cast<int>(gait->speed)) >>
                     kFixedPointShift);
  const int new_z = static_cast<std::uint16_t>(state.position.vz) +
                    ((Core_Gte_Sin(static_cast<int>(turned)) *
                      static_cast<int>(gait->speed)) >>
                     kFixedPointShift);
  const int tile_x = static_cast<int>(state.tile.word & kTileMask);
  const int tile_z =
      static_cast<int>((state.tile.word >> kTileZShift) & kTileMask);
  const std::array<int, 2> xs{((new_x - state.radius) >> kTileShift) - tile_x,
                              ((new_x + state.radius) >> kTileShift) - tile_x};
  const std::array<int, 2> zs{((new_z - state.radius) >> kTileShift) - tile_z,
                              ((new_z + state.radius) >> kTileShift) - tile_z};
  for (const auto dx : xs) {
    for (const auto dz : zs) {
      if (CornerBlocked(state, dx, dz)) {
        SlideAtTileEdge(state, actor, new_x, new_z);
        return;
      }
    }
  }
}

void Turn(const Battle_Ai_ActorAiState& state, Battle_Actor& actor) {
  Require(actor.model != nullptr, "AI turning requires an actor model");
  const auto target = static_cast<unsigned>(state.angle) & kAngleMask;
  const int heading = actor.model->facing;
  const int turn = std::bit_cast<int16_t>(actor.model->lookYaw);
  const auto offset =
      (target - static_cast<unsigned>(turn + heading)) & kAngleMask;
  const bool positive = offset < kHalfTurn;
  const bool limit =
      positive ? turn < kUpperBodyTurnLimit : turn > -kUpperBodyTurnLimit;
  auto opcode =
      (static_cast<unsigned char>(state.hasTaggedBone3) & 1u) != 0 && limit
          ? Opcode::kLook
          : Opcode::kTurn;
  const auto rate = state.opaque13A;
  int value = 0;
  if (kFullTurn - offset < rate || offset < rate || offset == 0) {
    opcode = Opcode::kFace;
    value = static_cast<int>(target);
  } else {
    value = (positive ? rate : -static_cast<int>(rate)) +
            (opcode == Opcode::kTurn ? heading : turn);
  }
  SetOpcode(actor, opcode);
  actor.commandArg0 = Packed(value, rate);
}

int TileCommandZ(unsigned tile) {
  return static_cast<int>((tile << kPackedByteShift) >> kPackedSubtileShift) +
         kTileCenter;
}

void StageTilePosition(Battle_Ai_ActorAiState& state, int x, int z) {
  state.nextPosition.vx = Half(x);
  state.nextPosition.vy = Half(Field_GroundHeightAtWorldPosition(x, z));
  state.nextPosition.vz = Half(z);
}

void ReserveTile(unsigned tile) {
  Battle_Ai_ReserveAiDestinationTile(
      static_cast<int>(tile & kTileMask),
      static_cast<int>((tile >> kTileZShift) & kTileMask));
}

void WalkToTile(Battle_Ai_ActorAiState& state, Battle_Actor& actor) {
  const auto tile = state.nextTile.word;
  const int x = static_cast<int>(tile & kTileMask) * kTileSize + kTileCenter;
  const int z = TileCommandZ(tile);
  SetOpcode(actor, Opcode::kWalkToTile);
  actor.commandArg0 = std::bit_cast<int>(tile);
  StageTilePosition(state, x, z);
  actor.commandArg1.word = static_cast<int>(
      Core_Gte_Atan2(static_cast<std::uint16_t>(state.position.vx) - x,
                     static_cast<std::uint16_t>(state.position.vz) - z));
  ReserveTile(tile);
}

void StepToTile(Battle_Ai_ActorAiState& state, Battle_Actor& actor) {
  const auto tile = state.nextTile.word;
  const int rise = std::abs(Field_GroundHeightAtPackedTile(tile) -
                            Field_GroundHeightAtPackedTile(state.tile.word));
  std::uint16_t threshold = 0;
  std::memcpy(&threshold, state.step_height_bytes, sizeof(threshold));
  Core_Debugf("ai.move",
              "step actor=%d from=%08X to=%08X rise=%d threshold=%u flags=%02X",
              actor.id, state.tile.word, tile, rise,
              static_cast<unsigned>(threshold),
              static_cast<unsigned>(state.movementFlags));
  auto opcode = Opcode::kStep;
  if (rise >= threshold) {
    if ((state.movementFlags & kWalkInsteadOfStep) != 0) {
      WalkToTile(state, actor);
      return;
    }
    if ((state.movementFlags & kJumpFlags) != 0) {
      opcode = Opcode::kJump;
    } else if ((state.movementFlags & kDropFlags) != 0) {
      opcode = Opcode::kDrop;
    }
  }
  SetOpcode(actor, opcode);
  actor.commandArg0 = std::bit_cast<int>(tile);
  StageTilePosition(
      state, static_cast<int>(tile & kTileMask) * kTileSize + kTileCenter,
      static_cast<int>((tile >> kTileZShift) & kTileMask) * kTileSize +
          kTileCenter);
  const auto gait = SelectGait(state, kStepWalkHighFlags);
  if (!gait) {
    SetOpcode(actor, Opcode::kStand);
    return;
  }
  actor.commandArg1.word = static_cast<int>(gait->speed);
  if (actor.id != 0 && (tile & kRouteLayerMask) == 0) ReserveTile(tile);
}

void Attack(const Battle_Ai_ActorAiState& state, Battle_Actor& actor) {
  const auto* candidate = state.currentCandidate;
  Require(candidate != nullptr && candidate->row < kLimbCount &&
              candidate->column < kActionsPerLimb,
          "AI attack requires a valid selected action slot");
  actor.commandArg0 = Packed(candidate->row, candidate->column);
  const auto kind = state.actionSlots[candidate->row][candidate->column].kind;
  if (kind == 1 || kind == 2) {
    actor.commandArg1.word = Packed(candidate->targetId, candidate->targetLimb);
  } else {
    const auto* target = Actors_actorRecords[candidate->targetId];
    Require(target != nullptr, "AI positional attack requires a target model");
    actor.commandArg1.word =
        Packed(target->instance.position.vx,
               target->instance.position.vy -
                   static_cast<int>(candidate->opaqueCBit7));
    actor.commandArg2 = Packed(target->instance.position.vz,
                               actor.commandArg2 >> kHalfWordBits);
  }
  SetOpcode(actor, Opcode::kAttack);
}

bool StoreDirectPacket(Intent intent, const Battle_Ai_ActorAiState& state,
                       Battle_Actor& actor) {
  switch (intent) {
    case Intent::kStand:
      SetOpcode(actor, Opcode::kStand);
      break;
    case Intent::kStop:
      SetOpcode(actor, Opcode::kStop);
      break;
    case Intent::kHold:
      SetOpcode(actor, Opcode::kHold);
      break;
    case Intent::kSetHeading:
      SetOpcode(actor, Opcode::kTurn);
      actor.commandArg0 = Packed(
          static_cast<int>(static_cast<unsigned>(state.heading) & kAngleMask),
          state.facingDelay);
      break;
    case Intent::kMoveVector:
      SetOpcode(actor, Opcode::kMoveVector);
      actor.commandArg0 = state.moveX;
      actor.commandArg1.word = state.moveZ;
      actor.commandArg2 = state.moveY;
      break;
    case Intent::kLunge:
    case Intent::kDash:
      SetOpcode(actor,
                intent == Intent::kLunge ? Opcode::kLunge : Opcode::kDash);
      actor.commandArg0 = Packed(state.moveX, state.moveY);
      actor.commandArg1.word =
          Packed(state.moveZ, actor.commandArg1.word >> kHalfWordBits);
      break;
    case Intent::kTimedTurn:
      SetOpcode(actor, Opcode::kTimedTurn);
      actor.commandArg0 = Packed(state.heading, state.opaque13A);
      break;
    case Intent::kCancel:
      SetOpcode(actor, Opcode::kCancel);
      break;
    default:
      return false;
  }
  return true;
}
}  // namespace

// decomp: func_800DEEFC / jtbl_800F16F8 (src/BATTLE/BATTLE.PRG/766FC.s:14).
void Battle_Ai_IssueActorCommand(Battle_Ai_ActorAiState* state, int action) {
  Require(state != nullptr && action >= 0 &&
              action < static_cast<int>(Intent::kCount),
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
  const auto intent = static_cast<Intent>(action);
  if (StoreDirectPacket(intent, *state, *actor)) return;
  switch (intent) {
    case Intent::kTurn:
      Turn(*state, *actor);
      break;
    case Intent::kMoveDirection:
      Move(*state, *actor, state->heading * kDirectionAngle);
      break;
    case Intent::kMoveDestination: {
      const auto source = state->tile.word;
      const auto target = state->destination.word;
      const int dx = static_cast<int>(source & kTileMask) -
                     static_cast<int>(target & kTileMask);
      const int dz =
          std::bit_cast<int>(((source >> kPackedByteShift) << kRunSpeedShift) -
                             ((target >> kPackedByteShift) << kRunSpeedShift));
      Move(*state, *actor, static_cast<int>(Core_Gte_Atan2(dx, dz)));
      break;
    }
    case Intent::kMoveHeading:
      Move(*state, *actor, state->heading);
      break;
    case Intent::kMoveTile:
      SetOpcode(*actor, Opcode::kMoveTile);
      actor->commandArg0 = std::bit_cast<int>(state->nextTile.word);
      ReserveTile(state->nextTile.word);
      break;
    case Intent::kHover: {
      SetOpcode(*actor, Opcode::kHover);
      const int x =
          static_cast<int>(state->nextTile.word & kTileMask) * kTileSize;
      const int z = TileCommandZ(state->nextTile.word);
      actor->commandArg1.word =
          Packed(z, actor->commandArg1.word >> kHalfWordBits);
      actor->commandArg0 =
          Packed(x + kTileCenter,
                 Field_GroundHeightAtWorldPosition(x, z) + state->flightHeight);
      break;
    }
    case Intent::kWalkToTile:
      WalkToTile(*state, *actor);
      break;
    case Intent::kStepToTile:
      StepToTile(*state, *actor);
      break;
    case Intent::kAttack:
      Attack(*state, *actor);
      break;
    default:
      Require(false, "AI actor command intent has no dispatch handler");
      break;
  }
}
