// PLG263.BIN's module data (src/EFFECT/PLG263.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg263.hpp"

namespace VS::Effects::Plg263 {
// decomp: vs_effect_plg263_0_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FDCB0;
// src/EFFECT/sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_plg263_1_colors (src/EFFECT/rings.h:20; was D_800FDF30;
// src/EFFECT/rings.h:20)
plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FDF50 (PLG263.BIN/110.c:10; src/EFFECT/closedRings.h:58)
plugins::RingPaletteBytes ringColorFactors2;
// decomp: vs_effect_plg263_3_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FDF70; src/EFFECT/particleSetup.h:35)
std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg263
