#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace VS::Content {

struct IgnoredFontBits {
  std::size_t markup_offset = 0;
  std::uint8_t bits = 0;
};

struct DecodedGameText {
  std::string markup;
  // Includes E7. Bytes after this position are provenance, not text.
  std::size_t consumed = 0;
  // FB move-down ignores its low three bits. Keep them out of authoring text.
  std::vector<IgnoredFontBits> ignored_font_bits{};
};

// Offline import codec, not a runtime string-table builder. Decodes English
// glyphs and the verified E5/E6/E8 and F8..FF control vocabulary.
// Literal braces use {{ and }}; special glyphs use named markup.
[[nodiscard]] std::expected<DecodedGameText, std::string> DecodeGameText(
    std::span<const std::byte> source, std::string_view id);
// Produces one E7-terminated string, without import padding. Diagnostics use
// UTF-8 byte positions in the supplied markup.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeGameText(
    std::string_view markup, std::string_view id);
// Exact import reconstruction, including ignored FB bits. Offsets must still
// identify their original move-down tokens; malformed provenance is refused.
[[nodiscard]] std::expected<std::vector<std::byte>, std::string> EncodeGameText(
    const DecodedGameText& decoded, std::string_view id);

// Informational labels retain the existing visible markers for unmapped
// glyphs; this is deliberately less strict than the authoring codec.
void AppendGameGlyphLabel(std::string& text, std::uint8_t glyph);

// Shared offline glyph facts for font import. A literal Unicode character or
// a named symbol token; not a runtime Unicode-to-bytecode encoder.
[[nodiscard]] std::optional<std::string> GameGlyphIdentity(unsigned glyph);

}  // namespace VS::Content
