// Hand-maintained (tools/port/owned.csv).
// Live binding for the encoded-text formatter (EncodedText.cpp): the battle
// string context and the variable-width font's widths (module data).
#include "Render/EncodedTextLive.hpp"

#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>

#include "Render/EncodedText.hpp"

extern "C" {
#include "Battle/Targeting.h"
#include "Core/Diagnostics.h"
#include "Render/Text/VariableFont.h"
}

namespace VS::Render {
namespace {
std::uint64_t refused_text = 0;
}  // namespace

std::uint64_t RefusedLiveEncodedTextCount() noexcept { return refused_text; }

EncodedTextArguments LiveEncodedTextArguments() {
  EncodedTextArguments arguments;
  for (std::size_t index = 0; index < kEncodedTextArgumentCount; ++index) {
    arguments.integers.at(index) = Battle_stringContext.integers[index];
    arguments.strings.at(index) =
        encoded_text_live::TrustedText(Battle_stringContext.strings[index]);
  }
  return arguments;
}

char* encoded_text_live::Finish(std::uint8_t* destination,
                                std::optional<std::size_t> end,
                                const std::source_location& site) {
  if (end) {
    return reinterpret_cast<char*>(destination + *end);
  }
  ++refused_text;
  // The first and then every power-of-two refusal, so a per-frame request
  // cannot flood the log.
  if (std::has_single_bit(refused_text)) {
    Core_Errorf("text",
                "encoded text at %s:%u refused: no destination or source, a "
                "text without a terminator, a string past the ten strings, "
                "references nested deeper than %d or a destination too small "
                "(refusal %" PRIu64 ")",
                site.file_name(), static_cast<unsigned>(site.line()),
                kMaxEncodedTextNesting, refused_text);
  }
  if (destination == nullptr) {
    return nullptr;
  }
  *destination = kTextTerminator;
  return reinterpret_cast<char*>(destination);
}

std::span<const std::uint8_t> LiveVariableFontGlyphWidths() noexcept {
  return {reinterpret_cast<const std::uint8_t*>(Render_variableFontGlyphWidths),
          sizeof Render_variableFontGlyphWidths};
}

int encoded_text_live::MeasureLine(std::span<const std::uint8_t> text) {
  return MeasureEncodedTextLine(text, LiveVariableFontGlyphWidths());
}
}  // namespace VS::Render
