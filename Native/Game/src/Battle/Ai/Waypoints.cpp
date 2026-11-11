// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/Waypoints.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
namespace {
constexpr int kTileShift = 7;
constexpr int kTileCentre = 64;
constexpr int kPatrolStepBase = 8;
constexpr unsigned kPatrolStepMask = kPatrolWaypointsPerRow - 1;
constexpr unsigned kPatrolRateMask = 31;
constexpr std::uint8_t kPatrolArrivalWait = 10;

std::nullopt_t Refuse(const char* message) {
  Core_Warnf("ai", "%s", message);
  return std::nullopt;
}

int TileCentre(std::uint8_t tile) { return (tile << kTileShift) + kTileCentre; }
}  // namespace

// decomp: nearestWaypointValue (src/BATTLE/BATTLE.PRG/74088.c:946) and
// patrolWaypoint (src/BATTLE/BATTLE.PRG/771A8.c:1857).
std::optional<std::uint16_t> WaypointValue(std::span<const std::uint16_t> row,
                                           int index) {
  if (index < 0 || static_cast<std::size_t>(index) >= row.size()) {
    return Refuse("AI waypoint index is outside its row");
  }
  return row[static_cast<std::size_t>(index)];
}

// decomp: nearestWaypointAddress (src/BATTLE/BATTLE.PRG/74088.c:952).
std::optional<std::span<const std::uint16_t>> WaypointsFrom(
    std::span<const std::uint16_t> row, int index) {
  if (index < 0 || static_cast<std::size_t>(index) > row.size()) {
    return Refuse("AI waypoint address is outside its row");
  }
  return row.subspan(static_cast<std::size_t>(index));
}

// decomp: func_800DD7CC (src/BATTLE/BATTLE.PRG/74088.c:957).
std::optional<WaypointSearch> FindNearestActiveWaypoint(
    std::span<const std::uint16_t, kRoomWaypointCount> waypoints,
    Core::Math::Position3 position, Field::TerrainGrid special_heights) {
  WaypointSearch search;
  auto closest = std::numeric_limits<std::uint32_t>::max();
  for (const std::uint16_t waypoint : waypoints) {
    if (!IsActiveWaypoint(waypoint)) continue;
    if (special_heights.Empty()) {
      return Refuse("AI waypoint search requires the special terrain heights");
    }
    const auto tile = WaypointTileOf(waypoint);
    const int x = TileCentre(tile.x);
    const int z = TileCentre(tile.z);
    const int height =
        Field::GroundHeightAtWorldPosition(special_heights, x, z);
    const auto distance = Core::Math::SquaredVectorLength(
        position.x - x, position.y - height, position.z - z);
    if (distance < closest) {
      search.nearest =
          NearestWaypoint{.tile = tile, .squared_distance = distance};
      closest = distance;
    }
  }
  return search;
}

// decomp: func_800E23AC (src/BATTLE/BATTLE.PRG/771A8.c:1863).
std::optional<WaypointTile> UpdateWaypointPatrol(
    PatrolCursor& cursor, std::span<const std::uint16_t> routes,
    WaypointTile tile, PatrolRandom& random) {
  const std::size_t first = std::size_t{cursor.row} * kPatrolWaypointsPerRow;
  if (first + kPatrolWaypointsPerRow > routes.size()) {
    return Refuse("AI patrol row is outside the room's routes");
  }
  const auto row = routes.subspan(first, kPatrolWaypointsPerRow);
  auto next = cursor;
  const auto current = WaypointValue(row, next.index);
  if (!current) return std::nullopt;

  bool advance = true;
  if (WaypointTileOf(*current) != tile) {
    if (next.cooldown != 0) --next.cooldown;
    // The original shifted by the low five bits and passed the signed word.
    const int range = std::bit_cast<int>(1U << (next.rate & kPatrolRateMask));
    const bool keep_walking =
        random.NextMasked(range) != 0 || next.cooldown != 0;
    if (keep_walking) {
      advance = false;
    } else {
      next.direction = static_cast<std::uint8_t>(-next.direction);
    }
  }

  if (advance) {
    const unsigned step = next.direction + kPatrolStepBase;
    bool found = false;
    // The index cycles within eight steps; the original looped forever when
    // the cycle held no active waypoint.
    for (std::size_t attempt = 0; attempt < kPatrolWaypointsPerRow && !found;
         ++attempt) {
      next.index =
          static_cast<std::uint8_t>((next.index + step) & kPatrolStepMask);
      found = IsActiveWaypoint(row[next.index]);
    }
    if (!found) return Refuse("AI patrol step reaches no active waypoint");
    next.cooldown = kPatrolArrivalWait;
  }

  const auto target = WaypointValue(row, next.index);
  if (!target) return std::nullopt;
  cursor = next;
  return WaypointTileOf(*target);
}
}  // namespace VS::Battle::Ai
