#pragma once
// PLG169.BIN's module data (src/EFFECT/PLG169.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg169 {
// decomp: vs_effect_vermillionAura_0_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FD590; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_vermillionAura_1_colors (src/EFFECT/rings.h:20; was
// D_800FD594; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_vermillionAura_2_colors (src/EFFECT/closedRings.h:58; was
// D_800FD5B4; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg169
