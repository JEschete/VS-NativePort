// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:1247-1503,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Battle/Ai/ActorCommands.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/TerrainHeight.hpp"

namespace VS::Battle::Ai {
namespace {
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

std::int16_t Half(int value) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::int32_t Packed(int low, int high) {
  return std::bit_cast<std::int32_t>(
      (static_cast<std::uint32_t>(low) & kHalfWordMask) |
      (static_cast<std::uint32_t>(high) << kHalfWordBits));
}

int TileX(std::uint32_t tile) { return static_cast<int>(tile & kTileMask); }

int TileZ(std::uint32_t tile) {
  return static_cast<int>((tile >> kTileZShift) & kTileMask);
}

// The original moves from the unsigned halfword of the stored position.
int Unsigned(std::int16_t coordinate) {
  return std::bit_cast<std::uint16_t>(coordinate);
}

struct Gait {
  CommandOpcode opcode;
  unsigned speed;
};

std::optional<Gait> SelectGait(const ActorCommandState& state,
                               unsigned walk_high_flags) {
  const unsigned flags = state.locomotion_flags;
  if (state.may_run) {
    if ((flags & kRunFlags) != 0) {
      return Gait{CommandOpcode::kRun, state.walk_step};
    }
    if ((flags & kHighRunFlags) != 0) {
      return Gait{CommandOpcode::kRun, state.run_step};
    }
  }
  if ((flags & kWalkFlags) != 0) {
    return Gait{CommandOpcode::kWalk, state.walk_step};
  }
  const unsigned packed =
      flags | (static_cast<unsigned>(state.walk_step) << kHalfWordBits) |
      (static_cast<unsigned>(state.run_step) << kRunSpeedShift);
  if ((packed & walk_high_flags) != 0) {
    return Gait{CommandOpcode::kWalk, state.run_step};
  }
  return std::nullopt;
}

bool CornerBlocked(const ActorCommandRoom& room, const ActorCommandState& state,
                   int dx, int dz) {
  if (dx == 0 && dz == 0) return false;
  return room.IsCornerBlocked(state.tile, dx, dz);
}

void SlideAtTileEdge(ActorCommandState& state, ActorCommandPacket& packet,
                     int new_x, int new_z) {
  const int centre_x = TileX(state.tile) * kTileSize + kTileCenter;
  const int centre_z = TileZ(state.tile) * kTileSize + kTileCenter;
  const int x = Unsigned(state.position_x);
  const int z = Unsigned(state.position_z);
  const int radius = state.radius;
  const int slide_x = (static_cast<std::uint16_t>(new_x) < centre_x
                           ? centre_x - kTileCenter + radius
                           : centre_x + kTileCenter - 1 - radius) -
                      x;
  const int slide_z = (static_cast<std::uint16_t>(new_z) < centre_z
                           ? centre_z - kTileCenter + radius
                           : centre_z + kTileCenter - 1 - radius) -
                      z;
  const std::uint32_t tile = state.tile & kTileCoordinatesMask;
  if ((slide_x == 0 && slide_z == 0) ||
      (state.last_slide_tile & kTileCoordinatesMask) == tile) {
    return;
  }
  state.last_slide_tile = tile;
  packet.opcode = CommandOpcode::kSlide;
  packet.arg0 = slide_x * kFixedPointOne;
  packet.arg1 = slide_z * kFixedPointOne;
}

void Move(const ActorCommandRoom& room, ActorCommandState& state,
          ActorCommandPacket& packet, int heading) {
  const unsigned angle = static_cast<unsigned>(heading) & kAngleMask;
  packet.arg0 = Packed(static_cast<int>(angle), packet.arg0 >> kHalfWordBits);
  const auto gait = SelectGait(state, kMoveWalkHighFlags);
  if (!gait) {
    packet.opcode = CommandOpcode::kStand;
    return;
  }
  packet.opcode = gait->opcode;
  const auto turned =
      static_cast<std::int32_t>((kForwardAngle - angle) & kAngleMask);
  const int speed = static_cast<int>(gait->speed);
  const int new_x = Unsigned(state.position_x) +
                    ((Core::Math::Cosine(turned) * speed) >> kFixedPointShift);
  const int new_z = Unsigned(state.position_z) +
                    ((Core::Math::Sine(turned) * speed) >> kFixedPointShift);
  const int tile_x = TileX(state.tile);
  const int tile_z = TileZ(state.tile);
  const int radius = state.radius;
  const std::array<int, 2> xs{((new_x - radius) >> kTileShift) - tile_x,
                              ((new_x + radius) >> kTileShift) - tile_x};
  const std::array<int, 2> zs{((new_z - radius) >> kTileShift) - tile_z,
                              ((new_z + radius) >> kTileShift) - tile_z};
  for (const int dx : xs) {
    for (const int dz : zs) {
      if (CornerBlocked(room, state, dx, dz)) {
        SlideAtTileEdge(state, packet, new_x, new_z);
        return;
      }
    }
  }
}

void Turn(const ActorCommandRoom& room, const ActorCommandState& state,
          ActorCommandPacket& packet) {
  const ActorFacing facing = room.Facing();
  const unsigned target = static_cast<unsigned>(state.angle) & kAngleMask;
  const int heading = facing.facing;
  const int turn = facing.look_yaw;
  const unsigned offset =
      (target - static_cast<unsigned>(turn + heading)) & kAngleMask;
  const bool positive = offset < kHalfTurn;
  const bool limit =
      positive ? turn < kUpperBodyTurnLimit : turn > -kUpperBodyTurnLimit;
  auto opcode = state.looks_with_upper_body && limit ? CommandOpcode::kLook
                                                     : CommandOpcode::kTurn;
  const unsigned rate = state.turn_rate;
  int value = 0;
  if (kFullTurn - offset < rate || offset < rate || offset == 0) {
    opcode = CommandOpcode::kFace;
    value = static_cast<int>(target);
  } else {
    value = (positive ? static_cast<int>(rate) : -static_cast<int>(rate)) +
            (opcode == CommandOpcode::kTurn ? heading : turn);
  }
  packet.opcode = opcode;
  packet.arg0 = Packed(value, static_cast<int>(rate));
}

int TileCommandZ(std::uint32_t tile) {
  return static_cast<int>((tile << kPackedByteShift) >> kPackedSubtileShift) +
         kTileCenter;
}

void StageTilePosition(const ActorCommandRoom& room, ActorCommandState& state,
                       int x, int z) {
  state.next_position = CommandPosition{
      Half(x), Half(Field::GroundHeightAtWorldPosition(room.Terrain(), x, z)),
      Half(z)};
}

void ReserveTile(ActorCommandRoom& room, std::uint32_t tile) {
  room.ReserveTile(TileX(tile), TileZ(tile));
}

void WalkToTile(ActorCommandRoom& room, ActorCommandState& state,
                ActorCommandPacket& packet) {
  const std::uint32_t tile = state.next_tile;
  const int x = TileX(tile) * kTileSize + kTileCenter;
  const int z = TileCommandZ(tile);
  packet.opcode = CommandOpcode::kWalkToTile;
  packet.arg0 = std::bit_cast<std::int32_t>(tile);
  StageTilePosition(room, state, x, z);
  packet.arg1 = Core::Math::Arctangent2(Unsigned(state.position_x) - x,
                                        Unsigned(state.position_z) - z);
  ReserveTile(room, tile);
}

void StepToTile(ActorCommandRoom& room, ActorCommandState& state,
                ActorCommandPacket& packet) {
  const std::uint32_t tile = state.next_tile;
  const Field::TerrainGrid terrain = room.Terrain();
  const int rise =
      std::abs(Field::GroundHeightAtPackedTile(
                   terrain, Core::Math::UnpackTileCoordinate(tile)) -
               Field::GroundHeightAtPackedTile(
                   terrain, Core::Math::UnpackTileCoordinate(state.tile)));
  const unsigned threshold = state.step_height;
  Core_Debugf("ai.move",
              "step actor=%d from=%08X to=%08X rise=%d threshold=%u flags=%02X",
              state.actor_id, state.tile, tile, rise, threshold,
              static_cast<unsigned>(state.movement_flags));
  auto opcode = CommandOpcode::kStep;
  if (rise >= static_cast<int>(threshold)) {
    if ((state.movement_flags & kWalkInsteadOfStep) != 0) {
      WalkToTile(room, state, packet);
      return;
    }
    if ((state.movement_flags & kJumpFlags) != 0) {
      opcode = CommandOpcode::kJump;
    } else if ((state.movement_flags & kDropFlags) != 0) {
      opcode = CommandOpcode::kDrop;
    }
  }
  packet.opcode = opcode;
  packet.arg0 = std::bit_cast<std::int32_t>(tile);
  StageTilePosition(room, state, TileX(tile) * kTileSize + kTileCenter,
                    TileZ(tile) * kTileSize + kTileCenter);
  const auto gait = SelectGait(state, kStepWalkHighFlags);
  if (!gait) {
    packet.opcode = CommandOpcode::kStand;
    return;
  }
  packet.arg1 = static_cast<std::int32_t>(gait->speed);
  if (state.actor_id != 0 && (tile & kRouteLayerMask) == 0) {
    ReserveTile(room, tile);
  }
}

void Attack(const ActorCommandRoom& room, ActorCommandPacket& packet) {
  const AttackSelection attack = room.SelectedAttack();
  packet.arg0 =
      Packed(static_cast<int>(attack.row), static_cast<int>(attack.column));
  if (attack.slot_kind == 1 || attack.slot_kind == 2) {
    packet.arg1 = Packed(static_cast<int>(attack.target_id),
                         static_cast<int>(attack.target_limb));
  } else {
    const CommandPosition target = room.TargetPosition(attack.target_id);
    packet.arg1 = Packed(target.x, target.y - attack.height_offset);
    packet.arg2 = Packed(target.z, packet.arg2 >> kHalfWordBits);
  }
  packet.opcode = CommandOpcode::kAttack;
}

bool StoreDirectPacket(CommandIntent intent, const ActorCommandState& state,
                       ActorCommandPacket& packet) {
  switch (intent) {
    case CommandIntent::kStand:
      packet.opcode = CommandOpcode::kStand;
      break;
    case CommandIntent::kStop:
      packet.opcode = CommandOpcode::kStop;
      break;
    case CommandIntent::kHold:
      packet.opcode = CommandOpcode::kHold;
      break;
    case CommandIntent::kSetHeading:
      packet.opcode = CommandOpcode::kTurn;
      packet.arg0 = Packed(
          static_cast<int>(static_cast<unsigned>(state.heading) & kAngleMask),
          state.facing_delay);
      break;
    case CommandIntent::kMoveVector:
      packet.opcode = CommandOpcode::kMoveVector;
      packet.arg0 = state.move_x;
      packet.arg1 = state.move_z;
      packet.arg2 = state.move_y;
      break;
    case CommandIntent::kLunge:
    case CommandIntent::kDash:
      packet.opcode = intent == CommandIntent::kLunge ? CommandOpcode::kLunge
                                                      : CommandOpcode::kDash;
      packet.arg0 = Packed(state.move_x, state.move_y);
      packet.arg1 = Packed(state.move_z, packet.arg1 >> kHalfWordBits);
      break;
    case CommandIntent::kTimedTurn:
      packet.opcode = CommandOpcode::kTimedTurn;
      packet.arg0 = Packed(state.heading, state.turn_rate);
      break;
    case CommandIntent::kCancel:
      packet.opcode = CommandOpcode::kCancel;
      break;
    default:
      return false;
  }
  return true;
}
}  // namespace

// decomp: func_800DEEFC / jtbl_800F16F8 (src/BATTLE/BATTLE.PRG/766FC.s:14).
void IssueActorCommand(ActorCommandRoom& room, ActorCommandState& state,
                       ActorCommandPacket& packet, CommandIntent intent) {
  if (StoreDirectPacket(intent, state, packet)) return;
  switch (intent) {
    case CommandIntent::kTurn:
      Turn(room, state, packet);
      break;
    case CommandIntent::kMoveDirection:
      Move(room, state, packet, state.heading * kDirectionAngle);
      break;
    case CommandIntent::kMoveDestination: {
      const std::uint32_t source = state.tile;
      const std::uint32_t target = state.destination;
      const int dx = TileX(source) - TileX(target);
      const int dz =
          std::bit_cast<int>(((source >> kPackedByteShift) << kRunSpeedShift) -
                             ((target >> kPackedByteShift) << kRunSpeedShift));
      Move(room, state, packet, Core::Math::Arctangent2(dx, dz));
      break;
    }
    case CommandIntent::kMoveHeading:
      Move(room, state, packet, state.heading);
      break;
    case CommandIntent::kMoveTile:
      packet.opcode = CommandOpcode::kMoveTile;
      packet.arg0 = std::bit_cast<std::int32_t>(state.next_tile);
      ReserveTile(room, state.next_tile);
      break;
    case CommandIntent::kHover: {
      packet.opcode = CommandOpcode::kHover;
      const int x = TileX(state.next_tile) * kTileSize;
      const int z = TileCommandZ(state.next_tile);
      packet.arg1 = Packed(z, packet.arg1 >> kHalfWordBits);
      packet.arg0 = Packed(x + kTileCenter, Field::GroundHeightAtWorldPosition(
                                                room.Terrain(), x, z) +
                                                state.flight_height);
      break;
    }
    case CommandIntent::kWalkToTile:
      WalkToTile(room, state, packet);
      break;
    case CommandIntent::kStepToTile:
      StepToTile(room, state, packet);
      break;
    case CommandIntent::kAttack:
      Attack(room, packet);
      break;
    default:
      Core_Debugf("ai", "AI actor command intent %d has no dispatch handler",
                  static_cast<int>(intent));
      std::abort();
  }
}
}  // namespace VS::Battle::Ai
