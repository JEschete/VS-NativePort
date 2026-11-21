#pragma once
// PLG086.BIN's module data (src/EFFECT/PLG086.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg086 {
// decomp: vs_effect_judgment_0_colors (src/EFFECT/closedRings.h:58; was
// D_800FE390; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_judgment_3_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FE3B0; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg086
