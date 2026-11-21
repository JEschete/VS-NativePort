#pragma once
// PLG147.BIN's module data (src/EFFECT/PLG147.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg147 {
// decomp: vs_effect_bearClaw_0_colors (src/EFFECT/rings.h:20; was D_800FBB10;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_bearClaw_1_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FBB30; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg147
