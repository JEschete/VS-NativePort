#pragma once
// PLG150.BIN's module data (src/EFFECT/PLG150.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg150 {
// decomp: D_800FCA1C (PLG150.BIN/18.c:5; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FCA3C (PLG150.BIN/18.c:9; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FCCBC (PLG150.BIN/18.c:12; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg150
