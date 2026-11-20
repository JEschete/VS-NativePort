#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Content/ModuleData.hpp"
#include "Core/Math/FixedPoint.hpp"
#include "Effects/ScreenDistortion/RadialWarpLive.hpp"
#include "Effects/ScreenDistortion/ScreenOverlaysLive.hpp"
#include "Effects/ScreenEffectState.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Legacy::GpuHandle;

namespace {
std::uint32_t Point(int x, int y) {
  return static_cast<std::uint16_t>(x) |
         (static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16);
}

struct WarpFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  VS::Effects::LiveScreenEffectOverlay* saved_overlay =
      VS::Effects::screenEffectOverlay;
  VS::Effects::LiveScreenEffectOverlay overlay{};
  std::array<std::uint32_t, 2048> ordering{};
  std::array<std::uint32_t, 5> triangle{};
  std::array<std::uint8_t, 1296> saved_lookup{};

  WarpFixture() {
    std::copy_n(VS::Core::Math::kArcTangentTable, saved_lookup.size(),
                saved_lookup.begin());
    for (std::size_t index = 0; index < saved_lookup.size(); ++index) {
      VS::Core::Math::kArcTangentTable[index] =
          static_cast<std::uint8_t>((index * 37 + 13) & 255);
    }
    for (unsigned index = 0; index < 192; ++index) {
      const auto value = static_cast<unsigned>(
          std::sqrt(static_cast<double>((index + 64) * 262144)));
      VS::Core::Math::kArcTangentTable[516 + index * 2] =
          static_cast<std::uint8_t>(value & 255);
      VS::Core::Math::kArcTangentTable[517 + index * 2] =
          static_cast<std::uint8_t>(value >> 8);
    }
    overlay.radial_warp_scale = 2048;
    VS::Effects::screenEffectOverlay = &overlay;
    VS::Render::FrameScratchpad() = {};
    VS::Render::FrameScratchpad().ordering_table = ordering.data();
    ordering[0] = 0x12FFFFFF;
    triangle[0] = 0x04000000 | GpuHandle(ordering.data());
    triangle[1] = 0x20010203;
    triangle[2] = Point(32, 64);
    triangle[3] = Point(288, 64);
    triangle[4] = Point(160, 192);
    ordering[2046] = GpuHandle(triangle.data());
  }

  ~WarpFixture() {
    std::copy(saved_lookup.begin(), saved_lookup.end(),
              VS::Core::Math::kArcTangentTable);
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Effects::screenEffectOverlay = saved_overlay;
  }
};

std::int64_t FloorDivide(std::int64_t value, std::int64_t divisor) {
  return value / divisor - (value < 0 && value % divisor != 0);
}

std::int64_t SignedWord(std::uint32_t bits) {
  return bits < 0x80000000u ? bits : std::int64_t{bits} - 0x100000000LL;
}

int SignedHalf(std::uint32_t bits) {
  bits &= 65535;
  return bits < 32768 ? static_cast<int>(bits) : static_cast<int>(bits) - 65536;
}

std::array<std::uint16_t, 444> ReferenceScales(std::uint16_t base,
                                               std::int16_t growth) {
  std::array<std::uint16_t, 444> result{};
  std::int64_t value = std::int64_t{base} * 16;
  for (auto& entry : result) {
    entry = static_cast<std::uint16_t>(FloorDivide(value, 8));
    const auto low = static_cast<std::uint32_t>(value * (growth + 4096));
    value = FloorDivide(SignedWord(low), 4096);
  }
  return result;
}

unsigned ReferenceRadius(int x, int y) {
  const int ir_x = SignedHalf(static_cast<std::uint32_t>(x));
  const int ir_y = SignedHalf(static_cast<std::uint32_t>(y));
  const auto sum = static_cast<std::uint32_t>(std::int64_t{ir_x} * ir_x +
                                              std::int64_t{ir_y} * ir_y);
  if (sum == 0) return 0;
  auto scan = sum;
  const auto sign = sum >> 31;
  unsigned count = 0;
  while (count < 32 && (scan >> 31) == sign) {
    scan *= 2;
    ++count;
  }
  const unsigned aligned = count & ~1u;
  const unsigned shift =
      static_cast<unsigned>(24 - static_cast<int>(aligned)) % 32;
  const auto normalized =
      FloorDivide(SignedWord(sum), std::int64_t{1} << shift);
  const auto offset = static_cast<std::size_t>(516 + (normalized - 64) * 2);
  const unsigned sample = VS::Core::Math::kArcTangentTable[offset] +
                          VS::Core::Math::kArcTangentTable[offset + 1] * 256u;
  const auto scaled = static_cast<std::uint32_t>(
      std::uint64_t{sample} * (std::uint64_t{1} << ((31 - aligned) / 2)));
  const auto radius = FloorDivide(SignedWord(scaled), 4096);
  return std::min(static_cast<unsigned>(radius), 443u);
}

std::uint32_t ReferencePoint(std::uint32_t packed,
                             const std::array<std::uint16_t, 444>& scales) {
  const int x = SignedHalf(packed) - 160;
  const int y = SignedHalf(packed >> 16) - 112;
  const auto scale = scales[ReferenceRadius(x, y)];
  const auto transform = [scale](int component, int center) {
    const auto low =
        static_cast<std::uint32_t>(std::int64_t{component} * scale);
    return static_cast<int>(FloorDivide(SignedWord(low), 8192)) + center;
  };
  return Point(transform(x, 160), transform(y, 112));
}

std::vector<std::uint32_t> Polygon(WarpFixture& fixture, unsigned kind,
                                   unsigned prefixes, unsigned flags) {
  const unsigned count = (kind & 2) != 0 ? 4 : 3;
  const std::size_t stride = 1 + (kind & 1) + ((kind >> 2) & 1);
  const std::size_t length = prefixes + count * stride + ((kind & 4) ? 0 : 1);
  std::vector<std::uint32_t> packet(length + 1, 0xDEADBEEF);
  packet[0] = static_cast<std::uint32_t>(length << 24) |
              GpuHandle(fixture.ordering.data());
  for (unsigned prefix = 0; prefix < prefixes; ++prefix) {
    packet[1 + prefix] = (prefix & 1) ? 0xE6000001 : 0xE1000640;
  }
  packet[1 + prefixes] = ((0x20 + kind * 4 + flags) << 24) | 0x010203;
  constexpr std::array<std::uint32_t, 4> kPoints{0x00400020, 0x00400120,
                                                 0x00C000A0, 0x00E1FFCE};
  for (unsigned vertex = 0; vertex < count; ++vertex) {
    packet[2 + prefixes + std::size_t{vertex} * stride] = kPoints[vertex];
  }
  return packet;
}
void CheckWarpPoints(WarpFixture& fixture,
                     const std::array<std::uint32_t, 3>& points,
                     const std::array<std::uint16_t, 444>& scales) {
  for (unsigned vertex = 0; vertex < points.size(); ++vertex) {
    fixture.triangle[2 + vertex] = points[vertex];
  }
  VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
  for (unsigned vertex = 0; vertex < points.size(); ++vertex) {
    CHECK(fixture.triangle[2 + vertex] ==
          ReferencePoint(points[vertex], scales));
  }
}
}  // namespace

TEST_CASE("The production screen overlay applies radial warp to scene vertices",
          "[effects][screen-warp]") {
  WarpFixture fixture;
  const auto tag = fixture.triangle[0];
  const auto root = fixture.ordering[0];
  VS::Effects::ScreenDistortion::DrawLiveScreenOverlay();
  CHECK(fixture.triangle[2] == Point(96, 88));
  CHECK(fixture.triangle[3] == Point(224, 88));
  CHECK(fixture.triangle[4] == Point(160, 152));
  CHECK(fixture.triangle[0] == tag);
  CHECK(fixture.triangle[1] == 0x20010203);
  CHECK(fixture.ordering[0] == root);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor == nullptr);
}

TEST_CASE(
    "Screen warp preserves every polygon layout prefix and non-coordinate word",
    "[effects][screen-warp]") {
  WarpFixture fixture;
  const unsigned kind = GENERATE(0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u);
  const unsigned prefixes = GENERATE(0u, 1u, 2u);
  const unsigned flags = GENERATE(0u, 1u, 2u, 3u);
  auto packet = Polygon(fixture, kind, prefixes, flags);
  fixture.ordering[2046] = GpuHandle(packet.data());
  auto expected = packet;
  const auto scales = ReferenceScales(2048, 0);
  const std::size_t stride = 1 + (kind & 1) + ((kind >> 2) & 1);
  for (unsigned vertex = 0; vertex < ((kind & 2) ? 4u : 3u); ++vertex) {
    const auto index = 2 + prefixes + std::size_t{vertex} * stride;
    expected[index] = ReferencePoint(expected[index], scales);
  }
  const auto ordering = fixture.ordering;
  VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
  CHECK(packet == expected);
  CHECK(fixture.ordering == ordering);
}

TEST_CASE(
    "Screen warp retains wrapped scale products across signed parameter words",
    "[effects][screen-warp]") {
  WarpFixture fixture;
  std::uint32_t state = 0xF9BD8;
  for (unsigned index = 0; index < 4096; ++index) {
    state = state * 1664525u + 1013904223u;
    const auto base = static_cast<std::uint16_t>(state);
    const auto growth =
        std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(state >> 16));
    fixture.overlay.radial_warp_scale = std::bit_cast<std::int16_t>(base);
    fixture.overlay.radial_warp_growth = growth;
    const auto scales = ReferenceScales(base, growth);
    const std::array<std::uint32_t, 3> points{state, ~state, Point(164, 112)};
    CheckWarpPoints(fixture, points, scales);
  }
}

TEST_CASE(
    "Screen warp retains signed coordinate narrowing and the extreme radius "
    "case",
    "[effects][screen-warp]") {
  WarpFixture fixture;
  fixture.overlay.radial_warp_scale = 4371;
  fixture.overlay.radial_warp_growth = -19;
  const auto scales = ReferenceScales(4371, -19);
  for (unsigned bits = 0; bits <= UINT16_MAX; ++bits) {
    const std::array<std::uint32_t, 3> points{
        static_cast<std::uint32_t>(bits | ((bits ^ 0x8000u) << 16)),
        Point(-32608, -32656), Point(160, 112)};
    CheckWarpPoints(fixture, points, scales);
  }
}

TEST_CASE(
    "Screen warp keeps the original masked-shift prefix lookup near the center",
    "[effects][screen-warp]") {
  WarpFixture fixture;
  fixture.overlay.radial_warp_scale = 4096;
  fixture.overlay.radial_warp_growth = 64;
  const auto scales = ReferenceScales(4096, 64);
  for (const int distance : {0, 1, 2, 3, 4, 7, 8, 15, 16, 443, 444, 1023}) {
    const auto original = Point(160 + distance, 112);
    fixture.triangle[2] = original;
    VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
    CHECK(fixture.triangle[2] == ReferencePoint(original, scales));
  }
}

TEST_CASE(
    "Screen warp leaves the sky ordering root and resident data untouched",
    "[effects][screen-warp]") {
  WarpFixture fixture;
  auto sky = fixture.triangle;
  fixture.ordering[2047] = GpuHandle(sky.data());
  const auto before = sky;
  std::array<std::uint8_t, 1296> lookup{};
  std::copy_n(VS::Core::Math::kArcTangentTable, lookup.size(), lookup.begin());
  VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
  CHECK(sky == before);
  CHECK(std::equal(lookup.begin(), lookup.end(),
                   VS::Core::Math::kArcTangentTable));
}

TEST_CASE("Screen warp no longer publishes an original overlay entry",
          "[effects][screen-warp][redirect-binding]") {
  WarpFixture fixture;
  const auto symbols = VS::Content::ModuleSymbols();
  CHECK(VS::Content::ResolveModuleAddress(symbols, "GIM/SCREFF2.PRG",
                                          0x800F9BD8u) == nullptr);
  VS::Effects::ScreenDistortion::DrawLiveScreenOverlay();
  CHECK(fixture.triangle[2] == Point(96, 88));
}

TEST_CASE(
    "Screen warp follows native packet chains without changing frame scratch",
    "[effects][screen-warp]") {
  WarpFixture fixture;
  auto second = fixture.triangle;
  fixture.triangle[0] = 0x04000000 | GpuHandle(second.data());
  std::array<std::uint32_t, 2> mode{
      0x01000000 | GpuHandle(fixture.triangle.data()), 0xE1000640};
  fixture.ordering[2046] = GpuHandle(mode.data());
  const auto expected_mode = mode;
  std::array<std::byte, sizeof(VS::Render::FrameScratch)> before{}, after{};
  std::memcpy(before.data(), &VS::Render::FrameScratchpad(), before.size());
  VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
  std::memcpy(after.data(), &VS::Render::FrameScratchpad(), after.size());
  CHECK(mode == expected_mode);
  CHECK(fixture.triangle[2] == Point(96, 88));
  CHECK(fixture.triangle[3] == Point(224, 88));
  CHECK(fixture.triangle[4] == Point(160, 152));
  CHECK(second[2] == Point(96, 88));
  CHECK(second[3] == Point(224, 88));
  CHECK(second[4] == Point(160, 152));
  CHECK(before == after);
}

TEST_CASE("Identity screen warp preserves all signed coordinate words",
          "[effects][screen-warp]") {
  WarpFixture fixture;
  fixture.overlay.radial_warp_scale = 4096;
  fixture.triangle[2] = Point(-32768, 32767);
  fixture.triangle[3] = Point(32767, -32768);
  fixture.triangle[4] = Point(-32608, -32656);
  const auto before = fixture.triangle;
  VS::Effects::ScreenDistortion::WarpLivePolygonVertices();
  CHECK(fixture.triangle == before);
}
