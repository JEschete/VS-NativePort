// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/SpecialTerrain.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
namespace {
constexpr std::size_t kGridSide = kNavigationGridSide;
constexpr std::size_t kSteppedDirections = 4;
constexpr std::size_t kSteppedDirectionStride = 2;
constexpr unsigned kDirectionComponentMask = 3;
constexpr unsigned kDirectionSignBit = 2;
constexpr int kDirectionComponentModulus = 4;
constexpr unsigned kDirectionZShift = 4;
constexpr int kFineTileShift = 7;
constexpr int kFineTileCentre = 0x40;
constexpr int kFloorHeightShift = 6;

struct TileStep {
  int dx = 0;
  int dz = 0;
};

// Each component is a two-bit two's-complement value, -2..1.
int DirectionComponent(unsigned code) {
  const auto value = code & kDirectionComponentMask;
  return (value & kDirectionSignBit) != 0
             ? static_cast<int>(value) - kDirectionComponentModulus
             : static_cast<int>(value);
}

TileStep SteppedDirection(const NavigationDirectionCodes& directions,
                          std::size_t step) {
  const unsigned code = directions[step * kSteppedDirectionStride];
  return {DirectionComponent(code),
          DirectionComponent(code >> kDirectionZShift)};
}

// The original compared the coordinates unsigned, so negative ones are
// outside the grid too.
std::optional<std::size_t> CellOf(int x, int z) {
  if (x < 0 || z < 0 || x >= kNavigationGridSide || z >= kNavigationGridSide) {
    return std::nullopt;
  }
  return (static_cast<std::size_t>(z) * kGridSide) +
         static_cast<std::size_t>(x);
}

bool AwaitsHeight(SpecialTerrainMaps maps, std::size_t cell) {
  return (maps.tile_flags[cell] & kSpecialTerrainTileFlag) != 0 &&
         (maps.heights[cell] & kSpecialHeightAssignedFlag) == 0;
}

void AssignHeight(SpecialTerrainMaps maps, std::size_t cell,
                  std::uint16_t assigned) {
  maps.heights[cell] = static_cast<std::uint16_t>(
      (maps.heights[cell] & kSpecialHeightKeptMask) | assigned);
}
}  // namespace

// decomp: func_800E5EC0 (src/BATTLE/BATTLE.PRG/7CD68.c:760).
// The original recursed once per tile, up to 1024 deep. Every tile is
// assigned the same value and marked before its neighbors are examined, so
// marking tiles as they are queued assigns exactly the same tiles.
bool FloodFillAiTerrainHeights(SpecialTerrainMaps maps,
                               const NavigationDirectionCodes& directions,
                               int x, int z, int height) {
  const auto start = CellOf(x, z);
  if (!start) {
    Core_Warnf("ai", "Special terrain fill from (%d,%d) is outside the grid", x,
               z);
    return false;
  }
  const auto assigned = static_cast<std::uint16_t>(
      (static_cast<unsigned>(height) & kSpecialHeightValueMask) |
      kSpecialHeightAssignedFlag);
  std::array<std::size_t, kSpecialTerrainCells> queued{};
  std::size_t count = 0;
  AssignHeight(maps, *start, assigned);
  queued.at(count++) = *start;
  while (count > 0) {
    const auto cell = queued.at(--count);
    const auto cell_x = static_cast<int>(cell % kGridSide);
    const auto cell_z = static_cast<int>(cell / kGridSide);
    for (std::size_t step = 0; step < kSteppedDirections; ++step) {
      const auto delta = SteppedDirection(directions, step);
      const auto next = CellOf(cell_x + delta.dx, cell_z + delta.dz);
      if (!next || !AwaitsHeight(maps, *next)) continue;
      AssignHeight(maps, *next, assigned);
      queued.at(count++) = *next;
    }
  }
  return true;
}

// decomp: func_800E5FDC (src/BATTLE/BATTLE.PRG/7CD68.c:779).
std::optional<std::int16_t> FindAdjacentAiGroundHeight(
    TileFlagCells tile_flags, const FloorHeightQuery& floor_height,
    const NavigationDirectionCodes& directions, int x, int z) {
  if (!floor_height) {
    Core_Warnf("ai", "Adjacent ground height requires a floor query");
    return std::nullopt;
  }
  for (std::size_t step = 0; step < kSteppedDirections; ++step) {
    const auto delta = SteppedDirection(directions, step);
    const auto next_x = x + delta.dx;
    const auto next_z = z + delta.dz;
    const auto cell = CellOf(next_x, next_z);
    if (!cell || (tile_flags[*cell] & kGroundBlockingTileFlags) != 0) continue;
    // The original kept the floor height's low 16 bits, then shifted them
    // arithmetically.
    const auto floor = static_cast<std::int16_t>(
        floor_height((next_x << kFineTileShift) | kFineTileCentre,
                     (next_z << kFineTileShift) | kFineTileCentre));
    return static_cast<std::int16_t>(floor >> kFloorHeightShift);
  }
  return std::nullopt;
}

// decomp: func_800E6098 (src/BATTLE/BATTLE.PRG/7CD68.c:795).
void BuildAiSpecialTerrainHeights(SpecialTerrainMaps maps,
                                  const FloorHeightQuery& floor_height,
                                  const NavigationDirectionCodes& directions) {
  for (int z = 0; z < kNavigationGridSide; ++z) {
    for (int x = 0; x < kNavigationGridSide; ++x) {
      const auto cell = (static_cast<std::size_t>(z) * kGridSide) +
                        static_cast<std::size_t>(x);
      if (!AwaitsHeight(maps, cell)) continue;
      const auto height = FindAdjacentAiGroundHeight(
          maps.tile_flags, floor_height, directions, x, z);
      if (height && *height >= 0) {
        FloodFillAiTerrainHeights(maps, directions, x, z, *height);
      }
    }
  }
}
}  // namespace VS::Battle::Ai
