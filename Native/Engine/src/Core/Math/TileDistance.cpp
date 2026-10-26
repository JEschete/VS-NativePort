// Hand-maintained (tools/port/owned.csv).
#include "Core/Math/TileDistance.h"

#include "Core/Math/ScalarMath.h"

namespace {
constexpr unsigned kCoordinateMask = 0xFF;
constexpr int kZShift = 16;
}  // namespace

// decomp: func_800E4604 (src/BATTLE/BATTLE.PRG/7BDD4.s:35).
// Former t0/t1 inputs are explicit; the shared SQR entry consumes signed16
// deltas.
unsigned int Core_Math_SquaredTileDistanceFromComponents(int first_x,
                                                         int first_z,
                                                         unsigned int second) {
  const int dx = Core_Math_WrapSigned16(static_cast<unsigned>(first_x) -
                                        (second & kCoordinateMask));
  const int dz = Core_Math_WrapSigned16(
      static_cast<unsigned>(first_z) - ((second >> kZShift) & kCoordinateMask));
  return Core_Math_SquaredVectorLength(dx, 0, dz);
}

// decomp: func_800E45F4 (src/BATTLE/BATTLE.PRG/7BDD4.s:23).
// C counterpart: rood-boi pc/src/battle_movement.c:153 (CC0-1.0).
unsigned int Core_Math_SquaredTileDistance(unsigned int first,
                                           unsigned int second) {
  return Core_Math_SquaredTileDistanceFromComponents(
      static_cast<int>(first & kCoordinateMask),
      static_cast<int>((first >> kZShift) & kCoordinateMask), second);
}
