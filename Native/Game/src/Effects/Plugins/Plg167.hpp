#pragma once
// PLG167.BIN's module data (src/EFFECT/PLG167.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg167 {
// decomp: vs_effect_lotusPalm_0_colors (src/EFFECT/closedRings.h:58; was
// D_800FD824; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_lotusPalm_2_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FD844; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg167
