// Hand-maintained (tools/port/owned.csv).
#include "Render/ScaledFont.hpp"

#include <cstddef>
#include <cstdint>

namespace VS::Render {
namespace {
constexpr std::uint16_t kPaletteBaseCode = 0x3780;
constexpr unsigned kFirstPaletteIndex = 0x34;
constexpr unsigned kPaletteIndexMask = 0x3F;

std::int16_t ScreenCoordinate(int fixed) {
  return static_cast<std::int16_t>(fixed >> kScaledFontPositionFractionBits);
}
}  // namespace

// decomp: BATTLE.PRG _printFixedWidthFontChar
// (src/BATTLE/BATTLE.PRG/5BF94.c:1977).
int DrawScaledFontChar(int char_id, int x, int y, int width, int height,
                       int scale, const ScaledFontStyle& style,
                       ScaledGlyphTarget& target) {
  if (char_id == kScaledFontSpaceGlyph) {
    return x + kScaledFontSpaceAdvance * scale;
  }
  const unsigned glyph = static_cast<unsigned>(char_id) +
                         style.table_select * kScaledFontGlyphsPerTable;
  if (glyph >= style.glyph_widths.size() || !target.CanDraw()) return x;
  const int next_x =
      x + std::to_integer<int>(style.glyph_widths[glyph]) * scale;
  const unsigned column =
      glyph % kScaledFontGlyphsPerRow * kScaledFontGlyphCellSize;
  const unsigned row =
      glyph / kScaledFontGlyphsPerRow * kScaledFontGlyphCellSize;
  ScaledGlyphQuad quad;
  quad.left = ScreenCoordinate(x);
  quad.top = ScreenCoordinate(y);
  quad.right = ScreenCoordinate(x + width);
  quad.bottom = ScreenCoordinate(y + height);
  quad.u = static_cast<std::uint8_t>(column);
  quad.v = static_cast<std::uint8_t>(row);
  quad.clut = static_cast<std::uint16_t>(
      kPaletteBaseCode |
      ((style.palette_offset + kFirstPaletteIndex) & kPaletteIndexMask));
  quad.texture_page = kScaledFontTexturePage;
  quad.color = style.color;
  target.Draw(quad);
  return next_x;
}
}  // namespace VS::Render
