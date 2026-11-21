#pragma once
// PLG135.BIN's module data (src/EFFECT/PLG135.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg135 {
// decomp: vs_effect_rendingGale_0_colors (src/EFFECT/closedRings.h:58; was
// D_800FC96C; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_rendingGale_1_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FC98C; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
}  // namespace VS::Effects::Plg135
