// Hand-maintained (tools/port/owned.csv).
// Live binding for the encoded-text formatter (EncodedText.cpp): the refusal
// report and the variable-width font's widths (module data).
#include "Render/EncodedTextLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>

#include "Core/Diagnostics.hpp"
#include "Render/EncodedText.hpp"
#include "Render/Text/VariableFont.hpp"

namespace VS::Render {
namespace {
std::uint64_t refusedText = 0;
}  // namespace

std::uint64_t RefusedLiveEncodedTextCount() noexcept { return refusedText; }

char* encoded_text_live::Finish(std::uint8_t* destination,
                                std::optional<std::size_t> end,
                                const std::source_location& site) {
  if (end) {
    return reinterpret_cast<char*>(destination + *end);
  }
  ++refusedText;
  // The first and then every power-of-two refusal, so a per-frame request
  // cannot flood the log.
  if (std::has_single_bit(refusedText)) {
    VS::Core::Errorf(
        "text",
        "encoded text at %s:%u refused: no destination or source, a "
        "text without a terminator, a string past the ten strings, "
        "references nested deeper than %d or a destination too small "
        "(refusal %" PRIu64 ")",
        site.file_name(), static_cast<unsigned>(site.line()),
        kMaxEncodedTextNesting, refusedText);
  }
  if (destination == nullptr) {
    return nullptr;
  }
  *destination = kTextTerminator;
  return reinterpret_cast<char*>(destination);
}

std::span<const std::uint8_t> LiveVariableFontGlyphWidths() noexcept {
  return {reinterpret_cast<const std::uint8_t*>(variableFontGlyphWidths),
          sizeof variableFontGlyphWidths};
}

int encoded_text_live::MeasureLine(std::span<const std::uint8_t> text) {
  return MeasureEncodedTextLine(text, LiveVariableFontGlyphWidths());
}
}  // namespace VS::Render
