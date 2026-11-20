#pragma once
// The shapes of the effect overlays' module data (src/EFFECT/PLGnnn.BIN): the
// palettes, surface profiles and ten-vertex models each overlay's templates
// read. Effects/Plugins/PlgNNN.hpp declare every overlay's tables in these
// shapes; the templates' live bindings read them.
#include <cstddef>
#include <cstdint>

#include "Effects/Templates/Mesh10.hpp"
#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Effects::plugins {
// A palette entry's four bytes: the intensities of a ring's three rows
// (RingPaletteEntry), then a byte the templates do not read.
inline constexpr std::size_t kRingPaletteEntryBytes = 4;
// A ten-vertex mesh face: its three vertex indices, then three texture
// offsets.
inline constexpr std::size_t kMesh10FaceBytes = Templates::kMesh10Corners * 2;

// A closed or tiled ring's palette (closedRings.h): one entry per
// transparency value (#289), the eight colours VS_STANDARD_COLORS
// (src/EFFECT/effectCommon.h:36) defines.
using RingPalette = VS::Core::Gte::CVECTOR[Templates::kRingPaletteEntries];
// A palette the open and repeating rings read byte by byte.
using RawRingPaletteEntry = std::uint8_t[kRingPaletteEntryBytes];
using RingPaletteBytes = RawRingPaletteEntry[Templates::kRingPaletteEntries];
// A surface's unit profile (sphericalSurfaceVertices.h), row by row.
using SurfaceProfileRow = VS::Core::Gte::SVECTOR[Templates::kSurfaceColumns];
using SurfaceProfile = SurfaceProfileRow[Templates::kSurfaceRows];
// A ten-vertex mesh's model (meshes10.h).
using Mesh10VertexTable = VS::Core::Gte::SVECTOR[Templates::kMesh10Vertices];
using Mesh10FaceTable = std::uint8_t[Templates::kMesh10Faces][kMesh10FaceBytes];

static_assert(sizeof(RingPalette) == sizeof(RingPaletteBytes));
}  // namespace VS::Effects::plugins
