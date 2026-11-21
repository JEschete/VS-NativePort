#pragma once
// PLG065.BIN's module data (src/EFFECT/PLG065.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg065 {
// decomp: vs_effect_gaeaStrikeLv1_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FD934; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_gaeaStrikeLv1_2_colors (src/EFFECT/rings.h:20; was
// D_800FD954; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_gaeaStrikeLv1_3_origin_state
// (src/EFFECT/particleSetup.h:35; was D_800FD974;
// src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg065
