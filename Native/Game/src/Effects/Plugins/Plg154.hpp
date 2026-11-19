#pragma once
// PLG154.BIN's module data (src/EFFECT/PLG154.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg154 {
// decomp: D_800FB29C (PLG154.BIN/0.c:8; meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: D_800FB2EC (PLG154.BIN/0.c:8; meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: D_800FB34C (PLG154.BIN/0.c:12; closedRings.h:58)
extern plugins::RingPalette closedRingColorFactors;
}  // namespace VS::Effects::Plg154
