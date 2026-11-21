#pragma once
// PLG087.BIN's module data (src/EFFECT/PLG087.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg087 {
// decomp: vs_effect_apocalypse_0_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FF75C;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_apocalypse_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FF9DC; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_apocalypse_3_origin_state (src/EFFECT/trailEffect.h:70; was
// D_800FF9FC; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_apocalypse_4_origin_state (src/EFFECT/particleSetup.h:35;
// was D_800FFA00; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg087
