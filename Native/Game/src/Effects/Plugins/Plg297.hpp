#pragma once
// PLG297.BIN's module data (src/EFFECT/PLG297.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg297 {
// decomp: D_800FCB70 (PLG297.BIN/18.c:5; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: D_800FCDF0 (PLG297.BIN/18.c:8; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FCE10 (PLG297.BIN/18.c:13; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg297
