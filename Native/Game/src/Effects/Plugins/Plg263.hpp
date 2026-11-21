#pragma once
// PLG263.BIN's module data (src/EFFECT/PLG263.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg263 {
// decomp: vs_effect_plg263_0_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FDCB0;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_plg263_1_colors (src/EFFECT/rings.h:20; was D_800FDF30;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FDF50 (PLG263.BIN/110.c:10; src/EFFECT/closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors2;
// decomp: vs_effect_plg263_3_origin_state (src/EFFECT/particleSetup.h:35; was
// D_800FDF70; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg263
