#pragma once
// PLG061.BIN's module data (src/EFFECT/PLG061.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg061 {
// decomp: vs_effect_flameSphereLv1_0_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FCA90;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_flameSphereLv1_2_colors (src/EFFECT/rings.h:20; was
// D_800FCD10; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_flameSphereLv1_3_colors (src/EFFECT/closedRings.h:58; was
// D_800FCD30; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg061
