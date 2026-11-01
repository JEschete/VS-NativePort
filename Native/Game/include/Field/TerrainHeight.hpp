#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Distance.hpp"

// Room terrain height queries used by battle AI movement, flight and line of
// sight. Callers pass the grid they query and decoded coordinates, so nothing
// reads a hidden frame global, a packed register word or a PS1 address, and a
// mod or tool can query any grid it owns.
namespace VS::Field {
inline constexpr int kTerrainGridSide = 32;
using TerrainRow = std::uint16_t[kTerrainGridSide];

// Read-only view of a room's 32x32 terrain cells, indexed [z][x]. Each cell
// packs a signed height (bits 0-9, in 64-unit steps), a slope layer (bits
// 10-12) and a zero-ground flag (bit 13). A default grid is empty: rooms
// without terrain have none.
class TerrainGrid {
 public:
  constexpr TerrainGrid() noexcept = default;
  constexpr explicit TerrainGrid(
      std::span<const TerrainRow, kTerrainGridSide> rows) noexcept
      : rows_(rows) {}

  [[nodiscard]] constexpr bool empty() const noexcept { return rows_.empty(); }

  [[nodiscard]] constexpr bool Contains(int x, int z) const noexcept {
    return !rows_.empty() && x >= 0 && z >= 0 && x < kTerrainGridSide &&
           z < kTerrainGridSide;
  }

  // Raw cell word. Aborts with a diagnostic when the grid is empty or the
  // tile is outside it: the original read past the map instead.
  [[nodiscard]] std::uint16_t Cell(int x, int z) const;

 private:
  std::span<const TerrainRow> rows_;
};

// Unwrapped height: the signed 10-bit height times 64 minus 128 per slope
// layer, before the 15-bit wrap GroundHeightAtTile applies. The original names
// it the "flags" query; the zero-ground bit is not part of the value
// (func_800E4320).
[[nodiscard]] int GroundHeightFlagsAtTile(TerrainGrid grid, int x, int z);

// Same as GroundHeightFlagsAtTile for a decoded AI tile (func_800E4308).
[[nodiscard]] int GroundHeightFlagsAtPackedTile(
    TerrainGrid grid, Core::Math::TileCoordinate tile);

// Same as GroundHeightFlagsAtTile at a world-space position; one tile spans
// 128 units (func_800E4318).
[[nodiscard]] int GroundHeightFlagsAtWorldPosition(TerrainGrid grid, int x,
                                                   int z);

// Unwrapped height reduced to a signed 15-bit value (func_800E42EC).
[[nodiscard]] int GroundHeightAtTile(TerrainGrid grid, int x, int z);

// Ground height for a decoded AI tile (func_800E42E0).
[[nodiscard]] int GroundHeightAtPackedTile(TerrainGrid grid,
                                           Core::Math::TileCoordinate tile);

// Ground height at a world-space position (func_800E42D4).
[[nodiscard]] int GroundHeightAtWorldPosition(TerrainGrid grid, int x, int z);

// Ground height, or nothing when the grid is empty or the tile is outside it.
[[nodiscard]] std::optional<int> TryGroundHeightAtTile(TerrainGrid grid, int x,
                                                       int z);

// True when y is above the ground (smaller y is higher); zero-ground cells
// count as height 0 (func_800E4478).
[[nodiscard]] bool IsPositionAboveGround(TerrainGrid grid, int x, int y, int z);

// Samples the segment from `from` to `to`, each raised by its own height, in
// sixteen steps and reports whether any sample is at or below the ground
// (func_800E44E8). X is read as unsigned 16-bit, as the original did.
[[nodiscard]] bool IsTerrainSegmentBlocked(TerrainGrid grid,
                                           Core::Math::Position3 from,
                                           Core::Math::Position3 to,
                                           int from_height, int to_height);
}  // namespace VS::Field
