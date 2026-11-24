#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/GameTextCodec.hpp"

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
  const auto bytes = GENERATE(BytesOf({0xFA}), BytesOf({0x0A}),
                              BytesOf({0xFB, 6, 0xE7}), BytesOf({0x7F, 0xE7}));
  const auto decoded = content::DecodeGameText(bytes, "text.invalid");
  REQUIRE_FALSE(decoded.has_value());
  CHECK(decoded.error().starts_with("text.invalid: text byte "));
}
