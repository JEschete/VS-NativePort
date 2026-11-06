// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomRenderingLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomTriggerMarkersLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"

extern "C" {
#include "Battle/AbilityState.h"
#include "Battle/Actor.h"
#include "Battle/Feedback.h"
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Field/CameraLiveState.h"
#include "Field/Gimmicks.h"
#include "Field/Minimap.h"
#include "Field/Mpd.h"
#include "Field/Room.h"
#include "Field/RoomRenderingLiveState.h"
#include "Input/ControllerLiveState.h"
#include "Legacy/Psx.h"
#include "Render/Display.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "State/Timers.h"

// decomp: D_800F190C (D_800F190C@BATTLE/BATTLE.PRG)
int Field_isFieldRenderingEnabled;
// decomp: D_800F1BA4 (D_800F1BA4@BATTLE/BATTLE.PRG)
u_short Field_areRoomUpdatesPaused;
// decomp: D_800F1BA8 (D_800F1BA8@BATTLE/BATTLE.PRG)
u_short* Field_roomPalettes;
// decomp: D_800F1BB0 (D_800F1BB0@BATTLE/BATTLE.PRG)
Field_RoomTint Field_roomTint;
}

namespace {
// The original's layouts (146C.c:168, Mpd.h and 146C.c:15966).
constexpr std::size_t kRoomTintBytes = 8;
constexpr std::size_t kViewTriggerHeaderBytes = 4;
}  // namespace

static_assert(sizeof(Field_RoomTint) == kRoomTintBytes);
static_assert(offsetof(Field_MpdTriangle, textureId) ==
              VS::Field::kRoomPolygonTextureIdOffset);
static_assert(offsetof(Field_MpdQuad, textureId) ==
              VS::Field::kRoomPolygonTextureIdOffset);
static_assert(sizeof(Field_MpdViewTrigger) == kViewTriggerHeaderBytes);

namespace VS::Field {
namespace {
constexpr const char* kChannel = "field";
// The minimap, its frame and their draw areas link at scene entry 5
// (146C.c:15791 orderingTable + 5).
constexpr int kMinimapOrderingSlot = 5;
// The frame's corners and left edge on the near display buffer.
constexpr int kFrameLeft = 215;
constexpr int kFrameTop = 155;
constexpr int kFrameRight = 313;
constexpr int kFrameBottom = 220;
// _insertTPage(5, getTPage(0, 3, 0, 0)) after the frame: dithered drawing,
// additive-quarter blending from page 0 (insertTPage.h:3).
constexpr std::uint32_t kFrameDrawModeWord = 0xE1000200U | 0x60U;
constexpr std::uint8_t kFrameDrawModeLength = 2;
// Both display buffers' drawing environments; the one drawn next is
// (Render_frameBuf + 1) & 1.
constexpr int kDisplayBufferMask = 1;
constexpr std::uint32_t kViewFacingMask = 0xFF;

std::uint64_t dropped_packets = 0;
std::uint64_t refused_calls = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

void RefuseCall(std::string_view entry, std::string_view reason) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN(kChannel, "{} refused: {} ({} refusals)", entry, reason,
                refused_calls);
  }
}

[[nodiscard]] Render_FrameScratch& Scratch() {
  return *static_cast<Render_FrameScratch*>(Core_ScratchBase());
}

template <typename Byte>
[[nodiscard]] std::span<Byte> Section(void* data, std::uint32_t length) {
  if (data == nullptr || static_cast<std::int32_t>(length) <= 0) {
    return {};
  }
  return {static_cast<Byte*>(data), length};
}

[[nodiscard]] std::span<std::byte> RoomGeometry() {
  return Section<std::byte>(Field_roomData.geometry,
                            Field_roomData.header.geometryLength);
}

[[nodiscard]] std::span<const std::byte> RoomViewTriggers() {
  return Section<const std::byte>(Field_roomData.viewTriggers,
                                  Field_roomData.header.viewTriggersLength);
}

// The 1024 colours RoomStateLive.cpp allocates, or none before a session.
[[nodiscard]] std::span<std::uint16_t> RoomPalettes() {
  if (Field_roomPalettes == nullptr) {
    return {};
  }
  return {Field_roomPalettes, kRoomPaletteColors};
}

[[nodiscard]] RoomTintState LoadTint() {
  return RoomTintState{.parameters = {.red = Field_roomTint.red,
                                      .green = Field_roomTint.green,
                                      .blue = Field_roomTint.blue,
                                      .strength = Field_roomTint.strength},
                       .active = Field_roomTint.isActive != 0};
}

void StoreTint(const RoomTintState& tint) {
  Field_roomTint.red = tint.parameters.red;
  Field_roomTint.green = tint.parameters.green;
  Field_roomTint.blue = tint.parameters.blue;
  Field_roomTint.strength = tint.parameters.strength;
  Field_roomTint.isActive = static_cast<std::int16_t>(tint.active ? 1 : 0);
}

void LoadViewTransform() {
  Core_Gte_SetRotMatrix(&Scratch().viewMatrix);
  Core_Gte_SetTransMatrix(&Scratch().viewMatrix);
}

[[nodiscard]] DRAWENV& DrawnEnvironment() {
  return Render_legacyDrawEnvironments[(Render_frameBuf + 1) &
                                       kDisplayBufferMask];
}

// Writes a packet into the frame's primitive buffer and links it at the
// minimap's entry, as the original's AddPrim(orderingTable + 5) did.
template <typename Packet>
void LinkPacket(const Packet& packet, std::string_view kind) {
  constexpr std::size_t kWords = sizeof(Packet) / sizeof(u_long);
  static_assert(sizeof(Packet) % sizeof(u_long) == 0);
  u_long* const entry = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kMinimapOrderingSlot);
  u_long* const slot =
      entry != nullptr ? Render::ReserveLivePrimitiveWords(kWords) : nullptr;
  if (slot == nullptr) {
    if (ShouldReport(dropped_packets)) {
      VS_LOG_WARN(kChannel,
                  "minimap {} has no ordering entry or buffer room; dropped "
                  "({} drops)",
                  kind, dropped_packets);
    }
    return;
  }
  std::memcpy(slot, &packet, sizeof(Packet));
  AddPrim(entry, slot);
  Scratch().primitiveCursor = slot + kWords;
}

void LinkDrawArea(RECT area, std::string_view kind) {
  DR_AREA packet{};
  SetDrawArea(&packet, &area);
  LinkPacket(packet, kind);
}

[[nodiscard]] LINE_G4 FrameBorder(const MinimapFrameColors& colors) {
  LINE_G4 border{};
  setLineG4(&border);
  setXY4(&border, kFrameLeft, kFrameTop, kFrameRight, kFrameTop, kFrameRight,
         kFrameBottom, kFrameLeft, kFrameBottom);
  setSemiTrans(&border, 1);
  const auto& corners = colors.border;
  setRGB0(&border, corners[0].red, corners[0].green, corners[0].blue);
  setRGB1(&border, corners[1].red, corners[1].green, corners[1].blue);
  setRGB2(&border, corners[2].red, corners[2].green, corners[2].blue);
  setRGB3(&border, corners[3].red, corners[3].green, corners[3].blue);
  return border;
}

[[nodiscard]] LINE_G2 FrameEdge(const MinimapFrameColors& colors) {
  LINE_G2 edge{};
  setLineG2(&edge);
  setSemiTrans(&edge, 1);
  setXY2(&edge, kFrameLeft, kFrameBottom, kFrameLeft, kFrameTop);
  const auto& ends = colors.edge;
  setRGB0(&edge, ends[0].red, ends[0].green, ends[0].blue);
  setRGB1(&edge, ends[1].red, ends[1].green, ends[1].blue);
  return edge;
}

// The three-word texture page packet _insertTPage writes: a two-word length,
// the draw mode word and a zero word.
[[nodiscard]] DR_MODE FrameDrawMode() {
  DR_MODE mode{};
  setlen(&mode, kFrameDrawModeLength);
  mode.code[0] = kFrameDrawModeWord;
  return mode;
}

class LiveGeometryHost final : public RoomGeometryHost {
 public:
  explicit LiveGeometryHost(void* geometry) : geometry_(geometry) {}

  bool GeometryValid() override {
    return Render::ValidateLiveRoomGeometry(geometry_, Bytes());
  }
  void LoadViewTransform() override { Field::LoadViewTransform(); }
  void UpdateGroupTransforms() override {
    Render::UpdateLiveRoomGroupTransforms(geometry_, Bytes());
  }
  void DrawGroups() override {
    Field_roomPrimitiveRangeStart = Scratch().primitiveCursor;
    Render::DrawLiveRoomGeometry(geometry_, Bytes(),
                                 Field_roomGeometryDrawMode);
    Field_roomPrimitiveRangeEnd = Scratch().primitiveCursor;
  }
  void TintDrawnPrimitives(const Render::RoomTint& tint) override {
    const Field_RoomTint packed{.red = tint.red,
                                .green = tint.green,
                                .blue = tint.blue,
                                .reserved03 = 0,
                                .strength = tint.strength,
                                .isActive = 1};
    Render::TintLiveRoomGeometryPrimitives(
        Field_roomPrimitiveRangeStart, Field_roomPrimitiveRangeEnd, packed, 0);
  }

 private:
  // 146C.c:15926 bounds any geometry by the loaded room's extent.
  [[nodiscard]] static std::size_t Bytes() {
    return Field_roomData.header.geometryLength;
  }

  void* geometry_;
};

class LiveSceneHost final : public RoomSceneHost {
 public:
  int TickSpeed() override { return State_tickSpeed; }
  bool RoomUpdatesPaused() override { return Field_areRoomUpdatesPaused != 0; }
  void LoadViewTransform() override { Field::LoadViewTransform(); }
  void DrawTriggerMarkers() override { DrawLiveRoomTriggerMarkers(); }
  void DrawActorMarkers() override { Battle_DrawActorMarkers(); }
  void UpdateGimmickAnimations() override {
    Field_UpdateGimmickAnimationQueue();
  }
  void UpdateRoomEffects() override { LiveUpdateRoomEffects(); }
  void DrawPlatformShadows() override { Field_DrawMovingPlatformShadows(); }
  void UpdateWeather() override { LiveUpdateRoomWeather(); }
  // 146C.c:15791 compares isLoaded and displayFlag as one halfword to 0x101.
  bool RoomShown() override {
    return Field_roomData.isLoaded == 1 && Field_roomData.displayFlag == 1;
  }
  bool HasGeometry() override { return Field_roomData.geometry != nullptr; }
  int CameraYaw() override {
    return static_cast<int>(Scratch().camera.angles.vy);
  }
  void SetViewFacing(std::uint8_t mask) override {
    u_int& facing = Scratch().firstPersonViewFacing;
    facing = (facing & ~kViewFacingMask) | mask;
  }
  void DrawGeometry() override {
    LiveDrawRoomGeometry(Field_roomData.geometry);
  }
  MinimapSettings Minimap() override {
    return MinimapSettings{.shown = (Input_settings.simpleMap & 1) != 0,
                           .framed = (Input_settings.simpleMap & 2) != 0,
                           .has_scene = Field_roomData.minimapScene != nullptr};
  }
  bool MinimapVisible() override { return Field_isMinimapVisible != 0; }
  std::int16_t MinimapSlideFrame() override { return Field_minimapSlideFrame; }
  void SetMinimapSlideFrame(std::int16_t frame) override {
    Field_minimapSlideFrame = frame;
  }
  int DisplayClipX() override { return DrawnEnvironment().clip.x; }
  void DrawMinimap(const MinimapLayout& layout) override;
  bool WeaponDrawn() override {
    return Battle_characterState != nullptr &&
           Battle_characterState->weaponDrawn == 1;
  }
  int BorderPulsePhase() override { return Field_minimapBorderPulsePhase; }
  void SetBorderPulsePhase(int phase) override {
    Field_minimapBorderPulsePhase = phase;
  }
  void DrawMinimapFrame(const MinimapFrameColors& colors) override {
    LinkPacket(FrameBorder(colors), "frame border");
    LinkPacket(FrameEdge(colors), "frame edge");
    LinkPacket(FrameDrawMode(), "frame texture page");
  }
};

// Restores the projection centre the minimap moved, also when the scene
// draw throws.
class ProjectionOffsetScope {
 public:
  explicit ProjectionOffsetScope(const Render_ScreenOffset& offset)
      : saved_(Render::GetLiveProjectionScreenOffset()) {
    Render::SetLiveProjectionScreenOffset(offset);
  }
  ProjectionOffsetScope(const ProjectionOffsetScope&) = delete;
  ProjectionOffsetScope& operator=(const ProjectionOffsetScope&) = delete;
  ProjectionOffsetScope(ProjectionOffsetScope&&) = delete;
  ProjectionOffsetScope& operator=(ProjectionOffsetScope&&) = delete;
  ~ProjectionOffsetScope() { Render::SetLiveProjectionScreenOffset(saved_); }

 private:
  Render_ScreenOffset saved_;
};

void LiveSceneHost::DrawMinimap(const MinimapLayout& layout) {
  MATRIX rotation{};
  BuildLiveMinimapRotation(&rotation, layout.map_yaw);
  Core_Gte_SetRotMatrix(&rotation);
  LinkDrawArea(DrawnEnvironment().clip, "display draw area");
  {
    const ProjectionOffsetScope scope(
        Render_ScreenOffset{.x = layout.offset_x, .y = layout.offset_y});
    DrawLiveMinimapScene(Field_roomData.minimapScene,
                         Field_roomData.header.minimapSceneLength);
  }
  Field::LoadViewTransform();
  LinkDrawArea(RECT{.x = layout.area.x,
                    .y = layout.area.y,
                    .w = layout.area.width,
                    .h = layout.area.height},
               "map draw area");
}

class LivePaletteHost final : public RoomPaletteHost {
 public:
  void UploadClutRow(std::span<const std::uint16_t> colors, int row) override {
    Render::LoadLiveClutColors(colors.data(), static_cast<u_int>(row), 0,
                               static_cast<u_int>(colors.size()));
  }
  void SetGimmickPalettes(std::span<std::uint16_t> palettes) override {
    Field_SetGimmickPaletteTable(palettes.data());
  }
};
}  // namespace

RoomRenderState LiveRoomRenderState() {
  return RoomRenderState{
      .field_rendering_enabled = Field_isFieldRenderingEnabled != 0,
      .updates_paused = Field_areRoomUpdatesPaused != 0,
      .tint = LoadTint()};
}

void LiveSetFieldRenderingEnabled(int enabled) {
  RoomRenderState state = LiveRoomRenderState();
  SetFieldRenderingEnabled(state, enabled != 0);
  Field_isFieldRenderingEnabled = state.field_rendering_enabled ? 1 : 0;
}

void LiveDrawRoomScene() {
  LiveSceneHost host;
  DrawRoomScene(host);
}

void LiveDrawRoomGeometry(void* geometry) {
  LiveGeometryHost host(geometry);
  static_cast<void>(DrawRoomGeometry(LoadTint(), host));
}

void LiveAdvanceRoomAnimationClock() {
  Field_roomAnimationTicks =
      AdvanceRoomAnimationClock(Field_areRoomUpdatesPaused != 0,
                                Field_roomAnimationTicks, State_tickSpeed);
}

void LiveSetRoomRenderingEnabled(int enabled) {
  RoomDisplay display{.loaded = Field_roomData.isLoaded != 0,
                      .displayed = Field_roomData.displayFlag != 0,
                      .owner_room_index = Field_roomData.roomIndex};
  if (SetRoomRenderingEnabled(display, enabled != 0)) {
    Field_roomData.displayFlag = static_cast<char>(display.displayed ? 1 : 0);
  }
}

void LiveCopyZonePaletteRow(const void* colors, int row, int first, int count) {
  if (count < 0) {
    RefuseCall("Zone palette row copy", "negative colour count");
    return;
  }
  if (count > 0 && colors == nullptr) {
    RefuseCall("Zone palette row copy", "null colours");
    return;
  }
  const std::span<std::uint16_t> palettes = RoomPalettes();
  if (palettes.empty()) {
    RefuseCall("Zone palette row copy", "no room palettes");
    return;
  }
  const std::span<const std::uint16_t> source{
      static_cast<const std::uint16_t*>(colors),
      static_cast<std::size_t>(count)};
  static_cast<void>(CopyZonePaletteRow(palettes, source, row, first));
}

void LiveUploadRoomPalettes() {
  LivePaletteHost host;
  static_cast<void>(UploadRoomPalettes(RoomPalettes(), host));
}

void LiveOffsetRoomTextureIds(int offset, int* geometry) {
  if (geometry == nullptr || geometry != Field_roomData.geometry) {
    RefuseCall("Texture id offset", "geometry is not the loaded room's");
    return;
  }
  static_cast<void>(OffsetRoomTextureIds(RoomGeometry(), offset));
}

void LiveSetRoomTint(const Render_ColorCode* color, int strength) {
  if (color == nullptr) {
    RefuseCall("Room tint", "null colour");
    return;
  }
  RoomTintState tint = LoadTint();
  SetRoomTint(
      tint,
      TintColor{.red = color->red, .green = color->green, .blue = color->blue},
      strength);
  StoreTint(tint);
}

void LiveClearRoomTint() {
  RoomTintState tint = LoadTint();
  ClearRoomTint(tint);
  StoreTint(tint);
}

void LiveSetRoomAnimationsPaused(int paused) {
  RoomRenderState state = LiveRoomRenderState();
  SetRoomAnimationsPaused(state, paused != 0);
  Field_areRoomUpdatesPaused = static_cast<u_short>(state.updates_paused);
}

int LiveApplyCameraGeometryOcclusion(int tile_x, int tile_z, int floor_layer,
                                     int distance_mode) {
  const OcclusionQuery query{
      .tile_x = tile_x,
      .tile_z = tile_z,
      .floor_layer = floor_layer,
      .distance_mode = distance_mode,
      .camera_yaw = static_cast<int>(Scratch().camera.angles.vy)};
  return ApplyCameraGeometryOcclusion(RoomGeometry(), RoomViewTriggers(), query)
             ? 1
             : 0;
}

void LiveClearGeometryOcclusion() { ClearGeometryOcclusion(RoomGeometry()); }
}  // namespace VS::Field
