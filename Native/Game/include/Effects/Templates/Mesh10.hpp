#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Effects/Templates/RingCore.hpp"
#include "Effects/Templates/SurfaceCore.hpp"

// The ten-vertex mesh template (decomp src/EFFECT/meshes10.h:29 vs_mesh10):
// an overlay's ten-vertex model whose first four vertices scale by the
// sampled extent and the other six by the attraction jitter, turned by an
// accumulating rotation, placed at the sampled position and drawn as sixteen
// Gouraud-shaded triangles in two colours.
namespace VS::Effects::Templates {
inline constexpr std::size_t kMesh10Vertices = 10;
inline constexpr std::size_t kMesh10Faces = 16;
// The vertices the extent scales; the others scale by the attraction jitter.
inline constexpr std::size_t kMesh10ExtentVertices = 4;
inline constexpr std::size_t kMesh10Corners = 3;
inline constexpr std::size_t kMesh10Colors = 2;
// Six texture coordinate pairs, as the bytes a face's offsets index.
inline constexpr std::size_t kMesh10UvBytes = 12;
inline constexpr std::size_t kMesh10PacketWords = 12;

// The persistent record (vs_mesh10State).
struct Mesh10State {
  std::uint8_t lifetime{};
  std::uint8_t definition_index{};
  std::int32_t age{};
  RingPoint rotation{};

  friend bool operator==(const Mesh10State&, const Mesh10State&) = default;
};

using Mesh10Host = SurfaceRecordHost<Mesh10State>;

// A model vertex: a Q12 unit point and the colour (0 or 1) its corners take.
struct Mesh10Vertex {
  RingPoint point{};
  std::uint16_t color{};

  friend bool operator==(const Mesh10Vertex&, const Mesh10Vertex&) = default;
};

// A model face: three vertex indices and, for each corner, the byte offset
// of its texture coordinate pair in Mesh10UvBytes.
struct Mesh10Face {
  std::array<std::uint8_t, kMesh10Corners> vertices{};
  std::array<std::uint8_t, kMesh10Corners> uvs{};

  friend bool operator==(const Mesh10Face&, const Mesh10Face&) = default;
};

// An overlay's mesh (its VS_MESH10_VERTICES and VS_MESH10_FACES tables).
struct Mesh10Model {
  std::span<const Mesh10Vertex, kMesh10Vertices> vertices;
  std::span<const Mesh10Face, kMesh10Faces> faces;
};

// One textured triangle in GPU corner order.
struct Mesh10Triangle {
  std::array<std::uint32_t, kMesh10Corners> screens{};
  std::array<RingColor, kMesh10Corners> colors{};
  std::array<RingUv, kMesh10Corners> uvs{};
  std::uint16_t clut{};
  std::uint16_t tpage{};

  friend bool operator==(const Mesh10Triangle&,
                         const Mesh10Triangle&) = default;
};

// The six texture coordinate pairs of `texture` a face picks from, as bytes:
// the top centre, the two quarter points halfway down, and the bottom left,
// centre and right.
[[nodiscard]] std::array<std::uint8_t, kMesh10UvBytes> Mesh10UvBytes(
    const RingTexture& texture) noexcept;

// The GPU words of `triangle` below an empty link: the length, the draw mode,
// the POLY_GT3 and the restored draw mode.
[[nodiscard]] std::array<std::uint32_t, kMesh10PacketWords> EncodeMesh10Packet(
    const Mesh10Triangle& triangle) noexcept;

// One renderer command of a mesh; false releases the slot.
bool Mesh10(SurfaceCommand command, std::int32_t packed, Mesh10Model model,
            Mesh10Host& host);
}  // namespace VS::Effects::Templates

// The plugin instances (Mesh10.cpp); SurfacesLive.hpp passes the overlay's
// model.
namespace VS::Effects {
namespace Plg069 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg069
namespace Plg077 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg077
namespace Plg129 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg129
namespace Plg137 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg137
namespace Plg153 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg153
namespace Plg154 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg154
namespace Plg260 {
bool Mesh10(Templates::SurfaceCommand command, std::int32_t packed,
            Templates::Mesh10Model model, Templates::Mesh10Host& host);
}  // namespace Plg260
}  // namespace VS::Effects
