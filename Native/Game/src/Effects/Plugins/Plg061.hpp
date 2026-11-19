#pragma once
// PLG061.BIN's module data (src/EFFECT/PLG061.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg061 {
// decomp: D_800FCA90 (PLG061.BIN/18.c:5; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FCD10 (PLG061.BIN/18.c:9; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FCD30 (PLG061.BIN/18.c:11; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg061
