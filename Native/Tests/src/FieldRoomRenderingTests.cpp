#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Field/RoomEffectsCore.hpp"
#include "Field/RoomRendering.hpp"
#include "Field/RoomState.hpp"
#include "Render/RoomDraw.hpp"

namespace {
namespace fx = VS::Field;
namespace re = VS::Field::room_effects;
using Calls = std::vector<std::string>;
using Bytes = std::vector<std::byte>;

template <typename T>
void Put(Bytes& bytes, std::size_t at, T value) {
  std::memcpy(bytes.data() + at, &value, sizeof value);
}

template <typename T>
T Get(const Bytes& bytes, std::size_t at) {
  T value{};
  std::memcpy(&value, bytes.data() + at, sizeof value);
  return value;
}

std::size_t GroupAt(std::size_t group) {
  return re::kGeometryCountBytes + (group * re::kGroupRecordBytes);
}

Bytes Geometry(std::int32_t count, std::size_t whole_groups,
               std::size_t extra = 0) {
  Bytes bytes(GroupAt(whole_groups) + extra);
  Put(bytes, 0, count);
  return bytes;
}

void SetFlags(Bytes& geometry, std::size_t group, std::uint32_t flags) {
  Put(geometry, GroupAt(group), flags);
}

std::uint32_t Flags(const Bytes& geometry, std::size_t group) {
  return Get<std::uint32_t>(geometry, GroupAt(group));
}

std::uint16_t TriggerBits(int x, int z, int layer, int direction, int mode) {
  return static_cast<std::uint16_t>(x | (z << 5) | (layer << 10) |
                                    (direction << 11) | (mode << 14));
}

void AddTrigger(Bytes& triggers, std::uint16_t bits,
                const std::vector<std::int8_t>& groups) {
  const std::size_t at = triggers.size();
  const std::size_t padded = (groups.size() + 3) / 4 * 4;
  triggers.resize(at + 4 + padded);
  Put(triggers, at, bits);
  Put(triggers, at + 2, static_cast<std::uint16_t>(groups.size()));
  for (std::size_t index = 0; index < groups.size(); ++index) {
    Put(triggers, at + 4 + index, groups[index]);
  }
}

class FakePaletteHost final : public fx::RoomPaletteHost {
 public:
  void UploadClutRow(std::span<const std::uint16_t> colors, int row) override {
    rows.push_back(row);
    row_starts.push_back(colors.data());
    row_sizes.push_back(colors.size());
  }
  void SetGimmickPalettes(std::span<std::uint16_t> palettes) override {
    gimmick = palettes;
  }

  std::vector<int> rows;
  std::vector<const std::uint16_t*> row_starts;
  std::vector<std::size_t> row_sizes;
  std::span<std::uint16_t> gimmick;
};

class FakeGeometryHost final : public fx::RoomGeometryHost {
 public:
  bool GeometryValid() override { return valid; }
  void LoadViewTransform() override { calls.emplace_back("View"); }
  void UpdateGroupTransforms() override { calls.emplace_back("Groups"); }
  void DrawGroups() override { calls.emplace_back("Draw"); }
  void TintDrawnPrimitives(const VS::Render::RoomTint& tint) override {
    calls.push_back(std::format("Tint {} {} {} {}", tint.red, tint.green,
                                tint.blue, tint.strength));
  }

  bool valid = true;
  Calls calls;
};

class FakeSceneHost final : public fx::RoomSceneHost {
 public:
  int TickSpeed() override { return tick_speed; }
  bool RoomUpdatesPaused() override { return paused; }
  void LoadViewTransform() override { calls.emplace_back("View"); }
  void DrawTriggerMarkers() override { calls.emplace_back("Triggers"); }
  void DrawActorMarkers() override { calls.emplace_back("Actors"); }
  void UpdateGimmickAnimations() override { calls.emplace_back("Gimmicks"); }
  void UpdateRoomEffects() override { calls.emplace_back("Effects"); }
  void DrawPlatformShadows() override { calls.emplace_back("Shadows"); }
  void UpdateWeather() override { calls.emplace_back("Weather"); }
  bool RoomShown() override { return shown; }
  bool HasGeometry() override { return geometry; }
  int CameraYaw() override { return yaw; }
  void SetViewFacing(std::uint8_t mask) override {
    calls.push_back(std::format("Facing {}", mask));
  }
  void DrawGeometry() override { calls.emplace_back("Geometry"); }
  fx::MinimapSettings Minimap() override { return minimap; }
  bool MinimapVisible() override { return visible; }
  std::int16_t MinimapSlideFrame() override { return slide; }
  void SetMinimapSlideFrame(std::int16_t frame) override { slide = frame; }
  int DisplayClipX() override { return clip_x; }
  void DrawMinimap(const fx::MinimapLayout& layout) override {
    calls.push_back(
        std::format("Minimap {} {}", layout.area.x, layout.offset_x));
  }
  bool WeaponDrawn() override { return weapon; }
  int BorderPulsePhase() override { return phase; }
  void SetBorderPulsePhase(int value) override { phase = value; }
  void DrawMinimapFrame(const fx::MinimapFrameColors& colors) override {
    calls.push_back(std::format("Frame {}", colors.border[0].green));
  }

  int tick_speed = 4;
  bool paused = false;
  bool shown = true;
  bool geometry = true;
  int yaw = 0;
  fx::MinimapSettings minimap{.shown = true, .framed = true, .has_scene = true};
  bool visible = true;
  std::int16_t slide = 0;
  int clip_x = 0;
  bool weapon = false;
  int phase = 7;
  Calls calls;
};
}  // namespace

TEST_CASE("Room tint switches on any colour and off on black at zero strength",
          "[field-room-rendering]") {
  fx::RoomTintState tint;
  fx::SetRoomTint(tint, {.red = 200, .green = 3, .blue = 0}, 0x12345);
  CHECK(tint.active);
  CHECK(tint.parameters.red == -56);
  CHECK(tint.parameters.strength == 0x2345);
  fx::SetRoomTint(tint, {}, 0);
  CHECK_FALSE(tint.active);
  CHECK(tint.parameters.red == -56);
  fx::SetRoomTint(tint, {}, 1);
  CHECK(tint.active);
  fx::ClearRoomTint(tint);
  CHECK_FALSE(tint.active);
}

TEST_CASE("Room switches, the animation clock and the room display",
          "[field-room-rendering]") {
  fx::RoomRenderState state;
  fx::SetFieldRenderingEnabled(state, true);
  fx::SetRoomAnimationsPaused(state, true);
  CHECK(state.field_rendering_enabled);
  CHECK(state.updates_paused);
  CHECK(fx::AdvanceRoomAnimationClock(true, 40, 4) == 40);
  CHECK(fx::AdvanceRoomAnimationClock(false, 0xFFFF, 2) == 1);

  fx::RoomDisplay display;
  CHECK_FALSE(fx::SetRoomRenderingEnabled(display, true));
  CHECK_FALSE(display.displayed);
  display.loaded = true;
  CHECK(fx::SetRoomRenderingEnabled(display, true));
  CHECK(display.displayed);
}

TEST_CASE("Zone palette rows copy whole pairs inside the palettes",
          "[field-room-rendering]") {
  std::vector<std::uint16_t> palettes(fx::kRoomPaletteColors);
  const std::array<std::uint16_t, 5> colors{1, 2, 3, 4, 5};
  CHECK(fx::CopyZonePaletteRow(palettes, colors, 1, 2));
  CHECK(palettes[258] == 1);
  CHECK(palettes[261] == 4);
  CHECK(palettes[262] == 0);
  CHECK(fx::CopyZonePaletteRow(palettes, colors, 3, 252));
  CHECK(palettes[1023] == 4);
  CHECK_FALSE(fx::CopyZonePaletteRow(palettes, colors, 3, 254));
  CHECK_FALSE(fx::CopyZonePaletteRow(palettes, colors, 0, -2));
  CHECK_FALSE(fx::CopyZonePaletteRow(palettes, colors, 4, 2));
}

TEST_CASE("Room palettes upload three rows and hand the fourth on",
          "[field-room-rendering]") {
  std::vector<std::uint16_t> palettes(fx::kRoomPaletteColors - 1);
  FakePaletteHost refused;
  CHECK_FALSE(fx::UploadRoomPalettes(palettes, refused));
  CHECK(refused.rows.empty());

  palettes.resize(fx::kRoomPaletteColors);
  FakePaletteHost host;
  CHECK(fx::UploadRoomPalettes(palettes, host));
  CHECK(host.rows == std::vector<int>{0, 1, 2});
  CHECK(host.row_starts[2] == palettes.data() + 512);
  CHECK(host.row_sizes[1] == fx::kRoomPaletteRowColors);
  CHECK(host.gimmick.data() == palettes.data() + 768);
  CHECK(host.gimmick.size() == fx::kRoomPaletteRowColors);
}

TEST_CASE("Room texture ids are offset per group and wrap at 16 bits",
          "[field-room-rendering]") {
  constexpr std::size_t kBlock =
      re::kGeometryCountBytes + (2 * re::kGroupRecordBytes);
  constexpr std::size_t kFirst = kBlock + re::kPolygonBlockCountsBytes;
  constexpr std::size_t kQuad = kFirst + VS::Render::kRoomTriangleRecordBytes;
  Bytes geometry = Geometry(2, 2,
                            re::kPolygonBlockCountsBytes +
                                VS::Render::kRoomTriangleRecordBytes +
                                VS::Render::kRoomQuadRecordBytes);
  const std::size_t block_word =
      re::kGroupPolygonBlockWord * re::kGroupWordBytes;
  Put(geometry, GroupAt(0) + block_word, static_cast<std::int32_t>(kBlock + 3));
  Put(geometry, GroupAt(1) + block_word, std::int32_t{0x7FFFFFF0});
  Put(geometry, kBlock, std::int32_t{1});
  Put(geometry, kBlock + 4, std::int32_t{1});
  Put(geometry, kFirst + fx::kRoomPolygonTextureIdOffset,
      std::uint16_t{0xFFFF});
  Put(geometry, kQuad + fx::kRoomPolygonTextureIdOffset, std::uint16_t{7});

  CHECK(fx::OffsetRoomTextureIds(geometry, fx::kRoomTextureIdOffset) == 1);
  CHECK(Get<std::uint16_t>(geometry,
                           kFirst + fx::kRoomPolygonTextureIdOffset) == 10);
  CHECK(Get<std::uint16_t>(geometry, kQuad + fx::kRoomPolygonTextureIdOffset) ==
        18);
  CHECK(fx::OffsetRoomTextureIds({}, 1) == 0);
}

TEST_CASE("Clearing the occlusion re-enables only whole groups",
          "[field-room-rendering]") {
  Bytes geometry = Geometry(3, 2);
  SetFlags(geometry, 0, re::kGroupDisabledFlag | re::kGroupHiddenFlag);
  SetFlags(geometry, 1, re::kGroupDisabledFlag);
  fx::ClearGeometryOcclusion(geometry);
  CHECK(Flags(geometry, 0) == re::kGroupHiddenFlag);
  CHECK(Flags(geometry, 1) == 0);

  Bytes negative = Geometry(-1, 1);
  SetFlags(negative, 0, re::kGroupDisabledFlag);
  fx::ClearGeometryOcclusion(negative);
  CHECK(Flags(negative, 0) == re::kGroupDisabledFlag);
}

TEST_CASE("The camera's view direction and facing eighths",
          "[field-room-rendering]") {
  CHECK(fx::CameraViewDirection(0) == 4);
  CHECK(fx::CameraViewDirection(0x800) == 0);
  CHECK(fx::CameraViewDirection(-0x200) == 3);
  CHECK(fx::RoomViewFacingMask(0) == 0x10);
  CHECK(fx::RoomViewFacingMask(0x100) == 0x18);
  CHECK(fx::RoomViewFacingMask(-0x100) == 0x30);
}

TEST_CASE("The first matching view trigger disables its groups",
          "[field-room-rendering]") {
  const fx::OcclusionQuery query{.tile_x = 3,
                                 .tile_z = 5,
                                 .floor_layer = 1,
                                 .distance_mode = 1,
                                 .camera_yaw = 0x800};
  Bytes triggers;
  AddTrigger(triggers, TriggerBits(3, 5, 1, 1, 1), {2});
  AddTrigger(triggers, TriggerBits(3, 5, 1, 0, 1), {0, 2});
  AddTrigger(triggers, TriggerBits(3, 5, 1, 0, 1), {1});
  Bytes geometry = Geometry(3, 3);
  SetFlags(geometry, 1, re::kGroupDisabledFlag);

  CHECK(fx::ApplyCameraGeometryOcclusion(geometry, triggers, query));
  CHECK(Flags(geometry, 0) == re::kGroupDisabledFlag);
  CHECK(Flags(geometry, 1) == 0);
  CHECK(Flags(geometry, 2) == re::kGroupDisabledFlag);

  SetFlags(geometry, 1, re::kGroupDisabledFlag);
  CHECK_FALSE(fx::ApplyCameraGeometryOcclusion(geometry, {}, query));
  CHECK(Flags(geometry, 1) == re::kGroupDisabledFlag);
}

TEST_CASE("Truncated view triggers end the walk", "[field-room-rendering]") {
  const fx::OcclusionQuery query{.tile_x = 1, .camera_yaw = 0x800};
  Bytes geometry = Geometry(1, 1);
  Bytes header_cut;
  AddTrigger(header_cut, TriggerBits(2, 0, 0, 0, 0), {0});
  header_cut.resize(header_cut.size() + 2);
  CHECK_FALSE(fx::ApplyCameraGeometryOcclusion(geometry, header_cut, query));

  Bytes ids_cut;
  AddTrigger(ids_cut, TriggerBits(1, 0, 0, 0, 0), {0, 0, 0, 0, 0});
  ids_cut.resize(6);
  CHECK_FALSE(fx::ApplyCameraGeometryOcclusion(geometry, ids_cut, query));
  CHECK(Flags(geometry, 0) == 0);
}

TEST_CASE("Room geometry draws then tints when the tint is on",
          "[field-room-rendering]") {
  fx::RoomTintState tint;
  FakeGeometryHost refused;
  refused.valid = false;
  CHECK_FALSE(fx::DrawRoomGeometry(tint, refused));
  CHECK(refused.calls.empty());

  FakeGeometryHost plain;
  CHECK(fx::DrawRoomGeometry(tint, plain));
  CHECK(plain.calls == Calls{"View", "Groups", "Draw"});

  fx::SetRoomTint(tint, {.red = 4, .green = 5, .blue = 6}, 0x800);
  FakeGeometryHost tinted;
  CHECK(fx::DrawRoomGeometry(tint, tinted));
  CHECK(tinted.calls == Calls{"View", "Groups", "Draw", "Tint 4 5 6 2048"});
}

TEST_CASE("The minimap slides in by one and out by two",
          "[field-room-rendering]") {
  CHECK(fx::AdvanceMinimapSlide(7, true) == 8);
  CHECK(fx::AdvanceMinimapSlide(8, true) == 8);
  CHECK(fx::AdvanceMinimapSlide(8, false) == 6);
  CHECK(fx::AdvanceMinimapSlide(1, false) == 0);
  CHECK(fx::AdvanceMinimapSlide(32767, true) == -32768);
  CHECK(fx::AdvanceMinimapSlide(-32768, false) == 32766);
}

TEST_CASE("The minimap layout follows the drawn display and the camera",
          "[field-room-rendering]") {
  const fx::MinimapLayout near = fx::MinimapLayoutFor(8, 0, 0x100);
  CHECK(near.area ==
        fx::MinimapArea{.x = 216, .y = 156, .width = 96, .height = 64});
  CHECK(near.offset_x == 264);
  CHECK(near.offset_y == 196);
  CHECK(near.map_yaw == 0x700);
  CHECK(fx::MinimapLayoutFor(1, 320, 0).area.x == 536);
  CHECK(fx::MinimapLayoutFor(1, 320, 0).offset_x == 376);
}

TEST_CASE("The minimap frame pulses red while a weapon is drawn",
          "[field-room-rendering]") {
  int phase = 9;
  const fx::MinimapFrameColors calm =
      fx::AdvanceMinimapFrameColors(false, phase, 4);
  CHECK(phase == 0);
  CHECK(calm.border[2] == fx::FrameColor{0, 255, 0});

  const fx::MinimapFrameColors start =
      fx::AdvanceMinimapFrameColors(true, phase, 4);
  CHECK(phase == 2);
  CHECK(start.border[0] == fx::FrameColor{.red = 128});
  CHECK(start.border[2] == fx::FrameColor{.red = 191});
  CHECK(start.edge[0] == fx::FrameColor{.red = 160});

  phase = 4;
  const fx::MinimapFrameColors peak =
      fx::AdvanceMinimapFrameColors(true, phase, 3);
  CHECK(phase == 5);
  CHECK(peak.border[0] == fx::FrameColor{.red = 64});
}

TEST_CASE("A room frame draws its passes, geometry and minimap in order",
          "[field-room-rendering]") {
  FakeSceneHost host;
  host.tick_speed = 5;
  fx::DrawRoomScene(host);
  CHECK(host.calls == Calls{"View", "Triggers", "Actors", "Gimmicks", "Effects",
                            "Effects", "Shadows", "Weather", "Facing 16",
                            "Geometry", "Minimap 216 376", "Frame 128"});
  CHECK(host.slide == 1);
  CHECK(host.phase == 0);
}

TEST_CASE("A paused or hidden room frame skips its later passes",
          "[field-room-rendering]") {
  FakeSceneHost paused;
  paused.paused = true;
  paused.shown = false;
  fx::DrawRoomScene(paused);
  CHECK(paused.calls ==
        Calls{"View", "Triggers", "Actors", "Gimmicks", "Shadows", "Weather"});

  FakeSceneHost unframed;
  unframed.tick_speed = 1;
  unframed.geometry = false;
  unframed.visible = false;
  unframed.minimap.framed = false;
  unframed.slide = 3;
  fx::DrawRoomScene(unframed);
  CHECK(unframed.calls.back() == "Minimap 216 376");
  CHECK(unframed.slide == 1);

  FakeSceneHost hidden_map;
  hidden_map.visible = false;
  fx::DrawRoomScene(hidden_map);
  CHECK(hidden_map.calls.back() == "Geometry");
  CHECK(hidden_map.slide == 0);
}
