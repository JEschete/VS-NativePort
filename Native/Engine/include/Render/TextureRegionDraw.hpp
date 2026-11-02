#pragma once

#include <cstdint>
#include <span>

// Fading, flashing and wiping draws of one texture-atlas region (the results
// screens' renderTextures.h). The policy turns a region, a screen position and
// a left/right color pair into semi-transparent textured quads and hands them
// to a RegionQuadTarget, so it reaches no global render state.
namespace VS::Render {
// The tint fade scales the colors by step/8 below this step.
inline constexpr int kRegionFadeSteps = 8;
// The flash shows light grey, then brighter grey, then blends back to the
// colors over four steps.
inline constexpr int kRegionFlashLightStep = 8;
inline constexpr int kRegionFlashBrightStep = 9;
inline constexpr int kRegionFlashBlendStep = 10;
inline constexpr int kRegionFlashBlendEnd = 14;
// The blend mixes in quarters: weight 0 is all grey, weight 4 all color.
inline constexpr int kRegionFlashBlendWeights = 4;
inline constexpr std::uint8_t kRegionFlashLightGrey = 192;
inline constexpr std::uint8_t kRegionFlashBrightGrey = 224;
// The wipe draws strips this many texels wide and fades them over this many
// pixels before its edge.
inline constexpr int kRegionWipeStripWidth = 12;
inline constexpr int kRegionWipeFeather = 64;
// While fading in, the quad uses the next palette and additive blending.
inline constexpr std::uint16_t kRegionFadePaletteStep = 1;
inline constexpr std::uint16_t kRegionAdditiveBlend = 0x20;

// One atlas entry: its texel rectangle, texture page and palette.
struct TextureRegion {
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  std::uint8_t width = 0;
  std::uint8_t height = 0;
  std::uint16_t texture_page = 0;
  std::uint16_t clut = 0;
};

// A region color and the shared animation step (the left color's step drives
// every draw).
struct RegionColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t step = 0;
};

struct QuadColor {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;

  friend bool operator==(const QuadColor&, const QuadColor&) = default;
};

// An axis-aligned, semi-transparent textured quad. The left color shades the
// left vertices and the right color the right ones. Texel coordinates wrap
// within the page as on the original hardware.
struct RegionQuad {
  std::int16_t left = 0;
  std::int16_t top = 0;
  std::int16_t right = 0;
  std::int16_t bottom = 0;
  std::uint8_t u_left = 0;
  std::uint8_t u_right = 0;
  std::uint8_t v_top = 0;
  std::uint8_t v_bottom = 0;
  QuadColor left_color;
  QuadColor right_color;
  std::uint16_t clut = 0;
  std::uint16_t texture_page = 0;
};

// Where region quads are drawn.
class RegionQuadTarget {
 public:
  virtual ~RegionQuadTarget() = default;
  // Queues the quad; a full primitive buffer drops it.
  virtual void Draw(const RegionQuad& quad) = 0;
};

// Mixes a color component into the flash grey: weight 0 is all grey and
// weight 4 is all component (renderTextures.h _adjust).
[[nodiscard]] int BlendFlashComponent(int component, int weight);

// Draws the region at (x, y) faded in by the left color's step: nothing at
// step 0, the colors scaled by step/8 with the fade palette and additive
// blending below step 8, the colors themselves afterwards
// (renderTextures.h _renderTextureFadeInTint).
void DrawTextureTintFade(const TextureRegion& region, int x, int y,
                         std::span<const RegionColor, 2> colors,
                         RegionQuadTarget& target);

// Draws the region popping in: the tint fade below step 8, then light grey,
// brighter grey and a four-step blend back to the colors
// (renderTextures.h _renderTexturePopIn).
void DrawTextureFlashFade(const TextureRegion& region, int x, int y,
                          std::span<const RegionColor, 2> colors,
                          RegionQuadTarget& target);

// Draws the region revealed by a wipe whose edge is at wipe_edge_x: whole when
// it lies a feather width before the edge, as 12-texel strips that fade
// toward the edge when it straddles it, and not at all past it
// (renderTextures.h _renderTextureWipe).
void DrawTextureWipe(const TextureRegion& region, int x, int y,
                     std::span<const RegionColor, 2> colors, int wipe_edge_x,
                     RegionQuadTarget& target);
}  // namespace VS::Render
