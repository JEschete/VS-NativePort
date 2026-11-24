#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Content {

struct DecodedGameText {
  std::string markup;
  // Includes E7. Bytes after this position are provenance, not text.
  std::size_t consumed = 0;
};

// English glyphs and page, confirm, newline and spacing controls. Other
// controls are refused until their authoring semantics are implemented.
// Literal braces use {{ and }}; special glyphs use named markup.
[[nodiscard]] std::expected<DecodedGameText, std::string> DecodeGameText(
    std::span<const std::byte> source, std::string_view id);
// Produces one E7-terminated string, without import padding. Diagnostics use
// UTF-8 byte positions in the supplied markup.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeGameText(
    std::string_view markup, std::string_view id);

// Informational labels retain the existing visible markers for unmapped
// glyphs; this is deliberately less strict than the authoring codec.
void AppendGameGlyphLabel(std::string& text, std::uint8_t glyph);

}  // namespace VS::Content
