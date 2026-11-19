#pragma once
// PLG300.BIN's module data (src/EFFECT/PLG300.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg300 {
// decomp: D_800FE0CC (PLG300.BIN/110.c:5; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: D_800FE34C (PLG300.BIN/110.c:7; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: D_800FE350 (PLG300.BIN/110.c:12; closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FE370 (PLG300.BIN/110.c:17; closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors2;
}  // namespace VS::Effects::Plg300
