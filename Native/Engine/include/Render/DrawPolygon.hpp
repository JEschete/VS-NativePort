#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// A typed polygon draw: what a GP0 polygon packet expressed, without the
// packet. PageSpace rasterizes it directly and FrameDrawList (DrawList.hpp)
// orders it among the frame's remaining packets.
namespace VS::Render {

// A texture page in the page space, as the GPU addresses it: the page x in
// 64-pixel units (bits 0-3), y 256 (bit 4), the semi-transparency mode (bits
// 5-6) and the texel depth (bits 7-8). Drawing a textured polygon makes its
// page and mode current, as the GPU's polygon tpage field did. A VRAM handle
// until owned textures replace it (#208).
struct TexturePageId {
  std::uint16_t word = 0;

  friend bool operator==(const TexturePageId&, const TexturePageId&) = default;
};

// A palette's page-space position: x in 16-pixel units (bits 0-5) and the row
// (bits 6-14).
struct PaletteId {
  std::uint16_t word = 0;

  friend bool operator==(const PaletteId&, const PaletteId&) = default;
};

struct DrawMaterial {
  // Colours interpolate across the polygon; otherwise corner 0's colour fills
  // it.
  bool gouraud = false;
  bool textured = false;
  bool semi_transparent = false;
  // Textured texels are drawn without the corner colour modulating them.
  bool raw_texture = false;
  TexturePageId page{};
  PaletteId palette{};

  friend bool operator==(const DrawMaterial&, const DrawMaterial&) = default;
};

struct DrawCorner {
  // Screen position before the draw offset; the GPU reads 11 signed bits.
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  std::uint8_t u = 0;
  std::uint8_t v = 0;

  friend bool operator==(const DrawCorner&, const DrawCorner&) = default;
};

inline constexpr std::size_t kDrawTriangleCorners = 3;
inline constexpr std::size_t kDrawQuadCorners = 4;

// A triangle, or a quad drawn as triangles (0, 1, 2) and (1, 2, 3).
struct DrawPolygon {
  DrawMaterial material{};
  std::uint8_t corner_count = kDrawTriangleCorners;
  std::array<DrawCorner, kDrawQuadCorners> corners{};

  [[nodiscard]] constexpr bool HasValidCornerCount() const noexcept {
    return corner_count == kDrawTriangleCorners ||
           corner_count == kDrawQuadCorners;
  }

  friend bool operator==(const DrawPolygon&, const DrawPolygon&) = default;
};

}  // namespace VS::Render
