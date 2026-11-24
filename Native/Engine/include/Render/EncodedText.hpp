#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/String/GameText.hpp"

// Game-encoded text (vs_string.h): codes below kTextFirstControl are glyphs,
// kTextFirstControl to kTextFirstParameterized - 1 are one-byte controls, and
// the codes from kTextFirstParameterized take one parameter byte. Formatting
// expands the integer and string references from an EncodedTextArguments, so
// the policy reaches no battle string context (EncodedTextLive.hpp binds it).
// The end of a span reads as its terminator.
namespace VS::Render {
inline constexpr auto kTextFirstControl = Core::String::kGameTextFirstControl;
inline constexpr auto kTextPageBreak = Core::String::kGameTextPageBreak;
inline constexpr auto kTextConfirm = Core::String::kGameTextConfirm;
inline constexpr auto kTextTerminator = Core::String::kGameTextTerminator;
inline constexpr auto kTextNewline = Core::String::kGameTextNewline;
inline constexpr auto kTextFirstParameterized =
    // NOLINTNEXTLINE(whitespace/indent_namespace): wrapped constant alias.
    Core::String::kGameTextFirstParameterized;
inline constexpr auto kTextChunkSize = Core::String::kGameTextChunkSize;
inline constexpr auto kTextSoundEffect = Core::String::kGameTextSoundEffect;
inline constexpr auto kTextSpacing = Core::String::kGameTextSpacing;
inline constexpr auto kTextFontControl = Core::String::kGameTextFontControl;
inline constexpr auto kTextSpeakerPause = Core::String::kGameTextSpeakerPause;
inline constexpr auto kTextPrintHex = Core::String::kGameTextPrintHex;
inline constexpr auto kTextPrintDecimal = Core::String::kGameTextPrintDecimal;
inline constexpr auto kTextPrintString = Core::String::kGameTextPrintString;
inline constexpr auto kTextSpaceGlyph = Core::String::kGameTextSpaceGlyph;
inline constexpr auto kTextMinusGlyph = Core::String::kGameTextMinusGlyph;

// The battle string context's ten integers and ten strings
// (vs_battle_stringContext_t, 5BF94.h:165).
inline constexpr std::size_t kEncodedTextArgumentCount = 10;

struct EncodedTextArguments {
  std::array<std::int32_t, kEncodedTextArgumentCount> integers{};
  // An empty span is a string that was never set.
  std::array<std::span<const std::uint8_t>, kEncodedTextArgumentCount>
      strings{};
};

// A minus sign and at most ten digits.
inline constexpr std::size_t kMaxEncodedIntegerBytes = 11;

struct EncodedInteger {
  std::array<std::uint8_t, kMaxEncodedIntegerBytes> bytes{};
  std::size_t size = 0;

  [[nodiscard]] std::span<const std::uint8_t> View() const noexcept {
    return std::span(bytes).first(size);
  }
};

// The digit glyphs kTextPrintHex or kTextPrintDecimal print for `parameter`
// (BATTLE.PRG vs_battle_printf, 5BF94.c:1445). The parameter's units digit
// selects the integer. Below 10 it prints every significant digit (zero as one
// digit); from 10 it prints parameter / 10 low digits, so a hex count of nine
// or more prints none. A negative integer starts with kTextMinusGlyph and is
// negated with 32-bit wrap. Decimal counts above nine wrap their divisor as
// the original's 32-bit multiply did, which can give negative digits.
[[nodiscard]] EncodedInteger EncodeTextInteger(std::uint8_t code,
                                               std::uint8_t parameter,
                                               std::int32_t value) noexcept;

// The original recursed through string references without a limit.
inline constexpr int kMaxEncodedTextNesting = 8;

// The bytes FormatEncodedText writes, terminator included, or nothing when it
// would refuse the source.
[[nodiscard]] std::optional<std::size_t> EncodedTextBytes(
    std::span<const std::uint8_t> source,
    const EncodedTextArguments& arguments) noexcept;

// BATTLE.PRG vs_battle_printf (5BF94.c:1445): copies the source into the
// destination with kTextPrintHex, kTextPrintDecimal and kTextPrintString
// expanded and other parameterised codes copied with their parameter, then
// terminates it. Returns the terminator's offset. Refused, leaving the
// destination unchanged, when a source or string has no terminator, a string
// index is past the ten strings, references nest deeper than
// kMaxEncodedTextNesting or the destination is too small.
std::optional<std::size_t> FormatEncodedText(
    std::span<std::uint8_t> destination, std::span<const std::uint8_t> source,
    const EncodedTextArguments& arguments) noexcept;

// BATTLE.PRG vs_battle_getTextLineLength (5BF94.c:1654): the font-table-0
// width of the text before its next newline, page break or terminator, in
// six-pixel units. Spacing and decimal-print parameters add their width; the
// other codes from kTextChunkSize skip their parameter. A glyph past the
// width table adds nothing.
[[nodiscard]] int MeasureEncodedTextLine(
    std::span<const std::uint8_t> text,
    std::span<const std::uint8_t> glyph_widths) noexcept;
}  // namespace VS::Render
