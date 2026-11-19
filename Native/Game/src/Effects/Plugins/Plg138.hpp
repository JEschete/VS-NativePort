#pragma once
// PLG138.BIN's module data (src/EFFECT/PLG138.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg138 {
// decomp: D_800FCD3C (PLG138.BIN/18.c:7; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FCD5C (PLG138.BIN/18.c:11; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg138
