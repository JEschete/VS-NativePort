#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Room.h"
#include "Render/RoomDraw.h"
}

namespace {
using ColorTransform = std::uint32_t (*)(std::uint32_t, std::int16_t,
                                         std::int16_t, std::int16_t,
                                         std::int16_t);
static_assert(
    std::is_same_v<decltype(&Render_TintRoomPrimitiveColor), ColorTransform>);

std::uint32_t ReferenceTint(std::uint32_t color, std::int16_t strength,
                            std::int16_t red, std::int16_t green,
                            std::int16_t blue) {
  const std::array offsets{red, green, blue};
  const double luminance = (5.0 * (color & 255) + 9.0 * ((color >> 8) & 255) +
                            2.0 * ((color >> 16) & 255)) /
                           16.0;
  const double coefficient = strength / 4096.0;
  std::uint32_t result = color & 0xFF000000u;
  for (std::size_t channel = 0; channel < offsets.size(); ++channel) {
    const auto shift = channel * 8;
    const double input = (color >> shift) & 255;
    const double faded = std::clamp(
        std::floor(input + (luminance - input) * coefficient), 0.0, 255.0);
    result |= static_cast<std::uint32_t>(
                  std::clamp(faded + offsets[channel], 0.0, 255.0))
              << shift;
  }
  return result;
}

struct TintPackets {
  std::array<std::uint32_t, 40> words{};
  std::size_t extra;
  std::size_t quad;
  std::size_t end;

  explicit TintPackets(int flags)
      : extra((flags & 2) ? 2 : 0), quad(11 + extra), end(quad + 13 + extra) {
    words.fill(0xB6B6B6B6);
    words[1] = 0x09000012;
    words[quad] = 0x0C000034;
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
      words[2 + vertex * 3] =
          0x34102030u + (static_cast<std::uint32_t>(vertex) << 24);
    }
    for (std::size_t vertex = 0; vertex < 4; ++vertex) {
      words[quad + 1 + vertex * 3] =
          0x3C102030u + (static_cast<std::uint32_t>(vertex) << 24);
    }
  }

  auto Expected(bool include_quad = true) const {
    auto expected = words;
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
      expected[2 + vertex * 3] =
          0x3418242Cu + (static_cast<std::uint32_t>(vertex) << 24);
    }
    if (include_quad) {
      for (std::size_t vertex = 0; vertex < 4; ++vertex) {
        expected[quad + 1 + vertex * 3] =
            0x3C18242Cu + (static_cast<std::uint32_t>(vertex) << 24);
      }
    }
    return expected;
  }
};
}  // namespace

TEST_CASE(
    "Room tint transforms GT3 and GT4 payloads without changing packet data",
    "[render][room-tint]") {
  const int flags = GENERATE(0, 1, 2, 3);
  TintPackets packets(flags);
  const auto expected = packets.Expected();
  Field_RoomTint tint{-1, 1, 2, 0, 0, 1};
  Render_TintRoomGeometryPrimitives(packets.words.data() + 1,
                                    packets.words.data() + packets.end, &tint,
                                    flags);
  CHECK(packets.words == expected);
}

TEST_CASE("Room tint preserves the original stop at a non-room packet",
          "[render][room-tint]") {
  TintPackets packets(0);
  packets.words[packets.quad] = 0x08000034;
  const auto expected = packets.Expected(false);
  Field_RoomTint tint{-1, 1, 2, 0, 0, 1};
  Render_TintRoomGeometryPrimitives(
      packets.words.data() + 1, packets.words.data() + packets.end, &tint, 0);
  CHECK(packets.words == expected);
}

TEST_CASE("Room color tint preserves grayscale weights and clamp order",
          "[render][room-tint]") {
  CHECK(Render_TintRoomPrimitiveColor(0xAA10C020, 4096, 0, 0, 0) == 0xAA787878);
  CHECK(Render_TintRoomPrimitiveColor(0xAA10C020, 2048, 0, 0, 0) == 0xAA449C4C);
  CHECK(Render_TintRoomPrimitiveColor(0xC700FF00, -32768, 40, -40, 0) ==
        0xC700D728);
  CHECK(Render_TintRoomPrimitiveColor(0xC7000000, 0, -512, 508, 0) ==
        0xC700FF00);
  CHECK(Render_TintRoomPrimitiveColor(0xC7FFFFFF, 0, 508, -512, 0) ==
        0xC7FF00FF);
}

TEST_CASE(
    "Room color tint matches every signed16 strength without global state",
    "[render][room-tint]") {
  for (std::uint32_t word = 0; word <= UINT16_MAX; ++word) {
    const auto strength =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(word));
    for (const auto color : {0xC700FF00u, 0xA310C020u}) {
      const auto expected = ReferenceTint(color, strength, 40, -40, 8);
      CHECK(Render_TintRoomPrimitiveColor(color, strength, 40, -40, 8) ==
            expected);
    }
  }
}

TEST_CASE(
    "Room tint postpass uses signed offsets and its strength for every vertex",
    "[render][room-tint]") {
  const int flags = GENERATE(0, 2);
  TintPackets packets(flags);
  auto expected = packets.words;
  for (std::size_t vertex = 0; vertex < 3; ++vertex) {
    const auto index = 2 + vertex * 3;
    expected[index] = ReferenceTint(expected[index], 2048, 508, -512, 12);
  }
  for (std::size_t vertex = 0; vertex < 4; ++vertex) {
    const auto index = packets.quad + 1 + vertex * 3;
    expected[index] = ReferenceTint(expected[index], 2048, 508, -512, 12);
  }
  Field_RoomTint tint{127, -128, 3, 0, 2048, 1};
  Render_TintRoomGeometryPrimitives(packets.words.data() + 1,
                                    packets.words.data() + packets.end, &tint,
                                    flags);
  CHECK(packets.words == expected);
}

TEST_CASE("Zero room tint and an empty primitive range have no side effects",
          "[render][room-tint]") {
  TintPackets packets(0);
  const auto expected = packets.words;
  Field_RoomTint tint{};
  Render_TintRoomGeometryPrimitives(
      packets.words.data() + 1, packets.words.data() + packets.end, &tint, 0);
  CHECK(packets.words == expected);
  Render_TintRoomGeometryPrimitives(nullptr, nullptr, nullptr, -1);
  CHECK(packets.words == expected);
}

TEST_CASE("Original room tint entries resolve to explicit native contracts",
          "[render][room-tint]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x80098E50u);
  REQUIRE(resolved != nullptr);
  const auto transform = reinterpret_cast<ColorTransform>(resolved);
  CHECK(transform == Render_TintRoomPrimitiveColor);
  CHECK(transform(0xAA10C020, 4096, 0, 0, 0) == 0xAA787878);
  using Postpass = void (*)(void*, void*, Field_RoomTint*, int);
  auto* pass = VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                                 0x80098D6Cu);
  REQUIRE(pass != nullptr);
  CHECK(reinterpret_cast<Postpass>(pass) == Render_TintRoomGeometryPrimitives);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098D6Du) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098E51u) == nullptr);
}
