#pragma once
// PLG272.BIN's module data (src/EFFECT/PLG272.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg272 {
// decomp: vs_effect_plg272_1_colors (src/EFFECT/closedRings.h:58; was
// D_800FBE70; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_plg272_2_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FBE90;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
}  // namespace VS::Effects::Plg272
