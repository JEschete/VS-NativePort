#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <span>

// Game-encoded text (vs_string.h): glyph codes ended by kGameTextTerminator.
// Every operation is bounded by its spans and returns the bytes written; a
// request that does not fit, or a source without a terminator, writes nothing
// and returns 0 instead of running past either buffer.
namespace VS::Core::String {
// Encoding bytes are shared by import codecs and renderers, not owned by
// rendering. Original layout is relevant only at the text import boundary.
inline constexpr std::uint8_t kGameTextFirstControl = 0xE5;
inline constexpr auto kGameTextPageBreak = kGameTextFirstControl;
inline constexpr std::uint8_t kGameTextConfirm = 0xE6;
inline constexpr std::uint8_t kGameTextTerminator = 0xE7;
inline constexpr std::uint8_t kGameTextNewline = 0xE8;
inline constexpr std::uint8_t kGameTextFirstParameterized = 0xEC;
inline constexpr std::uint8_t kGameTextChunkSize = 0xF8;
inline constexpr std::uint8_t kGameTextSoundEffect = 0xF9;
inline constexpr std::uint8_t kGameTextSpacing = 0xFA;
inline constexpr std::uint8_t kGameTextFontControl = 0xFB;
inline constexpr std::uint8_t kGameTextSpeakerPause = 0xFC;
inline constexpr std::uint8_t kGameTextPrintHex = 0xFD;
inline constexpr std::uint8_t kGameTextPrintDecimal = 0xFE;
inline constexpr std::uint8_t kGameTextPrintString = 0xFF;
inline constexpr std::uint8_t kGameTextSpaceGlyph = 0x8F;
inline constexpr std::uint8_t kGameTextMinusGlyph = 0xA7;
// encode_raw("%") in the decomp's vsstring transformer.
inline constexpr std::uint8_t kGameTextPercent = 0x94;

// MENU9.PRG _strcpy (238.c:2176): copies the text and its terminator.
std::size_t CopyGameText(std::span<std::uint8_t> destination,
                         std::span<const std::uint8_t> source) noexcept;

// MENU9.PRG _strcat (238.c:2185): copies the source text and terminator over
// the destination's terminator.
std::size_t AppendGameText(std::span<std::uint8_t> destination,
                           std::span<const std::uint8_t> source) noexcept;

// MENU9.PRG _toVsStringInt (238.c:2348): the decimal digits as digit glyphs
// (each character less '0', so a minus sign becomes 0xFD as on the console),
// then the terminator.
std::size_t FormatGameInteger(std::span<std::uint8_t> destination,
                              std::int32_t value) noexcept;
// The bytes FormatGameInteger writes for `value`.
[[nodiscard]] std::size_t GameIntegerBytes(std::int32_t value) noexcept;

// MENU9.PRG _toVsStringPercent (238.c:2370): the digits, then the four-byte
// vsstring suffix {0x94, 0xE7, 0, 0}: the percent glyph, the terminator and
// the literal's padding (#277).
std::size_t FormatGamePercent(std::span<std::uint8_t> destination,
                              std::int32_t value) noexcept;
[[nodiscard]] std::size_t GamePercentBytes(std::int32_t value) noexcept;
}  // namespace VS::Core::String
