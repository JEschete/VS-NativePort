// The room navigation bindings (Navigation.hpp).
#include "Battle/Ai/Navigation.hpp"

namespace VS::Battle::Ai {
// decomp: D_800F16EC (D_800F16EC@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<DirectionDelta, kDirectionDeltaCount> directionDeltas;
// decomp: D_800F175C (D_800F175C@BATTLE/BATTLE.PRG); contents: module image
// (ModuleData.cpp)
std::array<std::uint8_t, kRouteStepDirectionCount> routeStepDirections;
// decomp: D_800F58B8 (D_800F58B8@BATTLE/BATTLE.PRG)
std::uint16_t (*tileFlagMap)[kNavigationGridSide];
// decomp: D_800F58C8 (D_800F58C8@BATTLE/BATTLE.PRG)
std::uint16_t* roomWaypoints;
// decomp: D_800F58D0 (D_800F58D0@BATTLE/BATTLE.PRG)
std::uint16_t (*specialTerrainHeights)[kNavigationGridSide];
// decomp: D_800F5910 (D_800F5910@BATTLE/BATTLE.PRG)
RouteGateEndRecord* routeGates;

std::array<RouteGateEnd, kRouteGateEndCount> DecodeRouteGateEnds(
    std::span<const RouteGateEndRecord, kRouteGateEndCount> records) {
  std::array<RouteGateEnd, kRouteGateEndCount> ends{};
  for (std::size_t index = 0; index < ends.size(); ++index) {
    ends[index] = {.x = static_cast<std::uint8_t>(records[index].tile_x),
                   .z = static_cast<std::uint8_t>(records[index].tile_z)};
  }
  return ends;
}
}  // namespace VS::Battle::Ai
