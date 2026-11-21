#pragma once
// PLG170.BIN's module data (src/EFFECT/PLG170.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg170 {
// decomp: vs_effect_retribution_0_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FE5C4;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: vs_effect_retribution_2_colors (src/EFFECT/rings.h:20; was
// D_800FE844; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FE864 (PLG170.BIN/18.c:16;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_retribution_5_colors (src/EFFECT/closedRings.h:58; was
// D_800FEAE4; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg170
