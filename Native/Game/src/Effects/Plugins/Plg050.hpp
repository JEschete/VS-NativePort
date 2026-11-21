#pragma once
// PLG050.BIN's module data (src/EFFECT/PLG050.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg050 {
// decomp: vs_effect_darkChant_0_colors (src/EFFECT/rings.h:20; was D_800FC624;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_darkChant_1_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FC644; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg050
