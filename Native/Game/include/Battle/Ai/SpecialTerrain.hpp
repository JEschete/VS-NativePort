#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Ai/NavigationPrimitives.hpp"
#include "Battle/Ai/TileHeightSampling.hpp"

// Special terrain heights: each connected region of special terrain tiles
// (tile flag 0x20) takes the floor height beside the first of its tiles that
// has open ground next to it. The battle runtime owns both maps; these
// routines see them as explicit spans indexed z * 32 + x.
namespace VS::Battle::Ai {
inline constexpr std::size_t kSpecialTerrainSide = 32;
inline constexpr std::size_t kSpecialTerrainCells = 1024;
static_assert(static_cast<int>(kSpecialTerrainSide) == kNavigationGridSide);
static_assert(kSpecialTerrainCells ==
              kSpecialTerrainSide * kSpecialTerrainSide);
inline constexpr std::uint16_t kSpecialTerrainTileFlag = 0x20;
// Tile flags that keep a neighbor from supplying its floor height.
inline constexpr std::uint16_t kGroundBlockingTileFlags = 0x70;
inline constexpr std::uint16_t kSpecialHeightAssignedFlag = 0x2000;
inline constexpr std::uint16_t kSpecialHeightValueMask = 0x3FF;
// Height bits a fill keeps: the assigned flag and the bits above it.
inline constexpr std::uint16_t kSpecialHeightKeptMask = 0xFC00;

using TileFlagCells = std::span<const std::uint16_t, kSpecialTerrainCells>;
using SpecialHeightCells = std::span<std::uint16_t, kSpecialTerrainCells>;

struct SpecialTerrainMaps {
  TileFlagCells tile_flags;
  SpecialHeightCells heights;
};

// decomp: func_800E5EC0 (src/BATTLE/BATTLE.PRG/7CD68.c:760).
// Stores the low ten bits of the height, marked assigned, in the tile and in
// every special tile connected to it through the even (orthogonal) direction
// steps whose height is not yet assigned. Other height bits are kept. False
// refuses a start tile outside the grid, which the original wrote unchecked.
bool FloodFillAiTerrainHeights(SpecialTerrainMaps maps,
                               const NavigationDirectionCodes& directions,
                               int x, int z, int height);

// decomp: func_800E5FDC (src/BATTLE/BATTLE.PRG/7CD68.c:779).
// The floor height at the centre of the first even-direction neighbor inside
// the grid without ground-blocking flags, as the original's 16-bit floor
// height shifted right by six; the height may be negative. Nothing when no
// neighbor qualifies or the floor query is empty.
[[nodiscard]] std::optional<std::int16_t> FindAdjacentAiGroundHeight(
    TileFlagCells tile_flags, const FloorHeightQuery& floor_height,
    const NavigationDirectionCodes& directions, int x, int z);

// decomp: func_800E6098 (src/BATTLE/BATTLE.PRG/7CD68.c:795).
// Scans rows z, then columns x, and fills each special tile whose height is
// not yet assigned from its adjacent ground height when that height is not
// negative. The scan order decides which height a region receives.
void BuildAiSpecialTerrainHeights(SpecialTerrainMaps maps,
                                  const FloorHeightQuery& floor_height,
                                  const NavigationDirectionCodes& directions);
}  // namespace VS::Battle::Ai
