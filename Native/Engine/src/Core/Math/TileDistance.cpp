// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/TileDistance.h"

namespace {
constexpr unsigned kCoordinateMask = 0xFF;
constexpr int kZShift = 16;
}  // namespace

// decomp: func_800E45F4 (src/BATTLE/BATTLE.PRG/7BDD4.s:23).
// C counterpart: rood-boi pc/src/battle_movement.c:153 (CC0-1.0).
unsigned int Core_Math_SquaredTileDistance(unsigned int first,
                                           unsigned int second) {
  const int dx = static_cast<int>(first & kCoordinateMask) -
                 static_cast<int>(second & kCoordinateMask);
  const int dz = static_cast<int>((first >> kZShift) & kCoordinateMask) -
                 static_cast<int>((second >> kZShift) & kCoordinateMask);
  return static_cast<unsigned>(dx * dx + dz * dz);
}
