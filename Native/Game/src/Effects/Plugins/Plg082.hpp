#pragma once
// PLG082.BIN's module data (src/EFFECT/PLG082.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg082 {
// decomp: vs_effect_thunderbolt_0_origin_state (src/EFFECT/particleSetup.h:35;
// was D_800FD088; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: vs_effect_thunderbolt_2_colors (src/EFFECT/closedRings.h:58; was
// D_800FD08C; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg082
