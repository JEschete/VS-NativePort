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
using NativeSubdivision = int (*)(const Render_RoomVertex*, int,
                                  const Render_RoomPolygonMaterial*,
                                  std::size_t);

struct SubdivisionFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  int saved_framebuffer = Render_frameBuf;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  std::vector<unsigned char> memory =
      std::vector<unsigned char>(0x20000 + 16, 0xA5);
  std::array<u_long, 2048> ordering{};
  std::array<Render_RoomVertex, 4> vertices{};
  Render_RoomPolygonMaterial material{0, 0x3D, 0x1234, 0x15};

  SubdivisionFixture() {
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 120);
    const VECTOR translation{0, 0, 1024, 0};
    Core_Gte_SetTransVector(&translation);
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = Buffer();
    Render_frameScratch = {};
    Render_frameScratch.primitiveCursor = Buffer();
    Render_frameScratch.orderingTable = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_SetRoomPolygonNearClip(8);
    Render_SetRoomPolygonFarClip(2047);
    Render_SetRoomPolygonScreenBounds(0, 0, 320, 224);
    constexpr std::array<SVECTOR, 4> kPositions{
        SVECTOR{-128, -128, 0, 77}, SVECTOR{128, -128, 0, 77},
        SVECTOR{-128, 128, 0, 77}, SVECTOR{128, 128, 0, 77}};
    constexpr std::array<unsigned, 4> kColors{0x102030, 0x708090, 0xD0E0F0,
                                              0x305070};
    constexpr std::array<std::uint16_t, 4> kUv{0x2010, 0x4030, 0x6050, 0x8070};
    for (std::size_t index = 0; index < vertices.size(); ++index) {
      vertices[index] = {kPositions[index], kColors[index], kUv[index]};
    }
  }

  ~SubdivisionFixture() {
    Render_frameScratch = saved_frame;
    Render_frameBuf = saved_framebuffer;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_SetRoomPolygonNearClip(8);
    Render_SetRoomPolygonFarClip(2047);
    Render_SetRoomPolygonScreenBounds(0, 0, 320, 224);
    Core_Gte_Reset();
  }

  unsigned char* Buffer() { return memory.data() + 8; }

  std::size_t Used() const {
    return static_cast<unsigned char*>(Render_frameScratch.primitiveCursor) -
           (memory.data() + 8);
  }

  int Draw(bool quad, int levels, std::size_t bytes = 0x20000) {
    const NativeSubdivision draw =
        quad ? Render_SubdivideRoomQuad : Render_SubdivideRoomTriangle;
    return draw(vertices.data(), levels, &material, bytes);
  }
};

void CheckPacket(const u_long* packet, bool quad) {
  CHECK((packet[0] >> 24) == (quad ? 12 : 9));
  CHECK((packet[1] >> 24) == (quad ? 0x3C : 0x34));
  CHECK((packet[3] >> 16) == 0x1234);
  CHECK((packet[6] >> 16) == 0x15);
}

std::array<int, 2> CheckPackets(SubdivisionFixture& fixture) {
  int triangle_count = 0;
  int quad_count = 0;
  const auto* cursor = reinterpret_cast<const u_long*>(fixture.Buffer());
  const auto* end =
      static_cast<const u_long*>(Render_frameScratch.primitiveCursor);
  while (cursor < end) {
    REQUIRE(end - cursor >= 2);
    const auto words = 1 + (cursor[0] >> 24);
    REQUIRE(words <= static_cast<std::size_t>(end - cursor));
    const bool is_quad = (cursor[1] >> 24) == 0x3C;
    CheckPacket(cursor, is_quad);
    is_quad ? ++quad_count : ++triangle_count;
    cursor += words;
  }
  CHECK(cursor == end);
  return {triangle_count, quad_count};
}

void CheckUnchangedMaterials(const std::array<Render_RoomVertex, 4>& actual,
                             const std::array<Render_RoomVertex, 4>& before) {
  for (std::size_t index = 0; index < before.size(); ++index) {
    CHECK(actual[index].position.pad == before[index].position.pad);
    CHECK(actual[index].color == before[index].color);
    CHECK(actual[index].uv == before[index].uv);
  }
}

void CheckLevel(bool quad, int levels) {
  SubdivisionFixture fixture;
  constexpr std::array<int, 4> kTriangles{1, 7, 31, 127};
  constexpr std::array<int, 4> kQuadLeaves{1, 4, 16, 64};
  constexpr std::array<int, 4> kEdgeTriangles{0, 4, 20, 84};
  const int triangles = quad ? kEdgeTriangles[levels] : kTriangles[levels];
  const int quads = quad ? kQuadLeaves[levels] : 0;
  const auto before = fixture.vertices;
  CHECK(fixture.Draw(quad, levels) == triangles + quads);
  CHECK(fixture.Used() ==
        triangles * sizeof(POLY_GT3) + quads * sizeof(POLY_GT4));
  const std::array expected_counts{triangles, quads};
  CHECK(CheckPackets(fixture) == expected_counts);
  CHECK(fixture.memory.front() == 0xA5);
  CHECK(fixture.memory.back() == 0xA5);
  CheckUnchangedMaterials(fixture.vertices, before);
}

void CheckCapacity(bool quad) {
  SubdivisionFixture fixture;
  const std::size_t full =
      quad ? 4 * sizeof(POLY_GT4) + 4 * sizeof(POLY_GT3) : 7 * sizeof(POLY_GT3);
  CHECK(fixture.Draw(quad, 1, full - 1) == -1);
  CHECK(fixture.Used() == full - sizeof(POLY_GT3));
  CHECK(*static_cast<unsigned char*>(Render_frameScratch.primitiveCursor) ==
        0xA5);
  CHECK(fixture.memory.back() == 0xA5);
}

void CheckAliasedInputs(bool quad) {
  SubdivisionFixture fixture;
  auto* vertices = reinterpret_cast<Render_RoomVertex*>(fixture.Buffer());
  auto* material =
      reinterpret_cast<Render_RoomPolygonMaterial*>(fixture.Buffer() + 64);
  std::memcpy(vertices, fixture.vertices.data(), sizeof fixture.vertices);
  std::memcpy(material, &fixture.material, sizeof fixture.material);
  const NativeSubdivision draw =
      quad ? Render_SubdivideRoomQuad : Render_SubdivideRoomTriangle;
  CHECK(draw(vertices, 1, material, 0x20000) == (quad ? 8 : 7));
  const auto* packet = reinterpret_cast<const u_long*>(fixture.Buffer());
  CheckPacket(packet, quad);
  CHECK((packet[1] & 0xFFFFFF) == 0x102030);
}
}  // namespace

TEST_CASE(
    "Room subdivision exposes bounded typed triangle and quad entry points",
    "[render][room-subdivision]") {
  CHECK((std::is_same_v<decltype(&Render_SubdivideRoomTriangle),
                        NativeSubdivision>));
  CHECK(
      (std::is_same_v<decltype(&Render_SubdivideRoomQuad), NativeSubdivision>));
}

TEST_CASE(
    "Shared subdivision preserves every source-valid recursive packet count",
    "[render][room-subdivision]") {
  for (bool quad : {false, true}) {
    for (int levels = 0; levels <= 3; ++levels) CheckLevel(quad, levels);
  }
}

TEST_CASE("Room subdivision emits no partial packet at a capacity boundary",
          "[render][room-subdivision]") {
  CheckCapacity(false);
  CheckCapacity(true);
}

TEST_CASE("An empty room subdivision span stops without modifying its buffer",
          "[render][room-subdivision]") {
  SubdivisionFixture fixture;
  CHECK(fixture.Draw(false, 0, 0) == -1);
  CHECK(fixture.Used() == 0);
  CHECK(fixture.Buffer()[0] == 0xA5);
  CHECK(fixture.Draw(true, 0, 0) == -1);
  CHECK(fixture.Used() == 0);
}

TEST_CASE(
    "Room subdivision retains source midpoint position color and UV order",
    "[render][room-subdivision]") {
  SubdivisionFixture fixture;
  REQUIRE(fixture.Draw(false, 1) == 7);
  const auto* first = reinterpret_cast<const POLY_GT3*>(fixture.Buffer());
  const std::array<std::int16_t, 6> actual{first->x0, first->y0, first->x1,
                                           first->y1, first->x2, first->y2};
  constexpr std::array<std::int16_t, 6> kExpected{128, 88, 160, 88, 128, 120};
  CHECK(actual == kExpected);
  CHECK((std::uint32_t{first->r1} | (std::uint32_t{first->g1} << 8) |
         (std::uint32_t{first->b1} << 16)) == 0x405060);
  CHECK(first->u1 == 32);
  CHECK(first->v1 == 48);
  CHECK(first->u2 == 48);
  CHECK(first->v2 == 64);
}

TEST_CASE("Subdivision captures native inputs before overlapping packet writes",
          "[render][room-subdivision]") {
  CheckAliasedInputs(false);
  CheckAliasedInputs(true);
}

TEST_CASE("Room subdivision preserves far rejection and signed depth bias",
          "[render][room-subdivision]") {
  for (bool quad : {false, true}) {
    SubdivisionFixture fixture;
    Render_SetRoomPolygonFarClip(256);
    CHECK(fixture.Draw(quad, 1) == 0);
    CHECK(fixture.Used() == 0);
    fixture.material.flags = 0xFFFF0000;
    CHECK(fixture.Draw(quad, 1) == (quad ? 8 : 7));
  }
}

TEST_CASE("Room subdivision rejects off-screen leaves without emitting packets",
          "[render][room-subdivision]") {
  SubdivisionFixture fixture;
  Core_Gte_SetGeomOffset(1000, 1000);
  CHECK(fixture.Draw(false, 3) == 0);
  CHECK(fixture.Draw(true, 3) == 0);
  CHECK(fixture.Used() == 0);
}

TEST_CASE(
    "Original subdivision entries invoke the shared typed native recursion",
    "[render][room-subdivision][redirect-binding]") {
  for (bool quad : {false, true}) {
    SubdivisionFixture fixture;
    const auto symbols =
        std::span(Content_moduleSymbols, Content_moduleSymbolCount);
    const unsigned address = quad ? 0x80097CF0u : 0x80097AECu;
    auto* entry = VS::Content::ResolveModuleAddress(
        symbols, "BATTLE/BATTLE.PRG", address);
    REQUIRE(entry != nullptr);
    const auto draw = reinterpret_cast<NativeSubdivision>(entry);
    CHECK(draw ==
          (quad ? Render_SubdivideRoomQuad : Render_SubdivideRoomTriangle));
    CHECK(draw(fixture.vertices.data(), 1, &fixture.material, 0x20000) ==
          (quad ? 8 : 7));
    CHECK(VS::Content::ResolveModuleAddress(symbols, "BATTLE/BATTLE.PRG",
                                            address + 1) == nullptr);
  }
}
