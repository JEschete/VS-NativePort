// PLG146.BIN's module data (src/EFFECT/PLG146.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/Plg146.hpp"

namespace VS::Effects::Plg146 {
// decomp: D_800FF87C (PLG146.BIN/1D8.c:5; src/EFFECT/closedRings.h:58)
plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FF89C (PLG146.BIN/1D8.c:7;
// src/EFFECT/sphericalSurfaceVertices.h:21)
plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: vs_effect_blackNebula_2_origin_state (src/EFFECT/particleSetup.h:35;
// was D_800FFB1C; src/EFFECT/particleSetup.h:35)
std::int32_t particleEmissionCounter;
// decomp: vs_effect_blackNebula_3_colors (src/EFFECT/rings.h:20; was
// D_800FFB20; src/EFFECT/rings.h:20)
plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_blackNebula_4_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FFB40; src/EFFECT/trailEffect.h:70)
std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg146
