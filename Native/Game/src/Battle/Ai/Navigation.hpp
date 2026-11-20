#pragma once
// The packed navigation records and the room navigation bindings: the eight
// direction deltas, the route step directions, the tile flag and special
// terrain height maps, the room waypoints and the route gates.
// Navigation.cpp defines the globals.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Battle/Ai/NavigationPrimitives.hpp"

namespace VS::Battle::Ai {

// decomp: D_800F16EC_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:64)
struct DirectionDelta {
  std::int8_t dx : 2;
  std::int8_t reserved_bits2 : 2;
  std::int8_t dz : 2;
  std::int8_t reserved_bits6 : 2;
};
static_assert(sizeof(DirectionDelta) == 0x1,
              "DirectionDelta must keep its PS1 layout");

// The on-disc route gate end; RouteGateEnd (NavigationPrimitives.hpp) is the
// typed tile it decodes to.
// decomp: D_800F5910_t (src/BATTLE/BATTLE.PRG/6E4CCShared.h:12)
struct RouteGateEndRecord {
  std::uint16_t tile_x : 5;
  std::uint16_t tile_z : 5;
  std::uint16_t reserved_bits10 : 6;
};
static_assert(sizeof(RouteGateEndRecord) == 0x2,
              "RouteGateEndRecord must keep its PS1 layout");

// decomp: vs_TilePosition (src/include/tilePosition.h:4)
union TilePosition {
  std::uint32_t word;
  struct {
    std::uint32_t tile_x : 8, tag : 8, tile_z : 8, search_status : 8;
  } tile;
};
static_assert(sizeof(TilePosition) == sizeof(std::uint32_t),
              "TilePosition must keep its PS1 layout");

inline constexpr std::size_t kDirectionDeltaCount = 8;
inline constexpr std::size_t kRouteStepDirectionCount = 272;

// decomp: D_800F16EC (src/BATTLE/BATTLE.PRG/7CD68.c:65)
extern std::array<DirectionDelta, kDirectionDeltaCount> directionDeltas;
// decomp: D_800F175C (src/BATTLE/BATTLE.PRG/770A4.c:23)
extern std::array<std::uint8_t, kRouteStepDirectionCount> routeStepDirections;
// decomp: D_800F58B8 (src/BATTLE/BATTLE.PRG/7CD68.c:70)
extern std::uint16_t (*tileFlagMap)[kNavigationGridSide];
// decomp: D_800F58C8 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:524)
extern std::uint16_t* roomWaypoints;
// decomp: D_800F58D0 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:525)
extern std::uint16_t (*specialTerrainHeights)[kNavigationGridSide];
// decomp: D_800F5910 (src/BATTLE/BATTLE.PRG/6E4CCShared.h:535)
extern RouteGateEndRecord* routeGates;

// Decodes the room's on-disc route gate ends into typed tiles.
[[nodiscard]] std::array<RouteGateEnd, kRouteGateEndCount> DecodeRouteGateEnds(
    std::span<const RouteGateEndRecord, kRouteGateEndCount> records);

}  // namespace VS::Battle::Ai
