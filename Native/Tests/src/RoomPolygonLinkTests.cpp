#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Legacy/Psx.h"

extern "C" {
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/RoomDraw.h"
#include "Render/RoomGeometry.h"
#include "Render/UiPrimitives.h"
}

namespace {
using NativeLink = int (*)(void*, std::size_t, Core_GteWord, unsigned);

struct LinkFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  int saved_framebuffer = Render_frameBuf;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  std::vector<u_long> memory =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 2, 0xA5A5A5A5);
  std::array<u_long, 2048> ordering{};

  LinkFixture() {
    Render_frameScratch = {};
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = Packet();
    Render_frameScratch.primitiveCursor = Packet();
    Render_frameScratch.orderingTable = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_SetRoomPolygonNearClip(8);
    Render_SetRoomPolygonFarClip(2047);
  }

  ~LinkFixture() {
    Render_frameScratch = saved_frame;
    Render_frameBuf = saved_framebuffer;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_SetRoomPolygonNearClip(8);
    Render_SetRoomPolygonFarClip(2047);
  }

  u_long* Packet() { return memory.data() + 1; }
};

void CheckDepth(Core_GteWord raw_depth, int bias, int far_depth, bool quad) {
  LinkFixture fixture;
  Render_SetRoomPolygonFarClip(far_depth);
  const auto before_order = fixture.ordering;
  const auto before_memory = fixture.memory;
  const auto biased = std::int64_t{raw_depth} + bias;
  const bool expected =
      raw_depth >= 8 && biased >= 8 && biased < far_depth && biased < 2047;
  const auto flags =
      (static_cast<unsigned>(static_cast<std::uint16_t>(bias)) << 16) | 0xA5A5u;
  const auto words = quad ? 13u : 10u;
  CHECK(Render_LinkRoomPolygonByDepth(fixture.Packet(), words, raw_depth,
                                      flags) == expected);
  CHECK(Render_frameScratch.primitiveCursor == fixture.Packet());
  if (expected) {
    CHECK(nextPrim(&fixture.ordering[static_cast<std::size_t>(biased)]) ==
          fixture.Packet());
    CHECK((fixture.Packet()[0] >> 24) == words - 1);
    CHECK(std::equal(fixture.memory.begin() + 2, fixture.memory.end(),
                     before_memory.begin() + 2));
  } else {
    CHECK(fixture.ordering == before_order);
    CHECK(fixture.memory == before_memory);
  }
}
}  // namespace

TEST_CASE("Room polygon linking exposes packet span and explicit native depth",
          "[render][room-link]") {
  CHECK((std::is_same_v<decltype(&Render_LinkRoomPolygonByDepth), NativeLink>));
}

TEST_CASE(
    "Room depth linking retains source near far and signed-bias boundaries",
    "[render][room-link]") {
  constexpr std::array<Core_GteWord, 10> kDepths{
      INT32_MIN, -1, 0, 7, 8, 9, 2046, 2047, 16383, INT32_MAX};
  constexpr std::array<int, 7> kBias{-32768, -8, -1, 0, 1, 8, 32767};
  constexpr std::array<int, 6> kFar{-1, 0, 8, 256, 2047, 32767};
  for (Core_GteWord depth : kDepths) {
    for (int bias : kBias) {
      for (int far_depth : kFar) {
        CheckDepth(depth, bias, far_depth, false);
        CheckDepth(depth, bias, far_depth, true);
      }
    }
  }
}

TEST_CASE(
    "Room packet linking keeps native OT chains and does not emit body bytes",
    "[render][room-link]") {
  LinkFixture fixture;
  auto* triangle = fixture.Packet();
  auto* quad = triangle + 10;
  auto* previous = nextPrim(&fixture.ordering[256]);
  REQUIRE(Render_LinkRoomPolygonByDepth(triangle, 10, 256, 0) == 1);
  REQUIRE(Render_LinkRoomPolygonByDepth(quad, 13, 256, 0) == 1);
  CHECK(nextPrim(&fixture.ordering[256]) == quad);
  CHECK(nextPrim(quad) == triangle);
  CHECK(nextPrim(triangle) == previous);
  CHECK(Render_frameScratch.primitiveCursor == triangle);
  CHECK(fixture.memory.back() == 0xA5A5A5A5);
}

TEST_CASE("Rejected biased room links cannot address before the ordering table",
          "[render][room-link]") {
  LinkFixture fixture;
  Render_SetRoomPolygonNearClip(-32768);
  const auto before = fixture.ordering;
  CHECK(Render_LinkRoomPolygonByDepth(fixture.Packet(), 10, 0, 0xFFFF0000u) ==
        0);
  CHECK(fixture.ordering == before);
  CHECK(fixture.Packet()[0] == 0xA5A5A5A5);
}

TEST_CASE(
    "Room link preserves configurable far cutoff and the physical OT bound",
    "[render][room-link]") {
  CheckDepth(2046, 0, 32767, false);
  CheckDepth(2047, 0, 32767, false);
  CheckDepth(255, 0, 256, true);
  CheckDepth(256, 0, 256, true);
}

TEST_CASE(
    "Original room depth-link entry resolves to the typed native OT helper",
    "[render][room-link][redirect-binding]") {
  LinkFixture fixture;
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  auto* entry = VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                                  0x800980F8u);
  REQUIRE(entry != nullptr);
  const auto link = reinterpret_cast<NativeLink>(entry);
  CHECK(link == Render_LinkRoomPolygonByDepth);
  CHECK(link(fixture.Packet(), 10, 256, 0) == 1);
  CHECK(nextPrim(&fixture.ordering[256]) == fixture.Packet());
  CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                          0x800980F9u) == nullptr);
}
