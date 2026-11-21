#pragma once
// PLG161.BIN's module data (src/EFFECT/PLG161.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg161 {
// decomp: vs_effect_gigaTempest_1_colors (src/EFFECT/rings.h:20; was
// D_800FEC68; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_gigaTempest_2_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FEC88; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_gigaTempest_4_colors (src/EFFECT/rings.h:20; was
// D_800FEC8C; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors2;
}  // namespace VS::Effects::Plg161
