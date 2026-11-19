#pragma once
// PLG263.BIN's module data (src/EFFECT/PLG263.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg263 {
// decomp: D_800FDCB0 (PLG263.BIN/110.c:5; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FDF30 (PLG263.BIN/110.c:7; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FDF50 (PLG263.BIN/110.c:10; closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors2;
// decomp: D_800FDF70 (PLG263.BIN/110.c:12; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg263
