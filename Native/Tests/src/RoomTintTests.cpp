#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
#include "Render/RoomGeometry.h"
}

namespace {
using Vertex = Render_RoomVertex;
using Midpoint = void (*)(const Vertex*, const Vertex*, Vertex*);
static_assert(
    std::is_same_v<decltype(&Render_InterpolateRoomVertexMidpoint), Midpoint>);
using ScreenBounds = Render_RoomScreenBounds;
using Visibility = int (*)(const ScreenBounds*);
static_assert(
    std::is_same_v<decltype(&Render_IsRoomPolygonOnScreen), Visibility>);

struct ResetRoomClip {
  ~ResetRoomClip() { Render_SetRoomPolygonScreenBounds(0, 0, 320, 224); }
};

bool ReferenceVisibility(const std::array<DVECTOR, 4>& vertices,
                         const ScreenBounds& clip) {
  return !std::all_of(vertices.begin(), vertices.end(), [&](DVECTOR v) {
    return v.vy >= clip.bottom;
  }) && !std::all_of(vertices.begin(), vertices.end(), [&](DVECTOR v) {
    return v.vy < clip.top;
  }) && !std::all_of(vertices.begin(), vertices.end(), [&](DVECTOR v) {
    return v.vx < clip.left;
  }) && !std::all_of(vertices.begin(), vertices.end(), [&](DVECTOR v) {
    return v.vx >= clip.right;
  });
}

template <typename T>
auto ObjectBytes(const T& object) {
  std::array<std::byte, sizeof object> bytes;
  std::memcpy(bytes.data(), &object, bytes.size());
  return bytes;
}

void CheckVisibility(const std::array<DVECTOR, 4>& vertices,
                     const ScreenBounds& clip) {
  ScreenBounds bounds{INT_MAX, INT_MAX, INT_MIN, INT_MIN};
  for (const auto vertex : vertices) {
    bounds.left = std::min(bounds.left, static_cast<int>(vertex.vx));
    bounds.right = std::max(bounds.right, static_cast<int>(vertex.vx));
    bounds.top = std::min(bounds.top, static_cast<int>(vertex.vy));
    bounds.bottom = std::max(bounds.bottom, static_cast<int>(vertex.vy));
  }
  const auto before = ObjectBytes(bounds);
  CHECK(Render_IsRoomPolygonOnScreen(&bounds) ==
        ReferenceVisibility(vertices, clip));
  CHECK(ObjectBytes(bounds) == before);
}

std::uint32_t NextScreenSample(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

void CheckScreenSample(std::uint32_t& state) {
  std::array<int, 4> wide, wrapped;
  for (std::size_t i = 0; i < wide.size(); ++i) {
    const auto bits = NextScreenSample(state);
    wide[i] = std::bit_cast<std::int32_t>(bits);
    wrapped[i] = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
  }
  Render_SetRoomPolygonScreenBounds(wide[0], wide[1], wide[2], wide[3]);
  const ScreenBounds clip{wrapped[0], wrapped[1], wrapped[2], wrapped[3]};
  std::array<DVECTOR, 4> vertices;
  for (auto& vertex : vertices) {
    vertex.vx = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(NextScreenSample(state) >> 16));
    vertex.vy = std::bit_cast<std::int16_t>(
        static_cast<std::uint16_t>(NextScreenSample(state) >> 16));
  }
  CheckVisibility(vertices, clip);
}

Vertex ReferenceMidpoint(const Vertex& first, const Vertex& second,
                         const Vertex& destination) {
  Vertex result;
  std::memcpy(&result, &destination, sizeof result);
  result.position.vx = static_cast<std::int16_t>(
      std::floor((first.position.vx + second.position.vx) / 2.0));
  result.position.vy = static_cast<std::int16_t>(
      std::floor((first.position.vy + second.position.vy) / 2.0));
  result.position.vz = static_cast<std::int16_t>(
      std::floor((first.position.vz + second.position.vz) / 2.0));
  result.color &= 0xFF000000u;
  for (unsigned shift = 0; shift < 24; shift += 8) {
    result.color |=
        (((first.color >> shift & 255) + (second.color >> shift & 255)) / 2)
        << shift;
  }
  result.uv = static_cast<std::uint16_t>(
      ((first.uv & 255) + (second.uv & 255)) / 2 +
      (((first.uv >> 8) + (second.uv >> 8)) / 2) * 256);
  return result;
}

void CheckMidpointOutput(const Vertex& first, const Vertex& second) {
  struct GuardedVertex {
    unsigned before;
    Vertex value;
    unsigned after;
  };
  GuardedVertex output;
  std::memset(&output, 0xA5, sizeof output);
  GuardedVertex expected;
  std::memcpy(&expected, &output, sizeof expected);
  const auto vertex = ReferenceMidpoint(first, second, output.value);
  std::memcpy(&expected.value, &vertex, sizeof vertex);
  const auto first_before = ObjectBytes(first);
  const auto second_before = ObjectBytes(second);
  Render_InterpolateRoomVertexMidpoint(&first, &second, &output.value);
  CHECK(ObjectBytes(output) == ObjectBytes(expected));
  CHECK(ObjectBytes(first) == first_before);
  CHECK(ObjectBytes(second) == second_before);
}

void CheckMidpointAliases(const Vertex& first, const Vertex& second) {
  Vertex output;
  std::memcpy(&output, &first, sizeof output);
  const auto first_expected = ReferenceMidpoint(first, second, first);
  const auto second_before = ObjectBytes(second);
  Render_InterpolateRoomVertexMidpoint(&output, &second, &output);
  CHECK(ObjectBytes(output) == ObjectBytes(first_expected));
  CHECK(ObjectBytes(second) == second_before);
  std::memcpy(&output, &second, sizeof output);
  const auto second_expected = ReferenceMidpoint(first, second, second);
  const auto first_before = ObjectBytes(first);
  Render_InterpolateRoomVertexMidpoint(&first, &output, &output);
  CHECK(ObjectBytes(output) == ObjectBytes(second_expected));
  CHECK(ObjectBytes(first) == first_before);
  std::memcpy(&output, &first, sizeof output);
  Render_InterpolateRoomVertexMidpoint(&output, &output, &output);
  CHECK(ObjectBytes(output) == first_before);
}

void CheckMidpointSample(std::uint32_t bits) {
  Vertex first, second;
  std::memset(&first, 0x19, sizeof first);
  std::memset(&second, 0xE2, sizeof second);
  first.position.vx =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits));
  first.position.vy =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits ^ 0x8000u));
  first.position.vz = first.position.vx;
  second.position.vx = INT16_MAX;
  second.position.vy = INT16_MIN;
  second.position.vz =
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(bits * 7 + 13));
  const auto low = bits & 255, high = bits >> 8;
  first.color = 0x19000000u | low | ((255 - low) << 8) | (low << 16);
  second.color = 0xE2000000u | high | (high << 8) | ((255 - high) << 16);
  first.uv = static_cast<std::uint16_t>(high | ((255 - high) << 8));
  second.uv = static_cast<std::uint16_t>(low | (low << 8));
  CheckMidpointOutput(first, second);
  CheckMidpointAliases(first, second);
}

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

TEST_CASE("Room screen predicate matches original per-vertex rejection",
          "[render][room-screen]") {
  const ResetRoomClip reset;
  std::uint32_t state = 0x80098014u;
  for (unsigned sample = 0; sample <= UINT16_MAX; ++sample) {
    CheckScreenSample(state);
  }
}

TEST_CASE("Room visibility keeps inclusive origins and exclusive extents",
          "[render][room-screen]") {
  const ResetRoomClip reset;
  Render_SetRoomPolygonScreenBounds(0, 0, 320, 224);
  constexpr std::array<DVECTOR, 10> kPoints{{{0, 0},
                                             {319, 223},
                                             {320, 223},
                                             {319, 224},
                                             {-1, 0},
                                             {0, -1},
                                             {INT16_MIN, 0},
                                             {INT16_MAX, 0},
                                             {0, INT16_MIN},
                                             {0, INT16_MAX}}};
  for (const auto point : kPoints) {
    std::array<DVECTOR, 4> vertices;
    vertices.fill(point);
    CheckVisibility(vertices, {0, 0, 320, 224});
  }
  CheckVisibility({{{-1, -1}, {320, -1}, {-1, 224}, {320, 224}}},
                  {0, 0, 320, 224});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x80098014u);
  REQUIRE(resolved != nullptr);
  const auto visible = reinterpret_cast<Visibility>(resolved);
  CHECK(visible == Render_IsRoomPolygonOnScreen);
  ScreenBounds inside{0, 0, 319, 223}, outside{320, 0, 320, 223};
  CHECK(visible(&inside) == 1);
  CHECK(visible(&outside) == 0);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098015u) == nullptr);
}

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

TEST_CASE("Room vertex midpoint preserves signed floors and every byte pair",
          "[render][room-midpoint]") {
  for (std::uint32_t bits = 0; bits <= UINT16_MAX; ++bits) {
    CheckMidpointSample(bits);
  }
}

TEST_CASE(
    "Room midpoint binding exposes native vertices without register context",
    "[render][room-midpoint]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x80097F70u);
  REQUIRE(resolved != nullptr);
  const auto midpoint = reinterpret_cast<Midpoint>(resolved);
  CHECK(midpoint == Render_InterpolateRoomVertexMidpoint);
  Vertex first{{-1, INT16_MAX, INT16_MIN, 0}, 0xAA0000FFu, 0xFF00u};
  Vertex second{{0, INT16_MIN, INT16_MAX, 0}, 0x55FF0000u, 0x00FFu};
  Vertex output{{0, 0, 0, 123}, 0xCCFFFFFFu, 0};
  midpoint(&first, &second, &output);
  CHECK(output.position.vx == -1);
  CHECK(output.position.vy == -1);
  CHECK(output.position.vz == -1);
  CHECK(output.position.pad == 123);
  CHECK(output.color == 0xCC7F007Fu);
  CHECK(output.uv == 0x7F7Fu);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80097F71u) == nullptr);
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
