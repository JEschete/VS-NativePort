// Hand-maintained (tools/port/owned.csv).
// The variable-width and compact fonts (BATTLE.PRG 5BF94.c:1345-1704): glyph
// sprites built as words for a PacketQueue from a FontStyle and the font
// tables.
#include "Render/GameFont.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "Render/EncodedText.hpp"
#include "Render/Sprites.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Render {
namespace {
// Texture page 13 (x 832) with additive blending.
constexpr std::uint32_t kVariableGlyphDrawMode = 0xE100002DU;
constexpr std::uint32_t kVariableGlyphSize = 12U | (12U << 16);
constexpr std::uint32_t kGlyphCellSize = 12;
constexpr std::uint32_t kGlyphRowTexels = kGlyphCellSize << 8;
// getClut(832 + 16 * palette offset, 222): row 222 in bits 6-14 and x / 16 in
// the low six bits.
constexpr std::uint32_t kVariableClutBase = 0x3780;
constexpr std::uint32_t kVariableClutRowBias = 0x34;
constexpr std::uint32_t kVariableClutRowMask = 0x3F;
constexpr int kHalfShift = 16;
constexpr std::uint32_t kLowHalf = 0xFFFF;

// The compact font's texture page 12 (x 768) draw mode, its cells at
// (u, 76 + 10v) with CLUT (864, 223), and the '*' glyph's own cell.
constexpr std::uint32_t kCompactDrawMode = 0xE100000CU;
constexpr std::size_t kCompactGlyphWords = 5;
constexpr std::uint32_t kCompactGlyphLength = 4;
constexpr int kCommandShift = 24;
constexpr std::uint32_t kCompactClut = 0x37F6;
constexpr std::uint32_t kCompactCellColumnMask = 0xFE;
constexpr std::uint32_t kCompactCellRowHeight = 10;
constexpr std::uint32_t kCompactCellTop = 76;
constexpr std::uint32_t kCompactGlyphHeight = 10;
constexpr std::uint8_t kCompactStarGlyph = '*' - ' ';
constexpr std::uint32_t kCompactStarUv = 232U | (75U << 8);
constexpr std::uint32_t kCompactStarHeight = 11;
constexpr std::uint8_t kCompactFirstSkipped = '[' - ' ';
constexpr std::uint8_t kCompactRightAlign = '#' - ' ';
constexpr std::uint8_t kCompactLetterCount = 26;
constexpr std::uint8_t kCompactFirstLetter = 'A' - ' ';
constexpr std::uint32_t kWidthNibbleMask = 0xF;
constexpr std::uint32_t kWidthNibbleBits = 4;
constexpr std::uint32_t kTrimMask = 3;
constexpr std::uint32_t kPairsPerTrimByte = 4;
constexpr std::uint32_t kTexelRowShift = 8;
// The words of a compact glyph sprite after its link word.
constexpr std::size_t kCompactColorWord = 1;
constexpr std::size_t kCompactPositionWord = 2;
constexpr std::size_t kCompactTextureWord = 3;
constexpr std::size_t kCompactSizeWord = 4;

using CompactGlyph = std::array<std::uint32_t, kCompactGlyphWords>;

[[nodiscard]] std::uint32_t PackPosition(std::int32_t x,
                                         std::int32_t y) noexcept {
  return (static_cast<std::uint32_t>(x) & kLowHalf) |
         (static_cast<std::uint32_t>(y) << kHalfShift);
}

[[nodiscard]] std::uint8_t TableByte(std::span<const std::uint8_t> table,
                                     std::size_t index) noexcept {
  return index < table.size() ? table[index] : 0;
}

// The compact glyph's 4-bit advance; two share a width byte.
[[nodiscard]] std::uint32_t CompactWidth(const CompactFontTables& tables,
                                         std::uint8_t glyph) noexcept {
  const std::uint32_t pair = TableByte(tables.widths, glyph >> 1U);
  return (pair >> ((glyph & 1U) * kWidthNibbleBits)) & kWidthNibbleMask;
}

// The kerning trim between two capitals: `glyph` is the code past ' ' and
// `next` the following character.
[[nodiscard]] std::uint32_t CompactKerning(const CompactFontTables& tables,
                                           std::uint8_t glyph,
                                           char next) noexcept {
  const auto letter = static_cast<std::uint8_t>(glyph - kCompactFirstLetter);
  const auto following = static_cast<std::uint32_t>(
      static_cast<std::uint8_t>(next) - static_cast<std::uint32_t>('A'));
  if (letter >= kCompactLetterCount || following >= kCompactLetterCount) {
    return 0;
  }
  const std::uint32_t pair = following + (letter * kCompactLetterCount);
  const std::uint32_t trims =
      TableByte(tables.left_trim, pair / kPairsPerTrimByte);
  return (trims >> ((pair % kPairsPerTrimByte) * 2U)) & kTrimMask;
}

[[nodiscard]] char CharacterAt(std::string_view text,
                               std::size_t index) noexcept {
  return index < text.size() ? text[index] : '\0';
}

// The width of the text after an alignment mark, kerned as it is drawn.
[[nodiscard]] std::uint32_t CompactRestWidth(const CompactFontTables& tables,
                                             std::string_view rest) noexcept {
  std::uint32_t width = 0;
  for (std::size_t at = 0; at < rest.size() && rest[at] != '\0'; ++at) {
    const auto glyph = static_cast<std::uint8_t>(rest[at] - ' ');
    if (glyph > kCompactFirstSkipped) {
      continue;
    }
    width += CompactWidth(tables, glyph);
    width -= CompactKerning(tables, glyph, CharacterAt(rest, at + 1));
  }
  return width;
}

[[nodiscard]] CompactGlyph BuildCompactGlyph(const CompactFontTables& tables,
                                             std::uint8_t glyph,
                                             std::uint32_t width,
                                             std::int32_t x, std::int32_t y,
                                             std::uint32_t color) noexcept {
  const std::uint32_t cell = TableByte(tables.indices, glyph);
  CompactGlyph words{};
  words[kCompactColorWord] = (kSpriteCommand << kCommandShift) | color;
  words[kCompactPositionWord] = PackPosition(x, y);
  words[kCompactTextureWord] =
      (cell & kCompactCellColumnMask) |
      (((cell & 1U) * kCompactCellRowHeight + kCompactCellTop)
       << kTexelRowShift) |
      (kCompactClut << kHalfShift);
  words[kCompactSizeWord] = width | (kCompactGlyphHeight << kHalfShift);
  if (glyph == kCompactStarGlyph) {
    words[kCompactPositionWord] = PackPosition(x - 1, y - 1);
    words[kCompactTextureWord] = kCompactStarUv | (kCompactClut << kHalfShift);
    words[kCompactSizeWord] = width | (kCompactStarHeight << kHalfShift);
  }
  return words;
}
}  // namespace

void SetMenuFontStyle(FontStyle& style, std::uint8_t palette_offset) noexcept {
  style.table_select = 0;
  style.color = kMenuFontColor;
  style.palette_offset = palette_offset;
}

int DrawVariableWidthGlyph(PacketQueue& queue, const FontStyle& style,
                           std::span<const std::uint8_t> glyph_widths,
                           std::uint32_t glyph, int x, int y) {
  if (glyph == kTextSpaceGlyph) {
    return x + kVariableFontSpaceAdvance;
  }
  const std::uint32_t id =
      glyph + (std::uint32_t{style.table_select} * kVariableFontGlyphsPerTable);
  if (id >= glyph_widths.size()) {
    return x;
  }
  SpritePacket packet = BuildSpritePacket(
      {.draw_mode = kVariableGlyphDrawMode,
       .brightness_transparency = static_cast<int>(style.color),
       .packed_position = static_cast<int>(PackPosition(x, y)),
       .packed_size = static_cast<int>(kVariableGlyphSize)},
      queue.ChainLink());
  const std::uint32_t clut =
      ((style.palette_offset + kVariableClutRowBias) & kVariableClutRowMask) |
      kVariableClutBase;
  packet[kSpriteTextureWord] =
      ((id % kVariableFontGlyphsPerRow) * kGlyphCellSize) |
      ((id / kVariableFontGlyphsPerRow) * kGlyphRowTexels) |
      (clut << kHalfShift);
  queue.Emit(packet);
  return x + glyph_widths[id];
}

bool DrawCompactText(PacketQueue& queue, const CompactFontTables& tables,
                     std::string_view text, std::int32_t position,
                     std::uint32_t color) {
  const std::int32_t y = position >> kHalfShift;
  auto x = static_cast<std::int32_t>(static_cast<std::int16_t>(position));
  std::vector<CompactGlyph> glyphs;
  for (std::size_t at = 0; at < text.size() && text[at] != '\0'; ++at) {
    const auto glyph = static_cast<std::uint8_t>(text[at] - ' ');
    if (glyph >= kCompactFirstSkipped) {
      x -= glyph == kCompactFirstSkipped ? 1 : 0;
      continue;
    }
    if (glyph == kCompactRightAlign || glyph == kCompactRightAlign + 1) {
      const std::uint32_t width =
          CompactRestWidth(tables, text.substr(at + 1)) >>
          (glyph - kCompactRightAlign);
      x = static_cast<std::int32_t>(static_cast<std::uint32_t>(x) - width);
      continue;
    }
    const std::uint32_t width = CompactWidth(tables, glyph);
    if (width == 0) {
      continue;
    }
    glyphs.push_back(BuildCompactGlyph(tables, glyph, width, x, y, color));
    x += static_cast<std::int32_t>(width);
    x -= static_cast<std::int32_t>(
        CompactKerning(tables, glyph, CharacterAt(text, at + 1)));
  }
  // Each packet goes in front of the chain, so the last glyph is queued
  // first and the draw mode leads the chain.
  for (auto glyph = glyphs.rbegin(); glyph != glyphs.rend(); ++glyph) {
    (*glyph)[0] = (queue.ChainLink() & kPacketLinkMask) |
                  (kCompactGlyphLength << kCommandShift);
    if (!queue.Emit(*glyph)) {
      return false;
    }
  }
  return queue.Emit(BuildDrawModePacket(kCompactDrawMode, queue.ChainLink()));
}
}  // namespace VS::Render
