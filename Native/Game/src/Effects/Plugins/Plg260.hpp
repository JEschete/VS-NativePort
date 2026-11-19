#pragma once
// PLG260.BIN's module data (src/EFFECT/PLG260.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg260 {
// decomp: D_800FA32C (PLG260.BIN/0.c:5; meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: D_800FA37C (PLG260.BIN/0.c:5; meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
}  // namespace VS::Effects::Plg260
