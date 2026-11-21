// PLG300.BIN's module data (src/EFFECT/PLG300.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg300.hpp"

namespace VS::Effects::Plg300 {
// decomp: D_800FE0CC (PLG300.BIN/110.c:5;
// src/EFFECT/sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: vs_effect_plg300_1_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FE34C; src/EFFECT/particleSetup.h:35)
std::int32_t particleEmissionCounter;
// decomp: D_800FE350 (PLG300.BIN/110.c:12; src/EFFECT/closedRings.h:58)
plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FE370 (PLG300.BIN/110.c:17; src/EFFECT/closedRings.h:58)
plugins::RingPaletteBytes ringColorFactors2;
}  // namespace VS::Effects::Plg300
