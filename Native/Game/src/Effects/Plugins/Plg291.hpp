#pragma once
// PLG291.BIN's module data (src/EFFECT/PLG291.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg291 {
// decomp: vs_effect_plg291_0_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FD4D8; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_plg291_2_colors (src/EFFECT/closedRings.h:58; was
// D_800FD4DC; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg291
