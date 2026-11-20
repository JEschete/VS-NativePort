#pragma once

#include "Core/Gte.hpp"

namespace VS::Battle {
// Applies DrawSphereTerrainIntersection to the active target sphere, projecting
// through the current GTE state and linking translucent line packets into the
// frame ordering table. Throws std::runtime_error without an active sphere or
// frame buffers.
void DrawLiveSphereTerrainIntersection(const VS::Core::Gte::SVECTOR& position,
                                       const VS::Core::Gte::MATRIX& shape,
                                       int scale);
}  // namespace VS::Battle
