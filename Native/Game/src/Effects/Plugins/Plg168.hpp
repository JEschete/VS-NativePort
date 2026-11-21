#pragma once
// PLG168.BIN's module data (src/EFFECT/PLG168.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

namespace VS::Effects::Plg168 {
// decomp: vs_effect_vertigo_0_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FDC54; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_vertigo_1_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FDC58; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg168
