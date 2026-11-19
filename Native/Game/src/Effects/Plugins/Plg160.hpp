#pragma once
// PLG160.BIN's module data (src/EFFECT/PLG160.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg160 {
// decomp: D_800FB31C (PLG160.BIN/0.c:5; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: D_800FB33C (PLG160.BIN/0.c:7; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg160
