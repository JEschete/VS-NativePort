// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/Wander.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Diagnostics.h"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr std::uint32_t kTileByteMask = 0xFF;
constexpr unsigned kTileTagShift = 8;
constexpr std::uint32_t kTileTagMask = 0xFF00;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kTileXZMask = 0xFF00FF;

// The chain steps along the even (axis) directions.
constexpr int kChainDirections = 4;
constexpr int kChainDirectionStride = 2;
constexpr int kOppositeDirection = 4;
constexpr int kDirectionMask = 7;

// Tile flags.
constexpr std::uint16_t kTileAreaMask = 15;
constexpr std::uint16_t kTileNoWander = 0x20;
constexpr std::uint16_t kTileRoamBarrier = 0x800;

// Movement flags: the actor projects its destination clear of obstacles.
constexpr std::uint16_t kProjectsDestination = 8;

constexpr int kChoiceRange = 0x1000;
constexpr int kTimerRange = 128;
// A timer draw below this (44 in 128) switches between moving and resting.
constexpr std::uint32_t kTimerSwitchDraws = 44;
constexpr std::uint8_t kHoldingBehaviorMode = 1;
constexpr std::uint8_t kReleasedBehaviorMode = 2;
constexpr std::uint16_t kHoldTicks = 31;
constexpr std::uint32_t kRoamingRange = 63;
constexpr int kRoamDistanceBase = 128;
constexpr int kStartDirectionRange = 4;
constexpr int kHeadingJitterRange = 0x200;
constexpr unsigned kDirectionHeadingShift = 9;
// Half a direction, as a halfword: the jitter starts that far before the
// direction's centre.
constexpr int kHeadingJitterBase = 0xFF00;
constexpr std::uint16_t kCooldownTicks = 60;
constexpr int kBoundsChecks = 2;
constexpr unsigned kRateShiftMask = 31;
constexpr int kRoamLocomotion = 1;
constexpr int kGoalLocomotion = 0;
constexpr std::uint16_t kStartPhase = 0;
constexpr std::uint16_t kChainPhase = 1;
constexpr std::uint16_t kGoalPhase = 2;
constexpr int kTileSize = 128;
constexpr int kTileCentre = 64;
constexpr std::uint32_t kAngleMask = 0xFFF;

std::nullopt_t Refuse(const char* message) {
  Core_Warnf("ai", "%s", message);
  return std::nullopt;
}

bool Fail(const char* message) {
  Core_Warnf("ai", "%s", message);
  return false;
}

int TileX(std::uint32_t tile) { return static_cast<int>(tile & kTileByteMask); }

int TileZ(std::uint32_t tile) {
  return static_cast<int>((tile >> kTileZShift) & kTileByteMask);
}

// The original added to each byte, which wraps.
std::uint32_t Stepped(std::uint32_t tile, NavigationStep step) {
  const auto x = static_cast<std::uint8_t>(TileX(tile) + step.dx);
  const auto z = static_cast<std::uint8_t>(TileZ(tile) + step.dz);
  return (tile & ~kTileXZMask) |
         (static_cast<std::uint32_t>(z) << kTileZShift) | x;
}

std::optional<std::uint16_t> FlagsAt(const WanderWorld& world,
                                     std::uint32_t tile) {
  const auto flags = world.TileFlags(TileX(tile), TileZ(tile));
  if (!flags) return Refuse("AI wander reads a tile outside the tile flags");
  return flags;
}

// Rate 31 passes INT_MIN, as the original's shift did.
int RateRange(std::uint8_t rate) {
  return static_cast<int>(1U << (rate & kRateShiftMask));
}

void UpdateCooldown(WanderActor& actor) {
  if (actor.stayed_on_tile == 0) {
    actor.cooldown = kCooldownTicks;
  } else if (actor.cooldown != 0) {
    --actor.cooldown;
  }
}

// The directions a chain may extend the goal along, in draw order.
struct ChainChoices {
  std::array<int, kChainDirections> directions{};
  int count = 0;
};

std::optional<ChainChoices> ChainChoicesFrom(
    WanderWorld& world, WanderActor& actor,
    const NavigationDirectionCodes& directions) {
  ChainChoices choices;
  // The original kept one candidate across the directions; each unprojected
  // direction rewrites its tag and coordinates, never its status byte.
  auto candidate = actor.goal;
  for (int index = 0; index < kChainDirections; ++index) {
    const int direction = index * kChainDirectionStride;
    if ((actor.movement_flags & kProjectsDestination) == 0) {
      bool blocked = false;
      if (world.HasRoomData()) {
        const auto cell =
            world.TerrainRegionCell(TileX(actor.goal), TileZ(actor.goal));
        if (!cell) return Refuse("AI wander goal is outside the regions");
        blocked = ((*cell >> direction) & 1U) != 0;
      } else {
        blocked = world.IsAdjacentStepBlocked(actor.goal, direction);
      }
      const auto step = WanderDirectionStep(directions, direction);
      if (!step) return std::nullopt;
      candidate = (Stepped(actor.goal, *step) & ~kTileTagMask) |
                  (static_cast<std::uint32_t>(blocked) << kTileTagShift);
      if (blocked) {
        if (!world.CanTraverseEdge(actor, actor.goal, candidate, direction)) {
          continue;
        }
        candidate &= ~kTileTagMask;
      }
      if (actor.is_stuck == 0) {
        const auto to = FlagsAt(world, candidate);
        if (!to) return std::nullopt;
        const auto from = FlagsAt(world, actor.goal);
        if (!from) return std::nullopt;
        if ((*to & kTileAreaMask) != (*from & kTileAreaMask)) continue;
      }
    }
    const auto flags = FlagsAt(world, candidate);
    if (!flags) return std::nullopt;
    if ((*flags & kTileNoWander) == 0) {
      choices.directions[static_cast<std::size_t>(choices.count++)] = direction;
    }
  }
  return choices;
}

int DrawChoice(WanderWorld& world, int count) {
  return static_cast<int>(
      static_cast<std::uint32_t>(world.NextRandomMasked(kChoiceRange)) %
      static_cast<std::uint32_t>(count));
}

// The roaming branch: restart the heading when due, then dispatch the roam.
bool Roam(WanderWorld& world, WanderActor& actor, std::uint32_t range) {
  if (actor.phase == 0 || world.NextRandomMasked(RateRange(actor.rate)) == 0 ||
      actor.cooldown == 0) {
    const auto distance =
        static_cast<std::uint32_t>(world.NextRandomMasked(kChoiceRange)) %
            range +
        kRoamDistanceBase;
    actor.phase = kChainPhase;
    actor.cooldown = kCooldownTicks;
    actor.range_squared = static_cast<std::int32_t>(distance * distance);
    const int start =
        world.NextRandomMasked(kStartDirectionRange) * kChainDirectionStride;
    bool found = false;
    for (int index = 0; index < kChainDirections; ++index) {
      const int direction =
          (start + index * kChainDirectionStride) & kDirectionMask;
      const bool open = actor.flies
                            ? world.CanFlyInDirection(actor, direction)
                            : world.CanWalkInDirection(actor, direction);
      if (open) {
        actor.heading = RandomWanderHeading(world, direction);
        found = true;
        break;
      }
    }
    if (!found) {
      actor.heading =
          static_cast<std::int16_t>(world.NextRandomMasked(kChoiceRange));
    }
  }
  UpdateCooldown(actor);
  if ((actor.movement_flags & kProjectsDestination) != 0) {
    world.ProjectClearRadialDestination(actor, actor.tile, actor.heading,
                                        actor.move_distance);
  }
  const auto bounds = world.RoomBounds();
  if (!bounds) return Fail("AI wander has no room bounds");
  const auto directions = world.DirectionCodes();
  // The original indexed the codes by the signed heading >> 9, which reads
  // the byte before them for a negative heading.
  int direction = (actor.heading >> kDirectionHeadingShift) & kDirectionMask;
  for (int check = 0; check < kBoundsChecks; ++check) {
    const auto step = WanderDirectionStep(directions, direction);
    if (!step) return false;
    const int x = TileX(actor.tile) + step->dx;
    const int z = TileZ(actor.tile) + step->dz;
    if (bounds->max_z < z || z < bounds->min_z || bounds->max_x < x ||
        x < bounds->min_x) {
      actor.cooldown = 0;
      break;
    }
    const auto flags = world.TileFlags(x, z);
    if (!flags) return Fail("AI wander reads a tile outside the tile flags");
    if ((*flags & kTileRoamBarrier) != 0) {
      actor.cooldown = 0;
      break;
    }
    direction = (direction + kChainDirectionStride) & kDirectionMask;
  }
  world.DispatchLocomotion(actor, kRoamLocomotion, actor.heading);
  return true;
}

// The goal phase: walk to the chain's goal until there or out of time.
void WalkToGoal(WanderWorld& world, WanderActor& actor) {
  actor.destination = actor.goal;
  if ((actor.movement_flags & kProjectsDestination) != 0) {
    const auto angle =
        static_cast<std::uint32_t>(Core::Math::Arctangent2(
            actor.x - TileX(actor.destination) * kTileSize - kTileCentre,
            actor.z - TileZ(actor.destination) * kTileSize - kTileCentre)) &
        kAngleMask;
    actor.saved_destination = actor.destination;
    world.ProjectClearRadialDestination(
        actor, actor.tile, static_cast<int>(angle), actor.move_distance);
  }
  world.DispatchLocomotion(actor, kGoalLocomotion, 0);
  UpdateCooldown(actor);
  if ((actor.destination & kTileXZMask) == (actor.tile & kTileXZMask) ||
      actor.cooldown == 0) {
    actor.phase = kStartPhase;
  }
}
}  // namespace

// decomp: wanderDirection (src/BATTLE/BATTLE.PRG/771A8.c:1633).
std::optional<NavigationStep> WanderDirectionStep(
    const NavigationDirectionCodes& directions, int index) {
  if (index < 0 || index >= kNavigationDirections) {
    return Refuse("AI wander direction is outside the codes");
  }
  return DecodeDirectionCode(directions[static_cast<std::size_t>(index)]);
}

// decomp: func_800E1BB8 (src/BATTLE/BATTLE.PRG/771A8.c:1638).
std::optional<WanderTileChoice> ChooseNextWanderTile(WanderWorld& world,
                                                     WanderActor& actor,
                                                     std::uint32_t tile,
                                                     std::uint32_t range) {
  if (range == 0) return WanderTileChoice::kBlocked;
  if (actor.count == 0) actor.goal = tile;
  const auto directions = world.DirectionCodes();
  const auto choices = ChainChoicesFrom(world, actor, directions);
  if (!choices) return std::nullopt;
  if (choices->count == 0) return WanderTileChoice::kBlocked;
  int chosen = DrawChoice(world, choices->count);
  if (actor.heading > 0 &&
      ((actor.heading + kOppositeDirection) & kDirectionMask) ==
          choices->directions[static_cast<std::size_t>(chosen)]) {
    chosen = DrawChoice(world, choices->count);
  }
  const int direction = choices->directions[static_cast<std::size_t>(chosen)];
  const auto step = WanderDirectionStep(directions, direction);
  if (!step) return std::nullopt;
  actor.goal = Stepped(actor.goal, *step);
  actor.heading = static_cast<std::int16_t>(direction);
  ++actor.count;
  return range < actor.count ? WanderTileChoice::kComplete
                             : WanderTileChoice::kExtended;
}

// decomp: wanderRandomHeading (src/BATTLE/BATTLE.PRG/771A8.c:1700).
std::int16_t RandomWanderHeading(WanderWorld& world, int direction) {
  const int jitter = world.NextRandomMasked(kHeadingJitterRange);
  return static_cast<std::int16_t>(
      (static_cast<std::uint32_t>(direction) << kDirectionHeadingShift) +
      static_cast<std::uint32_t>(jitter + kHeadingJitterBase));
}

// decomp: wanderStartWalk (src/BATTLE/BATTLE.PRG/771A8.c:1708).
void StartWanderWalk(WanderWorld& world, WanderActor& actor) {
  actor.heading = -1;
  actor.count = 0;
  actor.cooldown = kCooldownTicks;
  ++actor.phase;
  world.RecoverMovement(actor);
}

// decomp: func_800E1EAC (src/BATTLE/BATTLE.PRG/771A8.c:1717).
bool UpdateWanderMovement(WanderWorld& world, WanderActor& actor,
                          std::uint32_t tile, std::uint32_t range) {
  ++actor.timer;
  if (actor.behavior_mode == kHoldingBehaviorMode) {
    if (actor.timer < kHoldTicks) return true;
    if (static_cast<std::uint32_t>(world.NextRandomMasked(kTimerRange)) <
        kTimerSwitchDraws) {
      actor.behavior_mode = kReleasedBehaviorMode;
    } else {
      actor.timer = 0;
      return true;
    }
  }
  if (actor.timer == actor.move_period) {
    if (static_cast<std::uint32_t>(world.NextRandomMasked(kTimerRange)) <
        kTimerSwitchDraws) {
      actor.timer = 0;
    }
  } else if (static_cast<int>(actor.timer) >=
             actor.rest_period + actor.move_period) {
    actor.timer = static_cast<std::uint32_t>(
                      world.NextRandomMasked(kTimerRange)) < kTimerSwitchDraws
                      ? actor.move_period
                      : 0;
  }
  actor.is_stuck = 1;
  world.ClearDirectionCache();
  if (actor.timer >= actor.move_period) {
    world.RecoverMovement(actor);
    return true;
  }
  if (range >= kRoamingRange) return Roam(world, actor, range);
  actor.height_filter = 1;
  if (world.NextRandomMasked(RateRange(actor.rate)) == 0) {
    actor.phase = kStartPhase;
  }
  switch (actor.phase) {
    case kStartPhase:
      StartWanderWalk(world, actor);
      return true;
    case kChainPhase: {
      const auto choice = ChooseNextWanderTile(world, actor, tile, range);
      if (!choice) return false;
      if (*choice == WanderTileChoice::kBlocked) {
        actor.phase = kStartPhase;
      } else if (*choice == WanderTileChoice::kComplete) {
        ++actor.phase;
        actor.destination = actor.goal;
      }
      break;
    }
    case kGoalPhase:
      WalkToGoal(world, actor);
      return true;
    default:
      actor.phase = kStartPhase;
      break;
  }
  world.RecoverMovement(actor);
  return true;
}
}  // namespace VS::Battle::Ai
