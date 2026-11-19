#pragma once
// PLG128.BIN's module data (src/EFFECT/PLG128.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg128 {
// decomp: D_801004F0 (PLG128.BIN/F8.c:24; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_80100770 (PLG128.BIN/F8.c:27; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_80100790 (PLG128.BIN/F8.c:34; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices2;
// decomp: D_80100A10 (PLG128.BIN/F8.c:227; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg128
