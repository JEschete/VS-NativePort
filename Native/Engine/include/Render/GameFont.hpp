#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "Render/UiPackets.hpp"

// The menu and dialogue fonts. The variable-width font draws game-encoded
// glyphs from a 21-glyph-wide sheet of 12x12 cells in two 189-glyph tables
// with a FontStyle; the compact font draws ASCII through its width, kerning
// and cell tables. Both build packets as words for a PacketQueue, so the
// policies reach no global render state (GameFontLive.hpp binds them).
namespace VS::Render {
inline constexpr std::size_t kVariableFontGlyphCount = 413;
inline constexpr std::uint32_t kVariableFontGlyphsPerTable = 189;
inline constexpr std::uint32_t kVariableFontGlyphsPerRow = 21;
inline constexpr int kVariableFontSpaceAdvance = 6;
// The continuation arrow's three animation glyphs end at this code.
inline constexpr std::uint32_t kTextArrowGlyph = 0xBC;

// BATTLE.PRG's _fontTable, D_800F4CB8 and _fontBrightness (5BF94.c): the
// glyph table, the CLUT row offset and the sprite's brightness word. The text
// box drawers and the menus change it and later glyphs draw with it.
struct FontStyle {
  std::uint8_t table_select = 0;
  std::uint8_t palette_offset = 0;
  std::uint32_t color = 0;
};

// The opaque menu brightness vs_battle_setFontStyle sets.
inline constexpr std::uint32_t kMenuFontColor = 384;

// BATTLE.PRG vs_battle_setFontStyle (5BF94.c:1699): table 0, the menu
// brightness and the palette.
void SetMenuFontStyle(FontStyle& style, std::uint8_t palette_offset) noexcept;

// BATTLE.PRG vs_battle_printVariableWidthFontChar (5BF94.c:1678): queues the
// glyph's sprite at (x, y) and returns the next pen x. Space advances six
// pixels without drawing. A glyph whose table entry is past the width table
// is neither drawn nor advanced (the original read past it).
int DrawVariableWidthGlyph(PacketQueue& queue, const FontStyle& style,
                           std::span<const std::uint8_t> glyph_widths,
                           std::uint32_t glyph, int x, int y);

// The compact font's tables (Render/Text/CompactFont.cpp): two 4-bit widths a
// byte, 2-bit kerning trims for each capital pair and the cell of each glyph.
struct CompactFontTables {
  std::span<const std::uint8_t> widths;
  std::span<const std::uint8_t> left_trim;
  std::span<const std::uint8_t> indices;
};

inline constexpr std::uint32_t kCompactTextGray = 0x808080;

// BATTLE.PRG vs_battle_renderTextRawColor (5BF94.c:1345) and
// vs_battle_renderTextRaw (5BF94.c:1436): draws the ASCII text from packed
// position x | y << 16 in the colour, in front of the queue's chain. '#'
// right-aligns and '$' centres the rest of the text on the pen; '[' moves the
// pen back one pixel; other characters past 'Z' are skipped. The text ends at
// its first NUL. Returns false when a packet was dropped; the packets already
// queued stay linked.
bool DrawCompactText(PacketQueue& queue, const CompactFontTables& tables,
                     std::string_view text, std::int32_t position,
                     std::uint32_t color);
}  // namespace VS::Render
