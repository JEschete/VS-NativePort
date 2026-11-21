#pragma once
// PLG134.BIN's module data (src/EFFECT/PLG134.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg134 {
// decomp: vs_effect_wyrmScorn_0_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FC96C; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_wyrmScorn_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FC970; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg134
