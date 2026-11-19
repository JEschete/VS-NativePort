// Hand-maintained (tools/port/owned.csv).
#include "Render/TextureRegionDrawLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Legacy/Psx.h"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Images.hpp"
#include "Render/Render.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "Render/UiPrimitives.hpp"

namespace {
constexpr std::size_t kPrimitiveBufferBytes = 0x20000;
// The region draws sit seven ordering-table words before the frame's table
// pointer (renderTextures.h: `p[1] - 0x1C`).
constexpr std::size_t kOrderingOffsetBytes = 0x1C;

// The current frame's primitive buffer and ordering table.
class LiveRegionTarget final : public VS::Render::RegionQuadTarget {
 public:
  void Draw(const VS::Render::RegionQuad& quad) override {
    const auto base = reinterpret_cast<std::uintptr_t>(
        VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1]);
    const auto cursor = reinterpret_cast<std::uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
    if (base == 0 || cursor < base ||
        cursor - base > kPrimitiveBufferBytes - sizeof(POLY_GT4) ||
        VS::Render::FrameScratchpad().ordering_table == nullptr) {
      return;
    }
    auto* primitive =
        static_cast<POLY_GT4*>(VS::Render::FrameScratchpad().primitive_cursor);
    std::memset(primitive, 0, sizeof *primitive);
    setPolyGT4(primitive);
    setXY4(primitive, quad.left, quad.top, quad.right, quad.top, quad.left,
           quad.bottom, quad.right, quad.bottom);
    setUV4(primitive, quad.u_left, quad.v_top, quad.u_right, quad.v_top,
           quad.u_left, quad.v_bottom, quad.u_right, quad.v_bottom);
    setRGB0(primitive, quad.left_color.r, quad.left_color.g, quad.left_color.b);
    setRGB1(primitive, quad.right_color.r, quad.right_color.g,
            quad.right_color.b);
    setRGB2(primitive, quad.left_color.r, quad.left_color.g, quad.left_color.b);
    setRGB3(primitive, quad.right_color.r, quad.right_color.g,
            quad.right_color.b);
    setSemiTrans(primitive, 1);
    primitive->clut = quad.clut;
    primitive->tpage = quad.texture_page;
    AddPrim(static_cast<unsigned char*>(
                VS::Render::FrameScratchpad().ordering_table) -
                kOrderingOffsetBytes,
            primitive);
    VS::Render::FrameScratchpad().primitive_cursor = primitive + 1;
  }
};

// Copied code builds with unsigned plain char (/J); read the atlas bytes as
// unsigned whatever this translation unit uses.
VS::Render::TextureRegion Region(const VS::Render::TextureRegion& region) {
  return {static_cast<std::uint8_t>(region.u),
          static_cast<std::uint8_t>(region.v),
          static_cast<std::uint8_t>(region.width),
          static_cast<std::uint8_t>(region.height),
          region.texture_page,
          region.clut};
}

VS::Render::RegionColor Color(const CVECTOR& color) {
  return {color.r, color.g, color.b, color.cd};
}

// The region and color pair a live draw names, or false when either lies
// outside its array.
bool Resolve(std::span<const VS::Render::TextureRegion> atlas, int region_id,
             std::span<const CVECTOR> colors, VS::Render::TextureRegion& region,
             std::array<VS::Render::RegionColor, 2>& pair) {
  if (region_id < 0 || static_cast<std::size_t>(region_id) >= atlas.size() ||
      colors.size() < pair.size()) {
    return false;
  }
  region = Region(atlas[static_cast<std::size_t>(region_id)]);
  pair = {Color(colors[0]), Color(colors[1])};
  return true;
}
}  // namespace

namespace VS::Render {
void DrawLiveTextureTintFade(std::span<const VS::Render::TextureRegion> atlas,
                             int x, int y, int region_id,
                             std::span<const CVECTOR> colors) {
  TextureRegion region;
  std::array<RegionColor, 2> pair;
  if (!Resolve(atlas, region_id, colors, region, pair)) {
    return;
  }
  LiveRegionTarget target;
  DrawTextureTintFade(region, x, y, pair, target);
}

void DrawLiveTextureFlashFade(std::span<const VS::Render::TextureRegion> atlas,
                              int x, int y, int region_id,
                              std::span<const CVECTOR> colors) {
  TextureRegion region;
  std::array<RegionColor, 2> pair;
  if (!Resolve(atlas, region_id, colors, region, pair)) {
    return;
  }
  LiveRegionTarget target;
  DrawTextureFlashFade(region, x, y, pair, target);
}

void DrawLiveTextureWipe(std::span<const VS::Render::TextureRegion> atlas,
                         int x, int y, int region_id,
                         std::span<const CVECTOR> colors, int wipe_edge_x) {
  TextureRegion region;
  std::array<RegionColor, 2> pair;
  if (!Resolve(atlas, region_id, colors, region, pair)) {
    return;
  }
  LiveRegionTarget target;
  DrawTextureWipe(region, x, y, pair, wipe_edge_x, target);
}
}  // namespace VS::Render
