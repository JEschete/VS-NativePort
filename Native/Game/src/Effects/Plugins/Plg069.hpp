#pragma once
// PLG069.BIN's module data (src/EFFECT/PLG069.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg069 {
// decomp: D_800FD424 (PLG069.BIN/E0.c:5; meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: D_800FD474 (PLG069.BIN/E0.c:5; meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: D_800FD4D4 (PLG069.BIN/E0.c:7; trailEffect.h:70)
extern std::int32_t trailEmissionCounter;
// decomp: D_800FD4D8 (PLG069.BIN/E0.c:9; closedRings.h:58)
extern plugins::RingPalette tiledRingColorFactors;
}  // namespace VS::Effects::Plg069
