#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/SpecialTerrain.hpp"

// Binding from the room's AI tile flag and special terrain height maps, the
// AI direction table and the room floor collision to the typed special
// terrain heights (SpecialTerrain.cpp). Each aborts with a diagnostic while
// either map is missing. The battle module's pointer table stores these
// bindings at their original addresses.
namespace VS::Battle::Ai {
// Fills the tile's special region with `height` (func_800E5EC0). Aborts on a
// start tile outside the grid.
void FloodFillLiveAiTerrainHeights(int x, int z, int height);

// The adjacent ground height of the tile, or -1 when no neighbor qualifies
// (func_800E5FDC). A found height may itself be negative, which the builder
// treats as none, as the original did.
int FindLiveAdjacentAiGroundHeight(int x, int z);

// Builds every special terrain height of the room (func_800E6098).
void BuildLiveAiSpecialTerrainHeights();
}  // namespace VS::Battle::Ai
