#pragma once
// PLG160.BIN's module data (src/EFFECT/PLG160.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg160 {
// decomp: vs_effect_scytheWind_0_colors (src/EFFECT/closedRings.h:58; was
// D_800FB31C; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: vs_effect_scytheWind_1_colors (src/EFFECT/rings.h:20; was D_800FB33C;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg160
