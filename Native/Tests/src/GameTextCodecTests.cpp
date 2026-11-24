#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/GameTextCodec.hpp"
#include "Render/EncodedText.hpp"

namespace {
namespace content = VS::Content;
using Bytes = std::vector<std::byte>;

Bytes BytesOf(std::initializer_list<unsigned> values) {
  Bytes bytes;
  for (const auto value : values) {
    bytes.push_back(static_cast<std::byte>(value));
  }
  return bytes;
}

std::vector<std::uint8_t> Encoded(std::string_view markup) {
  const auto bytes = content::EncodeGameText(markup, "text.format");
  if (!bytes) FAIL(bytes.error());
  std::vector<std::uint8_t> result;
  for (const auto byte : *bytes) {
    result.push_back(std::to_integer<std::uint8_t>(byte));
  }
  return result;
}
}  // namespace

TEST_CASE("English text preserves Unicode, symbols, braces and named controls",
          "[game-text-codec]") {
  const auto bytes =
      BytesOf({0x0A, 0x6A, 0xFA, 6, 0xAB, 0xAC, 0xB0, 0xE8, 0xE6, 0xE5, 0xE7});
  const auto decoded = content::DecodeGameText(bytes, "text.example");
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->markup ==
        "A\xC3\xBC{spacing:6}{{}}{btn:circle}\n{confirm}{page}");
  CHECK(decoded->consumed == bytes.size());
  CHECK(content::EncodeGameText(decoded->markup, "text.example") == bytes);
}

TEST_CASE(
    "Text termination excludes provenance and does not end inside parameters",
    "[game-text-codec]") {
  const auto bytes = BytesOf({0xFA, 0xE7, 0xE7, 0xFB});
  const auto decoded = content::DecodeGameText(bytes, "text.padding");
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->markup == "{spacing:231}");
  CHECK(decoded->consumed == 3);
  auto encoded = content::EncodeGameText(decoded->markup, "text.padding");
  if (!encoded) FAIL(encoded.error());
  encoded->insert(
      encoded->end(),
      bytes.begin() + static_cast<std::ptrdiff_t>(decoded->consumed),
      bytes.end());
  CHECK(*encoded == bytes);
}

TEST_CASE("Every spacing parameter preserves its signed rendering meaning",
          "[game-text-codec]") {
  for (unsigned value = 0; value < 256; ++value) {
    const auto bytes = BytesOf({0xFA, value, 0xE7});
    const auto decoded = content::DecodeGameText(bytes, "text.spacing");
    if (!decoded) FAIL(decoded.error());
    CHECK(content::EncodeGameText(decoded->markup, "text.spacing") == bytes);
  }
}

TEST_CASE(
    "The level glyph is distinct from three ordinary letters and punctuation",
    "[game-text-codec]") {
  const auto symbol = content::EncodeGameText("{level}", "text.level");
  CHECK(symbol == BytesOf({0xB6, 0xE7}));
  CHECK(content::EncodeGameText("Lv.", "text.level") ==
        BytesOf({0x15, 0x39, 0xA0, 0xE7}));
}

TEST_CASE("Every mapped English glyph has an exact inverse",
          "[game-text-codec]") {
  for (unsigned code = 0; code <= 0xB9; ++code) {
    if (code >= 0x6B && code < 0x86) continue;
    const auto bytes = BytesOf({code, 0xE7});
    const auto decoded = content::DecodeGameText(bytes, "text.glyph");
    if (!decoded) FAIL(decoded.error());
    CHECK(content::EncodeGameText(decoded->markup, "text.glyph") == bytes);
  }
}

TEST_CASE("Bad markup and unencodable UTF-8 identify the string and position",
          "[game-text-codec]") {
  const std::string text =
      GENERATE("A{wait:30}", "A{spacing:-17}", "A{spacing:240}", "A{spacing:}",
               "A{spacing:6", "A}", "A\xC0\xAF", "A\xF0\x9F\x98\x80");
  const auto encoded = content::EncodeGameText(text, "text.invalid");
  REQUIRE_FALSE(encoded.has_value());
  CHECK(encoded.error().starts_with("text.invalid: text byte 1:"));
}

TEST_CASE("Unsupported or malformed encoded controls fail explicitly",
          "[game-text-codec]") {
  const auto bytes =
      GENERATE(BytesOf({0xFA}), BytesOf({0x0A}), BytesOf({0xEC, 6, 0xE7}),
               BytesOf({0x7F, 0xE7}), BytesOf({0xFF, 10, 0xE7}));
  const auto decoded = content::DecodeGameText(bytes, "text.invalid");
  REQUIRE_FALSE(decoded.has_value());
  CHECK(decoded.error().starts_with("text.invalid: text byte "));
}

TEST_CASE("Parameterized text decodes named operands rather than byte lists",
          "[game-text-codec]") {
  const auto bytes =
      BytesOf({0xF8, 2, 0xF9, 12, 0xFC, 20, 0xFD, 43, 0xFE, 2, 0xFF, 1, 0xE7});
  const auto decoded = content::DecodeGameText(bytes, "text.controls");
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->markup ==
        "{glyph-cost:2}{sound-selector:12}{speaker-pause:20}"
        "{hex:arg=3,digits=4}{decimal:arg=2,digits=auto}{string:arg=1}");
  CHECK(content::EncodeGameText(*decoded, "text.controls") == bytes);
}

TEST_CASE(
    "Font commands distinguish movement, banks, palettes and justification",
    "[game-text-codec]") {
  const auto bytes = BytesOf(
      {0xFB, 0, 0xFB, 3, 0xFB, 4, 0xFB, 5, 0xFB, 6, 0xFB, 7, 0xFB, 8, 0xE7});
  const auto decoded = content::DecodeGameText(bytes, "text.font");
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->markup ==
        "{palette:slot=0,bank=current}{palette:slot=3,bank=current}"
        "{justify:toggle}{font:alternate}{font:base}"
        "{palette:slot=3,bank=upper}{down:1}");
  CHECK(content::EncodeGameText(decoded->markup, "text.font") == bytes);
}

TEST_CASE("Ignored font bits remain import provenance at UTF-8 token positions",
          "[game-text-codec]") {
  const auto bytes = BytesOf({0x0A, 0x6A, 0xFB, 0x1D, 0xE7});
  const auto decoded = content::DecodeGameText(bytes, "text.font");
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->markup == "A\xC3\xBC{down:3}");
  REQUIRE(decoded->ignored_font_bits.size() == 1);
  CHECK(decoded->ignored_font_bits.front().markup_offset == 3);
  CHECK(decoded->ignored_font_bits.front().bits == 5);
  CHECK(content::EncodeGameText(*decoded, "text.font") == bytes);
  CHECK(content::EncodeGameText(decoded->markup, "text.font") ==
        BytesOf({0x0A, 0x6A, 0xFB, 0x18, 0xE7}));
}

TEST_CASE("Every valid text parameter survives offline reconstruction",
          "[game-text-codec]") {
  const auto code =
      GENERATE(0xF8U, 0xF9U, 0xFAU, 0xFBU, 0xFCU, 0xFDU, 0xFEU, 0xFFU);
  const unsigned count = code == 0xFF ? 10 : 256;
  for (unsigned parameter = 0; parameter < count; ++parameter) {
    CAPTURE(code, parameter);
    const auto bytes = BytesOf({code, parameter, 0xE7, 0xAA});
    const auto decoded = content::DecodeGameText(bytes, "text.parameter");
    if (!decoded) FAIL(decoded.error());
    CHECK(decoded->consumed == 3);
    CHECK(content::EncodeGameText(*decoded, "text.parameter") ==
          BytesOf({code, parameter, 0xE7}));
  }
}

TEST_CASE(
    "A parameterized text opcode needs its operand and a later terminator",
    "[game-text-codec]") {
  const auto code =
      GENERATE(0xF8U, 0xF9U, 0xFAU, 0xFBU, 0xFCU, 0xFDU, 0xFEU, 0xFFU);
  const auto truncated = content::DecodeGameText(BytesOf({code}), "text.short");
  REQUIRE_FALSE(truncated.has_value());
  CHECK(truncated.error() ==
        "text.short: text byte 0: control has no parameter");
  const auto no_end = content::DecodeGameText(BytesOf({code, 0}), "text.short");
  REQUIRE_FALSE(no_end.has_value());
  CHECK(no_end.error() == "text.short: text byte 2: missing E7 terminator");
}

TEST_CASE("Control operands reject impossible or ambiguous import encodings",
          "[game-text-codec]") {
  const std::string markup =
      GENERATE("{glyph-cost:256}", "{sound-selector:-1}", "{speaker-pause:2x}",
               "{font:5}", "{justify:on}", "{down:0}", "{down:32}",
               "{palette:slot=4,bank=current}", "{palette:slot=2,bank=upper}",
               "{decimal:arg=10,digits=1}", "{hex:arg=1,digits=0}",
               "{decimal:arg=6,digits=25}", "{decimal:1,4}", "{string:arg=10}",
               "{decimal:arg=0,digits=auto,extra=1}");
  const auto encoded = content::EncodeGameText(markup, "text.bad-operands");
  REQUIRE_FALSE(encoded.has_value());
  CHECK(encoded.error().starts_with("text.bad-operands: text byte 0:"));
}

TEST_CASE("Invalid or stale font provenance cannot silently alter other text",
          "[game-text-codec]") {
  using Bits = content::IgnoredFontBits;
  const auto provenance =
      GENERATE(std::vector<Bits>{{0, 1}}, std::vector<Bits>{{2, 1}},
               std::vector<Bits>{{1, 0}}, std::vector<Bits>{{1, 8}},
               std::vector<Bits>{{100, 1}}, std::vector<Bits>{{1, 1}, {1, 2}},
               std::vector<Bits>{{10, 1}, {1, 2}});
  const content::DecodedGameText decoded{.markup = "A{down:3}B{down:4}",
                                         .ignored_font_bits = provenance};
  const auto encoded = content::EncodeGameText(decoded, "text.provenance");
  REQUIRE_FALSE(encoded.has_value());
  CHECK(encoded.error().starts_with("text.provenance: text byte "));
}

TEST_CASE("Named number and string operands preserve the production formatter",
          "[game-text-codec]") {
  namespace render = VS::Render;
  const auto text =
      Encoded("{string:arg=1} {decimal:arg=2,digits=4} {hex:arg=3,digits=4}");
  const auto name = Encoded("HP");
  render::EncodedTextArguments arguments;
  arguments.strings[1] = name;
  arguments.integers[2] = 321;
  arguments.integers[3] = -175;
  std::array<std::uint8_t, 64> output{};
  const auto end = render::FormatEncodedText(output, text, arguments);
  if (!end) FAIL("Named operands did not fit the formatter output");
  const auto decoded = content::DecodeGameText(
      std::as_bytes(std::span(output).first(*end + 1)), "text.formatted");
  if (!decoded) FAIL(decoded.error());
  CHECK(decoded->markup == "HP 0321 -00AF");
}
