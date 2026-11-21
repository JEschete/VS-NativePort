// PLG158.BIN's module data (src/EFFECT/PLG158.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg158.hpp"

namespace VS::Effects::Plg158 {
// decomp: D_800FCE38 (PLG158.BIN/0.c:6;
// src/EFFECT/sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_hexFlux_1_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FD0B8;
// src/EFFECT/sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: vs_effect_hexFlux_2_colors (src/EFFECT/closedRings.h:58; was
// D_800FD338; src/EFFECT/closedRings.h:58)
plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg158
