#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Legacy/Psx.hpp"

// The ABI-compatible entries of the target sphere's drawing (SphereDraw.hpp)
// over the live target sphere, the scratchpad working copies, the GTE, the
// frame's primitive buffer and ordering table and the room's collision map.
// Null or out-of-range inputs and a missing frame buffer are refused with a
// throttled diagnostic instead of being dereferenced.
namespace VS::Battle {
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveDrawSphereWireLine(VS::Core::Gte::SVECTOR* start,
                            VS::Core::Gte::SVECTOR* end, std::uint32_t color,
                            int intensity);
void LiveDrawSphere();
// Returns the scratchpad yaw basis matrix.
void* LiveBuildSphereCameraBasis();
void LiveDrawSpherePositionCursor(VS::Core::Gte::SVECTOR* position,
                                  int lock_pitch);
void LiveCacheSphereTerrainRow(VS::Core::Gte::SVECTOR* position,
                               std::uint32_t row);
}  // namespace VS::Battle
