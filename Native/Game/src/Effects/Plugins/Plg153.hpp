#pragma once
// PLG153.BIN's module data (src/EFFECT/PLG153.BIN).
// ModuleData.cpp fills these tables from the module image.
#include <cstdint>

#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg153 {
// decomp: D_800FCD04 (PLG153.BIN/110.c:5; meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: D_800FCD54 (PLG153.BIN/110.c:5; meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: D_800FCDB4 (PLG153.BIN/110.c:8; rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: D_800FCDD4 (PLG153.BIN/110.c:10; particleSetup.h:35)
extern std::int32_t particleEmissionCounter;
}  // namespace VS::Effects::Plg153
