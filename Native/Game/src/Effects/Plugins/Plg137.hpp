#pragma once
// PLG137.BIN's module data (src/EFFECT/PLG137.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg137 {
// decomp: D_800FDF8C (PLG137.BIN/F8.c:5; meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: D_800FDFDC (PLG137.BIN/F8.c:5; meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: D_800FE03C (PLG137.BIN/F8.c:7; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FE040 (PLG137.BIN/F8.c:11; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg137
