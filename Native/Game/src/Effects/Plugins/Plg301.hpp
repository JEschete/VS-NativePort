#pragma once
// PLG301.BIN's module data (src/EFFECT/PLG301.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg301 {
// decomp: vs_effect_plg301_0_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FC524; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: vs_effect_plg301_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FC528; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg301
