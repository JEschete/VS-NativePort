#pragma once
// PLG081.BIN's module data (src/EFFECT/PLG081.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg081 {
// decomp: D_800FC24C (PLG081.BIN/0.c:7; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FC26C (PLG081.BIN/0.c:10; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg081
