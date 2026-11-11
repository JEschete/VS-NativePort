// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/FlightMovement.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/SpecialTerrain.hpp"
#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
using Core::Math::TileCoordinate;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrappingSubtract;

constexpr std::uint32_t kTileByteMask = 0xFF;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kTileXZMask = 0xFF00FF;
constexpr int kTileShift = 7;
constexpr int kTileCentre = 64;
constexpr unsigned kFractionShift = 12;
constexpr std::uint32_t kFixedOne = 0x1000;
constexpr std::int32_t kFallSpeed = -0x1000;
constexpr std::uint32_t kAngleMask = 0xFFF;
constexpr std::uint32_t kProjectionHeadingBase = 0xC00;
constexpr unsigned kDirectionAngleShift = 9;
constexpr unsigned kSegmentSampleShift = 4;

// A moving platform blocks a probe within its reach plus the actor's radius,
// and within half the actor's height plus this slack.
constexpr std::int32_t kPlatformReach = 0x60;
constexpr std::int32_t kPlatformHeightSlack = 0x20;
// Tile flag 0x40: closed to flight; on the destination, the tile the actor
// hovers beside while it approaches its target.
constexpr std::uint16_t kNoFlightTileFlag = 0x40;
// Movement flags 0x02 and 0x04: the actor may switch to the ground.
constexpr std::uint16_t kGroundTransitionFlags = 6;
constexpr std::uint32_t kNearDestinationSquared = 16;
constexpr std::uint32_t kAdjacentSquared = 2;
constexpr int kStraightDirection = 0;

// func_800E3600's flight modes and the movement results flight follows.
constexpr int kFlightResolveMode = 2;
constexpr int kFlightHeadingResolveMode = 3;
constexpr int IntentCode(CommandIntent intent) {
  return static_cast<int>(intent);
}
constexpr int kMoveDirectionResult = IntentCode(CommandIntent::kMoveDirection);
constexpr int kMoveHeadingResult = IntentCode(CommandIntent::kMoveHeading);
constexpr int kMoveVectorIntent = IntentCode(CommandIntent::kMoveVector);

// The glide divides a zero height sum without trapping (#342): the R3000
// leaves -1, and vs_gte_rsqrt (SLUS_010.40 0x80041B94) of -1 << 12 indexes
// below D_80040C18, reading the halfword 0x0AC8 of the instruction at
// 0x80040998, so its root is (0x0AC8 << 5) >> 12.
constexpr std::uint32_t kZeroSumRootInput = 0xFFFFF000;
constexpr std::int32_t kZeroSumRoot = 21;

bool Fail(const char* message) {
  Core_Warnf("ai", "%s", message);
  return false;
}

int TileX(std::uint32_t tile) { return static_cast<int>(tile & kTileByteMask); }

int TileZ(std::uint32_t tile) {
  return static_cast<int>((tile >> kTileZShift) & kTileByteMask);
}

TileCoordinate Unpack(std::uint32_t tile) {
  return Core::Math::UnpackTileCoordinate(tile);
}

int TileCentre(int tile) { return (tile << kTileShift) + kTileCentre; }

std::uint8_t DirectionBit(int direction) {
  return static_cast<std::uint8_t>(1U << static_cast<unsigned>(direction));
}

// The magnitude the original took with negu, wrapping INT32_MIN.
std::int32_t Magnitude(std::int32_t value) {
  return value < 0 ? WrappingNegate(value) : value;
}

// A Q12 coordinate.
std::int32_t Shifted(std::int32_t value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value)
                                   << kFractionShift);
}

// The projection heading the original took as (0xC00 - angle) & 0xFFF.
std::int32_t ProjectionHeading(std::uint32_t angle) {
  return static_cast<std::int32_t>((kProjectionHeadingBase - angle) &
                                   kAngleMask);
}

std::uint32_t DirectionAngle(std::int16_t heading) {
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(heading))
         << kDirectionAngleShift;
}

bool ValidDirection(int direction, const char* query) {
  if (direction >= 0 && direction < kFlightDirectionCount) return true;
  Core_Warnf("ai", "AI flight %s direction %d is invalid", query, direction);
  return false;
}

// The point an obstruction test probes from.
struct Probe {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t z = 0;
};

// Whether a contact offset from the probe lies behind it along the step.
bool Behind(NavigationStep step, std::int32_t dx, std::int32_t dz) {
  return (step.dx > 0 && dx < 0) || (step.dx < 0 && dx > 0) ||
         (step.dz > 0 && dz < 0) || (step.dz < 0 && dz > 0);
}

bool Within(std::int32_t offset, std::uint32_t reach) {
  return static_cast<std::uint32_t>(Magnitude(offset)) <= reach;
}

bool PlatformBlocks(const FlightMovementWorld& world, const FlightMover& mover,
                    const Probe& probe, NavigationStep step, bool ahead) {
  const auto contacts = world.PlatformPositions();
  const auto reach = static_cast<std::uint32_t>(mover.radius + kPlatformReach);
  const auto height = static_cast<std::uint32_t>(
      (mover.body_height + kPlatformHeightSlack) >> 1);
  for (auto contact = contacts.rbegin(); contact != contacts.rend();
       ++contact) {
    const auto dx = WrappingSubtract(contact->x, probe.x);
    const auto dy = WrappingSubtract(contact->height, probe.y);
    const auto dz = WrappingSubtract(contact->z, probe.z);
    if (ahead && Behind(step, dx, dz)) continue;
    if (Within(dx, reach) && Within(dz, reach) && Within(dy, height)) {
      return true;
    }
  }
  return false;
}

// The blocking actor's ID + 1, 0 for none; nothing when the world refuses.
std::optional<int> BlockingActor(FlightMovementWorld& world,
                                 const FlightMover& mover, const Probe& probe,
                                 NavigationStep step, bool ahead) {
  world.RewindActors();
  while (const auto id = world.NextActor()) {
    const auto obstacle = world.Obstacle(*id);
    if (!obstacle) {
      Core_Warnf("ai",
                 "AI flight obstruction reads actor %d without an AI "
                 "state",
                 *id);
      return std::nullopt;
    }
    if (obstacle->actor_id == mover.actor_id || obstacle->actor_id == 0) {
      continue;
    }
    if (!obstacle->position) {
      Core_Warnf("ai", "AI flight obstruction reads actor %d without a model",
                 *id);
      return std::nullopt;
    }
    const auto dx = WrappingSubtract(obstacle->position->x, probe.x);
    const auto dy = WrappingSubtract(obstacle->position->y, probe.y);
    const auto dz = WrappingSubtract(obstacle->position->z, probe.z);
    if (ahead && Behind(step, dx, dz)) continue;
    const auto reach = static_cast<std::uint32_t>(obstacle->radius) +
                       mover.radius + obstacle->max_range;
    const auto height = static_cast<std::uint32_t>(
        (mover.body_height + obstacle->body_height) >> 1);
    if (Within(dx, reach) && Within(dz, reach) && Within(dy, height)) {
      return WrappingAdd(*id, 1);
    }
  }
  return 0;
}

// What a flight test finds one tile along a direction.
enum class FlightTile : std::uint8_t {
  kRefused,
  kClosed,
  // Flying over the tile's ground would cross the ceiling; not cached.
  kCrossesCeiling,
  kOpen,
};

FlightTile ClassifyFlightTile(FlightMovementWorld& world,
                              const FlightMover& mover, int direction) {
  const auto obstruction =
      FlightDirectionObstruction(world, mover, direction, true);
  if (!obstruction) return FlightTile::kRefused;
  if (*obstruction != 0) return FlightTile::kClosed;
  const auto step = DecodeDirectionCode(
      world.DirectionCodes()[static_cast<std::size_t>(direction)]);
  const int x = TileX(mover.tile) + step.dx;
  const int z = TileZ(mover.tile) + step.dz;
  const auto room = world.Room();
  if (!room) {
    Fail("AI flight test requires the room AI state");
    return FlightTile::kRefused;
  }
  if (x > room->bounds.max_x || x < room->bounds.min_x ||
      z > room->bounds.max_z || z < room->bounds.min_z) {
    return FlightTile::kClosed;
  }
  const auto flags = world.TileFlags(x, z);
  if (!flags) {
    Core_Warnf("ai", "AI flight test reads tile (%d, %d) outside the flag map",
               x, z);
    return FlightTile::kRefused;
  }
  if ((*flags & kNoFlightTileFlag) != 0) return FlightTile::kClosed;
  if (TileX(mover.previous_tile) == x && TileZ(mover.previous_tile) == z) {
    return FlightTile::kClosed;
  }
  if (room->ceiling != 0) {
    const auto ground = Field::TryGroundHeightAtTile(world.Terrain(), x, z);
    if (!ground) {
      Core_Warnf("ai",
                 "AI flight test reads tile (%d, %d) outside the "
                 "terrain",
                 x, z);
      return FlightTile::kRefused;
    }
    if (WrappingAdd(*ground, mover.flight_height) < room->ceiling) {
      return FlightTile::kCrossesCeiling;
    }
  }
  return FlightTile::kOpen;
}

// decomp: func_800E0850 (src/BATTLE/BATTLE.PRG/771A8.c:942), through
// LowestReachableGroundHeight with the battle flight test; nothing when the
// world refuses.
std::optional<int> LowestReachableHeight(FlightMovementWorld& world,
                                         const FlightMover& mover,
                                         std::uint32_t angle) {
  const auto codes = world.DirectionCodes();
  FlightNeighborhood neighborhood;
  neighborhood.terrain = world.Terrain();
  neighborhood.origin = Unpack(mover.tile);
  for (std::size_t direction = 0; direction < codes.size(); ++direction) {
    const auto step = DecodeDirectionCode(codes[direction]);
    neighborhood.steps[direction] = {.dx = step.dx, .dz = step.dz};
  }
  bool refused = false;
  neighborhood.can_fly = [&](int direction) {
    if (refused) return false;
    const auto open = CanFlyInDirection(world, mover, direction);
    if (!open) {
      refused = true;
      return false;
    }
    if (!*open) return false;
    const auto step = neighborhood.steps[static_cast<std::size_t>(direction)];
    const int x = neighborhood.origin.x + step.dx;
    const int z = neighborhood.origin.z + step.dz;
    if (!neighborhood.terrain.Contains(x, z)) {
      Core_Warnf("ai", "AI flight reads tile (%d, %d) outside the terrain", x,
                 z);
      refused = true;
      return false;
    }
    return true;
  };
  const int lowest = LowestReachableGroundHeight(neighborhood, angle);
  if (refused) return std::nullopt;
  return lowest;
}

// decomp: func_800E6A6C (src/BATTLE/BATTLE.PRG/7E05C.s:187), after checking
// that each of its samples lies inside the terrain; nothing when one does not.
std::optional<int> LowestSegmentHeight(Field::TerrainGrid terrain, int start_x,
                                       int start_z, int end_x, int end_z) {
  const int step_x = (end_x - start_x) >> kSegmentSampleShift;
  const int step_z = (end_z - start_z) >> kSegmentSampleShift;
  int x = start_x;
  int z = start_z;
  for (int sample = 0; sample < kGroundSegmentSampleCount; ++sample) {
    x += step_x;
    z += step_z;
    if (!terrain.Contains(x >> kTileShift, z >> kTileShift)) {
      Core_Warnf("ai", "AI flight samples (%d, %d) outside the terrain", x, z);
      return std::nullopt;
    }
  }
  return MinimumAiGroundHeightAlongSegment(terrain, start_x, start_z, end_x,
                                           end_z);
}

std::optional<int> GroundHeight(const FlightMovementWorld& world,
                                TileCoordinate tile) {
  const auto terrain = world.Terrain();
  if (!terrain.Contains(tile.x, tile.z)) {
    Core_Warnf("ai", "AI flight reads tile (%d, %d) outside the terrain",
               tile.x, tile.z);
    return std::nullopt;
  }
  return Field::GroundHeightAtPackedTile(terrain, tile);
}

// vs_gte_rsqrt (SLUS_010.40 0x80041B94, src/SLUS_010.40/32154.s:209). The
// zero-sum root is the only negative input flight movement reaches; any other
// would read an unknown halfword below the table and is refused.
std::optional<std::int32_t> OriginalSquareRoot(std::uint32_t value) {
  if (value == kZeroSumRootInput) return kZeroSumRoot;
  if (static_cast<std::int32_t>(value) < 0) {
    Core_Warnf("ai", "AI flight takes the root of 0x%08X", value);
    return std::nullopt;
  }
  return Core::Math::SquareRootInteger(static_cast<std::int32_t>(value));
}

void Fall(FlightMover& mover) {
  mover.move_y = kFallSpeed;
  mover.move_z = 0;
  mover.move_x = 0;
}

void MoveOnGround(FlightMovementWorld& world, FlightMover& mover, int angle,
                  bool toward_destination) {
  if (toward_destination) {
    world.MoveTowardDestinationTile(mover, angle);
  } else {
    world.BeginGroundMovement(mover, angle);
  }
}

// The Q12 coordinate one step toward the centre, or the centre within a step.
std::int32_t StepToward(std::int32_t position, std::int32_t centre,
                        std::int32_t step, std::int32_t unit) {
  if (Magnitude(position - centre) < step) return Shifted(centre);
  return WrappingAdd(Shifted(position), WrappingMultiply(step, unit));
}

// Hovers the last step onto the actor's own tile at `level`.
void HoverOntoTile(FlightMovementWorld& world, FlightMover& mover,
                   FlightStep step, std::int32_t level) {
  const auto here = Unpack(mover.tile);
  const int centre_x = TileCentre(here.x);
  const int centre_z = TileCentre(here.z);
  const auto angle =
      Core::Math::Arctangent2(mover.x - centre_x, mover.z - centre_z);
  mover.move_y = Shifted(level);
  const auto heading = ProjectionHeading(static_cast<std::uint32_t>(angle));
  mover.move_x =
      StepToward(mover.x, centre_x, step.step, Core::Math::Cosine(heading));
  mover.move_z =
      StepToward(mover.z, centre_z, step.step, Core::Math::Sine(heading));
  world.IssueCommand(mover, kMoveVectorIntent);
}

// Rises or sinks toward the approach target's height beside its tile.
bool HoverBesideTarget(FlightMovementWorld& world, FlightMover& mover,
                       FlightStep step) {
  const auto target = world.TargetRecord(mover.approach_target);
  if (!target) return Fail("AI flight approach requires the target's model");
  const auto ground = GroundHeight(world, Unpack(mover.tile));
  if (!ground) return false;
  std::int32_t level = WrappingAdd(*ground, mover.flight_height);
  if (target->y < *ground) {
    const auto room = world.Room();
    if (!room) return Fail("AI flight approach requires the room AI state");
    level = room->ceiling != 0 && room->ceiling >= target->y ? room->ceiling
                                                             : target->y;
  }
  const auto rise = WrappingSubtract(level, mover.y);
  if (Magnitude(rise) < step.step) {
    HoverOntoTile(world, mover, step, level);
    return true;
  }
  mover.move_y = rise;
  mover.move_z = 0;
  mover.move_x = 0;
  if (rise > 0) {
    const auto blocked =
        FlightDirectionObstruction(world, mover, kStraightDirection, false);
    if (!blocked) return false;
    if (*blocked != 0) mover.move_y = 0;
  }
  world.IssueCommand(mover, step.intent);
  return true;
}

// Settles over the destination tile, or onto the approach target standing on
// it.
bool SettleOverDestination(FlightMovementWorld& world, FlightMover& mover,
                           FlightStep step) {
  if (mover.close_approach == 0) {
    world.Recover(mover);
    return true;
  }
  const auto blocked =
      FlightDirectionObstruction(world, mover, kStraightDirection, false);
  if (!blocked) return false;
  if (*blocked != 0) {
    world.Recover(mover);
    return true;
  }
  const auto target = world.TargetRecord(mover.approach_target);
  if (!target) return Fail("AI flight approach requires the target's model");
  const auto destination = Unpack(mover.destination);
  const auto flags = world.TileFlags(destination.x, destination.z);
  if (!flags) return Fail("AI flight reads a destination outside the flag map");
  std::int32_t level = mover.flight_height;
  if ((*flags & kSpecialTerrainTileFlag) == 0) {
    const auto ground = GroundHeight(world, destination);
    if (!ground) return false;
    level = WrappingAdd(level, *ground);
  }
  mover.move_x = (destination.x << kTileShift) - (mover.x - kTileCentre);
  mover.move_z = (destination.z << kTileShift) - (mover.z - kTileCentre);
  mover.move_y = WrappingSubtract(level, mover.y);
  if (target->tile_x == destination.x && target->tile_z == destination.z) {
    mover.move_y = target->y - (target->collision_height >> 1) - mover.y;
  }
  if (mover.move_y > 0 && level < WrappingAdd(mover.y, step.step)) {
    mover.move_y = 0;
  }
  if (Magnitude(mover.move_x) < step.step) mover.move_x = 0;
  if (Magnitude(mover.move_z) < step.step) mover.move_z = 0;
  world.IssueCommand(mover, step.intent);
  return true;
}

// Follows the heading func_800E3600 chose, sinking when every reachable
// neighbour is below the flight height.
bool FlyAlongHeading(FlightMovementWorld& world, FlightMover& mover,
                     FlightStep step) {
  const auto angle = DirectionAngle(mover.heading);
  const auto lowest = LowestReachableHeight(world, mover, angle);
  if (!lowest) return false;
  if (WrappingAdd(*lowest, mover.flight_height) < mover.y) {
    Fall(mover);
  } else {
    const auto heading = ProjectionHeading(angle);
    mover.move_y = 0;
    mover.move_x = Core::Math::Cosine(heading);
    mover.move_z = Core::Math::Sine(heading);
  }
  world.IssueCommand(mover, step.intent);
  return true;
}

// The destination tile's centre in world units.
struct Goal {
  std::int32_t x = 0;
  std::int32_t z = 0;
};

// The heights a glide moves between.
struct GlideProfile {
  std::int32_t arrival = 0;    // the flight height over the destination
  std::int32_t floor = 0;      // the lowest height allowed on the way
  std::int32_t reachable = 0;  // the lowest reachable neighbour's
  std::int32_t distance = 0;   // the horizontal distance left
};

std::optional<GlideProfile> MeasureGlide(FlightMovementWorld& world,
                                         const FlightMover& mover, Goal goal,
                                         std::int32_t bearing) {
  const auto arrival = GroundHeight(world, Unpack(mover.destination));
  if (!arrival) return std::nullopt;
  // The original read x as an unsigned halfword.
  const int start_x = static_cast<std::uint16_t>(mover.x);
  const auto segment =
      LowestSegmentHeight(world.Terrain(), start_x, mover.z, goal.x, goal.z);
  if (!segment) return std::nullopt;
  std::int32_t floor = WrappingAdd(*segment, mover.ground_clearance);
  const auto room = world.Room();
  if (!room) {
    Fail("AI flight glide requires the room AI state");
    return std::nullopt;
  }
  if (room->ceiling != 0) floor = std::max(floor, room->ceiling);
  const auto lowest =
      LowestReachableHeight(world, mover, static_cast<std::uint32_t>(bearing));
  if (!lowest) return std::nullopt;
  GlideProfile profile;
  profile.arrival = WrappingAdd(*arrival, mover.flight_height);
  profile.reachable = WrappingAdd(*lowest, mover.flight_height);
  profile.floor = std::min(floor, profile.reachable);
  const auto squared =
      Core::Math::SquaredVectorLength(goal.x - start_x, 0, goal.z - mover.z);
  const auto distance = OriginalSquareRoot(squared);
  if (!distance) return std::nullopt;
  profile.distance = *distance;
  return profile;
}

// The fraction of the distance left that a step covers, in the original's
// Q12-scaled form; the quotients keep the R3000's zero-divisor results (#342).
std::optional<std::uint32_t> GlideRatio(const GlideProfile& profile) {
  const auto sum = WrappingAdd(profile.arrival, profile.floor);
  // The original divided the shifted sum by itself: 4096 for the room heights
  // seen, or -1 for a zero sum.
  const auto quotient = Core::Math::R3000Quotient(Shifted(sum), sum);
  const auto root = OriginalSquareRoot(static_cast<std::uint32_t>(quotient)
                                       << kFractionShift);
  if (!root) return std::nullopt;
  // The divisor is at least 0x1000, so the original's zero-divisor guard never
  // fires.
  return Core::Math::R3000UnsignedQuotient(
      static_cast<std::uint32_t>(profile.distance) << kFractionShift,
      static_cast<std::uint32_t>(*root) + kFixedOne);
}

void Climb(FlightMovementWorld& world, FlightMover& mover, FlightStep step,
           const GlideProfile& profile, Goal goal, int start_x) {
  const auto rise = WrappingSubtract(profile.arrival, mover.y);
  mover.move_y = step.step < rise ? rise : 0;
  mover.move_x = WrappingSubtract(goal.x, start_x);
  mover.move_z = WrappingSubtract(goal.z, mover.z);
  world.IssueCommand(mover, step.intent);
  mover.stall_counter = 0;
}

// The height a glide step reaches: the remaining height above the floor
// scaled by the squared share of the distance still to cover.
std::int32_t GlideHeight(const GlideProfile& profile, const FlightMover& mover,
                         FlightStep step, std::uint32_t ratio) {
  const auto offset = static_cast<std::uint32_t>(
      Magnitude(WrappingSubtract(mover.y, profile.floor)));
  const auto remaining = static_cast<std::uint32_t>(step.step) - ratio;
  const auto quotient = Core::Math::R3000UnsignedQuotient(
      offset * remaining * remaining, ratio * ratio);
  return static_cast<std::int32_t>(quotient +
                                   static_cast<std::uint32_t>(profile.floor));
}

// Glides toward the destination, climbing first when the way rises above
// the actor.
bool GlideTowardDestination(FlightMovementWorld& world, FlightMover& mover,
                            FlightStep step, Goal goal, std::int32_t bearing) {
  const auto profile = MeasureGlide(world, mover, goal, bearing);
  if (!profile) return false;
  const auto ratio = GlideRatio(*profile);
  if (!ratio) return false;
  const int start_x = static_cast<std::uint16_t>(mover.x);
  if (WrappingAdd(mover.y, step.step) < profile->floor) {
    Climb(world, mover, step, *profile, goal, start_x);
    return true;
  }
  if (static_cast<std::uint32_t>(step.step) >=
      static_cast<std::uint32_t>(profile->distance)) {
    return UpdateFlightDirectionalMovement(world, mover, bearing);
  }
  auto level = GlideHeight(*profile, mover, step, *ratio);
  const auto heading = ProjectionHeading(static_cast<std::uint32_t>(bearing));
  const auto next_x =
      WrappingAdd(Shifted(start_x),
                  WrappingMultiply(step.step, Core::Math::Cosine(heading)));
  const auto next_z = WrappingAdd(
      Shifted(mover.z), WrappingMultiply(step.step, Core::Math::Sine(heading)));
  if (level < profile->floor) {
    level = profile->floor;
  } else if (profile->reachable < level) {
    if (profile->floor < mover.y) {
      Fall(mover);
      world.IssueCommand(mover, step.intent);
      return true;
    }
    level = profile->reachable;
  }
  const auto rise = WrappingSubtract(level, mover.y);
  mover.stall_counter = 0;
  if (step.step < Magnitude(rise)) {
    mover.move_x = WrappingSubtract(next_x >> kFractionShift, start_x);
    mover.move_z = WrappingSubtract(next_z >> kFractionShift, mover.z);
    mover.move_y = rise;
    world.IssueCommand(mover, step.intent);
  } else {
    mover.move_x = next_x;
    mover.move_y = Shifted(level);
    mover.move_z = next_z;
    world.IssueCommand(mover, kMoveVectorIntent);
  }
  return true;
}
}  // namespace

// decomp: func_800E02B4 (src/BATTLE/BATTLE.PRG/771A8.c:798).
std::optional<int> FlightDirectionObstruction(FlightMovementWorld& world,
                                              const FlightMover& mover,
                                              int direction, bool ahead) {
  if (!ValidDirection(direction, "obstruction")) return std::nullopt;
  const auto index = static_cast<std::size_t>(direction);
  const auto bit = DirectionBit(direction);
  if (const auto cache = world.DirectionCache();
      (cache.obstruction_tested & bit) != 0) {
    return cache.obstruction[index];
  }
  const auto step = DecodeDirectionCode(world.DirectionCodes()[index]);
  const int reach = ahead ? mover.max_range : 0;
  const Probe probe{
      .x = WrappingAdd(mover.x, WrappingMultiply(reach, step.dx)),
      .y = mover.y,
      .z = WrappingAdd(mover.z, WrappingMultiply(reach, step.dz))};
  int answer = 1;
  if (!PlatformBlocks(world, mover, probe, step, ahead)) {
    const auto actor = BlockingActor(world, mover, probe, step, ahead);
    if (!actor) return std::nullopt;
    answer = *actor;
  }
  auto cache = world.DirectionCache();
  cache.obstruction_tested |= bit;
  cache.obstruction[index] = static_cast<std::uint8_t>(answer);
  world.SetDirectionCache(cache);
  return answer;
}

// decomp: func_800E0678 (src/BATTLE/BATTLE.PRG/771A8.c:904).
std::optional<bool> CanFlyInDirection(FlightMovementWorld& world,
                                      const FlightMover& mover, int direction) {
  if (!ValidDirection(direction, "test")) return std::nullopt;
  const auto bit = DirectionBit(direction);
  if (const auto cache = world.DirectionCache();
      (cache.fly_tested & bit) != 0) {
    return (cache.fly_open & bit) != 0;
  }
  const auto tile = ClassifyFlightTile(world, mover, direction);
  if (tile == FlightTile::kRefused) return std::nullopt;
  if (tile == FlightTile::kCrossesCeiling) return false;
  // The obstruction test has updated the cache since it was read.
  auto cache = world.DirectionCache();
  cache.fly_tested |= bit;
  if (tile == FlightTile::kOpen) {
    cache.fly_open |= bit;
  } else {
    cache.fly_open &= static_cast<std::uint8_t>(~bit);
  }
  world.SetDirectionCache(cache);
  return tile == FlightTile::kOpen;
}

// decomp: func_800E0918 (src/BATTLE/BATTLE.PRG/771A8.c:985).
std::optional<bool> UpdateFlightGroundTransition(FlightMovementWorld& world,
                                                 FlightMover& mover, int angle,
                                                 bool toward_destination) {
  if ((mover.movement_flags & kGroundTransitionFlags) == 0) return false;
  if (toward_destination && Core::Math::SquaredTileDistance(
                                Unpack(mover.destination), Unpack(mover.tile)) <
                                kNearDestinationSquared) {
    if (!mover.flies) {
      Fail("AI flight transition requires the actor's model");
      return std::nullopt;
    }
    if (*mover.flies) return false;
    MoveOnGround(world, mover, angle, toward_destination);
    return true;
  }
  const auto room = world.Room();
  if (!room) {
    Fail("AI flight transition requires the room AI state");
    return std::nullopt;
  }
  if (room->ceiling == 0) return false;
  const auto ground = GroundHeight(world, Unpack(mover.tile));
  if (!ground) return std::nullopt;
  if (WrappingAdd(*ground, mover.flight_height) >= room->ceiling) return false;
  if (!mover.flies) {
    Fail("AI flight transition requires the actor's model");
    return std::nullopt;
  }
  if (!*mover.flies) {
    MoveOnGround(world, mover, angle, toward_destination);
    return true;
  }
  if (world.DestinationOccupancy(mover, mover.tile) <= 0) return false;
  world.ReleaseReservations(mover);
  world.BeginTerrainMovement(mover);
  return true;
}

// decomp: func_800E0A68 (src/BATTLE/BATTLE.PRG/771A8.c:1064).
bool UpdateFlightMovement(FlightMovementWorld& world, FlightMover& mover,
                          int angle) {
  const auto transition =
      UpdateFlightGroundTransition(world, mover, angle, true);
  if (!transition) return false;
  if (*transition) return true;
  if (!mover.flies) {
    return Fail("AI flight movement requires the actor's model");
  }
  if (!*mover.flies) {
    world.BeginAlternateTerrainMovement(mover);
    return true;
  }
  mover.movement_disabled = 1;
  const auto step = world.SelectVerticalStep(mover);
  if (!step) {
    world.Recover(mover);
    return true;
  }
  const auto destination = Unpack(mover.destination);
  const Goal goal{.x = TileCentre(destination.x),
                  .z = TileCentre(destination.z)};
  const auto bearing = static_cast<std::int32_t>(
      static_cast<std::uint32_t>(
          Core::Math::Arctangent2(mover.x - goal.x, mover.z - goal.z)) &
      kAngleMask);
  if (mover.close_approach != 0 &&
      Core::Math::SquaredTileDistance(Unpack(mover.tile), destination) <
          kAdjacentSquared) {
    const auto flags = world.TileFlags(destination.x, destination.z);
    if (!flags) {
      return Fail("AI flight reads a destination outside the flag map");
    }
    if ((*flags & kNoFlightTileFlag) != 0) {
      return HoverBesideTarget(world, mover, *step);
    }
  }
  if (((mover.tile ^ mover.destination) & kTileXZMask) == 0) {
    return SettleOverDestination(world, mover, *step);
  }
  const int result =
      world.ResolveDirectionalMovement(mover, bearing, kFlightResolveMode);
  if (result == kMoveDirectionResult) {
    return FlyAlongHeading(world, mover, *step);
  }
  if (result == kMoveHeadingResult) {
    return GlideTowardDestination(world, mover, *step, goal, bearing);
  }
  return true;
}

// decomp: func_800E1238 (src/BATTLE/BATTLE.PRG/771A8.c:1278).
bool UpdateFlightDirectionalMovement(FlightMovementWorld& world,
                                     FlightMover& mover, int angle) {
  const auto transition =
      UpdateFlightGroundTransition(world, mover, angle, false);
  if (!transition) return false;
  if (*transition) return true;
  const auto step = world.SelectVerticalStep(mover);
  if (!step) {
    world.Recover(mover);
    return true;
  }
  mover.movement_disabled = 1;
  const int result =
      world.ResolveDirectionalMovement(mover, angle, kFlightHeadingResolveMode);
  if (result != kMoveDirectionResult && result != kMoveHeadingResult) {
    world.ReleaseReservations(mover);
    world.Recover(mover);
    return true;
  }
  const auto direction = result == kMoveDirectionResult
                             ? DirectionAngle(mover.heading)
                             : static_cast<std::uint32_t>(angle);
  const auto heading = ProjectionHeading(direction);
  const auto cosine = Core::Math::Cosine(heading);
  const auto sine = Core::Math::Sine(heading);
  const auto lowest = LowestReachableHeight(world, mover, direction);
  if (!lowest) return false;
  if (WrappingAdd(*lowest, mover.flight_height) < mover.y) {
    Fall(mover);
  } else {
    mover.move_y = 0;
    mover.move_x = cosine;
    mover.move_z = sine;
  }
  if (!mover.flies) {
    return Fail("AI flight movement requires the actor's model");
  }
  if (!*mover.flies) {
    world.BeginAlternateTerrainMovement(mover);
    return true;
  }
  mover.stall_counter = 0;
  world.IssueCommand(mover, step->intent);
  return true;
}
}  // namespace VS::Battle::Ai
