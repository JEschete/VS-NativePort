#pragma once
// PLG166.BIN's module data (src/EFFECT/PLG166.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg166 {
// decomp: D_800FCEC0 (PLG166.BIN/0.c:7; sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: D_800FD140 (PLG166.BIN/0.c:10; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FD160 (PLG166.BIN/0.c:13; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg166
