// Hand-maintained (tools/port/owned.csv).
// C counterpart: rood-boi pc/src/battle_movement.c:663-673,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Field/TerrainHeight.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"

namespace VS::Field {
namespace {
constexpr int kTileShift = 7;
constexpr int kHeightShift = 6;
constexpr unsigned kLayerMask = 0x1C00;
constexpr int kLayerShift = 3;
constexpr unsigned kHeightMask = 0x7FFF;
constexpr unsigned kHeightSign = 0x4000;
constexpr int kHeightModulus = 0x8000;
constexpr unsigned kZeroGroundFlag = 0x2000;
constexpr int kSegmentSamples = 16;
constexpr int kSegmentStepShift = 4;

int UnwrappedHeight(unsigned cell) {
  const auto height = std::bit_cast<std::int16_t>(
      static_cast<std::uint16_t>(cell << kHeightShift));
  return height - static_cast<int>((cell & kLayerMask) >> kLayerShift);
}

int WrappedHeight(int unwrapped) {
  const auto value = static_cast<unsigned>(unwrapped) & kHeightMask;
  return (value & kHeightSign) != 0 ? static_cast<int>(value) - kHeightModulus
                                    : static_cast<int>(value);
}

int CheckedCoordinate(std::int64_t value) {
  if (!std::in_range<int>(value)) {
    Core_Debugf("ai", "Terrain segment coordinate overflows source arithmetic");
    std::abort();
  }
  return static_cast<int>(value);
}
}  // namespace

std::uint16_t TerrainGrid::Cell(int x, int z) const {
  if (!Contains(x, z)) {
    Core_Debugf("ai", "Terrain height requires a grid and valid tile (%d,%d)",
                x, z);
    std::abort();
  }
  return rows_[static_cast<std::size_t>(z)][static_cast<std::size_t>(x)];
}

// decomp: func_800E4320 (src/BATTLE/BATTLE.PRG/7B5DC.s:413).
int GroundHeightFlagsAtTile(TerrainGrid grid, int x, int z) {
  return UnwrappedHeight(grid.Cell(x, z));
}

// decomp: func_800E42EC (src/BATTLE/BATTLE.PRG/7B5DC.s:387).
int GroundHeightAtTile(TerrainGrid grid, int x, int z) {
  return WrappedHeight(GroundHeightFlagsAtTile(grid, x, z));
}

std::optional<int> TryGroundHeightAtTile(TerrainGrid grid, int x, int z) {
  if (!grid.Contains(x, z)) return std::nullopt;
  return GroundHeightAtTile(grid, x, z);
}

// decomp: func_800E4308 (src/BATTLE/BATTLE.PRG/7B5DC.s:399). The caller
// decodes the packed tile word with Core::Math::UnpackTileCoordinate.
int GroundHeightFlagsAtPackedTile(TerrainGrid grid,
                                  Core::Math::TileCoordinate tile) {
  return GroundHeightFlagsAtTile(grid, tile.x, tile.z);
}

// decomp: func_800E4318 (src/BATTLE/BATTLE.PRG/7B5DC.s:407).
int GroundHeightFlagsAtWorldPosition(TerrainGrid grid, int x, int z) {
  return GroundHeightFlagsAtTile(grid, x >> kTileShift, z >> kTileShift);
}

// decomp: func_800E42E0 (src/BATTLE/BATTLE.PRG/7B5DC.s:378).
int GroundHeightAtPackedTile(TerrainGrid grid,
                             Core::Math::TileCoordinate tile) {
  return WrappedHeight(GroundHeightFlagsAtPackedTile(grid, tile));
}

// decomp: func_800E42D4 (src/BATTLE/BATTLE.PRG/7B5DC.s:371).
int GroundHeightAtWorldPosition(TerrainGrid grid, int x, int z) {
  return WrappedHeight(GroundHeightFlagsAtWorldPosition(grid, x, z));
}

// func_800E4478, BATTLE/7B5DC.s:527; C counterpart battle_helpers.c:510.
bool IsPositionAboveGround(TerrainGrid grid, int x, int y, int z) {
  const unsigned cell = grid.Cell(x >> kTileShift, z >> kTileShift);
  const int ground =
      (cell & kZeroGroundFlag) != 0 ? 0 : WrappedHeight(UnwrappedHeight(cell));
  return y < ground;
}

// func_800E44E8, BATTLE/7B5DC.s:563; C counterpart battle_helpers.c:524.
bool IsTerrainSegmentBlocked(TerrainGrid grid, Core::Math::Position3 from,
                             Core::Math::Position3 to, int from_height,
                             int to_height) {
  int x = static_cast<std::uint16_t>(from.x);
  int y = CheckedCoordinate(static_cast<std::int64_t>(from.y) - from_height);
  int z = from.z;
  const int end_y =
      CheckedCoordinate(static_cast<std::int64_t>(to.y) - to_height);
  const int dx = (static_cast<int>(static_cast<std::uint16_t>(to.x)) - x) >>
                 kSegmentStepShift;
  const int dy = CheckedCoordinate(static_cast<std::int64_t>(end_y) - y) >>
                 kSegmentStepShift;
  const int dz = (static_cast<int>(to.z) - z) >> kSegmentStepShift;
  for (int sample = 0; sample < kSegmentSamples; ++sample) {
    x += dx;
    y = CheckedCoordinate(static_cast<std::int64_t>(y) + dy);
    z += dz;
    if (!IsPositionAboveGround(grid, x, y, z)) return true;
  }
  return false;
}
}  // namespace VS::Field
