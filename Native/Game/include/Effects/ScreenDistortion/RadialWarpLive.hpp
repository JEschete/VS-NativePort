#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Effects/ScreenDistortion/RadialWarp.hpp"

// Binding from the screen effect overlay to the typed radial warp.
namespace VS::Effects::ScreenDistortion {
// Warps the polygons queued in this frame's ordering table with the active
// screen effect overlay's warp words and the resident root table. Aborts with
// a diagnostic when the overlay or ordering table is missing, a link has no
// registered packet, or the ordering list is cyclic.
void WarpLivePolygonVertices();
}  // namespace VS::Effects::ScreenDistortion
