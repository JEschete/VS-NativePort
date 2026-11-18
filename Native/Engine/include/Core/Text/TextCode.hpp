#pragma once
// The glyph and control codes of the game's text encoding (decomp:
// vs_string_chars). Text is a byte stream in an open encoding: codes below
// kNonPrinting are glyphs, and the control codes take operand bytes. The
// enumeration is unscoped so the codes compare and store as those bytes.
#include <cstdint>

namespace VS::Core {
// decomp: vs_string_chars (src/include/vs_string.h:3)
enum TextCode : std::uint8_t {
  kNoChunking = 0,
  kZero = 0,
  kFontTableZero = 6,
  kUpperR = 0x1B,
  kLowerJ = 0x2D,
  kSpace = 0x8F,
  kColon = 0x9E,
  kForwardSlash = 0xA1,
  kHyphen = 0xA7,
  kArrow = 0xBC,
  kNonPrinting = 0xE5,
  kConfirm = 0xE6,
  kTerminator = 0xE7,
  kNewline = 0xE8,
  kControl = 0xEC,
  kChunkSize = 0xF8,
  kSpacing = 0xFA,
  kFontTable = 0xFB,
  kPrintHex = 0xFD,
  kPrintDecimal = 0xFE,
  kPrintString = 0xFF,
};
}  // namespace VS::Core
