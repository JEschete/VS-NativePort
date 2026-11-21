#pragma once
// PLG138.BIN's module data (src/EFFECT/PLG138.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg138 {
// decomp: vs_effect_papillonReel_1_colors (src/EFFECT/rings.h:20; was
// D_800FCD3C; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_papillonReel_3_colors (src/EFFECT/closedRings.h:58; was
// D_800FCD5C; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg138
