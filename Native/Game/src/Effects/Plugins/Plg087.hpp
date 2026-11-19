#pragma once
// PLG087.BIN's module data (src/EFFECT/PLG087.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg087 {
// decomp: D_800FF75C (PLG087.BIN/1F0.c:6; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FF9DC (PLG087.BIN/1F0.c:9; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FF9FC (PLG087.BIN/1F0.c:13; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FFA00 (PLG087.BIN/1F0.c:16; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg087
