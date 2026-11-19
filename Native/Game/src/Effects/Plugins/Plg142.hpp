#pragma once
// PLG142.BIN's module data (src/EFFECT/PLG142.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg142 {
// decomp: D_800FCDB8 (PLG142.BIN/18.c:8; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: D_800FCDD8 (PLG142.BIN/18.c:10; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg142
