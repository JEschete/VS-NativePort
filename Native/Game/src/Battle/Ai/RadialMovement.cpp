// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/RadialMovement.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr std::uint32_t kTileByteMask = 0xFF;
constexpr unsigned kTileZShift = 16;
constexpr unsigned kGridDepthShift = 8;
constexpr std::uint32_t kTileXZMask = 0xFF00FF;
// Bit 23, the top bit of the z byte, marks a saved destination as unset.
constexpr std::uint32_t kNoSavedDestination = 0x800000;

constexpr int kTileShift = 7;
constexpr int kTileCentre = 64;
constexpr int kFractionShift = 12;
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr std::uint32_t kProjectionHeadingBase = 0xC00;
constexpr std::uint32_t kHalfTurn = 0x800;

constexpr int kProjectionAttempts = 8;
constexpr unsigned kProjectionStepShift = 3;
// Movement flags 0x02 and 0x04, part of the hover set (0xC6).
constexpr std::uint16_t kDirectStepFlags = 6;
// Locomotion mode one.
constexpr std::uint16_t kModeOneFlags = 0x30;
constexpr std::uint32_t kNearDestinationSquared = 16;
constexpr std::uint16_t kWalkAttempts = 6;

constexpr int kAxisDirections = 4;
constexpr int kAxisDirectionStride = 2;
constexpr int kDirectionMask = 7;
constexpr std::int32_t kStallLimit = 120;
constexpr std::uint8_t kNoMovementTarget = 16;
constexpr unsigned kQuadrantShift = 10;
constexpr std::uint32_t kQuadrantMask = 3;
constexpr std::uint32_t kQuadrantAngleMask = 0x3FF;
constexpr std::uint32_t kQuadrantMidpoint = 512;
constexpr unsigned kDirectionAngleShift = 9;

bool Fail(const char* message) {
  Core_Warnf("ai", "%s", message);
  return false;
}

int TileX(std::uint32_t tile) { return static_cast<int>(tile & kTileByteMask); }

int TileZ(std::uint32_t tile) {
  return static_cast<int>((tile >> kTileZShift) & kTileByteMask);
}

// Replaces the x and z bytes, each wrapped to a byte as the original stored
// them.
std::uint32_t WithTile(std::uint32_t tile, int x, int z) {
  return (tile & ~kTileXZMask) |
         (static_cast<std::uint32_t>(static_cast<std::uint8_t>(z))
          << kTileZShift) |
         static_cast<std::uint8_t>(x);
}

int TileCentre(int tile) { return (tile << kTileShift) + kTileCentre; }

// The squared horizontal distance from (x, z) to the tile's centre.
std::uint32_t SquaredDistanceToTile(std::uint32_t tile, std::int32_t x,
                                    std::int32_t z) {
  return Core::Math::SquaredVectorLength(TileCentre(TileX(tile)) - x, 0,
                                         TileCentre(TileZ(tile)) - z);
}

// The 32-bit product's sign, as the original's mflo then sra.
int ScaledComponent(std::uint32_t distance, std::int32_t unit) {
  const auto product = distance * static_cast<std::uint32_t>(unit);
  return static_cast<std::int32_t>(product) >> kFractionShift;
}

std::uint8_t Clamp(std::uint8_t value, std::uint8_t low, std::uint8_t high) {
  if (value < low) return low;
  return value > high ? high : value;
}

// Arctangent2 of the vector from (x, z) to the target, wrapped to an angle.
std::int16_t AngleFrom(std::int32_t x, std::int32_t z, std::int32_t target_x,
                       std::int32_t target_z) {
  return static_cast<std::int16_t>(
      static_cast<std::uint32_t>(
          Core::Math::Arctangent2(x - target_x, z - target_z)) &
      kAngleMask);
}

void Turn(RadialMovementWorld& world, RadialMover& mover,
          CommandIntent intent) {
  mover.turn_timer = mover.turn_period;
  world.IssueCommand(mover, intent);
}

// The first axis direction searched: the quadrant the model faces, or the
// next one past its midpoint.
int FacingAxisDirection(const RadialMover& mover) {
  const auto facing = static_cast<std::uint32_t>(mover.model_facing) +
                      static_cast<std::uint32_t>(mover.model_look_yaw);
  auto quadrant = (facing & kAngleMask) >> kQuadrantShift;
  if ((facing & kQuadrantAngleMask) > kQuadrantMidpoint) {
    quadrant = (quadrant + 1) & kQuadrantMask;
  }
  return static_cast<int>(quadrant) * kAxisDirectionStride;
}

// The axis neighbour inside the grid with the highest ground; -1 for none.
std::optional<int> HighestNeighbourDirection(const RadialMovementWorld& world,
                                             const RadialMover& mover) {
  const auto codes = world.DirectionCodes();
  const auto grid = world.GridBounds();
  const auto width = static_cast<int>(grid & kTileByteMask);
  const auto depth =
      static_cast<int>((grid >> kGridDepthShift) & kTileByteMask);
  const auto terrain = world.Terrain();
  int best_height = std::numeric_limits<int>::min();
  int best_direction = -1;
  int direction = FacingAxisDirection(mover);
  // The original added the cumulative stride: d, d + 2, d + 6, d + 4.
  for (int index = 0; index < kAxisDirections; ++index) {
    direction = (direction + index * kAxisDirectionStride) & kDirectionMask;
    const auto step =
        DecodeDirectionCode(codes[static_cast<std::size_t>(direction)]);
    const int x = static_cast<std::uint8_t>(TileX(mover.tile) + step.dx);
    const int z = static_cast<std::uint8_t>(TileZ(mover.tile) + step.dz);
    if (x >= width || z >= depth) continue;
    const auto height = Field::TryGroundHeightAtTile(terrain, x, z);
    if (!height) {
      Core_Warnf("ai",
                 "AI movement recovery reads tile (%d, %d) outside the "
                 "terrain",
                 x, z);
      return std::nullopt;
    }
    if (*height > best_height) {
      best_direction = direction;
      best_height = *height;
    }
  }
  return best_direction;
}

// The stalled flier's release; nothing when the world refuses.
std::optional<bool> ReleaseStalledFlier(RadialMovementWorld& world,
                                        RadialMover& mover) {
  if (!mover.flies) return false;
  const auto stalled = mover.stall_counter;
  mover.stall_counter =
      static_cast<std::int32_t>(static_cast<std::uint32_t>(stalled) + 1);
  if (stalled <= kStallLimit ||
      (mover.movement_flags & kDirectStepFlags) == 0 ||
      (mover.movement_flags & kModeOneFlags) == 0) {
    return false;
  }
  const auto occupancy = world.DestinationOccupancy(mover, mover.tile);
  if (!occupancy) return std::nullopt;
  if (*occupancy <= 0) return false;
  world.ReleaseReservations(mover);
  world.BeginTerrainMovement(mover);
  return true;
}

// The destination or a free axis neighbour of it; nothing when none is free,
// or when the world refuses (`refused` set).
std::optional<std::uint32_t> FreeDestination(RadialMovementWorld& world,
                                             RadialMover& mover,
                                             bool& refused) {
  auto occupancy = world.DestinationOccupancy(mover, mover.destination);
  if (!occupancy) {
    refused = true;
    return std::nullopt;
  }
  if (*occupancy > 0) return mover.destination;
  const auto codes = world.DirectionCodes();
  for (int index = 0; index < kAxisDirections; ++index) {
    const int direction = index * kAxisDirectionStride;
    const auto step =
        DecodeDirectionCode(codes[static_cast<std::size_t>(direction)]);
    const auto tile =
        WithTile(mover.destination, TileX(mover.destination) + step.dx,
                 TileZ(mover.destination) + step.dz);
    occupancy = world.DestinationOccupancy(mover, tile);
    if (!occupancy) {
      refused = true;
      return std::nullopt;
    }
    if (*occupancy > 0) return tile;
  }
  return std::nullopt;
}
}  // namespace

bool ProjectClearRadialDestination(RadialMovementWorld& world,
                                   RadialMover& mover, std::uint32_t source,
                                   int angle, std::uint32_t distance) {
  const auto bounds = world.RoomBounds();
  if (!bounds) return Fail("AI radial projection requires the room AI state");
  const auto heading = static_cast<std::int32_t>(
      (kProjectionHeadingBase - static_cast<std::uint32_t>(angle)) &
      kAngleMask);
  const auto step = distance >> kProjectionStepShift;
  const int origin_x = TileX(source) << kTileShift;
  const int origin_z = TileZ(source) << kTileShift;
  std::uint8_t x = 0;
  std::uint8_t z = 0;
  for (int attempt = 0; attempt < kProjectionAttempts;
       ++attempt, distance -= step) {
    x = static_cast<std::uint8_t>(
        (origin_x + ScaledComponent(distance, Core::Math::Cosine(heading)) +
         kTileCentre) >>
        kTileShift);
    z = static_cast<std::uint8_t>(
        (origin_z + ScaledComponent(distance, Core::Math::Sine(heading)) +
         kTileCentre) >>
        kTileShift);
    const auto occupancy = world.DestinationOccupancy(mover, WithTile(0, x, z));
    if (!occupancy) return false;
    if (*occupancy > 0) break;
  }
  mover.destination = WithTile(0, Clamp(x, bounds->min_x, bounds->max_x),
                               Clamp(z, bounds->min_z, bounds->max_z));
  return true;
}

bool MoveToRadialDestination(RadialMovementWorld& world, RadialMover& mover,
                             int /*angle*/) {
  if ((mover.tile & kTileXZMask) == (mover.destination & kTileXZMask)) {
    return RecoverMovement(world, mover);
  }
  const int angle =
      Core::Math::Arctangent2(mover.x - TileCentre(TileX(mover.destination)),
                              mover.z - TileCentre(TileZ(mover.destination)));
  const auto distance = static_cast<std::uint32_t>(mover.move_distance);
  if (distance * distance <
          SquaredDistanceToTile(mover.destination, mover.x, mover.z) &&
      !ProjectClearRadialDestination(world, mover, mover.tile, angle,
                                     distance)) {
    return false;
  }
  if ((mover.movement_flags & kDirectStepFlags) != 0 &&
      (mover.approach_directly != 0 ||
       Core::Math::SquaredTileDistance(
           Core::Math::UnpackTileCoordinate(mover.destination),
           Core::Math::UnpackTileCoordinate(mover.tile)) <
           kNearDestinationSquared)) {
    if ((mover.saved_destination & kNoSavedDestination) == 0) {
      mover.destination = mover.saved_destination;
    }
    world.MoveTowardDestinationTile(mover, angle);
    return true;
  }
  if (++mover.move_attempts < kWalkAttempts) {
    return RecoverMovement(world, mover);
  }
  bool refused = false;
  const auto destination = FreeDestination(world, mover, refused);
  if (refused) return false;
  if (!destination) return RecoverMovement(world, mover);
  mover.destination = *destination;
  mover.move_attempts = 0;
  mover.next_tile = mover.destination;
  world.IssueCommand(mover, CommandIntent::kWalkToTile);
  return true;
}

bool MoveAwayFromReference(RadialMovementWorld& world, RadialMover& mover,
                           int angle) {
  const auto opposite = static_cast<int>(
      (static_cast<std::uint32_t>(angle) + kHalfTurn) & kAngleMask);
  const auto distance = static_cast<std::uint32_t>(mover.move_distance);
  if (!ProjectClearRadialDestination(world, mover, mover.tile, angle,
                                     distance)) {
    return false;
  }
  const auto along = mover.destination;
  if (!ProjectClearRadialDestination(world, mover, mover.tile, opposite,
                                     distance)) {
    return false;
  }
  if (SquaredDistanceToTile(mover.destination, mover.move_x, mover.move_z) <
      SquaredDistanceToTile(along, mover.move_x, mover.move_z)) {
    mover.destination = along;
  }
  return MoveToRadialDestination(world, mover, angle);
}

bool RecoverMovement(RadialMovementWorld& world, RadialMover& mover) {
  const auto released = ReleaseStalledFlier(world, mover);
  if (!released) return false;
  if (*released) return true;
  if (mover.movement_target != kNoMovementTarget) {
    const auto target = world.ActorPosition(mover.movement_target);
    if (!target) {
      Core_Warnf("ai", "AI movement recovery targets actor %d without a model",
                 static_cast<int>(mover.movement_target));
      return false;
    }
    mover.angle = AngleFrom(mover.x, mover.z, target->x, target->z);
    Turn(world, mover, CommandIntent::kTurn);
    return true;
  }
  if (mover.toward_point != 0) {
    mover.heading = AngleFrom(mover.x, mover.z, mover.target_x, mover.target_z);
    Turn(world, mover, CommandIntent::kSetHeading);
    return true;
  }
  const auto direction = HighestNeighbourDirection(world, mover);
  if (!direction) return false;
  if (*direction < 0) {
    world.IssueCommand(mover, CommandIntent::kStand);
    return true;
  }
  mover.angle = static_cast<std::int16_t>(*direction << kDirectionAngleShift);
  Turn(world, mover, CommandIntent::kTurn);
  return true;
}
}  // namespace VS::Battle::Ai
