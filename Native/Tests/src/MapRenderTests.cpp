#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Field/Gimmicks.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Menu/Map.hpp"
#include "Menu/MapDrawingLive.hpp"
#include "Menu/MapTerminalLive.hpp"
#include "Render/DepthCueLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Core::Gte::SetGeomOffset;
using VS::Core::Gte::SetGeomScreen;
using VS::Core::Gte::SetTransVector;
using VS::Core::Gte::VECTOR;

using VS::Legacy::ClearOTagR;
using VS::Legacy::DR_MODE;
using VS::Legacy::getTPage;
using VS::Legacy::isendprim;
using VS::Legacy::LINE_G2;
using VS::Legacy::nextPrim;
using VS::Legacy::POLY_F4;
using VS::Legacy::POLY_G3;
using VS::Legacy::SPRT;

namespace {
std::span<const std::byte> SceneBytes(const void* data, size_t size) {
  return {static_cast<const std::byte*>(data), size};
}

// The first room's payload in a resolved scene; its offset counts from the
// room's record.
void* FirstRoomData(std::vector<std::uint32_t>& scene) {
  auto* bytes = reinterpret_cast<unsigned char*>(scene.data());
  VS::Field::MapRoomRecord room{};
  std::memcpy(&room, bytes + VS::Field::kMapSceneHeaderBytes, sizeof(room));
  return bytes + VS::Field::kMapSceneHeaderBytes + room.data_offset;
}

struct MapFixture {
  VS::Render::FrameScratch saved_frame = VS::Render::FrameScratchpad();
  std::array<void*, 2> saved_buffers{VS::Render::legacyPrimitiveBuffers[0],
                                     VS::Render::legacyPrimitiveBuffers[1]};
  std::array<void*, 2> saved_tables{VS::Render::legacyOrderingTables[0],
                                    VS::Render::legacyOrderingTables[1]};
  VS::Field::MapScene* saved_minimap = VS::Field::roomData.minimap_scene;
  std::uint32_t saved_minimap_length =
      VS::Field::roomData.header.minimap_scene_length;
  int saved_framebuffer = VS::Render::frameBuf;
  int saved_room = VS::Menu::map::currentRoomIndex;
  std::vector<std::uint32_t> storage = std::vector<std::uint32_t>(
      0x20000 / sizeof(std::uint32_t) + 2, 0xDEADBEEF);
  std::array<std::uint32_t, 2084> ordering{};
  std::array<unsigned char, 1024 - sizeof(VS::Render::FrameScratch)>
      saved_tail{};

  explicit MapFixture(int framebuffer = 0) {
    VS::Core::Gte::Reset();
    SetGeomScreen(256);
    SetGeomOffset(160, 112);
    const VECTOR translation{0, 0, 512, 0};
    SetTransVector(&translation);
    VS::Render::frameBuf = framebuffer;
    VS::Render::legacyPrimitiveBuffers[framebuffer] = storage.data() + 1;
    VS::Render::legacyOrderingTables[framebuffer] = ordering.data();
    VS::Render::FrameScratchpad().primitive_cursor = storage.data() + 1;
    VS::Render::FrameScratchpad().overlay_ordering_table = ordering.data() + 2;
    VS::Render::FrameScratchpad().ordering_table = ordering.data() + 18;
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    std::copy_n(static_cast<unsigned char*>(VS::Core::ScratchBase()) +
                    sizeof(VS::Render::FrameScratch),
                saved_tail.size(), saved_tail.begin());
    VS::Menu::map::currentRoomIndex = 0;
    VS::Menu::map::ResetMapDrawingLive();
  }

  ~MapFixture() {
    VS::Render::FrameScratchpad() = saved_frame;
    VS::Render::legacyPrimitiveBuffers[0] = saved_buffers[0];
    VS::Render::legacyPrimitiveBuffers[1] = saved_buffers[1];
    VS::Render::legacyOrderingTables[0] = saved_tables[0];
    VS::Render::legacyOrderingTables[1] = saved_tables[1];
    VS::Field::roomData.minimap_scene = saved_minimap;
    VS::Field::roomData.header.minimap_scene_length = saved_minimap_length;
    VS::Menu::map::ResetMapDrawingLive();
    VS::Render::frameBuf = saved_framebuffer;
    VS::Menu::map::currentRoomIndex = saved_room;
    VS::Core::Gte::Reset();
  }

  // Loads scene as the room's minimap scene, the buffer room data must lie in.
  static void LoadMinimap(std::vector<std::uint32_t>& scene) {
    VS::Field::roomData.minimap_scene =
        reinterpret_cast<VS::Field::MapScene*>(scene.data());
    VS::Field::roomData.header.minimap_scene_length =
        static_cast<std::uint32_t>(scene.size() * sizeof(std::uint32_t));
  }

  // A one-room scene around room, validated, resolved and loaded as the
  // minimap scene.
  static std::vector<std::uint32_t> LoadRoomScene(
      const std::vector<std::uint32_t>& room) {
    std::vector<std::uint32_t> scene{
        1, 1, static_cast<std::uint32_t>(room.size() * sizeof(std::uint32_t)),
        0x00010001};
    scene.insert(scene.end(), room.begin(), room.end());
    REQUIRE(VS::Field::IsValidMapScene(
        SceneBytes(scene.data(), scene.size() * sizeof(std::uint32_t))));
    VS::Field::ResolveLiveSceneRoomData(scene.data(),
                                        scene.size() * sizeof(std::uint32_t));
    LoadMinimap(scene);
    return scene;
  }

  bool CompleteChain() const {
    const void* packet = &ordering.back();
    const void* last = nullptr;
    const auto begin = reinterpret_cast<uintptr_t>(storage.data() + 1);
    const auto end = reinterpret_cast<uintptr_t>(
        VS::Render::FrameScratchpad().primitive_cursor);
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
  VS::Menu::map::LiveDrawControlBackdrop(-20, 10, 40, height);
  const size_t bytes =
      static_cast<size_t>(height - 1) * sizeof(LINE_G2) + sizeof(POLY_F4);
  CHECK(VS::Render::FrameScratchpad().primitive_cursor ==
        reinterpret_cast<unsigned char*>(fixture.storage.data() + 1) + bytes);
  CHECK(fixture.CompleteChain());
  CHECK(fixture.storage.front() == 0xDEADBEEF);
  CHECK(fixture.storage.back() == 0xDEADBEEF);
}

TEST_CASE("Map framebuffer darkening preserves both native strip chains",
          "[render][map][framebuffer]") {
  const int framebuffer = GENERATE(0, 1);
  MapFixture fixture(framebuffer);
  VS::Menu::map::LiveDarkenBackground();
  const auto* bytes =
      reinterpret_cast<const unsigned char*>(fixture.storage.data() + 1);
  const auto* first = reinterpret_cast<const SPRT*>(bytes + sizeof(DR_MODE));
  const auto* first_page = reinterpret_cast<const std::uint32_t*>(first + 1);
  const auto* second =
      reinterpret_cast<const SPRT*>(bytes + 2 * sizeof(DR_MODE) + sizeof(SPRT));
  const auto* second_page = reinterpret_cast<const std::uint32_t*>(second + 1);
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
  std::vector<std::uint32_t> room(1 + 256 * 2);
  room[0] = 256;
  room[3] = 16;
  room[1 + 255 * 2] = 16 << 16;
  room.insert(room.end(), {1, 0x00FF0100, 0, 0, 0, 0});
  std::vector<std::uint32_t> scene = MapFixture::LoadRoomScene(room);
  CHECK(VS::Menu::map::LiveDrawRoom(1, FirstRoomData(scene), 1, 1) == 1);
  const auto* triangle =
      reinterpret_cast<const POLY_G3*>(fixture.storage.data() + 1);
  const std::array<int16_t, 6> coordinates{triangle->x0, triangle->y0,
                                           triangle->x1, triangle->y1,
                                           triangle->x2, triangle->y2};
  const std::array<int16_t, 6> expected{160, 112, 168, 112, 160, 120};
  CHECK(coordinates == expected);
  CHECK(std::equal(fixture.saved_tail.begin(), fixture.saved_tail.end(),
                   static_cast<const unsigned char*>(VS::Core::ScratchBase()) +
                       sizeof(VS::Render::FrameScratch)));
  CHECK(fixture.CompleteChain());
}

TEST_CASE("Actual Map room triangles keep their color at the fog near plane",
          "[render][map][fog]") {
  MapFixture fixture;
  SetGeomScreen(320);
  VS::Core::Gte::SetFarColor(0, 0, 0);
  const VECTOR translation{0, 0, 768, 0};
  SetTransVector(&translation);
  const std::vector<std::uint32_t> room{3, 0,          0, 16, 0, 16 << 16, 0,
                                        1, 0x00020100, 0, 0,  0, 0};
  std::vector<std::uint32_t> scene = MapFixture::LoadRoomScene(room);
  auto* data = FirstRoomData(scene);
  constexpr std::array<unsigned char, 9> kExpected{0,   64, 128, 0,  64,
                                                   128, 0,  64,  128};
  VS::Render::SetLiveFogNearDistance(0, 320);
  auto* baseline = reinterpret_cast<POLY_G3*>(
      VS::Render::FrameScratchpad().primitive_cursor);
  REQUIRE(VS::Menu::map::LiveDrawRoom(1, data, 1, 1) == 1);
  CHECK(TriangleRgb(*baseline) == kExpected);
  VS::Render::SetLiveFogNearDistance(768, 320);
  auto* fogged = reinterpret_cast<POLY_G3*>(
      VS::Render::FrameScratchpad().primitive_cursor);
  REQUIRE(VS::Menu::map::LiveDrawRoom(1, data, 1, 1) == 1);
  CHECK(TriangleRgb(*fogged) == kExpected);
  CHECK(fixture.CompleteChain());
}
