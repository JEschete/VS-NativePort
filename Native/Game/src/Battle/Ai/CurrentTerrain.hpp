#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Field/TerrainHeight.hpp"

namespace VS::Battle::Ai {
// Terrain grid of the room the AI frame is updating; empty when the room has
// none.
[[nodiscard]] Field::TerrainGrid CurrentTerrain() noexcept;
}  // namespace VS::Battle::Ai
