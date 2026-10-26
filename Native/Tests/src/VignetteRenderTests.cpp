#include <array>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"

extern "C" {
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Render/Render.h"
}

namespace {
constexpr int kGuard = 0x13572468;
constexpr std::size_t kTriangleWords = 7;
constexpr std::size_t kRingWords = 10;
constexpr std::size_t kTriangleCount = 16;
constexpr std::size_t kRingCount = 64;
constexpr std::size_t kCornerWords = kTriangleCount * kTriangleWords;
constexpr std::size_t kPacketWords = kCornerWords + kRingCount * kRingWords;

struct VignetteFixture {
  Render_FrameScratch* frame =
      static_cast<Render_FrameScratch*>(Core_ScratchBase());
  Render_FrameScratch saved = *frame;
  alignas(8) std::array<u_long, 8> ordering{};
  std::array<int, kPacketWords + 2> packets{};
  u_long tail = LEGACY_GPU_LINK_END;

  VignetteFixture() {
    ordering.fill(static_cast<u_long>(kGuard));
    packets.fill(kGuard);
    ordering[2] = LegacyGpu_Handle(&tail);
    *frame = {};
    frame->primitiveCursor = packets.data() + 1;
    frame->overlayOrderingTable = ordering.data() + 2;
  }

  ~VignetteFixture() { *frame = saved; }

  int* Triangle(std::size_t index) {
    return packets.data() + 1 + index * kTriangleWords;
  }

  int* Ring(std::size_t index) {
    return packets.data() + 1 + kCornerWords + index * kRingWords;
  }

  void CheckBounds() const {
    REQUIRE(ordering[3] == static_cast<u_long>(kGuard));
    CHECK(ordering[1] == static_cast<u_long>(kGuard));
    CHECK(packets.front() == kGuard);
    CHECK(packets.back() == kGuard);
    CHECK(frame->primitiveCursor == packets.data() + 1 + kPacketWords);
  }
};

void* CheckPacket(void* node, int* expected, std::size_t words) {
  REQUIRE(node == expected);
  CHECK(static_cast<P_TAG*>(node)->len == words - 1);
  return nextPrim(node);
}
}  // namespace

TEST_CASE("Vignette links occupy one word and retain the previous chain",
          "[render][effects][vignette]") {
  VignetteFixture fixture;
  Effects_ScreenDistortion_DrawVignette();

  fixture.CheckBounds();
  void* node = nextPrim(&fixture.ordering[2]);
  for (std::size_t index = kRingCount; index > 0; --index) {
    node = CheckPacket(node, fixture.Ring(index - 1), kRingWords);
  }
  for (std::size_t index = kTriangleCount; index > 0; --index) {
    node = CheckPacket(node, fixture.Triangle(index - 1), kTriangleWords);
  }
  CHECK(node == &fixture.tail);
  CHECK(nextPrim(node) == nullptr);
  CHECK(LegacyGpu_WalkList(&fixture.ordering[2]) == 82);
}

TEST_CASE("Vignette retains every original corner and elliptical ring packet",
          "[render][effects][vignette]") {
  VignetteFixture fixture;
  Effects_ScreenDistortion_DrawVignette();
  REQUIRE(fixture.ordering[3] == static_cast<u_long>(kGuard));
  constexpr std::array kCorners{0xE00140, 0xE00000, 0, 0x140};
  for (std::size_t index = 0; index < kTriangleCount; ++index) {
    const int angle = static_cast<int>((index / 4) * 16 + 6 + index % 4);
    const int* packet = fixture.Triangle(index);
    const std::array actual{packet[1], packet[2], packet[3],
                            packet[4], packet[5], packet[6]};
    const std::array expected{
        0x32FFFFFF, kCorners[index / 4],
        0xA0A0A0,   Effects_ScreenDistortion_EllipsePoint(angle + 1, 0x80),
        0xA0A0A0,   Effects_ScreenDistortion_EllipsePoint(angle, 0x80)};
    CHECK(actual == expected);
  }
  for (std::size_t index = 0; index < kRingCount; ++index) {
    const int angle = static_cast<int>(index);
    const int* packet = fixture.Ring(index);
    const std::array actual{packet[1], packet[2], packet[3],
                            packet[4], packet[5], packet[6],
                            packet[7], packet[8], packet[9]};
    const std::array expected{
        static_cast<int>(0xE1000220u),
        0x3AA0A0A0,
        Effects_ScreenDistortion_EllipsePoint(angle + 1, 0x80),
        0xA0A0A0,
        Effects_ScreenDistortion_EllipsePoint(angle, 0x80),
        0,
        Effects_ScreenDistortion_EllipsePoint(angle + 1, 0x60),
        0,
        Effects_ScreenDistortion_EllipsePoint(angle, 0x60)};
    CHECK(actual == expected);
  }
}

TEST_CASE(
    "Vignette ellipse cardinals retain original angle and radius arithmetic",
    "[render][effects][vignette]") {
  constexpr std::array kAngles{0, 16, 32, 48};
  constexpr std::array kOuter{0x00700150u, 0x00F000A0u, 0x0070FFF0u,
                              0xFFF000A0u};
  constexpr std::array kInner{0x00700124u, 0x00D000A0u, 0x0070001Cu,
                              0x001000A0u};
  for (std::size_t index = 0; index < kAngles.size(); ++index) {
    CHECK(static_cast<std::uint32_t>(Effects_ScreenDistortion_EllipsePoint(
              kAngles[index], 0x80)) == kOuter[index]);
    CHECK(static_cast<std::uint32_t>(Effects_ScreenDistortion_EllipsePoint(
              kAngles[index], 0x60)) == kInner[index]);
  }
}
