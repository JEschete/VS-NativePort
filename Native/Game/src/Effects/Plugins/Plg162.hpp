#pragma once
// PLG162.BIN's module data (src/EFFECT/PLG162.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg162 {
// decomp: D_800FD18C (PLG162.BIN/0.c:6; src/EFFECT/closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_spiralScourge_2_colors (src/EFFECT/rings.h:20; was
// D_800FD1AC; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors2;
// decomp: D_800FD1CC (PLG162.BIN/0.c:13; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg162
