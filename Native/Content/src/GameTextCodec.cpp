#include "Content/GameTextCodec.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "Core/String/GameText.hpp"

namespace VS::Content {
namespace {
constexpr std::uint8_t kFirstSign = 0x86;
constexpr std::uint8_t kFirstPunctuation = 0x8F;
constexpr std::uint8_t kFirstSymbol = 0xAD;
constexpr std::uint8_t kTildeGlyph = 0xB9;
constexpr std::uint8_t kOpenBraceGlyph = 0xAB;
constexpr std::uint8_t kCloseBraceGlyph = 0xAC;
constexpr int kSpacingBackLimit = 0xF0;
constexpr int kByteRange = 256;
constexpr std::string_view kAlphabet{
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
};
// English glyph identities: upstream tools/etc/vsString.py:6-39.
constexpr std::array<std::uint32_t, 45> kAccents{
    0x0152, 0x00C0, 0x00C1, 0x00C2, 0x00C4, 0x00C7, 0x00C8, 0x00C9, 0x00CA,
    0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, 0x00D2, 0x00D3, 0x00D4, 0x00D6,
    0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DF, 0x0153, 0x00E0, 0x00E1, 0x00E2,
    0x00E4, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE,
    0x00EF, 0x00F2, 0x00F3, 0x00F4, 0x00F6, 0x00F9, 0x00FA, 0x00FB, 0x00FC,
};
constexpr std::array<std::uint32_t, 9> kSigns{
    0x201E, 0x203C, 0x2260, 0x2266, 0x2267, 0x00F7, 0x00B7, 0x2014, 0x22EF,
};
constexpr std::string_view kPunctuation = " !\"#$%&'()=@[];:,./\\<>?_-+*`{}";

struct Symbol {
  std::string_view markup;
  std::string_view label;
};
constexpr std::array<Symbol, 12> kSymbols{{
    {"music", "[music]"},
    {"btn:triangle", "[triangle]"},
    {"btn:square", "[square]"},
    {"btn:circle", "[circle]"},
    {"btn:cross", "[cross]"},
    {"arrow:left", "[left]"},
    {"arrow:right", "[right]"},
    {"arrow:up", "[up]"},
    {"arrow:down", "[down]"},
    {"level", "Lv."},
    {"star", "[star]"},
    {"block", "[block]"},
}};

std::string Utf8(std::uint32_t code) {
  constexpr std::uint32_t kAsciiLimit = 0x80;
  constexpr std::uint32_t kTwoByteLimit = 0x800;
  constexpr std::uint32_t kTwoBytePrefix = 0xC0;
  constexpr std::uint32_t kThreeBytePrefix = 0xE0;
  constexpr std::uint32_t kContinuationPrefix = 0x80;
  constexpr std::uint32_t kPayloadMask = 0x3F;
  constexpr unsigned kPayloadBits = 6;
  std::string text;
  if (code < kAsciiLimit) {
    text += static_cast<char>(code);
  } else if (code < kTwoByteLimit) {
    text += static_cast<char>(kTwoBytePrefix | (code >> kPayloadBits));
    text += static_cast<char>(kContinuationPrefix | (code & kPayloadMask));
  } else {
    text += static_cast<char>(kThreeBytePrefix | (code >> (2 * kPayloadBits)));
    text += static_cast<char>(kContinuationPrefix |
                              ((code >> kPayloadBits) & kPayloadMask));
    text += static_cast<char>(kContinuationPrefix | (code & kPayloadMask));
  }
  return text;
}

std::optional<std::string> Literal(std::uint8_t glyph) {
  if (glyph < kAlphabet.size()) return std::string(1, kAlphabet[glyph]);
  if (glyph < kAlphabet.size() + kAccents.size()) {
    return Utf8(kAccents[glyph - kAlphabet.size()]);
  }
  if (glyph >= kFirstSign && glyph < kFirstSign + kSigns.size()) {
    return Utf8(kSigns[glyph - kFirstSign]);
  }
  if (glyph >= kFirstPunctuation &&
      glyph < kFirstPunctuation + kPunctuation.size()) {
    return std::string(1, kPunctuation[glyph - kFirstPunctuation]);
  }
  if (glyph == kTildeGlyph) return "~";
  return std::nullopt;
}

std::string Error(std::string_view id, std::size_t at,
                  std::string_view reason) {
  return std::format("{}: text byte {}: {}", id, at, reason);
}

std::expected<void, std::string> AppendGlyph(std::string& text,
                                             std::uint8_t code,
                                             std::string_view id,
                                             std::size_t at) {
  if (auto literal = Literal(code)) {
    if (code == kOpenBraceGlyph || code == kCloseBraceGlyph) text += *literal;
    text += *literal;
    return {};
  }
  if (code >= kFirstSymbol && code < kFirstSymbol + kSymbols.size()) {
    text += std::format("{{{}}}", kSymbols[code - kFirstSymbol].markup);
    return {};
  }
  return std::unexpected(
      Error(id, at, std::format("unmapped English glyph 0x{:02X}", code)));
}

std::expected<void, std::string> AppendControl(std::vector<std::byte>& bytes,
                                               std::string_view token,
                                               std::string_view id,
                                               std::size_t at) {
  if (token == "page" || token == "confirm") {
    bytes.push_back(static_cast<std::byte>(
        token == "page" ? Core::String::kGameTextPageBreak
                        : Core::String::kGameTextConfirm));
    return {};
  }
  if (token.starts_with("spacing:")) {
    const auto value = token.substr(std::string_view("spacing:").size());
    int spacing = 0;
    const auto parsed =
        std::from_chars(value.data(), value.data() + value.size(), spacing);
    if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size() ||
        spacing < kSpacingBackLimit - kByteRange ||
        spacing >= kSpacingBackLimit) {
      return std::unexpected(
          Error(id, at, "spacing must be an integer in -16..239"));
    }
    bytes.push_back(static_cast<std::byte>(Core::String::kGameTextSpacing));
    bytes.push_back(
        static_cast<std::byte>(spacing < 0 ? spacing + kByteRange : spacing));
    return {};
  }
  for (std::size_t index = 0; index < kSymbols.size(); ++index) {
    if (token == kSymbols[index].markup) {
      bytes.push_back(static_cast<std::byte>(kFirstSymbol + index));
      return {};
    }
  }
  return std::unexpected(
      Error(id, at, "unsupported markup {" + std::string(token) + "}"));
}

std::expected<std::size_t, std::string> EncodeGlyph(
    std::vector<std::byte>& bytes, std::string_view text, std::string_view id,
    std::size_t at) {
  for (std::uint8_t code = 0; code < Core::String::kGameTextFirstControl;
       ++code) {
    const auto literal = Literal(code);
    if (literal && text.starts_with(*literal)) {
      bytes.push_back(static_cast<std::byte>(code));
      return literal->size();
    }
  }
  return std::unexpected(
      Error(id, at, "unencodable character or invalid UTF-8"));
}
}  // namespace

void AppendGameGlyphLabel(std::string& text, std::uint8_t glyph) {
  if (auto literal = Literal(glyph)) {
    text += *literal;
  } else if (glyph >= kFirstSymbol && glyph < kFirstSymbol + kSymbols.size()) {
    text += kSymbols[glyph - kFirstSymbol].label;
  } else {
    text += std::format("[glyph {:02X}]", glyph);
  }
}

std::expected<DecodedGameText, std::string> DecodeGameText(
    std::span<const std::byte> source, std::string_view id) {
  DecodedGameText result;
  for (std::size_t at = 0; at < source.size(); ++at) {
    const auto code = std::to_integer<std::uint8_t>(source[at]);
    if (code == Core::String::kGameTextTerminator) {
      result.consumed = at + 1;
      return result;
    }
    if (code < Core::String::kGameTextFirstControl) {
      if (auto glyph = AppendGlyph(result.markup, code, id, at); !glyph) {
        return std::unexpected(glyph.error());
      }
    } else if (code == Core::String::kGameTextNewline) {
      result.markup += '\n';
    } else if (code == Core::String::kGameTextPageBreak ||
               code == Core::String::kGameTextConfirm) {
      result.markup +=
          code == Core::String::kGameTextPageBreak ? "{page}" : "{confirm}";
    } else if (code == Core::String::kGameTextSpacing) {
      if (at + 1 == source.size()) {
        return std::unexpected(
            Error(id, at, "spacing control has no parameter"));
      }
      const int value = std::to_integer<std::uint8_t>(source[++at]);
      result.markup +=
          std::format("{{spacing:{}}}",
                      value >= kSpacingBackLimit ? value - kByteRange : value);
    } else {
      return std::unexpected(
          Error(id, at, std::format("unsupported control 0x{:02X}", code)));
    }
  }
  return std::unexpected(Error(id, source.size(), "missing E7 terminator"));
}

std::expected<std::vector<std::byte>, std::string> EncodeGameText(
    std::string_view markup, std::string_view id) {
  std::vector<std::byte> bytes;
  for (std::size_t at = 0; at < markup.size();) {
    const auto rest = markup.substr(at);
    if (rest.starts_with("{{") || rest.starts_with("}}")) {
      bytes.push_back(static_cast<std::byte>(
          rest.front() == '{' ? kOpenBraceGlyph : kCloseBraceGlyph));
      at += 2;
    } else if (rest.front() == '{') {
      const auto end = markup.find('}', at + 1);
      if (end == std::string_view::npos) {
        return std::unexpected(Error(id, at, "unclosed markup"));
      }
      if (auto control =
              AppendControl(bytes, markup.substr(at + 1, end - at - 1), id, at);
          !control) {
        return std::unexpected(control.error());
      }
      at = end + 1;
    } else if (rest.front() == '}') {
      return std::unexpected(
          Error(id, at, "literal closing brace must be escaped as }}"));
    } else if (rest.front() == '\n') {
      bytes.push_back(static_cast<std::byte>(Core::String::kGameTextNewline));
      ++at;
    } else {
      auto count = EncodeGlyph(bytes, rest, id, at);
      if (!count) return std::unexpected(count.error());
      at += *count;
    }
  }
  bytes.push_back(static_cast<std::byte>(Core::String::kGameTextTerminator));
  return bytes;
}

}  // namespace VS::Content
