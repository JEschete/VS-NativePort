#pragma once
// PLG272.BIN's module data (src/EFFECT/PLG272.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg272 {
// decomp: D_800FBE70 (PLG272.BIN/18.c:8; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FBE90 (PLG272.BIN/18.c:10; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
}  // namespace VS::Effects::Plg272
