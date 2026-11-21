#pragma once
// PLG265.BIN's module data (src/EFFECT/PLG265.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg265 {
// decomp: D_800FFD98 (PLG265.BIN/2E8.c:5; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: vs_effect_plg265_1_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FFDB8; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
// decomp: vs_effect_plg265_2_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FFDBC; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_plg265_3_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FFDC0; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter2;
}  // namespace VS::Effects::Plg265
