#pragma once
// PLG077.BIN's module data (src/EFFECT/PLG077.BIN).
// ModuleData.cpp fills these tables from the module image.
#include "Effects/Plugins/PluginData.hpp"

namespace VS::Effects::Plg077 {
// decomp: vs_effect_meteorLv1_0_vertices (src/EFFECT/meshes10.h:38; was
// D_800FD908; src/EFFECT/meshes10.h:38)
extern plugins::Mesh10VertexTable mesh10Vertices;
// decomp: vs_effect_meteorLv1_0_faces (src/EFFECT/meshes10.h:45; was
// D_800FD958; src/EFFECT/meshes10.h:45)
extern plugins::Mesh10FaceTable mesh10Faces;
// decomp: D_800FD9B8 (PLG077.BIN/18.c:8; src/EFFECT/closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors;
// decomp: vs_effect_meteorLv1_3_vertices
// (src/EFFECT/sphericalSurfaceVertices.h:21; was D_800FD9D8;
// src/EFFECT/sphericalSurfaceVertices.h:21)
extern plugins::SurfaceProfile surfaceVertices;
// decomp: D_800FDC58 (PLG077.BIN/18.c:15; src/EFFECT/closedRings.h:58)
extern plugins::RingPaletteBytes ringColorFactors2;
}  // namespace VS::Effects::Plg077
