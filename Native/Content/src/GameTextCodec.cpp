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
constexpr unsigned kFontMoveShift = 3;
constexpr unsigned kIgnoredFontMask = 7;
constexpr unsigned kArgumentCount = 10;
constexpr unsigned kPaletteSlotCount = 4;
constexpr int kMaximumByte = kByteRange - 1;
constexpr int kMaximumDigits = kMaximumByte / kArgumentCount;
constexpr int kMaximumDown = kMaximumByte >> kFontMoveShift;
constexpr std::string_view kArgumentPrefix = "arg=";
constexpr unsigned kFontBase = 6;
constexpr unsigned kFontAlternate = 5;
constexpr unsigned kFontJustify = 4;
constexpr unsigned kUpperPalette = 7;
namespace encoding = Core::String;
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

std::expected<int, std::string> IntegerOperand(std::string_view value,
                                               int minimum, int maximum,
                                               std::string_view operand,
                                               std::string_view id,
                                               std::size_t at) {
  int result = 0;
  const auto parsed =
      std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc() || parsed.ptr != value.data() + value.size() ||
      result < minimum || result > maximum) {
    return std::unexpected(Error(id, at,
                                 std::format("{} must be an integer in {}..{}",
                                             operand, minimum, maximum)));
  }
  return result;
}

std::expected<unsigned, std::string> NumberOperand(std::string_view value,
                                                   std::string_view id,
                                                   std::size_t at) {
  constexpr std::string_view kDigits = ",digits=";
  const auto split = value.find(kDigits);
  if (!value.starts_with(kArgumentPrefix) || split == std::string_view::npos) {
    return std::unexpected(
        Error(id, at, "number requires arg=N,digits=auto or arg=N,digits=N"));
  }
  auto argument = IntegerOperand(
      value.substr(kArgumentPrefix.size(), split - kArgumentPrefix.size()), 0,
      kArgumentCount - 1, "integer argument", id, at);
  if (!argument) return std::unexpected(argument.error());
  const auto digits_text = value.substr(split + kDigits.size());
  auto digits =
      digits_text == "auto"
          ? std::expected<int, std::string>(0)
          : IntegerOperand(digits_text, 1, kMaximumDigits, "digits", id, at);
  if (!digits) return std::unexpected(digits.error());
  const auto parameter = static_cast<unsigned>(*digits) * kArgumentCount +
                         static_cast<unsigned>(*argument);
  if (parameter >= kByteRange) {
    return std::unexpected(
        Error(id, at, "number operands do not fit the imported byte encoding"));
  }
  return parameter;
}

std::expected<unsigned, std::string> FontOperand(std::string_view name,
                                                 std::string_view value,
                                                 std::string_view id,
                                                 std::size_t at) {
  if (name == "font" && value == "base") return kFontBase;
  if (name == "font" && value == "alternate") return kFontAlternate;
  if (name == "justify" && value == "toggle") return kFontJustify;
  if (name == "down") {
    auto amount = IntegerOperand(value, 1, kMaximumDown, "down", id, at);
    if (!amount) return std::unexpected(amount.error());
    return static_cast<unsigned>(*amount) << kFontMoveShift;
  }
  if (name == "palette") {
    if (value == "slot=3,bank=upper") return kUpperPalette;
    constexpr std::string_view kSlot = "slot=";
    constexpr std::string_view kBank = ",bank=current";
    if (value.starts_with(kSlot) && value.ends_with(kBank)) {
      auto slot = IntegerOperand(
          value.substr(kSlot.size(),
                       value.size() - kSlot.size() - kBank.size()),
          0, kPaletteSlotCount - 1, "palette slot", id, at);
      if (!slot) return std::unexpected(slot.error());
      return static_cast<unsigned>(*slot);
    }
  }
  return std::unexpected(Error(id, at, "unsupported font/palette operands"));
}

struct Parameter {
  std::uint8_t code = 0;
  unsigned value = 0;
};

std::expected<Parameter, std::string> EncodeParameter(std::string_view name,
                                                      std::string_view value,
                                                      std::string_view id,
                                                      std::size_t at) {
  if (name == "font" || name == "justify" || name == "down" ||
      name == "palette") {
    auto font = FontOperand(name, value, id, at);
    if (!font) return std::unexpected(font.error());
    return Parameter{encoding::kGameTextFontControl, *font};
  }
  if (name == "hex" || name == "decimal") {
    auto number = NumberOperand(value, id, at);
    if (!number) return std::unexpected(number.error());
    return Parameter{name == "hex" ? encoding::kGameTextPrintHex
                                   : encoding::kGameTextPrintDecimal,
                     *number};
  }
  if (name == "string" && value.starts_with(kArgumentPrefix)) {
    auto argument =
        IntegerOperand(value.substr(kArgumentPrefix.size()), 0,
                       kArgumentCount - 1, "string argument", id, at);
    if (!argument) return std::unexpected(argument.error());
    return Parameter{encoding::kGameTextPrintString,
                     static_cast<unsigned>(*argument)};
  }
  std::uint8_t code = 0;
  if (name == "glyph-cost") code = encoding::kGameTextChunkSize;
  if (name == "sound-selector") code = encoding::kGameTextSoundEffect;
  if (name == "speaker-pause") code = encoding::kGameTextSpeakerPause;
  if (code != 0) {
    auto operand = IntegerOperand(value, 0, kMaximumByte, name, id, at);
    if (!operand) return std::unexpected(operand.error());
    return Parameter{code, static_cast<unsigned>(*operand)};
  }
  return std::unexpected(Error(id, at, "unsupported control operands"));
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
    auto spacing = IntegerOperand(value, kSpacingBackLimit - kByteRange,
                                  kSpacingBackLimit - 1, "spacing", id, at);
    if (!spacing) return std::unexpected(spacing.error());
    bytes.push_back(static_cast<std::byte>(Core::String::kGameTextSpacing));
    bytes.push_back(static_cast<std::byte>(*spacing < 0 ? *spacing + kByteRange
                                                        : *spacing));
    return {};
  }
  for (std::size_t index = 0; index < kSymbols.size(); ++index) {
    if (token == kSymbols[index].markup) {
      bytes.push_back(static_cast<std::byte>(kFirstSymbol + index));
      return {};
    }
  }
  const auto colon = token.find(':');
  if (colon == std::string_view::npos) {
    return std::unexpected(
        Error(id, at, "unsupported markup {" + std::string(token) + "}"));
  }
  auto parameter =
      EncodeParameter(token.substr(0, colon), token.substr(colon + 1), id, at);
  if (!parameter) return std::unexpected(parameter.error());
  bytes.push_back(static_cast<std::byte>(parameter->code));
  bytes.push_back(static_cast<std::byte>(parameter->value));
  return {};
}

// decomp: BATTLE printVariableWidthFont (src/BATTLE/BATTLE.PRG/5BF94.c:1253)
void DecodeFontControl(DecodedGameText& text, unsigned parameter) {
  if ((parameter >> kFontMoveShift) != 0) {
    if ((parameter & kIgnoredFontMask) != 0) {
      text.ignored_font_bits.push_back(
          {.markup_offset = text.markup.size(),
           .bits = static_cast<std::uint8_t>(parameter & kIgnoredFontMask)});
    }
    text.markup += std::format("{{down:{}}}", parameter >> kFontMoveShift);
  } else if (parameter == kFontBase || parameter == kFontAlternate) {
    text.markup += parameter == kFontBase ? "{font:base}" : "{font:alternate}";
  } else if (parameter == kFontJustify) {
    text.markup += "{justify:toggle}";
  } else if (parameter == kUpperPalette) {
    text.markup += "{palette:slot=3,bank=upper}";
  } else {
    text.markup += std::format("{{palette:slot={},bank=current}}", parameter);
  }
}

std::expected<void, std::string> DecodeParameter(DecodedGameText& text,
                                                 std::uint8_t code,
                                                 unsigned parameter,
                                                 std::string_view id,
                                                 std::size_t at) {
  switch (code) {
    case encoding::kGameTextChunkSize:
      text.markup += std::format("{{glyph-cost:{}}}", parameter);
      break;
    case encoding::kGameTextSoundEffect:
      text.markup += std::format("{{sound-selector:{}}}", parameter);
      break;
    case encoding::kGameTextSpacing: {
      const int spacing = static_cast<int>(parameter);
      text.markup += std::format("{{spacing:{}}}", spacing >= kSpacingBackLimit
                                                       ? spacing - kByteRange
                                                       : spacing);
      break;
    }
    case encoding::kGameTextFontControl:
      DecodeFontControl(text, parameter);
      break;
    case encoding::kGameTextSpeakerPause:
      text.markup += std::format("{{speaker-pause:{}}}", parameter);
      break;
    case encoding::kGameTextPrintHex:
    case encoding::kGameTextPrintDecimal:
      text.markup +=
          std::format("{{{}:arg={},digits={}}}",
                      code == encoding::kGameTextPrintHex ? "hex" : "decimal",
                      parameter % kArgumentCount,
                      parameter < kArgumentCount
                          ? "auto"
                          : std::to_string(parameter / kArgumentCount));
      break;
    case encoding::kGameTextPrintString:
      if (parameter >= kArgumentCount) {
        return std::unexpected(
            Error(id, at, "string argument is outside the ten import slots"));
      }
      text.markup += std::format("{{string:arg={}}}", parameter);
      break;
    default:
      return std::unexpected(
          Error(id, at, std::format("unsupported control 0x{:02X}", code)));
  }
  return {};
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
    } else if (code >= Core::String::kGameTextChunkSize) {
      if (at + 1 == source.size()) {
        return std::unexpected(Error(id, at, "control has no parameter"));
      }
      const auto value = std::to_integer<std::uint8_t>(source[at + 1]);
      if (auto control = DecodeParameter(result, code, value, id, at);
          !control) {
        return std::unexpected(control.error());
      }
      ++at;
    } else {
      return std::unexpected(
          Error(id, at, std::format("unsupported control 0x{:02X}", code)));
    }
  }
  return std::unexpected(Error(id, source.size(), "missing E7 terminator"));
}

std::expected<std::vector<std::byte>, std::string> EncodeGameText(
    const DecodedGameText& decoded, std::string_view id) {
  const std::string_view markup = decoded.markup;
  std::vector<std::byte> bytes;
  std::size_t provenance = 0;
  for (std::size_t at = 0; at < markup.size();) {
    const auto* ignored = provenance < decoded.ignored_font_bits.size()
                              ? &decoded.ignored_font_bits[provenance]
                              : nullptr;
    if (ignored != nullptr && ignored->markup_offset < at) {
      return std::unexpected(Error(id, ignored->markup_offset,
                                   "stale or unordered font provenance"));
    }
    const auto rest = markup.substr(at);
    if (ignored != nullptr && ignored->markup_offset == at &&
        !rest.starts_with("{down:")) {
      return std::unexpected(
          Error(id, at, "font provenance needs a down token"));
    }
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
      if (ignored != nullptr && ignored->markup_offset == at) {
        if (ignored->bits == 0 || ignored->bits > kIgnoredFontMask) {
          return std::unexpected(
              Error(id, at, "ignored font bits must be 1..7"));
        }
        bytes.back() |= static_cast<std::byte>(ignored->bits);
        ++provenance;
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
  if (provenance != decoded.ignored_font_bits.size()) {
    return std::unexpected(Error(id, markup.size(), "unused font provenance"));
  }
  bytes.push_back(static_cast<std::byte>(Core::String::kGameTextTerminator));
  return bytes;
}

std::expected<std::vector<std::byte>, std::string> EncodeGameText(
    std::string_view markup, std::string_view id) {
  return EncodeGameText(DecodedGameText{.markup = std::string(markup)}, id);
}

}  // namespace VS::Content
