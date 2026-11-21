#pragma once
// PLG049.BIN's module data (src/EFFECT/PLG049.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg049 {
// decomp: vs_spiritSurge_0_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FE3D4; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_spiritSurge_1_colors (src/EFFECT/rings.h:20; was D_800FE3D8;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_spiritSurge_2_colors (src/EFFECT/closedRings.h:58; was D_800FE3F8;
// src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg049
