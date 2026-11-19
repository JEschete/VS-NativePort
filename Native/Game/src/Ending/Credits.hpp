#pragma once
// ENDING.PRG's staff roll data: the credits script and the trailing module
// span (glyph widths and the embedded TIM art), the illustration workspace,
// and the disc illustrations' locations and the buffers read into.
#include <cstddef>
#include <cstdint>

#include "Ending/Credits/StaffRollCore.hpp"
#include "Render/DisplayControl.hpp"

namespace VS::Ending::Credits {
inline constexpr std::size_t kScriptBytes = 7040;
// The glyph widths and, after them, the embedded TIMs (EmbeddedArt.cpp).
inline constexpr std::size_t kGlyphWidthSpanBytes = 440244;

// decomp: func_8006A9C0_t (src/ENDING/ENDING.PRG/D4.c:70)
// A TIM block's header; the block's pixels follow it.
struct TimImageBlockHeader {
  /* 0x00 */ std::int32_t block_size_bytes;
  /* 0x04 */ Render::ScreenRect upload_rect;
};

// decomp: func_8006A9C0_t2 (src/ENDING/ENDING.PRG/D4.c:79)
struct TimHeader {
  /* 0x00 */ std::uint8_t identifier;
  /* 0x01 */ std::uint8_t reserved01;
  /* 0x02 */ std::uint8_t reserved02;
  /* 0x03 */ std::uint8_t reserved03;
  /* 0x04 */ std::int32_t mode;
  /* 0x08 */ TimImageBlockHeader first_block;
};

namespace layout {
inline constexpr std::size_t kTimImageBlockHeaderBytes = 0xC;
inline constexpr std::size_t kTimHeaderBytes = 0x14;
static_assert(sizeof(TimImageBlockHeader) == kTimImageBlockHeaderBytes);
static_assert(sizeof(TimHeader) == kTimHeaderBytes);
}  // namespace layout

// decomp: D_8006E3FC (D_8006E3FC@ENDING/ENDING.PRG)
extern std::uint8_t scriptBytes[kScriptBytes];
// decomp: _glyphWidths (_glyphWidths@ENDING/ENDING.PRG)
extern std::uint8_t glyphWidths[kGlyphWidthSpanBytes];
// decomp: D_800DB72C (D_800DB72C@ENDING/ENDING.PRG)
extern std::uint8_t illustrationDecodePhase;
// decomp: D_800DB7B0 (D_800DB7B0@ENDING/ENDING.PRG)
extern TimHeader* illustrationTimWorkspace;
// decomp: D_800DBB6A (D_800DBB6A@ENDING/ENDING.PRG)
extern std::int16_t illustrationTexturePageX;
// decomp: D_800DBB6C (D_800DBB6C@ENDING/ENDING.PRG)
extern std::uint16_t illustrationPaletteY;
// decomp: D_800DC19A (D_800DC19A@ENDING/ENDING.PRG)
extern std::int16_t decodeBudgetStartTick;
// decomp: D_800DC1A8 (D_800DC1A8@ENDING/ENDING.PRG)
extern void* illustrationResources[kIllustrationCount];
// decomp: _illustLbas (_illustLbas@ENDING/ENDING.PRG)
extern std::int32_t illustrationDiscLocations[kIllustrationCount];
// decomp: _illustSizes (_illustSizes@ENDING/ENDING.PRG)
extern std::int32_t illustrationDiscSizes[kIllustrationCount];
}  // namespace VS::Ending::Credits
