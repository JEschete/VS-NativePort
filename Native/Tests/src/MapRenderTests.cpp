#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Field/MinimapScene.h"

extern "C" {
#include "Menu/Map.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
}

namespace {
struct MapFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  std::array<void*, 2> saved_buffers{Render_legacyPrimitiveBuffers[0],
                                     Render_legacyPrimitiveBuffers[1]};
  int saved_framebuffer = Render_frameBuf;
  int saved_room = Menu_Map_currentRoomIndex;
  std::vector<u_long> storage =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 2, 0xDEADBEEF);
  std::array<u_long, 2084> ordering{};
  std::array<unsigned char, 1024 - sizeof(Render_FrameScratch)> saved_tail{};

  explicit MapFixture(int framebuffer = 0) {
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(160, 112);
    const VECTOR translation{0, 0, 512, 0};
    Core_Gte_SetTransVector(&translation);
    Render_frameBuf = framebuffer;
    Render_legacyPrimitiveBuffers[framebuffer] = storage.data() + 1;
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.overlayOrderingTable = ordering.data() + 2;
    Render_frameScratch.orderingTable = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    std::copy_n(static_cast<unsigned char*>(Core_ScratchBase()) +
                    sizeof(Render_FrameScratch),
                saved_tail.size(), saved_tail.begin());
    Menu_Map_currentRoomIndex = 0;
  }

  ~MapFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffers[0];
    Render_legacyPrimitiveBuffers[1] = saved_buffers[1];
    Render_frameBuf = saved_framebuffer;
    Menu_Map_currentRoomIndex = saved_room;
    Core_Gte_Reset();
  }

  bool CompleteChain() const {
    const void* packet = &ordering.back();
    const void* last = nullptr;
    const auto begin = reinterpret_cast<uintptr_t>(storage.data() + 1);
    const auto end =
        reinterpret_cast<uintptr_t>(Render_frameScratch.primitiveCursor);
    const auto table = reinterpret_cast<uintptr_t>(ordering.data());
    for (size_t count = 0; packet != nullptr && count < 4096; ++count) {
      const auto address = reinterpret_cast<uintptr_t>(packet);
      if (!((address >= begin && address < end) ||
            (address >= table && address < table + sizeof ordering))) {
        return false;
      }
      last = packet;
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    return packet == nullptr && last == ordering.data();
  }
};

std::array<unsigned char, 9> TriangleRgb(const POLY_G3& triangle) {
  return {triangle.r0, triangle.g0, triangle.b0, triangle.r1, triangle.g1,
          triangle.b1, triangle.r2, triangle.g2, triangle.b2};
}
}  // namespace

TEST_CASE("Map control backdrops retain native cursor and ordering pointers",
          "[render][map][links]") {
  MapFixture fixture(GENERATE(0, 1));
  const int height = GENERATE(1, 2, 12);
  Menu_Map_DrawControlBackdrop(-20, 10, 40, height);
  const size_t bytes =
      static_cast<size_t>(height - 1) * sizeof(LINE_G2) + sizeof(POLY_F4);
  CHECK(Render_frameScratch.primitiveCursor ==
        reinterpret_cast<unsigned char*>(fixture.storage.data() + 1) + bytes);
  CHECK(fixture.CompleteChain());
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Map framebuffer darkening preserves both native strip chains",
          "[render][map][framebuffer]") {
  const int framebuffer = GENERATE(0, 1);
  MapFixture fixture(framebuffer);
  Menu_Map_DarkenBackground();
  const auto* bytes =
      reinterpret_cast<const unsigned char*>(fixture.storage.data() + 1);
  const auto* first = reinterpret_cast<const SPRT*>(bytes + sizeof(DR_MODE));
  const auto* first_page = reinterpret_cast<const u_long*>(first + 1);
  const auto* second =
      reinterpret_cast<const SPRT*>(bytes + 2 * sizeof(DR_MODE) + sizeof(SPRT));
  const auto* second_page = reinterpret_cast<const u_long*>(second + 1);
  const std::array<int, 6> strips{first->code,  first->x0,  first->w,
                                  second->code, second->x0, second->w};
  const std::array<int, 6> expected{0x67, 0, 192, 0x67, 192, 128};
  CHECK(strips == expected);
  CHECK((first_page[1] & 0x1FF) == getTPage(2, 0, framebuffer ? 320 : 0, 0));
  CHECK((second_page[1] & 0x1FF) == getTPage(2, 0, framebuffer ? 512 : 192, 0));
  CHECK(fixture.CompleteChain());
}

TEST_CASE("Full Map projection handles all byte-indexed vertices off scratch",
          "[render][map][bounds]") {
  MapFixture fixture;
  std::vector<u_long> room(1 + 256 * 2);
  room[0] = 256;
  room[3] = 16;
  room[1 + 255 * 2] = 16 << 16;
  room.insert(room.end(), {1, 0x00FF0100, 0, 0, 0, 0});
  std::vector<u_long> scene{
      1, 1, static_cast<u_long>(room.size() * sizeof(u_long)), 0x00010001};
  scene.insert(scene.end(), room.begin(), room.end());
  REQUIRE(Field_ValidateMapScene(scene.data(), scene.size() * sizeof(u_long)));
  Field_ResolveSceneRoomData(scene.data(), scene.size() * sizeof(u_long));
  auto* map = reinterpret_cast<Field_MapScene*>(scene.data());
  CHECK(Menu_Map_DrawRoom(1, VS_ONDISC(&map->rooms[0], dataAddress, void*), 1,
                          1) == 1);
  const auto* triangle =
      reinterpret_cast<const POLY_G3*>(fixture.storage.data() + 1);
  const std::array<int16_t, 6> coordinates{triangle->x0, triangle->y0,
                                           triangle->x1, triangle->y1,
                                           triangle->x2, triangle->y2};
  const std::array<int16_t, 6> expected{160, 112, 168, 112, 160, 120};
  CHECK(coordinates == expected);
  CHECK(std::equal(fixture.saved_tail.begin(), fixture.saved_tail.end(),
                   static_cast<const unsigned char*>(Core_ScratchBase()) +
                       sizeof(Render_FrameScratch)));
  CHECK(fixture.CompleteChain());
}

TEST_CASE("Actual Map room triangles keep their color at the fog near plane",
          "[render][map][fog]") {
  MapFixture fixture;
  Core_Gte_SetGeomScreen(320);
  Core_Gte_SetFarColor(0, 0, 0);
  const VECTOR translation{0, 0, 768, 0};
  Core_Gte_SetTransVector(&translation);
  const std::vector<u_long> room{3, 0,          0, 16, 0, 16 << 16, 0,
                                 1, 0x00020100, 0, 0,  0, 0};
  std::vector<u_long> scene{
      1, 1, static_cast<u_long>(room.size() * sizeof(u_long)), 0x00010001};
  scene.insert(scene.end(), room.begin(), room.end());
  REQUIRE(Field_ValidateMapScene(scene.data(), scene.size() * sizeof(u_long)));
  Field_ResolveSceneRoomData(scene.data(), scene.size() * sizeof(u_long));
  const auto* map = reinterpret_cast<Field_MapScene*>(scene.data());
  auto* data = VS_ONDISC(&map->rooms[0], dataAddress, void*);
  constexpr std::array<unsigned char, 9> kExpected{0,   64, 128, 0,  64,
                                                   128, 0,  64,  128};
  Core_Gte_SetFogNear(0, 320);
  auto* baseline =
      reinterpret_cast<POLY_G3*>(Render_frameScratch.primitiveCursor);
  REQUIRE(Menu_Map_DrawRoom(1, data, 1, 1) == 1);
  CHECK(TriangleRgb(*baseline) == kExpected);
  Core_Gte_SetFogNear(768, 320);
  auto* fogged =
      reinterpret_cast<POLY_G3*>(Render_frameScratch.primitiveCursor);
  REQUIRE(Menu_Map_DrawRoom(1, data, 1, 1) == 1);
  CHECK(TriangleRgb(*fogged) == kExpected);
  CHECK(fixture.CompleteChain());
}
