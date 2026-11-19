#pragma once
// PLG170.BIN's module data (src/EFFECT/PLG170.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg170 {
// decomp: D_800FE5C4 (PLG170.BIN/18.c:5; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: D_800FE844 (PLG170.BIN/18.c:11; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FE864 (PLG170.BIN/18.c:16; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FEAE4 (PLG170.BIN/18.c:19; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg170
