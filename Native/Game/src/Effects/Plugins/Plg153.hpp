#pragma once
// PLG153.BIN's module data (src/EFFECT/PLG153.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg153 {
// decomp: vs_effect_gravisAether_0_vertices (src/EFFECT/meshes10.h:38; was
// D_800FCD04; src/EFFECT/meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: vs_effect_gravisAether_0_faces (src/EFFECT/meshes10.h:45; was
// D_800FCD54; src/EFFECT/meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: vs_effect_gravisAether_1_colors (src/EFFECT/rings.h:20; was
// D_800FCDB4; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_gravisAether_2_origin_state (src/EFFECT/particleSetup.h:35;
// was D_800FCDD4; src/EFFECT/particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg153
