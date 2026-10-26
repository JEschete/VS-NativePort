#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Render/RoomLighting.h"
}

namespace {
using Interpolator = int (*)(std::uint32_t, Render_ColorCode*,
                             const Render_RoomLightTriangle*);
static_assert(std::is_same_v<decltype(&Render_InterpolateRoomLightTriangle),
                             Interpolator>);

constexpr std::uint32_t Pack(int x, int z) {
  return static_cast<std::uint16_t>(x) |
         (std::uint32_t{static_cast<std::uint16_t>(z)} << 16);
}

Render_RoomLightTriangle PrimaryTriangle() {
  return {0,
          0,
          128,
          128,
          {{0, 0}, {0, 128}, {128, 0}},
          {0x0000FF, 0xFF0000, 0x00FF00}};
}

auto TriangleBytes(const Render_RoomLightTriangle& triangle) {
  std::array<std::byte, sizeof(triangle)> bytes{};
  std::memcpy(bytes.data(), &triangle, bytes.size());
  return bytes;
}

std::int32_t ReferenceArea(const std::int16_t first[2],
                           const std::int16_t second[2],
                           const std::int16_t third[2]) {
  const std::int64_t expanded =
      std::int64_t{first[0]} * second[1] + std::int64_t{second[0]} * third[1] +
      std::int64_t{third[0]} * first[1] - std::int64_t{first[0]} * third[1] -
      std::int64_t{second[0]} * first[1] - std::int64_t{third[0]} * second[1];
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(expanded));
}

bool OriginalColor(std::uint32_t position,
                   const Render_RoomLightTriangle& triangle,
                   Render_ColorCode* color) {
  const std::int16_t point[]{
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(position)),
      std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(position >> 16))};
  const auto area =
      ReferenceArea(triangle.points[0], triangle.points[2], triangle.points[1]);
  const auto first =
      ReferenceArea(point, triangle.points[2], triangle.points[1]);
  const auto second =
      ReferenceArea(point, triangle.points[1], triangle.points[0]);
  const auto third = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(area) - static_cast<std::uint32_t>(first) -
      static_cast<std::uint32_t>(second));
  if (area <= 0 || first < 0 || second < 0 || third < 0) return false;
  auto bytes = std::bit_cast<std::array<unsigned char, 4>>(*color);
  for (std::size_t channel = 0; channel < 3; ++channel) {
    const auto shift = channel * 8;
    const std::uint64_t expanded =
        std::uint64_t{(triangle.colors[0] >> shift) & 255} *
            static_cast<std::uint32_t>(first) +
        std::uint64_t{(triangle.colors[2] >> shift) & 255} *
            static_cast<std::uint32_t>(second) +
        std::uint64_t{(triangle.colors[1] >> shift) & 255} *
            static_cast<std::uint32_t>(third);
    auto quotient =
        static_cast<std::uint32_t>(expanded) / static_cast<std::uint32_t>(area);
    if ((quotient & 0x80000000u) != 0) quotient = 0;
    bytes[channel] =
        static_cast<unsigned char>(quotient > 255 ? 255 : quotient);
  }
  *color = std::bit_cast<Render_ColorCode>(bytes);
  return true;
}

std::uint32_t NextWord(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

struct LightingTriangleImage {
  std::array<std::int16_t, 4> bounds{};
  std::array<std::array<std::int16_t, 2>, 3> points{};
  std::array<std::uint32_t, 3> colors{};
};
static_assert(sizeof(LightingTriangleImage) == 32);

struct LightingFixture {
  Field_RoomRecord saved = Field_roomData;
  struct {
    std::uint32_t ambient = 0x00332211;
    std::uint32_t floor_count = 1;
    std::uint32_t ceiling_count = 0;
    LightingTriangleImage triangle{};
  } image;

  LightingFixture() {
    Field_roomData.isLoaded = 1;
    Field_roomData.lighting = &image;
    Field_roomData.header.lightingLength = sizeof(image);
  }

  ~LightingFixture() { Field_roomData = saved; }
};
}  // namespace

TEST_CASE("Room lighting rejects the original signed32 area overflow",
          "[render][room-lighting]") {
  LightingFixture fixture;
  fixture.image.triangle.bounds = {-32768, -32768, 32767, 32767};
  fixture.image.triangle.points = {std::array<std::int16_t, 2>{-32768, -32768},
                                   std::array<std::int16_t, 2>{-32768, 32767},
                                   std::array<std::int16_t, 2>{32767, -32768}};
  fixture.image.triangle.colors.fill(0x00776655);
  Render_ColorCode color{0, 0, 0, 0x5A};
  // Original NCLIP MAC0: 65535 * 65535 wraps to signed -131071.
  Render_SampleRoomLighting(-1, 0, &color);
  CHECK(color.red == 0x11);
  CHECK(color.green == 0x22);
  CHECK(color.blue == 0x33);
  CHECK(color.code == 0);
}

TEST_CASE("Room light triangles interpolate corners and inclusive edges",
          "[render][room-lighting]") {
  const auto sample = GENERATE(
      std::array{0, 0, 255, 0, 0}, std::array{0, 128, 0, 0, 255},
      std::array{128, 0, 0, 255, 0}, std::array{64, 0, 127, 127, 0},
      std::array{32, 32, 127, 63, 63}, std::array{64, 64, 0, 127, 127});
  const auto triangle = PrimaryTriangle();
  const auto before = TriangleBytes(triangle);
  struct {
    std::uint32_t before = 0x12345678;
    Render_ColorCode color{0, 0, 0, 0xA5};
    std::uint32_t after = 0x87654321;
  } output;
  REQUIRE(Render_InterpolateRoomLightTriangle(Pack(sample[0], sample[1]),
                                              &output.color, &triangle) == 1);
  CHECK(output.color.red == sample[2]);
  CHECK(output.color.green == sample[3]);
  CHECK(output.color.blue == sample[4]);
  CHECK(output.color.code == 0xA5);
  CHECK(output.before == 0x12345678);
  CHECK(output.after == 0x87654321);
  CHECK(TriangleBytes(triangle) == before);
}

TEST_CASE("Rejected room light triangles leave the complete output untouched",
          "[render][room-lighting]") {
  const auto point = GENERATE(std::array{-1, 0}, std::array{0, -1},
                              std::array{129, 0}, std::array{80, 80});
  auto triangle = PrimaryTriangle();
  Render_ColorCode color{17, 34, 51, 0xA5};
  const auto before = std::bit_cast<std::array<unsigned char, 4>>(color);
  CHECK(Render_InterpolateRoomLightTriangle(Pack(point[0], point[1]), &color,
                                            &triangle) == 0);
  CHECK(std::bit_cast<std::array<unsigned char, 4>>(color) == before);
  std::swap(triangle.points[1], triangle.points[2]);
  CHECK(Render_InterpolateRoomLightTriangle(Pack(0, 0), &color, &triangle) ==
        0);
  CHECK(std::bit_cast<std::array<unsigned char, 4>>(color) == before);
  triangle.points[1][0] = 64;
  triangle.points[1][1] = 0;
  triangle.points[2][0] = 128;
  triangle.points[2][1] = 0;
  CHECK(Render_InterpolateRoomLightTriangle(Pack(0, 0), &color, &triangle) ==
        0);
  CHECK(std::bit_cast<std::array<unsigned char, 4>>(color) == before);
}

TEST_CASE("Room light color weights retain the original unsigned32 overflow",
          "[render][room-lighting]") {
  Render_RoomLightTriangle triangle{
      -16000,
      -16000,
      16000,
      16000,
      {{-16000, -16000}, {-16000, 16000}, {16000, -16000}},
      {0xFFFFFF, 0xFFFFFF, 0xFFFFFF}};
  Render_ColorCode color{0, 0, 0, 0xA5};
  REQUIRE(Render_InterpolateRoomLightTriangle(Pack(-1, -1), &color,
                                              &triangle) == 1);
  CHECK(color.red == 3);
  CHECK(color.green == 3);
  CHECK(color.blue == 3);
  CHECK(color.code == 0xA5);
}

TEST_CASE(
    "Room light interpolation matches original widths across signed16 input",
    "[render][room-lighting]") {
  std::uint32_t random = 0x1932ACB1;
  unsigned accepted = 0;
  unsigned rejected = 0;
  for (int iteration = 0; iteration < 4096; ++iteration) {
    Render_RoomLightTriangle triangle{};
    for (auto& point : triangle.points) {
      for (auto& coordinate : point) {
        coordinate = std::bit_cast<std::int16_t>(
            static_cast<std::uint16_t>(NextWord(random) >> 16));
      }
    }
    for (auto& color : triangle.colors) color = NextWord(random);
    const auto input = TriangleBytes(triangle);
    const auto position = NextWord(random);
    Render_ColorCode actual{17, 34, 51, 0xA5};
    auto expected = actual;
    const bool hit = OriginalColor(position, triangle, &expected);
    hit ? ++accepted : ++rejected;
    REQUIRE(Render_InterpolateRoomLightTriangle(position, &actual, &triangle) ==
            hit);
    CHECK(std::bit_cast<std::array<unsigned char, 4>>(actual) ==
          std::bit_cast<std::array<unsigned char, 4>>(expected));
    CHECK(TriangleBytes(triangle) == input);
  }
  CHECK(accepted > 128);
  CHECK(rejected > 128);
}

TEST_CASE("Original room light entry resolves to the typed shared kernel",
          "[render][room-lighting]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* resolved = VS::Content::ResolveModuleAddress(
      symbols, "BATTLE/BATTLE.PRG", 0x80098C3Cu);
  REQUIRE(resolved != nullptr);
  const auto function = reinterpret_cast<Interpolator>(resolved);
  CHECK(function == Render_InterpolateRoomLightTriangle);
  const auto triangle = PrimaryTriangle();
  Render_ColorCode color{0, 0, 0, 0xA5};
  REQUIRE(function(Pack(32, 32), &color, &triangle) == 1);
  CHECK(color.red == 127);
  CHECK(color.green == 63);
  CHECK(color.blue == 63);
  CHECK(color.code == 0xA5);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098C3Du) == nullptr);
}
