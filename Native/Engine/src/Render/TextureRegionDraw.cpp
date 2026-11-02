// Hand-maintained (tools/port/owned.csv).
#include "Render/TextureRegionDraw.hpp"

#include <algorithm>
#include <cstdint>
#include <span>

namespace VS::Render {
namespace {
// The hardware stores coordinates as 16-bit words and texels as bytes, so
// the original's int arithmetic truncates on store.
constexpr std::int16_t Coordinate(int value) {
  return static_cast<std::int16_t>(value);
}

constexpr std::uint8_t Texel(int value) {
  return static_cast<std::uint8_t>(value);
}

constexpr std::uint8_t Channel(int value) {
  return static_cast<std::uint8_t>(value);
}

QuadColor Rgb(const RegionColor& color) { return {color.r, color.g, color.b}; }

QuadColor Scaled(const RegionColor& color, int weight, int divisor) {
  return {Channel(color.r * weight / divisor),
          Channel(color.g * weight / divisor),
          Channel(color.b * weight / divisor)};
}

QuadColor Blended(const RegionColor& color, int weight) {
  return {Channel(BlendFlashComponent(color.r, weight)),
          Channel(BlendFlashComponent(color.g, weight)),
          Channel(BlendFlashComponent(color.b, weight))};
}

// The region's full rectangle at (x, y) with its own palette and page.
RegionQuad RegionAt(const TextureRegion& region, int x, int y) {
  RegionQuad quad;
  quad.left = Coordinate(x);
  quad.top = Coordinate(y);
  quad.right = Coordinate(x + region.width);
  quad.bottom = Coordinate(y + region.height);
  quad.u_left = region.u;
  quad.u_right = Texel(region.u + region.width);
  quad.v_top = region.v;
  quad.v_bottom = Texel(region.v + region.height);
  quad.clut = region.clut;
  quad.texture_page = region.texture_page;
  return quad;
}

void UseFadePalette(RegionQuad& quad, const TextureRegion& region) {
  quad.clut = static_cast<std::uint16_t>(region.clut + kRegionFadePaletteStep);
  quad.texture_page =
      static_cast<std::uint16_t>(region.texture_page | kRegionAdditiveBlend);
}

int FeatherWeight(int wipe_edge_x, int x) {
  return std::clamp(wipe_edge_x - x, 0, kRegionWipeFeather);
}
}  // namespace

// decomp: renderTextures.h _adjust (src/renderTextures.h:54)
int BlendFlashComponent(int component, int weight) {
  const int blended =
      (component * weight) +
      ((kRegionFlashBlendWeights - weight) * kRegionFlashLightGrey);
  return blended / kRegionFlashBlendWeights;
}

// decomp: renderTextures.h _renderTextureFadeInTint (src/renderTextures.h:3)
void DrawTextureTintFade(const TextureRegion& region, int x, int y,
                         std::span<const RegionColor, 2> colors,
                         RegionQuadTarget& target) {
  const int step = colors[0].step;
  if (step == 0) {
    return;
  }
  RegionQuad quad = RegionAt(region, x, y);
  if (step < kRegionFadeSteps) {
    quad.left_color = Scaled(colors[0], step, kRegionFadeSteps);
    quad.right_color = Scaled(colors[1], step, kRegionFadeSteps);
    UseFadePalette(quad, region);
  } else {
    quad.left_color = Rgb(colors[0]);
    quad.right_color = Rgb(colors[1]);
  }
  target.Draw(quad);
}

// decomp: renderTextures.h _renderTexturePopIn (src/renderTextures.h:60)
void DrawTextureFlashFade(const TextureRegion& region, int x, int y,
                          std::span<const RegionColor, 2> colors,
                          RegionQuadTarget& target) {
  const int step = colors[0].step;
  if (step < kRegionFadeSteps) {
    DrawTextureTintFade(region, x, y, colors, target);
    return;
  }
  RegionQuad quad = RegionAt(region, x, y);
  if (step == kRegionFlashLightStep) {
    quad.left_color = {kRegionFlashLightGrey, kRegionFlashLightGrey,
                       kRegionFlashLightGrey};
    quad.right_color = quad.left_color;
  } else if (step == kRegionFlashBrightStep) {
    quad.left_color = {kRegionFlashBrightGrey, kRegionFlashBrightGrey,
                       kRegionFlashBrightGrey};
    quad.right_color = quad.left_color;
  } else if (step < kRegionFlashBlendEnd) {
    const int weight = step - kRegionFlashBlendStep;
    quad.left_color = Blended(colors[0], weight);
    quad.right_color = Blended(colors[1], weight);
  } else {
    quad.left_color = Rgb(colors[0]);
    quad.right_color = Rgb(colors[1]);
  }
  if (step < kRegionFlashBlendStep) {
    UseFadePalette(quad, region);
  }
  target.Draw(quad);
}

// decomp: renderTextures.h _renderTextureWipe (src/renderTextures.h:130)
void DrawTextureWipe(const TextureRegion& region, int x, int y,
                     std::span<const RegionColor, 2> colors, int wipe_edge_x,
                     RegionQuadTarget& target) {
  if (x + region.width < wipe_edge_x - kRegionWipeFeather) {
    RegionQuad quad = RegionAt(region, x, y);
    quad.left_color = Rgb(colors[0]);
    quad.right_color = Rgb(colors[1]);
    target.Draw(quad);
    return;
  }
  if (x >= wipe_edge_x) {
    return;
  }
  int strip_x = x;
  int texel_u = region.u;
  for (int offset = 0; offset < region.width; offset += kRegionWipeStripWidth,
           strip_x += kRegionWipeStripWidth, texel_u += kRegionWipeStripWidth) {
    const int strip_width =
        std::min(kRegionWipeStripWidth, region.width - offset);
    const int strip_right = strip_x + strip_width;
    RegionQuad quad;
    quad.left = Coordinate(strip_x);
    quad.top = Coordinate(y);
    quad.right = Coordinate(strip_right);
    quad.bottom = Coordinate(y + region.height);
    quad.u_left = Texel(texel_u);
    quad.u_right = Texel(texel_u + strip_width);
    quad.v_top = region.v;
    quad.v_bottom = Texel(region.v + region.height);
    quad.left_color = Scaled(colors[0], FeatherWeight(wipe_edge_x, strip_x),
                             kRegionWipeFeather);
    quad.right_color = Scaled(
        colors[1], FeatherWeight(wipe_edge_x, strip_right), kRegionWipeFeather);
    UseFadePalette(quad, region);
    target.Draw(quad);
  }
}
}  // namespace VS::Render
