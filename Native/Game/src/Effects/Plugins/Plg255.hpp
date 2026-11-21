#pragma once
// PLG255.BIN's module data (src/EFFECT/PLG255.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

namespace VS::Effects::Plg255 {
// decomp: vs_effect_plg255_0_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FD0EC; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_plg255_1_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FD0F0; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg255
