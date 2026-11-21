#pragma once
// PLG150.BIN's module data (src/EFFECT/PLG150.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg150 {
// decomp: vs_effect_emeticBomb_0_colors (src/EFFECT/closedRings.h:58; was
// D_800FCA1C; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
// decomp: vs_effect_emeticBomb_2_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FCA3C;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_emeticBomb_3_colors (src/EFFECT/rings.h:20; was D_800FCCBC;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg150
