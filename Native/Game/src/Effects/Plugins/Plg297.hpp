#pragma once
// PLG297.BIN's module data (src/EFFECT/PLG297.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg297 {
// decomp: vs_effect_plg297_0_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FCB70;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: vs_effect_plg297_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FCDF0; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_plg297_3_colors (src/EFFECT/rings.h:20; was D_800FCE10;
// src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg297
