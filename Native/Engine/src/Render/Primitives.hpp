#pragma once
// A textured quad as the GPU reads it: nine words without the ordering-table
// tag.
#include <cstddef>
#include <cstdint>

namespace VS::Render {
// decomp: VS_POLY_FT4 (src/include/gpu.h:93)
struct PolyFt4 {
  std::uint32_t color_code;
  std::uint32_t xy0;
  std::uint32_t uv0_palette;
  std::uint32_t xy1;
  std::uint32_t uv1_texture_page;
  std::uint32_t xy2;
  std::uint32_t uv2;
  std::uint32_t xy3;
  std::uint32_t uv3;
};

namespace layout {
inline constexpr std::size_t kPolyFt4Bytes = 0x24;
static_assert(sizeof(PolyFt4) == kPolyFt4Bytes);
}  // namespace layout
}  // namespace VS::Render
