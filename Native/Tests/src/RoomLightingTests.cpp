#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.h"
#include "Field/RoomLightingLive.hpp"
#include "Legacy/Psx.h"
#include "Render/RoomLighting.hpp"

extern "C" {
#include "Field/Gimmicks.h"
#include "Field/Room.h"
#include "Render/Palette.h"
}

namespace {
using VS::Render::RoomLightColor;
using VS::Render::RoomLightLayer;
using VS::Render::RoomLightPoint;
using VS::Render::RoomLightRgb;
using VS::Render::RoomLightTriangle;

RoomLightTriangle PrimaryTriangle() {
  return {
      {0, 0},
      {128, 128},
      {RoomLightPoint{0, 0}, RoomLightPoint{0, 128}, RoomLightPoint{128, 0}},
      {0x0000FF, 0xFF0000, 0x00FF00}};
}

std::int32_t ReferenceArea(RoomLightPoint first, RoomLightPoint second,
                           RoomLightPoint third) {
  const std::int64_t expanded =
      std::int64_t{first.x} * second.z + std::int64_t{second.x} * third.z +
      std::int64_t{third.x} * first.z - std::int64_t{first.x} * third.z -
      std::int64_t{second.x} * first.z - std::int64_t{third.x} * second.z;
  return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(expanded));
}

std::optional<RoomLightRgb> OriginalColor(RoomLightPoint point,
                                          const RoomLightTriangle& triangle) {
  const auto& c = triangle.corners;
  const auto area = ReferenceArea(c[0], c[2], c[1]);
  const auto first = ReferenceArea(point, c[2], c[1]);
  const auto second = ReferenceArea(point, c[1], c[0]);
  const auto third = std::bit_cast<std::int32_t>(
      static_cast<std::uint32_t>(area) - static_cast<std::uint32_t>(first) -
      static_cast<std::uint32_t>(second));
  if (area <= 0 || first < 0 || second < 0 || third < 0) return std::nullopt;
  std::array<std::uint8_t, 3> bytes{};
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
    bytes[channel] = static_cast<std::uint8_t>(quotient > 255 ? 255 : quotient);
  }
  return RoomLightRgb{bytes[0], bytes[1], bytes[2]};
}

std::uint32_t NextWord(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

void PutWord(std::vector<std::byte>& out, std::uint32_t value) {
  for (int index = 0; index < 4; ++index) {
    out.push_back(static_cast<std::byte>(value >> (index * 8)));
  }
}

void PutHalf(std::vector<std::byte>& out, std::int16_t value) {
  const auto bits = std::bit_cast<std::uint16_t>(value);
  out.push_back(static_cast<std::byte>(bits));
  out.push_back(static_cast<std::byte>(bits >> 8));
}

void PutTriangle(std::vector<std::byte>& out, const RoomLightTriangle& t) {
  PutHalf(out, t.minimum.x);
  PutHalf(out, t.minimum.z);
  PutHalf(out, t.maximum.x);
  PutHalf(out, t.maximum.z);
  for (const auto& corner : t.corners) {
    PutHalf(out, corner.x);
    PutHalf(out, corner.z);
  }
  for (const auto color : t.colors) PutWord(out, color);
}

std::vector<std::byte> LightingBlock(
    std::uint32_t ambient, std::uint32_t floor_count,
    std::uint32_t ceiling_count,
    std::initializer_list<RoomLightTriangle> triangles) {
  std::vector<std::byte> out;
  PutWord(out, ambient);
  PutWord(out, floor_count);
  PutWord(out, ceiling_count);
  for (const auto& triangle : triangles) PutTriangle(out, triangle);
  return out;
}

struct LiveLightingFixture {
  Field_RoomRecord saved = Field_roomData;
  std::vector<std::byte> image;

  explicit LiveLightingFixture(std::vector<std::byte> block)
      : image(std::move(block)) {
    Field_roomData.isLoaded = 1;
    Field_roomData.lighting = image.data();
    Field_roomData.header.lightingLength =
        static_cast<std::uint32_t>(image.size());
  }

  ~LiveLightingFixture() { Field_roomData = saved; }
};
}  // namespace

TEST_CASE("Room light records decode little-endian bounds, corners and colours",
          "[render][room-lighting]") {
  const RoomLightTriangle triangle{
      {-2, 3},
      {300, -400},
      {RoomLightPoint{-32768, 1}, RoomLightPoint{2, 32767},
       RoomLightPoint{-5, -6}},
      {0x00112233, 0xFFEEDDCC, 0x01020304}};
  const auto bytes = LightingBlock(0, 0, 0, {triangle});
  REQUIRE(bytes.size() == VS::Render::kRoomLightingHeaderBytes +
                              VS::Render::kRoomLightTriangleBytes);
  const auto record = std::span(bytes)
                          .subspan(VS::Render::kRoomLightingHeaderBytes)
                          .first<VS::Render::kRoomLightTriangleBytes>();
  CHECK(VS::Render::DecodeRoomLightTriangle(record) == triangle);
  CHECK(VS::Render::UnpackRoomLightPoint(0x8001FFFFu) ==
        RoomLightPoint{-1, -32767});
}

TEST_CASE("Room lighting rejects the original signed32 area overflow",
          "[render][room-lighting]") {
  RoomLightTriangle triangle{
      {-32768, -32768},
      {32767, 32767},
      {RoomLightPoint{-32768, -32768}, RoomLightPoint{-32768, 32767},
       RoomLightPoint{32767, -32768}},
      {0x00776655, 0x00776655, 0x00776655}};
  LiveLightingFixture fixture(LightingBlock(0x00332211, 1, 0, {triangle}));
  Render_ColorCode color{0, 0, 0, 0x5A};
  // Original NCLIP MAC0: 65535 * 65535 wraps to signed -131071.
  VS::Field::SampleLiveRoomLighting(-1, 0, &color);
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
  const auto rgb = VS::Render::InterpolateRoomLightTriangle(
      {static_cast<std::int16_t>(sample[0]),
       static_cast<std::int16_t>(sample[1])},
      triangle);
  REQUIRE(rgb.has_value());
  CHECK(*rgb == RoomLightRgb{static_cast<std::uint8_t>(sample[2]),
                             static_cast<std::uint8_t>(sample[3]),
                             static_cast<std::uint8_t>(sample[4])});
  CHECK(triangle == PrimaryTriangle());
}

TEST_CASE("Rejected room light triangles yield no colour",
          "[render][room-lighting]") {
  const auto point = GENERATE(std::array{-1, 0}, std::array{0, -1},
                              std::array{129, 0}, std::array{80, 80});
  auto triangle = PrimaryTriangle();
  CHECK_FALSE(VS::Render::InterpolateRoomLightTriangle(
      {static_cast<std::int16_t>(point[0]),
       static_cast<std::int16_t>(point[1])},
      triangle));
  std::swap(triangle.corners[1], triangle.corners[2]);
  CHECK_FALSE(VS::Render::InterpolateRoomLightTriangle({0, 0}, triangle));
  triangle.corners[1] = {64, 0};
  triangle.corners[2] = {128, 0};
  CHECK_FALSE(VS::Render::InterpolateRoomLightTriangle({0, 0}, triangle));
}

TEST_CASE("Room light color weights retain the original unsigned32 overflow",
          "[render][room-lighting]") {
  const RoomLightTriangle triangle{
      {-16000, -16000},
      {16000, 16000},
      {RoomLightPoint{-16000, -16000}, RoomLightPoint{-16000, 16000},
       RoomLightPoint{16000, -16000}},
      {0xFFFFFF, 0xFFFFFF, 0xFFFFFF}};
  CHECK(VS::Render::InterpolateRoomLightTriangle({-1, -1}, triangle) ==
        RoomLightRgb{3, 3, 3});
}

TEST_CASE(
    "Room light interpolation matches original widths across signed16 input",
    "[render][room-lighting]") {
  std::uint32_t random = 0x1932ACB1;
  unsigned accepted = 0;
  unsigned rejected = 0;
  for (int iteration = 0; iteration < 4096; ++iteration) {
    RoomLightTriangle triangle{};
    for (auto& corner : triangle.corners) {
      corner.x = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(NextWord(random) >> 16));
      corner.z = std::bit_cast<std::int16_t>(
          static_cast<std::uint16_t>(NextWord(random) >> 16));
    }
    for (auto& color : triangle.colors) color = NextWord(random);
    const auto point = VS::Render::UnpackRoomLightPoint(NextWord(random));
    const auto expected = OriginalColor(point, triangle);
    expected ? ++accepted : ++rejected;
    REQUIRE(VS::Render::InterpolateRoomLightTriangle(point, triangle) ==
            expected);
  }
  CHECK(accepted > 128);
  CHECK(rejected > 128);
}

TEST_CASE("Room lighting keeps the code byte only for interpolated samples",
          "[render][room-lighting]") {
  auto ceiling = PrimaryTriangle();
  ceiling.colors = {0x806040, 0x806040, 0x806040};
  const auto block =
      LightingBlock(0x44332211, 1, 1, {PrimaryTriangle(), ceiling});
  const RoomLightColor current{9, 9, 9, 0x5A};
  CHECK(VS::Render::SampleRoomLighting(block, {32, 32}, RoomLightLayer::kFloor,
                                       current) ==
        RoomLightColor{127, 63, 63, 0x5A});
  CHECK(VS::Render::SampleRoomLighting(block, {32, 32},
                                       RoomLightLayer::kCeiling, current) ==
        RoomLightColor{64, 96, 128, 0x5A});
  CHECK(VS::Render::SampleRoomLighting(block, {200, 200},
                                       RoomLightLayer::kFloor, current) ==
        RoomLightColor{0x11, 0x22, 0x33, 0x44});
  CHECK(VS::Render::SampleRoomLighting({}, {0, 0}, RoomLightLayer::kFloor,
                                       current) ==
        RoomLightColor{128, 128, 128, 0});
}

TEST_CASE("Room lighting bounds counts to the complete records present",
          "[render][room-lighting]") {
  const RoomLightColor current{1, 2, 3, 4};
  const RoomLightColor ambient{0x11, 0x22, 0x33, 0};
  auto block = LightingBlock(0x00332211, 2, 0, {PrimaryTriangle()});
  CHECK(VS::Render::SampleRoomLighting(block, {32, 32}, RoomLightLayer::kFloor,
                                       current) == ambient);
  block = LightingBlock(0x00332211, 1, 1, {PrimaryTriangle()});
  CHECK(VS::Render::SampleRoomLighting(
            block, {32, 32}, RoomLightLayer::kCeiling, current) == ambient);
  block = LightingBlock(0x00332211, 0xFFFFFFFFu, 1, {PrimaryTriangle()});
  CHECK(VS::Render::SampleRoomLighting(
            block, {32, 32}, RoomLightLayer::kCeiling, current) == ambient);
  block = LightingBlock(0x00332211, 1, 0, {PrimaryTriangle()});
  block.pop_back();
  CHECK(VS::Render::SampleRoomLighting(block, {32, 32}, RoomLightLayer::kFloor,
                                       current) == ambient);
  block.resize(VS::Render::kRoomLightingHeaderBytes - 1);
  CHECK(VS::Render::SampleRoomLighting(block, {32, 32}, RoomLightLayer::kFloor,
                                       current) ==
        RoomLightColor{128, 128, 128, 0});
}

TEST_CASE("Live room lighting ignores unloaded rooms and null colours",
          "[render][room-lighting]") {
  LiveLightingFixture fixture(
      LightingBlock(0x00332211, 1, 0, {PrimaryTriangle()}));
  VS::Field::SampleLiveRoomLighting((32 << 16) | 32, 0, nullptr);
  Render_ColorCode color{0, 0, 0, 0xA5};
  VS::Field::SampleLiveRoomLighting((32 << 16) | 32, 0, &color);
  CHECK((color.red == 127 && color.green == 63 && color.blue == 63 &&
         color.code == 0xA5));
  Field_roomData.isLoaded = 0;
  VS::Field::SampleLiveRoomLighting((32 << 16) | 32, 0, &color);
  CHECK((color.red == 128 && color.green == 128 && color.blue == 128 &&
         color.code == 0));
}

TEST_CASE("Live room lighting samples a named layer as the original index",
          "[render][room-lighting]") {
  auto ceiling = PrimaryTriangle();
  ceiling.colors = {0x806040, 0x806040, 0x806040};
  LiveLightingFixture fixture(
      LightingBlock(0x44332211, 1, 1, {PrimaryTriangle(), ceiling}));
  constexpr std::uint32_t kPacked = (32U << 16) | 32U;
  Render_ColorCode floor{9, 9, 9, 0x5A};
  VS::Field::SampleLiveRoomLighting(kPacked, RoomLightLayer::kFloor, &floor);
  CHECK((floor.red == 127 && floor.green == 63 && floor.blue == 63 &&
         floor.code == 0x5A));
  Render_ColorCode floor_index{9, 9, 9, 0x5A};
  VS::Field::SampleLiveRoomLighting(static_cast<int>(kPacked), 0, &floor_index);
  CHECK(std::memcmp(&floor, &floor_index, sizeof floor) == 0);

  Render_ColorCode ceiling_color{9, 9, 9, 0x5A};
  VS::Field::SampleLiveRoomLighting(kPacked, RoomLightLayer::kCeiling,
                                    &ceiling_color);
  CHECK((ceiling_color.red == 64 && ceiling_color.green == 96 &&
         ceiling_color.blue == 128 && ceiling_color.code == 0x5A));
  Render_ColorCode ceiling_index{9, 9, 9, 0x5A};
  VS::Field::SampleLiveRoomLighting(static_cast<int>(kPacked), 1,
                                    &ceiling_index);
  CHECK(std::memcmp(&ceiling_color, &ceiling_index, sizeof ceiling_color) == 0);
  VS::Field::SampleLiveRoomLighting(kPacked, RoomLightLayer::kFloor, nullptr);
}

TEST_CASE("Original room light entries have no module address",
          "[render][room-lighting]") {
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098B38u) == nullptr);
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x80098C3Cu) == nullptr);
  // The retired ground colour wrapper (146C.c:16465 func_8008C458).
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x8008C458u) == nullptr);
}
