// Hand-maintained (tools/port/owned.csv).
// Live binding for the special terrain heights (SpecialTerrain.cpp).
#include "Battle/Ai/SpecialTerrainLive.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "Battle/Ai/LiveRequire.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Core/Diagnostics.hpp"
#include "Field/CollisionMapLive.hpp"

namespace VS::Battle::Ai {
namespace {
using LiveMapRow = std::remove_pointer_t<decltype(VS::Battle::Ai::tileFlagMap)>;
static_assert(std::is_same_v<LiveMapRow, std::uint16_t[kNavigationGridSide]>);
static_assert(std::is_same_v<decltype(VS::Battle::Ai::specialTerrainHeights),
                             decltype(VS::Battle::Ai::tileFlagMap)>);
static_assert(sizeof(LiveMapRow) * kNavigationGridSide ==
              kSpecialTerrainCells * sizeof(std::uint16_t));
static_assert(sizeof(VS::Battle::Ai::directionDeltas) ==
              sizeof(NavigationDirectionCodes));

SpecialTerrainMaps LiveMaps() {
  Require(VS::Battle::Ai::tileFlagMap != nullptr &&
              VS::Battle::Ai::specialTerrainHeights != nullptr,
          "Special terrain heights require the room's AI tile maps");
  return {
      .tile_flags = TileFlagCells(&VS::Battle::Ai::tileFlagMap[0][0],
                                  kSpecialTerrainCells),
      .heights = SpecialHeightCells(
          &VS::Battle::Ai::specialTerrainHeights[0][0], kSpecialTerrainCells)};
}

NavigationDirectionCodes LiveDirections() {
  NavigationDirectionCodes directions{};
  std::memcpy(directions.data(), VS::Battle::Ai::directionDeltas.data(),
              directions.size());
  return directions;
}

int LiveFloorHeight(int world_x, int world_z) {
  return Field::LiveFloorHeightAt(world_x, world_z);
}
}  // namespace

// decomp: BATTLE.PRG func_800E5EC0 (src/BATTLE/BATTLE.PRG/7CD68.c:760).
void FloodFillLiveAiTerrainHeights(int x, int z, int height) {
  Require(FloodFillAiTerrainHeights(LiveMaps(), LiveDirections(), x, z, height),
          "Special terrain fill refused its start tile");
}

// decomp: BATTLE.PRG func_800E5FDC (src/BATTLE/BATTLE.PRG/7CD68.c:779).
int FindLiveAdjacentAiGroundHeight(int x, int z) {
  return FindAdjacentAiGroundHeight(LiveMaps().tile_flags, LiveFloorHeight,
                                    LiveDirections(), x, z)
      .value_or(-1);
}

// decomp: BATTLE.PRG func_800E6098 (src/BATTLE/BATTLE.PRG/7CD68.c:795).
void BuildLiveAiSpecialTerrainHeights() {
  BuildAiSpecialTerrainHeights(LiveMaps(), LiveFloorHeight, LiveDirections());
}
}  // namespace VS::Battle::Ai
