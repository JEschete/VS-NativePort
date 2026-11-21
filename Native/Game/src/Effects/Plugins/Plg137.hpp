#pragma once
// PLG137.BIN's module data (src/EFFECT/PLG137.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg137 {
// decomp: vs_effect_cherryRonde_0_vertices (src/EFFECT/meshes10.h:38; was
// D_800FDF8C; src/EFFECT/meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: vs_effect_cherryRonde_0_faces (src/EFFECT/meshes10.h:45; was
// D_800FDFDC; src/EFFECT/meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: vs_effect_cherryRonde_1_origin_state (src/EFFECT/trailEffect.h:70;
// was D_800FE03C; src/EFFECT/trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: vs_effect_cherryRonde_3_colors (src/EFFECT/closedRings.h:58; was
// D_800FE040; src/EFFECT/closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg137
