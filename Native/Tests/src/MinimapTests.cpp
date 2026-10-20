#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Field/MinimapScene.h"
#include "Render/RoomDraw.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
}

namespace {
std::vector<u_long> Scene() {
  return {2,
          1,
          76,
          9 | (1 << 16),
          1,
          60,
          9 | (2 << 16),
          4,
          0xFFF0FFF0,
          0,
          0xFFF00010,
          0,
          0x00100010,
          0,
          0x0010FFF0,
          0,
          1,
          0x00020100,
          1,
          0x03020100,
          1,
          0x00000100,
          1,
          0x00000302,
          1,
          0,
          4,
          0xFFF0FFF0,
          0,
          0xFFF00010,
          0,
          0x00100010,
          0,
          0x0010FFF0,
          0,
          0,
          0,
          1,
          0x00000100,
          0,
          0};
}

u_long FileWord(const std::vector<unsigned char>& data, size_t offset) {
  REQUIRE(offset <= data.size());
  REQUIRE(data.size() - offset >= sizeof(u_long));
  u_long value;
  std::memcpy(&value, data.data() + offset, sizeof value);
  return value;
}

std::vector<unsigned char> FileBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

std::filesystem::path ExtractedRoot() {
  auto root = std::filesystem::current_path();
  while (!std::filesystem::exists(root / "Native" / "data" / "disc") &&
         root != root.parent_path()) {
    root = root.parent_path();
  }
  root /= std::filesystem::path("Native") / "data" / "disc";
  if (!std::filesystem::exists(root)) {
    SKIP("Extracted game data is not configured");
  }
  return root;
}

struct RenderFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  std::array<void*, 2> saved_buffers{Render_legacyPrimitiveBuffers[0],
                                     Render_legacyPrimitiveBuffers[1]};
  int saved_framebuffer = Render_frameBuf;
  u_short saved_ticks = Field_roomAnimationTicks;
  State_StateFlags saved_flags = Audio_Seq_unusedStateFlags;
  std::vector<u_long> packets =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 1);
  std::array<u_long, 16> ordering{};

  explicit RenderFixture(int framebuffer = 0) {
    Core_Gte_Reset();
    Core_Gte_SetGeomScreen(256);
    Core_Gte_SetGeomOffset(264, 196);
    Render_frameBuf = framebuffer;
    Render_legacyPrimitiveBuffers[framebuffer] = packets.data();
    Render_frameScratch.primitiveCursor = packets.data();
    Render_frameScratch.orderingTable = ordering.data();
    Field_roomAnimationTicks = 0;
    std::memset(&Audio_Seq_unusedStateFlags, 0,
                sizeof Audio_Seq_unusedStateFlags);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    packets.back() = 0xDEADBEEF;
  }

  ~RenderFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffers[0];
    Render_legacyPrimitiveBuffers[1] = saved_buffers[1];
    Render_frameBuf = saved_framebuffer;
    Field_roomAnimationTicks = saved_ticks;
    Audio_Seq_unusedStateFlags = saved_flags;
    Core_Gte_Reset();
  }

  std::vector<const P_TAG*> Commands() {
    std::vector<const P_TAG*> result;
    const void* packet = &ordering.back();
    for (size_t count = 0; packet != nullptr && count < 64; ++count) {
      const auto* tag = static_cast<const P_TAG*>(packet);
      if (tag->len != 0) result.push_back(tag);
      packet = isendprim(packet) ? nullptr : nextPrim(packet);
    }
    REQUIRE(packet == nullptr);
    return result;
  }
};
}  // namespace

TEST_CASE(
    "Map scenes retain packed records and resolve bounded relative offsets",
    "[content][minimap][layout]") {
  auto bytes = Scene();
  REQUIRE(Field_ValidateMapScene(bytes.data(), bytes.size() * sizeof(u_long)));
  auto* end =
      Field_ResolveSceneRoomData(bytes.data(), bytes.size() * sizeof(u_long));
  CHECK(reinterpret_cast<void*>(end) ==
        reinterpret_cast<unsigned char*>(bytes.data()) +
            bytes.size() * sizeof(u_long));
  CHECK(bytes[2] == 24);
  CHECK(bytes[5] == 88);
  auto* scene = reinterpret_cast<Field_MapScene*>(bytes.data());
  CHECK(scene->rooms[0].zoneId == 9);
  CHECK(scene->rooms[1].mapId == 2);
  CHECK(VS_ONDISC(&scene->rooms[0], dataAddress, void*) == bytes.data() + 7);
  CHECK(VS_ONDISC(&scene->rooms[1], dataAddress, void*) == bytes.data() + 26);
}

TEST_CASE("Malformed map counts, payloads and indices fail without mutation",
          "[content][minimap][bounds]") {
  const auto valid = Scene();
  for (size_t length = 0; length < valid.size() * sizeof(u_long); ++length) {
    CHECK_FALSE(Field_ValidateMapScene(valid.data(), length));
  }
  for (const size_t index : {0, 2, 7, 16, 17}) {
    auto bad = valid;
    bad[index] = UINT32_MAX;
    const auto saved = bad;
    CHECK_FALSE(
        Field_ValidateMapScene(bad.data(), bad.size() * sizeof(u_long)));
    CHECK(bad == saved);
    CHECK_THROWS_AS(
        Field_ResolveSceneRoomData(bad.data(), bad.size() * sizeof(u_long)),
        std::runtime_error);
    CHECK(bad == saved);
  }
  CHECK_FALSE(Field_ValidateMapScene(nullptr, valid.size() * sizeof(u_long)));
}

TEST_CASE("Minimap geometry, visited connections and markers use native links",
          "[render][minimap]") {
  RenderFixture fixture(GENERATE(0, 1));
  auto bytes = Scene();
  Field_ResolveSceneRoomData(bytes.data(), bytes.size() * sizeof(u_long));
  Field_DrawMinimapScene(bytes.data(), bytes.size() * sizeof(u_long));
  const auto packets = fixture.Commands();
  REQUIRE(packets.size() == 9);
  const std::array<int, 9> commands{0xE1, 0x42, 0x42, 0x42, 0x2A,
                                    0x22, 0xE1, 0x60, 0x60};
  for (size_t index = 0; index < packets.size(); ++index) {
    CAPTURE(index);
    CHECK(packets[index]->code == commands[index]);
  }
  const auto* quad = reinterpret_cast<const POLY_F4*>(packets[4]);
  const std::array<int16_t, 6> coordinates{quad->x0, quad->y0, quad->x2,
                                           quad->y2, quad->x3, quad->y3};
  const std::array<int16_t, 6> expected{248, 180, 248, 212, 280, 212};
  const std::array<unsigned char, 3> color{quad->r0, quad->g0, quad->b0};
  const std::array<unsigned char, 3> expected_color{0, 127, 63};
  CHECK(coordinates == expected);
  CHECK(color == expected_color);
  CHECK(fixture.packets.back() == 0xDEADBEEF);
}

TEST_CASE(
    "Minimap door gates, entry blinking and visited rooms retain semantics",
    "[render][minimap][doors]") {
  const auto door = GENERATE(0u, 0x00100000u, 0x01000000u);
  const auto ticks = GENERATE(u_short{0}, u_short{32});
  const auto gate = GENERATE(char{0}, char{1});
  RenderFixture fixture;
  auto bytes = Scene();
  bytes[4] = 0;
  bytes[25] = door;
  Field_roomAnimationTicks = ticks;
  Audio_Seq_unusedStateFlags.opaque33F[1] = gate;
  Field_ResolveSceneRoomData(bytes.data(), bytes.size() * sizeof(u_long));
  Field_DrawMinimapScene(bytes.data(), bytes.size() * sizeof(u_long));
  const auto packets = fixture.Commands();
  const bool locked = door == 0x01000000u && gate == 0;
  const bool hidden = door == 0x00100000u && ticks == 0;
  CAPTURE(door, ticks, gate);
  REQUIRE(packets.size() == (hidden ? 6 : locked ? 7 : 8));
  if (!hidden) CHECK(packets.back()->code == (locked ? 0x74 : 0x60));
}

TEST_CASE("Minimap capacity errors do not overwrite packets or ordering links",
          "[render][minimap][bounds]") {
  RenderFixture fixture;
  auto bytes = Scene();
  Field_ResolveSceneRoomData(bytes.data(), bytes.size() * sizeof(u_long));
  Render_frameScratch.primitiveCursor =
      fixture.packets.data() + 0x20000 / 4 - 1;
  const auto saved = fixture.ordering;
  CHECK_THROWS_AS(
      Field_DrawMinimapScene(bytes.data(), bytes.size() * sizeof(u_long)),
      std::runtime_error);
  CHECK(fixture.ordering == saved);
  CHECK(fixture.packets.back() == 0xDEADBEEF);
}

TEST_CASE(
    "Projection offsets restore the original center for independent draws",
    "[render][minimap][projection]") {
  Render_ScreenOffset original{160, 112};
  Render_ScreenOffset map{264, 196};
  Render_ScreenOffset observed{};
  Render_SetProjectionScreenOffset(&original);
  Render_GetProjectionScreenOffset(&observed);
  CHECK(observed.x == 160);
  CHECK(observed.y == 112);
  Render_SetProjectionScreenOffset(&map);
  Render_GetProjectionScreenOffset(&observed);
  CHECK(observed.x == 264);
  CHECK(observed.y == 196);
  Render_SetProjectionScreenOffset(&original);
  Render_GetProjectionScreenOffset(&observed);
  CHECK(observed.x == 160);
  CHECK(observed.y == 112);
  Core_Gte_Reset();
}

TEST_CASE("Projection centers preserve the original high-halfword semantics",
          "[render][minimap][projection]") {
  const int x = GENERATE(-32768, -1, 0, 32767, 65535);
  Render_ScreenOffset offset{x, 1 - x};
  Render_ScreenOffset observed{};
  Render_SetProjectionScreenOffset(&offset);
  Render_GetProjectionScreenOffset(&observed);
  CHECK(observed.x == static_cast<u_short>(offset.x));
  CHECK(observed.y == static_cast<u_short>(offset.y));
  Core_Gte_Reset();
}

TEST_CASE("Every extracted ARM scene decodes packed rooms and its name table",
          "[content][minimap][disc]") {
  const auto root = ExtractedRoot();
  size_t arm_count = 0;
  for (const auto& entry :
       std::filesystem::directory_iterator(root / "SMALL")) {
    if (entry.path().extension() != ".ARM" ||
        !entry.path().stem().string().starts_with("SCEN")) {
      continue;
    }
    auto bytes = FileBytes(entry.path());
    CAPTURE(entry.path().filename().string());
    REQUIRE(Field_ValidateMapScene(bytes.data(), bytes.size()));
    auto* names = Field_ResolveSceneRoomData(bytes.data(), bytes.size());
    const auto name_offset = static_cast<size_t>(
        reinterpret_cast<const unsigned char*>(names) - bytes.data());
    REQUIRE(name_offset <= bytes.size());
    if (entry.path().filename() == "SCEN031.ARM") {
      CHECK(name_offset == bytes.size());
    } else {
      CHECK(FileWord(bytes, 0) <=
            (bytes.size() - name_offset) / sizeof(Field_MapRoomName));
    }
    ++arm_count;
  }
  CHECK(arm_count == 31);
}

TEST_CASE("Every extracted MPD minimap decodes and draws within packet bounds",
          "[content][minimap][disc]") {
  const auto root = ExtractedRoot();
  size_t scene_count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(root / "MAP")) {
    if (entry.path().extension() != ".MPD") continue;
    auto bytes = FileBytes(entry.path());
    if (bytes.empty()) continue;
    const size_t header = FileWord(bytes, 0);
    const size_t size = FileWord(bytes, header + 14 * sizeof(u_long));
    if (size == 0) continue;
    size_t offset = header + 24 * sizeof(u_long);
    for (size_t section = 0; section < 14; ++section) {
      offset += FileWord(bytes, header + section * sizeof(u_long));
    }
    CAPTURE(entry.path().filename().string(), offset, size);
    REQUIRE(offset <= bytes.size());
    REQUIRE(size <= bytes.size() - offset);
    REQUIRE(Field_ValidateMapScene(bytes.data() + offset, size));
    Field_ResolveSceneRoomData(bytes.data() + offset, size);
    RenderFixture fixture;
    auto* scene = reinterpret_cast<Field_MapScene*>(bytes.data() + offset);
    scene->rooms[0].visited = 1;
    Field_DrawMinimapScene(scene, size);
    CHECK(fixture.packets.back() == 0xDEADBEEF);
    ++scene_count;
  }
  CHECK(scene_count > 0);
}
