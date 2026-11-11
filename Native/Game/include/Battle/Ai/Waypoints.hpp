#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/ActorSettings.hpp"
#include "Core/Math/Distance.hpp"
#include "Field/TerrainHeight.hpp"

// Room waypoints and the patrols that walk them. A waypoint halfword packs a
// tile (x in bits 0-4, z in bits 5-9) and an active flag in bit 15. The room
// holds eight waypoints and patrol route rows of eight waypoints each. The
// battle runtime owns the room data and the actor AI state; these routines
// see one row, the actor's patrol fields and the special terrain heights, so
// they reach no frame global or PS1 address.
namespace VS::Battle::Ai {
// Each patrol route row holds eight waypoint halfwords (D_800F58D8).
inline constexpr std::size_t kPatrolWaypointsPerRow = 8;
inline constexpr std::uint16_t kWaypointActive = 0x8000;
inline constexpr unsigned kWaypointCoordinateMask = 31;
inline constexpr unsigned kWaypointZShift = 5;

// A waypoint's tile coordinates.
struct WaypointTile {
  std::uint8_t x = 0;
  std::uint8_t z = 0;

  friend bool operator==(const WaypointTile&, const WaypointTile&) = default;
};

[[nodiscard]] constexpr bool IsActiveWaypoint(std::uint16_t waypoint) {
  return (waypoint & kWaypointActive) != 0;
}

[[nodiscard]] constexpr WaypointTile WaypointTileOf(std::uint16_t waypoint) {
  return {.x = static_cast<std::uint8_t>(waypoint & kWaypointCoordinateMask),
          .z = static_cast<std::uint8_t>((waypoint >> kWaypointZShift) &
                                         kWaypointCoordinateMask)};
}

// decomp: nearestWaypointValue (src/BATTLE/BATTLE.PRG/74088.c:946) and
// patrolWaypoint (src/BATTLE/BATTLE.PRG/771A8.c:1857).
// The waypoint at `index`. Nothing refuses an index outside the row, which
// the original read past.
[[nodiscard]] std::optional<std::uint16_t> WaypointValue(
    std::span<const std::uint16_t> row, int index);

// decomp: nearestWaypointAddress (src/BATTLE/BATTLE.PRG/74088.c:952).
// The waypoints from `index` to the end of the row, the typed form of the
// original's element address. Nothing refuses an index outside 0..row size.
[[nodiscard]] std::optional<std::span<const std::uint16_t>> WaypointsFrom(
    std::span<const std::uint16_t> row, int index);

// The nearest active waypoint and its GTE squared length.
struct NearestWaypoint {
  WaypointTile tile;
  std::uint32_t squared_distance = 0;

  friend bool operator==(const NearestWaypoint&,
                         const NearestWaypoint&) = default;
};

// A waypoint search's answer: nothing inside when no waypoint is active.
struct WaypointSearch {
  std::optional<NearestWaypoint> nearest;
};

// decomp: func_800DD7CC (src/BATTLE/BATTLE.PRG/74088.c:957).
// The active waypoint whose tile centre, at the special terrain's ground
// height there, is nearest the position. Distances are SquaredVectorLength
// (each delta reduced to 16 bits) compared unsigned; the first of equally
// near waypoints wins. Nothing refuses an active waypoint without special
// terrain heights.
[[nodiscard]] std::optional<WaypointSearch> FindNearestActiveWaypoint(
    std::span<const std::uint16_t, kRoomWaypointCount> waypoints,
    Core::Math::Position3 position, Field::TerrainGrid special_heights);

// An actor's patrol fields (AI state 0x100-0x104).
struct PatrolCursor {
  std::uint8_t index = 0;      // 0x100
  std::uint8_t row = 0;        // 0x101
  std::uint8_t rate = 0;       // 0x102: 1 in 2^rate draws may turn back
  std::uint8_t direction = 0;  // 0x103: the waypoint step, a signed byte
  std::uint8_t cooldown = 0;   // 0x104

  friend bool operator==(const PatrolCursor&, const PatrolCursor&) = default;
};

// The actor random stream the patrol draws from.
class PatrolRandom {
 public:
  virtual ~PatrolRandom() = default;
  // A draw masked by range - 1. The range is a power of two read as the
  // original's signed word, so rate 31 passes INT_MIN.
  virtual int NextMasked(int range) = 0;
};

// decomp: func_800E23AC (src/BATTLE/BATTLE.PRG/771A8.c:1863).
// Walks the cursor's route row. At the current waypoint's tile the cursor
// steps by its direction to the next active waypoint and waits ten checks.
// Elsewhere the cooldown counts down, a value below 2^rate is drawn and the
// actor keeps walking unless it is zero and the cooldown is over; the
// direction then reverses and the cursor steps. Returns the waypoint tile to
// walk to.
// Nothing refuses a row outside the routes, an index outside the row or a
// step that reaches no active waypoint, where the original looped forever;
// the cursor is then unchanged.
[[nodiscard]] std::optional<WaypointTile> UpdateWaypointPatrol(
    PatrolCursor& cursor, std::span<const std::uint16_t> routes,
    WaypointTile tile, PatrolRandom& random);
}  // namespace VS::Battle::Ai
