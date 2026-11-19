#pragma once
// One texture atlas entry: its texel rectangle, texture page and palette.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
// decomp: _texture_t (src/include/texture_t.h:12), whose x, y, w and h are
// the texel rectangle u, v, width and height.
struct TextureRegion {
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  std::uint8_t width = 0;
  std::uint8_t height = 0;
  std::uint16_t texture_page = 0;
  std::uint16_t clut = 0;
};

namespace layout {
inline constexpr std::size_t kTextureRegionBytes = 0x8;
static_assert(sizeof(TextureRegion) == kTextureRegionBytes);
}  // namespace layout
}  // namespace VS::Render
