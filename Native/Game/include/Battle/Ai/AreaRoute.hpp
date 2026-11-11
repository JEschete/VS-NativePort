#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/NavigationPrimitives.hpp"
#include "Field/PlatformPlacement.hpp"

// Routing an AI actor from one area of the room into a neighbouring one. The
// room holds a route word per area pair (area * 16 + neighbour): two packed
// endpoints, the first in the low halfword, each with tile z in bits 0-4, x
// in bits 5-9 and flags above. Routes through a moving platform use the
// room's eight route links: platform p owns links 2p and 2p + 1, and link i
// has a ground end (2i) and a platform end (2i + 1). The battle runtime owns
// the room, its maps and the actor AI state; these routines see them through
// the AreaRouteWorld host and the AreaRouteActor record.
namespace VS::Battle::Ai {
inline constexpr std::size_t kRouteGateLinkCount = kRouteGateEndCount / 2;
inline constexpr int kRouteGatePlatformCount = 4;
inline constexpr int kAreaRouteStepDirections = 4;
inline constexpr int kAreaRouteNeighbours = 16;
// The movement target that names no actor.
inline constexpr std::uint8_t kAreaRouteNoTarget = 16;

using RouteGateEnds = std::span<const RouteGateEnd, kRouteGateEndCount>;
// A route's two packed endpoints, the first stored in the low halfword.
using AreaRouteEndpoints = std::array<std::uint16_t, 2>;

// A packed tile word: x in byte 0, a tag in byte 1, z in byte 2 and a search
// status in byte 3.
inline constexpr unsigned kRouteTileZShift = 16;

[[nodiscard]] constexpr std::uint8_t RouteTileX(std::uint32_t tile) {
  return static_cast<std::uint8_t>(tile);
}
[[nodiscard]] constexpr std::uint8_t RouteTileZ(std::uint32_t tile) {
  return static_cast<std::uint8_t>(tile >> kRouteTileZShift);
}

// The partner link's platform end and the platform both links belong to.
struct RouteGateExit {
  std::uint8_t x = 0;
  std::uint8_t platform = 0;
  std::uint8_t z = 0;

  friend bool operator==(const RouteGateExit&, const RouteGateExit&) = default;
};

// decomp: func_800DF9A8 (src/BATTLE/BATTLE.PRG/771A8.c:459).
// The exit of the first link whose ground end is the entry tile: the platform
// end of the other link of the same platform. Nothing when no ground end is
// the entry tile; the original then copied the whole entry word.
[[nodiscard]] std::optional<RouteGateExit> ResolveRouteGateExit(
    RouteGateEnds ends, std::uint32_t entry_tile);

// decomp: func_800DFA54 (src/BATTLE/BATTLE.PRG/771A8.c:480).
// The ground end of the platform's link whose platform end is the tile under
// the world point (1/4096 units, 128 world units per tile), or of the
// platform's first link when neither is. Nothing refuses a platform outside
// 0..3, whose links the original read past the table.
[[nodiscard]] std::optional<RouteGateEnd> FindNearestRouteGateEntry(
    RouteGateEnds ends, int platform, std::int32_t world_x,
    std::int32_t world_z);

// decomp: func_800DFAF8 (src/BATTLE/BATTLE.PRG/771A8.c:498).
// The first link whose ground end (or platform end, without
// match_entry_end) is the tile while its platform end is the tile under the
// link platform's reported position, or -1. Positions are taken in report
// order, link i using position i / 2. A position past those reported never
// matches; the original compared uninitialised stack. Nothing refuses more
// than four positions, which overran the original's buffer.
[[nodiscard]] std::optional<int> FindRouteGateAtTile(
    RouteGateEnds ends, std::span<const Field::PlatformContact> positions,
    std::uint32_t tile, bool match_entry_end);

// decomp: routeWord (src/BATTLE/BATTLE.PRG/771A8.c:572).
// Both endpoints as one word, the first in the low halfword.
[[nodiscard]] std::uint32_t RouteEndpointPair(
    const AreaRouteEndpoints& endpoints);

// decomp: routeHalf (src/BATTLE/BATTLE.PRG/771A8.c:574).
// Endpoint 0 or 1. Nothing refuses another index.
[[nodiscard]] std::optional<std::uint16_t> RouteEndpointValue(
    const AreaRouteEndpoints& endpoints, int index);

// A point in world units.
struct AreaRoutePoint {
  std::int16_t x = 0;
  std::int16_t z = 0;
};

// The actor AI state fields area routing reads and writes.
struct AreaRouteActor {
  std::uint8_t target_distance_filter = 0;  // 0x05
  std::uint32_t tile = 0;                   // 0x34
  std::int16_t x = 0;                       // 0x4C
  std::int16_t z = 0;                       // 0x50
  std::uint8_t actor_id = 0;                // 0x88
  std::uint16_t turn_timer = 0;             // 0x13A
  std::uint16_t turn_period = 0;            // 0x13C
  std::uint8_t area = 0;                    // 0x154
  std::uint8_t movement_target = 0;         // 0x15C
  std::uint32_t destination = 0;            // 0x168
  std::uint32_t next_tile = 0;              // 0x170
  std::int32_t move_x = 0;                  // 0x17C
  std::int32_t move_z = 0;                  // 0x180
  std::int16_t heading = 0;                 // 0x188
  std::int16_t angle = 0;                   // 0x18A
  std::uint8_t route_gate = 0;              // 0x19B
  std::uint32_t route_exit = 0;             // 0x19C

  friend bool operator==(const AreaRouteActor&,
                         const AreaRouteActor&) = default;
};

// The room, its maps, platforms and actors as area routing sees them.
class AreaRouteWorld {
 public:
  virtual ~AreaRouteWorld() = default;
  // Route area * 16 + neighbour; nothing without room data.
  [[nodiscard]] virtual std::optional<AreaRouteEndpoints> Route(
      int index) const = 0;
  // Tile flag and occupancy height maps, each indexed below
  // kNavigationGridSide.
  [[nodiscard]] virtual bool HasTileMaps() const = 0;
  [[nodiscard]] virtual std::uint16_t TileFlags(int x, int z) const = 0;
  [[nodiscard]] virtual std::uint16_t TileHeight(int x, int z) const = 0;
  // The tile's blocked-direction bits; nothing outside the room's regions.
  [[nodiscard]] virtual std::optional<std::uint8_t> TerrainRegionCell(
      int x, int z) const = 0;
  [[nodiscard]] virtual NavigationDirectionCodes DirectionCodes() const = 0;
  // The direction of route step kind 0..3.
  [[nodiscard]] virtual std::uint8_t RouteStepDirection(int kind) const = 0;
  // Nothing without the room's route links.
  [[nodiscard]] virtual std::optional<
      std::array<RouteGateEnd, kRouteGateEndCount>>
  RouteGateEnds() const = 0;
  [[nodiscard]] virtual std::vector<Field::PlatformContact> PlatformPositions()
      const = 0;
  // Ticks the platform has waited at its stop; nothing while it moves.
  [[nodiscard]] virtual std::optional<std::uint16_t> PlatformDwellTicks(
      int platform) const = 0;
  // The room's closed route gates; nothing without room AI state.
  [[nodiscard]] virtual std::optional<std::uint8_t> ClosedGates() const = 0;
  virtual void SetContactOccupancy(int bit) = 0;
  // Whether the actor may step onto the tile: below zero when another actor
  // holds it, zero when it is blocked and above zero when it is free. The
  // query sees the actor as passed. Nothing refuses.
  [[nodiscard]] virtual std::optional<int> DestinationOccupancy(
      const AreaRouteActor& actor, std::uint32_t tile) = 0;
  [[nodiscard]] virtual bool HasActor(int slot) const = 0;
  // Nothing refuses an actor without a position.
  [[nodiscard]] virtual std::optional<AreaRoutePoint> ActorPosition(
      int slot) const = 0;
};

// decomp: routeTile (src/BATTLE/BATTLE.PRG/771A8.c:576).
// Nothing refuses a room without tile maps or a tile outside the grid.
[[nodiscard]] std::optional<std::uint16_t> NavigationTileFlags(
    const AreaRouteWorld& world, int z, int x);

// decomp: routeSetArea (src/BATTLE/BATTLE.PRG/771A8.c:581).
// Sets the actor's area to the tile's and asks to move to the destination.
// Nothing refuses as NavigationTileFlags.
[[nodiscard]] std::optional<CommandIntent> SetNavigationAreaFromTile(
    const AreaRouteWorld& world, AreaRouteActor& actor, std::uint32_t tile);

// decomp: func_800DFBCC (src/BATTLE/BATTLE.PRG/771A8.c:587).
// Moves the actor toward the route from `area` into neighbour `index`. An
// endpoint at the actor's tile steps along the route, boards an open platform
// gate or turns toward its target; otherwise the destination becomes the
// nearer usable endpoint and the actor turns once it stands there. Returns the
// command to issue. Nothing refuses an area or neighbour outside 0..15, a
// target slot above 16, a step direction outside 0..7, a tile outside the
// grid or anything the world refuses.
[[nodiscard]] std::optional<CommandIntent> ResolveNavigationAreaRoute(
    AreaRouteWorld& world, AreaRouteActor& actor, int index, int area);
}  // namespace VS::Battle::Ai
