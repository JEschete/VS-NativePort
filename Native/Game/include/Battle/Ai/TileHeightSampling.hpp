#pragma once

#include <cstdint>
#include <functional>
#include <span>

#include "Core/Math/Distance.hpp"

// Chooses the navigation ground height of a room tile from a 4x4 lattice of
// floor samples. The floor query is passed in, so room building, tests and
// tools can sample any collision source without global room state.
namespace VS::Battle::Ai {
inline constexpr int kTileSampleCount = 16;

// Floor height at a world-space position in the collision query's encoding:
// bits 0-14 hold a signed 15-bit height and a negative value marks a position
// without floor.
using FloorHeightQuery = std::function<int(int world_x, int world_z)>;

// Highest valid sample (compared as signed 15-bit heights, starting from
// `center`). If any sample lacks floor and no fully floored 2x2 patch of the
// lattice exists, the original falls back to sample 10.
[[nodiscard]] int SelectTileHeight(
    std::span<const int, kTileSampleCount> samples, int center) noexcept;

// Samples `tile` at its center and on a 32-unit lattice, then selects its
// height (func_800E71DC).
[[nodiscard]] int SampleAiTileGroundHeight(const FloorHeightQuery& floor_height,
                                           Core::Math::TileCoordinate tile);
}  // namespace VS::Battle::Ai
