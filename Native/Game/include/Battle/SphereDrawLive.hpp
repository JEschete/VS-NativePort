#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.h"

// The ABI-compatible entries of the target sphere's drawing (SphereDraw.hpp)
// over the live target sphere, the scratchpad working copies, the GTE, the
// frame's primitive buffer and ordering table and the room's collision map.
// Null or out-of-range inputs and a missing frame buffer are refused with a
// throttled diagnostic instead of being dereferenced.
namespace VS::Battle {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawSphereWireLine(SVECTOR* start, SVECTOR* end, u_int color,
                            int intensity);
void LiveDrawSphere();
// Returns the scratchpad yaw basis matrix.
void* LiveBuildSphereCameraBasis();
void LiveDrawSpherePositionCursor(SVECTOR* position, int lock_pitch);
void LiveCacheSphereTerrainRow(SVECTOR* position, u_int row);
}  // namespace VS::Battle
