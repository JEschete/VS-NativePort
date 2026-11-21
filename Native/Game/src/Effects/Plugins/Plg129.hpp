#pragma once
// PLG129.BIN's module data (src/EFFECT/PLG129.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg129 {
// decomp: vs_effect_lastAscension_0_colors (src/EFFECT/rings.h:20; was
// D_800FD5A8; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_lastAscension_1_vertices (src/EFFECT/meshes10.h:38; was
// D_800FD5E8; src/EFFECT/meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: vs_effect_lastAscension_1_faces (src/EFFECT/meshes10.h:45; was
// D_800FD638; src/EFFECT/meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: vs_effect_lastAscension_2_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FD698;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: vs_effect_lastAscension_3_colors (src/EFFECT/rings.h:20; was
// D_800FD918; src/EFFECT/rings.h:20)
extern plugins::RingPaletteBytes ringColorFactors2;
}  // namespace VS::Effects::Plg129
