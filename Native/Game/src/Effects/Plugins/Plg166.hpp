#pragma once
// PLG166.BIN's module data (src/EFFECT/PLG166.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg166 {
// decomp: vs_effect_sanctusFlare_1_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FCEC0;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile mirrorSurfaceVertices;
// decomp: vs_effect_sanctusFlare_2_colors (src/EFFECT/closedRings.h:58; was
// D_800FD140; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
// decomp: vs_effect_sanctusFlare_3_colors (src/EFFECT/rings.h:20; was
// D_800FD160; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
}  // namespace VS::Effects::Plg166
