#pragma once
// PLG148.BIN's module data (src/EFFECT/PLG148.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg148 {
// decomp: vs_effect_accursedUmbra_0_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FC8F8; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_accursedUmbra_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FC8FC; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg148
