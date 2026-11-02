#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Field/RoomTriggerMarkers.hpp"
#include "Field/RoomTriggerMarkersLive.hpp"
#include "Legacy/Psx.h"
#include "Render/PageSpace.hpp"
#include "Render/RoomDraw.hpp"
#include "Render/RoomDrawLive.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Field/Camera.h"
#include "Field/Gimmicks.h"
#include "Field/Mpd.h"
#include "Field/TriggerMarkers.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

extern unsigned int Content_battleTriggerMarkerBytes[29];
}

namespace {
struct MarkerDefinition {
  unsigned group_count = 1;
  std::array<std::byte, VS::Render::kRoomGroupRecordBytes> group{};
  unsigned triangle_count = 0;
  unsigned quad_count = 1;
  Field_MpdQuad quad{};
};
static_assert(sizeof(MarkerDefinition) == 116);
static_assert(offsetof(MarkerDefinition, triangle_count) == 68);
static_assert(offsetof(MarkerDefinition, quad) == 76);

// Rewrites the definition's group record through the typed accessors; the
// stored count is forced to one while the record is written.
template <typename Edit>
void EditGroup(MarkerDefinition& definition, Edit edit) {
  const auto count = definition.group_count;
  definition.group_count = 1;
  const auto bytes = std::as_writable_bytes(std::span(&definition, 1));
  const auto stored = VS::Render::ReadRoomGroup(bytes, 0);
  REQUIRE(stored.has_value());
  auto group = stored.value_or(VS::Render::RoomGroup{});
  edit(group);
  REQUIRE(VS::Render::WriteRoomGroup(bytes, 0, group));
  definition.group_count = count;
}

struct TriggerMarkerFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  Field_RoomRecord saved_room = Field_roomData;
  State_StateFlags saved_flags = Audio_Seq_unusedStateFlags;
  RECT saved_texture = Field_triggerMarkerTextureSource;
  RECT saved_palette = Field_triggerMarkerPaletteSource;
  int saved_tick_speed = State_tickSpeed;
  int saved_mode = Field_roomGeometryDrawMode;
  int saved_framebuffer = Render_frameBuf;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  u_short saved_animation = Field_roomAnimationTicks;
  u_short saved_marker_ticks = Field_triggerMarkerFrameTicks;
  std::array<unsigned int, 29> saved_definition{};
  std::array<unsigned int, 29> expected_definition{};
  std::vector<u_long> storage =
      std::vector<u_long>(0x20000 / sizeof(u_long) + 2, 0xDEADBEEF);
  std::array<u_long, 2048> ordering{};
  std::array<Field_MpdFloorTrigger, 2> triggers{};
  MarkerDefinition definition;

  TriggerMarkerFixture() {
    std::memcpy(saved_definition.data(), Content_battleTriggerMarkerBytes,
                sizeof saved_definition);
    EditGroup(definition, [](VS::Render::RoomGroup& group) {
      group.flags = 0x800;
      group.polygon_offset = offsetof(MarkerDefinition, triangle_count);
      group.view.translation.x = 12345;
    });
    definition.quad.vertex2OffsetZ = -64;
    definition.quad.vertex3OffsetX = -64;
    definition.quad.vertex4OffsetX = -64;
    definition.quad.vertex4OffsetZ = -64;
    definition.quad.polygonType = 0x3F;
    definition.quad.u1 = 10;
    definition.quad.v1 = 20;
    definition.quad.u2 = 30;
    definition.quad.v2 = 40;
    definition.quad.u3 = 50;
    definition.quad.v3 = 60;
    definition.quad.u4 = 70;
    definition.quad.v4 = 80;
    definition.quad.texelMask = 0x1234;
    definition.quad.textureId = 0x15;
    StoreDefinition();
    Field_roomData = {};
    Field_roomData.floorTriggers = triggers.data();
    Field_roomData.header.floorTriggersLength = sizeof triggers;
    for (auto& trigger : triggers) trigger.kind = 3;
    Audio_Seq_unusedStateFlags = {};
    State_tickSpeed = 1;
    Field_roomGeometryDrawMode = 0;
    Field_roomAnimationTicks = 0;
    Field_triggerMarkerFrameTicks = 0;
    // No image transfers unless a test explicitly supplies a source rectangle.
    Field_triggerMarkerTextureSource = {};
    Field_triggerMarkerPaletteSource = {};
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data() + 1;
    Render_frameScratch = {};
    Render_frameScratch.viewMatrix.m[0][0] = 4096;
    Render_frameScratch.viewMatrix.m[1][2] = -4096;
    Render_frameScratch.viewMatrix.m[2][1] = 4096;
    Render_frameScratch.viewMatrix.t[2] = 1024;
    Core_Gte_Reset();
    Core_Gte_SetGeomOffset(160, 120);
    Core_Gte_SetGeomScreen(256);
    ResetPackets();
  }

  ~TriggerMarkerFixture() {
    std::memcpy(Content_battleTriggerMarkerBytes, saved_definition.data(),
                sizeof saved_definition);
    Field_roomData = saved_room;
    Audio_Seq_unusedStateFlags = saved_flags;
    Field_triggerMarkerTextureSource = saved_texture;
    Field_triggerMarkerPaletteSource = saved_palette;
    State_tickSpeed = saved_tick_speed;
    Field_roomGeometryDrawMode = saved_mode;
    Field_roomAnimationTicks = saved_animation;
    Field_triggerMarkerFrameTicks = saved_marker_ticks;
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Core_Gte_Reset();
  }

  void StoreDefinition() {
    std::memcpy(Content_battleTriggerMarkerBytes, &definition,
                sizeof definition);
    std::memcpy(expected_definition.data(), &definition, sizeof definition);
  }

  void ResetPackets() {
    Render_frameScratch.primitiveCursor = storage.data() + 1;
    Render_frameScratch.orderingTable = ordering.data();
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
  }

  const u_long* Packet(std::size_t index = 0) const {
    return storage.data() + 1 + index * 13;
  }

  void CheckPackets(unsigned count) const {
    CHECK(Render_frameScratch.primitiveCursor == Packet(count));
    CHECK(storage.front() == 0xDEADBEEF);
    CHECK(storage.back() == 0xDEADBEEF);
    std::array<unsigned int, 29> current_definition{};
    std::memcpy(current_definition.data(), Content_battleTriggerMarkerBytes,
                sizeof current_definition);
    CHECK(current_definition == expected_definition);
  }

  void CheckMaterial() const {
    const auto* packet = Packet();
    CHECK((packet[1] >> 24) == 0x3E);
    for (unsigned vertex = 0; vertex < 4; ++vertex) {
      CHECK((packet[1 + vertex * 3] & 0xFFFFFF) == 0x494949);
    }
    CHECK((packet[3] & 0xFFFF) == 0x140A);
    CHECK((packet[6] & 0xFFFF) == 0x281E);
    CHECK((packet[9] & 0xFFFF) == 0x3C32);
    CHECK((packet[12] & 0xFFFF) == 0x5046);
    CHECK((packet[3] >> 16) == 0x1234);
    CHECK((packet[6] >> 16) == 0x15);
  }
};

struct AnimationPixels {
  std::array<VS::Render::PageSpace::Rect, 12> locations{};
  std::array<VS::u16, 12> saved{};

  AnimationPixels() {
    auto& pages = VS::Render::GamePages();
    for (int frame = 0; frame < 5; ++frame) {
      const auto index = static_cast<std::size_t>(frame) * 2;
      locations[index] = {72 + frame * 8, 10, 1, 1};
      locations[index + 1] = {4 + frame * 4, 20, 1, 1};
    }
    locations[10] = {64, 384, 1, 1};
    locations[11] = {0, 320, 1, 1};
    for (std::size_t index = 0; index < locations.size(); ++index) {
      pages.Download(locations[index], &saved[index]);
      const auto pixel = static_cast<VS::u16>(0x1200 + index);
      pages.Upload(locations[index], &pixel);
    }
  }

  ~AnimationPixels() {
    auto& pages = VS::Render::GamePages();
    for (std::size_t index = 0; index < locations.size(); ++index) {
      pages.Upload(locations[index], &saved[index]);
    }
  }

  void CheckAnimation(unsigned pending, int frame) const {
    CHECK(Field_triggerMarkerFrameTicks == pending);
    CHECK(Field_triggerMarkerTextureSource.x == 72 + frame * 8);
    CHECK(Field_triggerMarkerPaletteSource.x == 4 + frame * 4);
    CHECK(VS::Render::GamePages().At(64, 384) == 0x1200 + frame * 2);
    CHECK(VS::Render::GamePages().At(0, 320) == 0x1201 + frame * 2);
  }
};
}  // namespace

TEST_CASE("Room trigger markers use the native view and preserve quad material",
          "[field][render][trigger-marker]") {
  TriggerMarkerFixture fixture;
  REQUIRE(VS::Render::ValidateLiveRoomGeometry(&fixture.definition,
                                               sizeof fixture.definition));
  VS::Field::DrawLiveRoomTriggerMarker(0, 0, 0, 73);
  fixture.CheckPackets(1);
  fixture.CheckMaterial();
  const auto* packet = fixture.Packet();
  CHECK(static_cast<std::int16_t>(packet[2]) == 192);
  CHECK(static_cast<std::int16_t>(packet[2] >> 16) == 88);
  CHECK(static_cast<std::int16_t>(packet[5]) == 192);
  CHECK(static_cast<std::int16_t>(packet[5] >> 16) == 104);
  CHECK(static_cast<std::int16_t>(packet[8]) == 176);
  CHECK(static_cast<std::int16_t>(packet[11] >> 16) == 104);
  CHECK(nextPrim(&fixture.ordering[256]) == packet);
}

TEST_CASE("Room trigger marker definitions reject incomplete or unsafe shapes",
          "[field][render][trigger-marker]") {
  TriggerMarkerFixture fixture;
  const unsigned malformed = GENERATE(0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u);
  switch (malformed) {
    case 0:
      fixture.definition.group_count = 0;
      break;
    case 1:
      fixture.definition.group_count = 2;
      break;
    case 2:
    case 3:
    case 4: {
      constexpr std::array<std::uint32_t, 3> kOffsets{0, 72, UINT32_MAX};
      EditGroup(fixture.definition, [&](VS::Render::RoomGroup& group) {
        group.polygon_offset = kOffsets[malformed - 2];
      });
      break;
    }
    case 5:
      fixture.definition.triangle_count = 1;
      break;
    case 6:
      fixture.definition.quad_count = 0;
      break;
    case 7:
      fixture.definition.quad_count = UINT32_MAX;
      break;
    default:
      FAIL("Unexpected malformed marker case");
  }
  fixture.StoreDefinition();
  VS::Field::DrawLiveRoomTriggerMarker(0, 0, 0, 128);
  fixture.CheckPackets(0);
}

TEST_CASE(
    "Room trigger marker fades preserve reveal saturation and state gates",
    "[field][trigger-marker]") {
  struct FadeCase {
    unsigned initial;
    int ticks;
    bool active;
    bool gate20;
    bool gate40;
    unsigned expected;
  };
  const auto sample = GENERATE(FadeCase{0, 1, true, false, false, 2},
                               FadeCase{127, 1, true, false, false, 128},
                               FadeCase{128, 4, true, false, false, 128},
                               FadeCase{255, 4, true, false, false, 255},
                               FadeCase{128, 3, false, false, false, 125},
                               FadeCase{4, 2, true, true, false, 2},
                               FadeCase{4, 2, true, false, true, 2},
                               FadeCase{2, 5, false, false, false, 0},
                               FadeCase{0, 4, false, false, false, 0},
                               FadeCase{128, 0, false, false, false, 128},
                               FadeCase{0, -1, true, false, false, 0});
  TriggerMarkerFixture fixture;
  auto& trigger = fixture.triggers[0];
  trigger.kind = 16;
  trigger.activation = static_cast<char>(sample.active);
  trigger.markerFade = static_cast<char>(sample.initial);
  State_tickSpeed = sample.ticks;
  Audio_Seq_unusedStateFlags.opaque20 = static_cast<char>(sample.gate20);
  Audio_Seq_unusedStateFlags.opaque40 = static_cast<char>(sample.gate40);
  VS::Field::DrawLiveRoomTriggerMarkers();
  CHECK(static_cast<unsigned char>(trigger.markerFade) == sample.expected);
  CHECK(Field_triggerMarkerFrameTicks == 0);
  fixture.CheckPackets(0);
}

TEST_CASE("Room trigger marker iteration ignores absent and partial records",
          "[field][trigger-marker]") {
  TriggerMarkerFixture fixture;
  for (auto& trigger : fixture.triggers) {
    trigger.activation = 1;
    trigger.kind = 16;
  }
  Field_roomData.floorTriggers = nullptr;
  VS::Field::DrawLiveRoomTriggerMarkers();
  CHECK(fixture.triggers[0].markerFade == 0);
  Field_roomData.floorTriggers = fixture.triggers.data();
  Field_roomData.header.floorTriggersLength = sizeof(Field_MpdFloorTrigger) - 1;
  VS::Field::DrawLiveRoomTriggerMarkers();
  CHECK(fixture.triggers[0].markerFade == 0);
  Field_roomData.header.floorTriggersLength = sizeof fixture.triggers - 1;
  VS::Field::DrawLiveRoomTriggerMarkers();
  CHECK(fixture.triggers[0].markerFade == 2);
  CHECK(fixture.triggers[1].markerFade == 0);
  fixture.CheckPackets(0);
}

TEST_CASE("Visible room trigger markers share the original animation update",
          "[field][render][trigger-marker]") {
  TriggerMarkerFixture fixture;
  AnimationPixels pixels;
  Field_triggerMarkerTextureSource = {0, 10, 1, 1};
  Field_triggerMarkerPaletteSource = {0, 20, 1, 1};
  Field_roomAnimationTicks = 24;
  Field_triggerMarkerFrameTicks = 7;
  State_tickSpeed = 3;
  for (auto& trigger : fixture.triggers) trigger.activation = 1;
  VS::Field::DrawLiveRoomTriggerMarkers();
  fixture.CheckPackets(2);
  pixels.CheckAnimation(2, 3);
  Field_roomAnimationTicks = 32;
  fixture.ResetPackets();
  VS::Field::DrawLiveRoomTriggerMarkers();
  fixture.CheckPackets(2);
  pixels.CheckAnimation(5, 3);
}

TEST_CASE("Room trigger marker entry uses fresh placement shade and view",
          "[field][render][trigger-marker]") {
  TriggerMarkerFixture fixture;
  VS::Field::DrawLiveRoomTriggerMarker(0, 0, 0, 128);
  fixture.CheckPackets(1);
  fixture.ResetPackets();
  Render_frameScratch.viewMatrix.t[0] = 64;
  VS::Field::DrawLiveRoomTriggerMarker(128, 128, 0, 7);
  fixture.CheckPackets(1);
  const auto* packet = fixture.Packet();
  CHECK(static_cast<std::int16_t>(packet[2]) == 240);
  CHECK(static_cast<std::int16_t>(packet[2] >> 16) == 56);
  for (unsigned vertex = 0; vertex < 4; ++vertex) {
    CHECK((packet[1 + vertex * 3] & 0xFFFFFF) == 0x070707);
  }
  fixture.ResetPackets();
  Field_roomData.floorTriggers = nullptr;
  VS::Field::DrawLiveRoomTriggerMarkers();
  fixture.CheckPackets(0);
  Field_roomData.floorTriggers = fixture.triggers.data();
  Field_triggerMarkerFrameTicks = 0;
  fixture.triggers[0].activation = 1;
  VS::Field::DrawLiveRoomTriggerMarkers();
  fixture.CheckPackets(1);
  CHECK(fixture.triggers[0].markerFade == 2);
  CHECK(Field_triggerMarkerFrameTicks == 1);
  for (unsigned vertex = 0; vertex < 4; ++vertex) {
    CHECK((fixture.Packet()[1 + vertex * 3] & 0xFFFFFF) == 0x020202);
  }
}

TEST_CASE("Room trigger marker height is applied before camera projection",
          "[field][render][trigger-marker]") {
  const auto height = GENERATE(std::int16_t{-64}, std::int16_t{64});
  TriggerMarkerFixture fixture;
  VS::Field::DrawLiveRoomTriggerMarker(0, 0, height, 128);
  fixture.CheckPackets(1);
  const unsigned depth = static_cast<unsigned>((1024 + height) / 4);
  CHECK(nextPrim(&fixture.ordering[depth]) == fixture.Packet());
}

TEST_CASE("Room trigger marker animation retains wrap and single-step timing",
          "[field][render][trigger-marker]") {
  TriggerMarkerFixture fixture;
  AnimationPixels pixels;
  Field_triggerMarkerTextureSource = {0, 10, 1, 1};
  Field_triggerMarkerPaletteSource = {0, 20, 1, 1};
  fixture.triggers[1].activation = 1;
  Field_roomAnimationTicks = 40;
  State_tickSpeed = 17;
  VS::Field::DrawLiveRoomTriggerMarkers();
  fixture.CheckPackets(1);
  pixels.CheckAnimation(9, 0);
  Field_triggerMarkerFrameTicks = UINT16_MAX;
  State_tickSpeed = 1;
  Field_roomAnimationTicks = 32;
  fixture.ResetPackets();
  VS::Field::DrawLiveRoomTriggerMarkers();
  fixture.CheckPackets(1);
  pixels.CheckAnimation(0, 0);
}

namespace {
class RecordingMarkerRenderer final
    : public VS::Field::RoomTriggerMarkerRenderer {
 public:
  std::uint16_t floor_code = 0;
  std::vector<VS::Field::TriggerMarkerAnimationFrame> frames;
  std::vector<VS::Field::TriggerMarkerPlacement> markers;
  std::vector<std::pair<int, int>> queries;

  [[nodiscard]] std::uint16_t FloorHeightCode(int world_x,
                                              int world_z) override {
    queries.emplace_back(world_x, world_z);
    return floor_code;
  }
  void ShowAnimationFrame(
      const VS::Field::TriggerMarkerAnimationFrame& frame) override {
    frames.push_back(frame);
  }
  void DrawMarker(const VS::Field::TriggerMarkerPlacement& placement) override {
    markers.push_back(placement);
  }
};
}  // namespace

TEST_CASE("Typed trigger markers place one shaded corner a tile beyond",
          "[field][trigger-marker]") {
  RecordingMarkerRenderer renderer;
  VS::Field::DrawRoomTriggerMarker(256, -128, -40, 0x1A5, renderer);
  REQUIRE(renderer.markers.size() == 1);
  CHECK(renderer.markers[0] ==
        VS::Field::TriggerMarkerPlacement{384, -40, 0, 0xA5});
  CHECK(renderer.frames.empty());
  CHECK(renderer.queries.empty());
}

TEST_CASE("Typed trigger markers decode the signed floor and skip hidden kinds",
          "[field][trigger-marker]") {
  RecordingMarkerRenderer renderer;
  renderer.floor_code = 0xFFC0;  // flag bit set above a 15-bit height of -64
  std::array<VS::Field::FloorTriggerMarker, 3> triggers{{
      {2, 3, 15, true, 0},
      {4, 5, 16, true, 0},
      {6, 7, 0, false, 0},
  }};
  VS::Field::TriggerMarkerAnimation animation{7};
  VS::Field::DrawRoomTriggerMarkers(triggers, {1, false, 16}, animation,
                                    renderer);
  CHECK(triggers[0].fade == 2);
  CHECK(triggers[1].fade == 2);
  CHECK(triggers[2].fade == 0);
  REQUIRE(renderer.markers.size() == 1);
  CHECK(renderer.markers[0] ==
        VS::Field::TriggerMarkerPlacement{384, -64, 512, 2});
  CHECK(renderer.queries == std::vector<std::pair<int, int>>{{256, 384}});
  CHECK(animation.pending_ticks == 0);
  CHECK(renderer.frames ==
        std::vector<VS::Field::TriggerMarkerAnimationFrame>{{88, 12}});
}

TEST_CASE("Typed trigger markers suppress reveals and step animation once",
          "[field][trigger-marker]") {
  RecordingMarkerRenderer renderer;
  std::array<VS::Field::FloorTriggerMarker, 2> triggers{{
      {0, 0, 0, true, 10},
      {1, 1, 0, true, 3},
  }};
  VS::Field::TriggerMarkerAnimation animation{};
  VS::Field::DrawRoomTriggerMarkers(triggers, {4, true, 0}, animation,
                                    renderer);
  CHECK(triggers[0].fade == 6);
  CHECK(triggers[1].fade == 0);
  CHECK(renderer.markers.size() == 1);
  CHECK(animation.pending_ticks == 4);
  CHECK(renderer.frames.empty());
  VS::Field::DrawRoomTriggerMarkers(triggers, {0, false, 0}, animation,
                                    renderer);
  CHECK(triggers[0].fade == 6);
  CHECK(animation.pending_ticks == 4);
}
