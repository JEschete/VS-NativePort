#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// Scaled fixed-cell font glyphs. The layout policy reads glyph advances and
// the active font table through ScaledFontStyle and hands each textured quad
// to a ScaledGlyphTarget, so it reaches no global render state.
namespace VS::Render {
// The space code advances the pen without drawing.
inline constexpr int kScaledFontSpaceGlyph = 0x8F;
inline constexpr int kScaledFontSpaceAdvance = 6;
inline constexpr unsigned kScaledFontGlyphsPerTable = 189;
inline constexpr unsigned kScaledFontGlyphsPerRow = 21;
inline constexpr unsigned kScaledFontGlyphCellSize = 12;
// Pen positions and sizes carry 16 fractional bits.
inline constexpr int kScaledFontPositionFractionBits = 16;
inline constexpr std::uint16_t kScaledFontTexturePage = 13;

// The font state a glyph is drawn with.
struct ScaledFontStyle {
  std::span<const std::byte> glyph_widths;
  std::uint8_t table_select = 0;
  std::uint8_t palette_offset = 0;
  // Packed RGB and primitive code, copied unchanged into the quad.
  std::uint32_t color = 0;
};

// One glyph's screen rectangle, texture cell and palette.
struct ScaledGlyphQuad {
  std::int16_t left = 0;
  std::int16_t top = 0;
  std::int16_t right = 0;
  std::int16_t bottom = 0;
  std::uint8_t u = 0;
  std::uint8_t v = 0;
  std::uint16_t clut = 0;
  std::uint16_t texture_page = 0;
  std::uint32_t color = 0;
};

// Where glyph quads are drawn.
class ScaledGlyphTarget {
 public:
  virtual ~ScaledGlyphTarget() = default;
  // False when no text box layer is active; the glyph then neither draws nor
  // advances the pen.
  [[nodiscard]] virtual bool CanDraw() const = 0;
  // Queues the quad. A full primitive buffer drops it; the pen still advances.
  virtual void Draw(const ScaledGlyphQuad& quad) = 0;
};

// Lays out one glyph at fixed-point pen position (x, y) with the given cell
// size and draws it (BATTLE.PRG _printFixedWidthFontChar). Returns the next
// pen x: unchanged for an unknown glyph or an unavailable target, advanced
// by scale times the glyph width otherwise, and by six times scale for space.
[[nodiscard]] int DrawScaledFontChar(int char_id, int x, int y, int width,
                                     int height, int scale,
                                     const ScaledFontStyle& style,
                                     ScaledGlyphTarget& target);

// Applies DrawScaledFontChar with the live font state, the active text box
// ordering table and the current frame's primitive buffer.
int DrawLiveScaledFontChar(int char_id, int x, int y, int width, int height,
                           int scale);
}  // namespace VS::Render
