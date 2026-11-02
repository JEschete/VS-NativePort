#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Field/MinimapScene.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Render/RoomDraw.h"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/StateFlags.h"
}

namespace {
std::span<const std::byte> SceneBytes(const void* data, size_t size) {
  return {static_cast<const std::byte*>(data), size};
}

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
  REQUIRE(VS::Field::IsValidMapScene(
      SceneBytes(bytes.data(), bytes.size() * sizeof(u_long))));
  auto* end = VS::Field::ResolveLiveSceneRoomData(
      bytes.data(), bytes.size() * sizeof(u_long));
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
    CHECK_FALSE(VS::Field::IsValidMapScene(SceneBytes(valid.data(), length)));
  }
  for (const size_t index : {0, 2, 7, 16, 17}) {
    auto bad = valid;
    bad[index] = UINT32_MAX;
    const auto saved = bad;
    CHECK_FALSE(VS::Field::IsValidMapScene(
        SceneBytes(bad.data(), bad.size() * sizeof(u_long))));
    CHECK(bad == saved);
    CHECK_THROWS_AS(VS::Field::ResolveLiveSceneRoomData(
                        bad.data(), bad.size() * sizeof(u_long)),
                    std::runtime_error);
    CHECK(bad == saved);
  }
  CHECK_FALSE(VS::Field::IsValidMapScene({}));
}

TEST_CASE("Minimap geometry, visited connections and markers use native links",
          "[render][minimap]") {
  RenderFixture fixture(GENERATE(0, 1));
  auto bytes = Scene();
  VS::Field::ResolveLiveSceneRoomData(bytes.data(),
                                      bytes.size() * sizeof(u_long));
  VS::Field::DrawLiveMinimapScene(bytes.data(), bytes.size() * sizeof(u_long));
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
  VS::Field::ResolveLiveSceneRoomData(bytes.data(),
                                      bytes.size() * sizeof(u_long));
  VS::Field::DrawLiveMinimapScene(bytes.data(), bytes.size() * sizeof(u_long));
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
  VS::Field::ResolveLiveSceneRoomData(bytes.data(),
                                      bytes.size() * sizeof(u_long));
  Render_frameScratch.primitiveCursor =
      fixture.packets.data() + 0x20000 / 4 - 1;
  const auto saved = fixture.ordering;
  CHECK_THROWS_AS(VS::Field::DrawLiveMinimapScene(
                      bytes.data(), bytes.size() * sizeof(u_long)),
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
    REQUIRE(VS::Field::IsValidMapScene(SceneBytes(bytes.data(), bytes.size())));
    auto* names =
        VS::Field::ResolveLiveSceneRoomData(bytes.data(), bytes.size());
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
    REQUIRE(
        VS::Field::IsValidMapScene(SceneBytes(bytes.data() + offset, size)));
    VS::Field::ResolveLiveSceneRoomData(bytes.data() + offset, size);
    RenderFixture fixture;
    auto* scene = reinterpret_cast<Field_MapScene*>(bytes.data() + offset);
    scene->rooms[0].visited = 1;
    VS::Field::DrawLiveMinimapScene(scene, size);
    CHECK(fixture.packets.back() == 0xDEADBEEF);
    ++scene_count;
  }
  CHECK(scene_count > 0);
}

namespace {
using VS::Field::MinimapColor;
using VS::Field::MinimapPrimitiveCounts;
using VS::Field::MinimapScreenPoint;
using VS::Field::MinimapVertex;

struct RecordingMinimapRenderer final : VS::Field::MinimapRenderer {
  MinimapPrimitiveCounts counts;
  std::vector<std::string> calls;
  std::vector<MinimapColor> colors;
  std::vector<std::array<MinimapScreenPoint, 4>> quads;
  std::vector<MinimapScreenPoint> markers;
  bool locked = false;
  int begins = 0;

  void Begin(const MinimapPrimitiveCounts& reserved) override {
    counts = reserved;
    ++begins;
  }
  MinimapScreenPoint Project(const MinimapVertex& vertex) override {
    return {vertex.x, vertex.y};
  }
  bool IsDoorGateLocked(std::uint8_t /*gate*/) override { return locked; }
  void DrawTriangle(const std::array<MinimapScreenPoint, 3>& /*corners*/,
                    MinimapColor color) override {
    calls.emplace_back("triangle");
    colors.push_back(color);
  }
  void DrawQuad(const std::array<MinimapScreenPoint, 4>& corners,
                MinimapColor color) override {
    calls.emplace_back("quad");
    colors.push_back(color);
    quads.push_back(corners);
  }
  void DrawLine(MinimapScreenPoint /*from*/, MinimapScreenPoint /*to*/,
                MinimapColor color) override {
    calls.emplace_back("line");
    colors.push_back(color);
  }
  void DrawLockedDoor(MinimapScreenPoint position) override {
    calls.emplace_back("locked");
    markers.push_back(position);
  }
  void DrawDoorCross(MinimapScreenPoint position, MinimapColor color) override {
    calls.emplace_back("cross");
    colors.push_back(color);
    markers.push_back(position);
  }
  void Finish() override { calls.emplace_back("finish"); }
};

std::vector<std::byte> ResolvedScene(std::vector<u_long> words) {
  const auto view = std::as_bytes(std::span(words));
  std::vector<std::byte> bytes(view.begin(), view.end());
  CHECK(VS::Field::ResolveSceneRoomData(bytes) == bytes.size());
  return bytes;
}
}  // namespace

TEST_CASE("Typed minimap scenes draw rooms in order through a renderer",
          "[render][minimap][typed]") {
  const auto bytes = ResolvedScene(Scene());
  RecordingMinimapRenderer renderer;
  VS::Field::DrawMinimapScene(bytes, false, renderer);
  CHECK(renderer.begins == 1);
  CHECK(renderer.counts == MinimapPrimitiveCounts{1, 1, 3, 1});
  const std::vector<std::string> calls{"triangle", "quad", "line",  "line",
                                       "cross",    "line", "finish"};
  CHECK(renderer.calls == calls);
  const std::vector<MinimapColor> colors{{0, 127, 63},    {0, 127, 63},
                                         {0, 127, 255},   {0, 95, 47},
                                         {127, 127, 127}, {0, 127, 255}};
  CHECK(renderer.colors == colors);
  REQUIRE(renderer.quads.size() == 1);
  const std::array<MinimapScreenPoint, 4> strip{
      {{-16, -16}, {16, -16}, {-16, 16}, {16, 16}}};
  CHECK(renderer.quads[0] == strip);
}

TEST_CASE("Typed minimap doors follow gates, entry pulses and special flags",
          "[render][minimap][typed][doors]") {
  auto words = Scene();
  words[25] = 0x01000000u;
  auto bytes = ResolvedScene(words);
  RecordingMinimapRenderer locked;
  locked.locked = true;
  VS::Field::DrawMinimapScene(bytes, false, locked);
  REQUIRE(locked.markers.size() == 1);
  CHECK(locked.markers[0] == MinimapScreenPoint{-16 - 3, -16 - 4});

  words[25] = 0x00100000u;
  bytes = ResolvedScene(words);
  RecordingMinimapRenderer hidden;
  VS::Field::DrawMinimapScene(bytes, false, hidden);
  CHECK(hidden.markers.empty());
  RecordingMinimapRenderer pulsing;
  VS::Field::DrawMinimapScene(bytes, true, pulsing);
  REQUIRE(pulsing.markers.size() == 1);
  CHECK(pulsing.colors[4] == MinimapColor{255, 0, 127});

  words[25] = 0x00040000u;
  bytes = ResolvedScene(words);
  RecordingMinimapRenderer special;
  VS::Field::DrawMinimapScene(bytes, false, special);
  CHECK(special.colors[4] == MinimapColor{0, 127, 255});

  words[25] = 0x00010000u;
  bytes = ResolvedScene(words);
  RecordingMinimapRenderer other;
  VS::Field::DrawMinimapScene(bytes, false, other);
  CHECK(other.markers.empty());
}

TEST_CASE("Typed minimap draws reject unresolved scenes before rendering",
          "[render][minimap][typed][bounds]") {
  const auto words = Scene();
  RecordingMinimapRenderer renderer;
  CHECK_THROWS_AS(VS::Field::DrawMinimapScene(std::as_bytes(std::span(words)),
                                              false, renderer),
                  std::runtime_error);
  CHECK(renderer.begins == 0);
  CHECK(renderer.calls.empty());
}
