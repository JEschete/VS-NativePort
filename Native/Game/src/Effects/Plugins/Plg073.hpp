#pragma once
// PLG073.BIN's module data (src/EFFECT/PLG073.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg073 {
// decomp: vs_effect_radialSurgeLv1_0_colors (src/EFFECT/closedRings.h:58; was
// D_800FEF3C; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: vs_effect_radialSurgeLv1_1_colors (src/EFFECT/rings.h:20; was
// D_800FEF5C; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_radialSurgeLv1_2_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FEF7C; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg073
