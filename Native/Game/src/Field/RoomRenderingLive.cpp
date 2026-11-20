// Hand-maintained (tools/port/owned.csv).
#include "Field/RoomRenderingLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Battle/AbilityState.hpp"
#include "Battle/Actor.hpp"
#include "Battle/ActorMarkersLive.hpp"
#include "Battle/Feedback.hpp"
#include "Core/Gte.hpp"
#include "Core/Log.hpp"
#include "Core/Scratch.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/EnvironmentLive.hpp"
#include "Field/GimmickAnimationQueueLive.hpp"
#include "Field/GimmickResourcesLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/MapLive.hpp"
#include "Field/Minimap.hpp"
#include "Field/MinimapSceneLive.hpp"
#include "Field/Mpd.hpp"
#include "Field/PlatformPlacementLive.hpp"
#include "Field/Room.hpp"
#include "Field/RoomEffectsLive.hpp"
#include "Field/RoomRenderingLiveState.hpp"
#include "Field/RoomTriggerMarkersLive.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Display.hpp"
#include "Render/Palette.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;

using VS::Legacy::AddPrim;
using VS::Legacy::DR_AREA;
using VS::Legacy::DR_MODE;
using VS::Legacy::DRAWENV;
using VS::Legacy::LINE_G2;
using VS::Legacy::LINE_G4;
using VS::Legacy::RECT;
using VS::Legacy::SetDrawArea;
using VS::Legacy::setlen;
using VS::Legacy::setLineG2;
using VS::Legacy::setLineG4;
using VS::Legacy::setRGB0;
using VS::Legacy::setRGB1;
using VS::Legacy::setRGB2;
using VS::Legacy::setRGB3;
using VS::Legacy::setSemiTrans;
using VS::Legacy::setXY2;
using VS::Legacy::setXY4;

// decomp: D_800F190C (D_800F190C@BATTLE/BATTLE.PRG)
int VS::Field::isFieldRenderingEnabled;
// decomp: D_800F1BA4 (D_800F1BA4@BATTLE/BATTLE.PRG)
std::uint16_t VS::Field::areRoomUpdatesPaused;
// decomp: D_800F1BA8 (D_800F1BA8@BATTLE/BATTLE.PRG)
std::uint16_t* VS::Field::roomPalettes;
// decomp: D_800F1BB0 (D_800F1BB0@BATTLE/BATTLE.PRG)
VS::Field::RoomTintRecord VS::Field::roomTint;

namespace {
// The original's layouts (146C.c:168, Mpd.hpp and 146C.c:15966).
constexpr std::size_t kRoomTintBytes = 8;
constexpr std::size_t kViewTriggerHeaderBytes = 4;
}  // namespace

static_assert(sizeof(VS::Field::RoomTintRecord) == kRoomTintBytes);
static_assert(offsetof(VS::Field::MpdTriangle, texture_id) ==
              VS::Field::kRoomPolygonTextureIdOffset);
static_assert(offsetof(VS::Field::MpdQuad, texture_id) ==
              VS::Field::kRoomPolygonTextureIdOffset);
static_assert(sizeof(VS::Field::MpdViewTrigger) == kViewTriggerHeaderBytes);

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
// (VS::Render::frameBuf + 1) & 1.
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
    VS::Core::LogWarn(kChannel, "{} refused: {} ({} refusals)", entry, reason,
                      refused_calls);
  }
}

[[nodiscard]] VS::Render::FrameScratch& Scratch() {
  return *static_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
}

template <typename Byte>
[[nodiscard]] std::span<Byte> Section(void* data, std::uint32_t length) {
  if (data == nullptr || static_cast<std::int32_t>(length) <= 0) {
    return {};
  }
  return {static_cast<Byte*>(data), length};
}

[[nodiscard]] std::span<std::byte> RoomGeometry() {
  return Section<std::byte>(VS::Field::roomData.geometry,
                            VS::Field::roomData.header.geometry_length);
}

[[nodiscard]] std::span<const std::byte> RoomViewTriggers() {
  return Section<const std::byte>(
      VS::Field::roomData.view_triggers,
      VS::Field::roomData.header.view_triggers_length);
}

// The 1024 colours RoomStateLive.cpp allocates, or none before a session.
[[nodiscard]] std::span<std::uint16_t> RoomPalettes() {
  if (roomPalettes == nullptr) {
    return {};
  }
  return {roomPalettes, kRoomPaletteColors};
}

[[nodiscard]] RoomTintState LoadTint() {
  return RoomTintState{.parameters = {.red = roomTint.red,
                                      .green = roomTint.green,
                                      .blue = roomTint.blue,
                                      .strength = roomTint.strength},
                       .active = roomTint.is_active != 0};
}

void StoreTint(const RoomTintState& tint) {
  roomTint.red = tint.parameters.red;
  roomTint.green = tint.parameters.green;
  roomTint.blue = tint.parameters.blue;
  roomTint.strength = tint.parameters.strength;
  roomTint.is_active = static_cast<std::int16_t>(tint.active ? 1 : 0);
}

void LoadViewTransform() {
  SetRotMatrix(&Scratch().view_matrix);
  SetTransMatrix(&Scratch().view_matrix);
}

[[nodiscard]] DRAWENV& DrawnEnvironment() {
  return VS::Render::legacyDrawEnvironments[(VS::Render::frameBuf + 1) &
                                            kDisplayBufferMask];
}

// Writes a packet into the frame's primitive buffer and links it at the
// minimap's entry, as the original's AddPrim(orderingTable + 5) did.
template <typename Packet>
void LinkPacket(const Packet& packet, std::string_view kind) {
  constexpr std::size_t kWords = sizeof(Packet) / sizeof(std::uint32_t);
  static_assert(sizeof(Packet) % sizeof(std::uint32_t) == 0);
  std::uint32_t* const entry = Render::LiveFrameOrderingEntry(
      Render::LiveOrderingBase::kScene, kMinimapOrderingSlot);
  std::uint32_t* const slot =
      entry != nullptr ? Render::ReserveLivePrimitiveWords(kWords) : nullptr;
  if (slot == nullptr) {
    if (ShouldReport(dropped_packets)) {
      VS::Core::LogWarn(
          kChannel,
          "minimap {} has no ordering entry or buffer room; dropped "
          "({} drops)",
          kind, dropped_packets);
    }
    return;
  }
  std::memcpy(slot, &packet, sizeof(Packet));
  AddPrim(entry, slot);
  Scratch().primitive_cursor = slot + kWords;
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
  setRGB3(&border, corners.back().red, corners.back().green,
          corners.back().blue);
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
    VS::Field::roomPrimitiveRangeStart = Scratch().primitive_cursor;
    Render::DrawLiveRoomGeometry(geometry_, Bytes(), roomGeometryDrawMode);
    VS::Field::roomPrimitiveRangeEnd = Scratch().primitive_cursor;
  }
  void TintDrawnPrimitives(const Render::RoomTint& tint) override {
    const VS::Field::RoomTintRecord packed{.red = tint.red,
                                           .green = tint.green,
                                           .blue = tint.blue,
                                           .reserved03 = 0,
                                           .strength = tint.strength,
                                           .is_active = 1};
    Render::TintLiveRoomGeometryPrimitives(VS::Field::roomPrimitiveRangeStart,
                                           VS::Field::roomPrimitiveRangeEnd,
                                           packed, 0);
  }

 private:
  // 146C.c:15926 bounds any geometry by the loaded room's extent.
  [[nodiscard]] static std::size_t Bytes() {
    return VS::Field::roomData.header.geometry_length;
  }

  void* geometry_;
};

class LiveSceneHost final : public RoomSceneHost {
 public:
  int TickSpeed() override { return VS::State::tickSpeed; }
  bool RoomUpdatesPaused() override { return areRoomUpdatesPaused != 0; }
  void LoadViewTransform() override { Field::LoadViewTransform(); }
  void DrawTriggerMarkers() override { DrawLiveRoomTriggerMarkers(); }
  void DrawActorMarkers() override { VS::Battle::LiveDrawActorMarkers(); }
  void UpdateGimmickAnimations() override {
    UpdateGimmickAnimationQueueEntry();
  }
  void UpdateRoomEffects() override { LiveUpdateRoomEffects(); }
  void DrawPlatformShadows() override { DrawLiveMovingPlatformShadows(); }
  void UpdateWeather() override { LiveUpdateRoomWeather(); }
  // 146C.c:15791 compares isLoaded and displayFlag as one halfword to 0x101.
  bool RoomShown() override {
    return VS::Field::roomData.is_loaded == 1 &&
           VS::Field::roomData.display_flag == 1;
  }
  bool HasGeometry() override {
    return VS::Field::roomData.geometry != nullptr;
  }
  int CameraYaw() override {
    return static_cast<int>(Scratch().camera.angles.vy);
  }
  void SetViewFacing(std::uint8_t mask) override {
    std::uint32_t& facing = Scratch().first_person_view_facing;
    facing = (facing & ~kViewFacingMask) | mask;
  }
  void DrawGeometry() override {
    LiveDrawRoomGeometry(VS::Field::roomData.geometry);
  }
  MinimapSettings Minimap() override {
    return MinimapSettings{
        .shown = (VS::Input::settings.simple_map & 1) != 0,
        .framed = (VS::Input::settings.simple_map & 2) != 0,
        .has_scene = VS::Field::roomData.minimap_scene != nullptr};
  }
  bool MinimapVisible() override { return isMinimapVisible != 0; }
  std::int16_t MinimapSlideFrame() override { return minimapSlideFrame; }
  void SetMinimapSlideFrame(std::int16_t frame) override {
    minimapSlideFrame = frame;
  }
  int DisplayClipX() override { return DrawnEnvironment().clip.x; }
  void DrawMinimap(const MinimapLayout& layout) override;
  bool WeaponDrawn() override {
    return VS::Battle::characterState != nullptr &&
           VS::Battle::characterState->weapon_drawn == 1;
  }
  int BorderPulsePhase() override { return minimapBorderPulsePhase; }
  void SetBorderPulsePhase(int phase) override {
    minimapBorderPulsePhase = phase;
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
  explicit ProjectionOffsetScope(const VS::Render::ScreenOffset& offset)
      : saved_(Render::GetLiveProjectionScreenOffset()) {
    Render::SetLiveProjectionScreenOffset(offset);
  }
  ProjectionOffsetScope(const ProjectionOffsetScope&) = delete;
  ProjectionOffsetScope& operator=(const ProjectionOffsetScope&) = delete;
  ProjectionOffsetScope(ProjectionOffsetScope&&) = delete;
  ProjectionOffsetScope& operator=(ProjectionOffsetScope&&) = delete;
  ~ProjectionOffsetScope() { Render::SetLiveProjectionScreenOffset(saved_); }

 private:
  VS::Render::ScreenOffset saved_;
};

void LiveSceneHost::DrawMinimap(const MinimapLayout& layout) {
  MATRIX rotation{};
  BuildLiveMinimapRotation(&rotation, layout.map_yaw);
  SetRotMatrix(&rotation);
  LinkDrawArea(DrawnEnvironment().clip, "display draw area");
  {
    const ProjectionOffsetScope scope(
        VS::Render::ScreenOffset{.x = layout.offset_x, .y = layout.offset_y});
    DrawLiveMinimapScene(VS::Field::roomData.minimap_scene,
                         VS::Field::roomData.header.minimap_scene_length);
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
    Render::LoadLiveClutColors(colors.data(), static_cast<std::uint32_t>(row),
                               0, static_cast<std::uint32_t>(colors.size()));
  }
  void SetGimmickPalettes(std::span<std::uint16_t> palettes) override {
    static_cast<void>(SetLiveGimmickPaletteTable(std::as_bytes(palettes)));
  }
};
}  // namespace

RoomRenderState LiveRoomRenderState() {
  return RoomRenderState{
      .field_rendering_enabled = isFieldRenderingEnabled != 0,
      .updates_paused = areRoomUpdatesPaused != 0,
      .tint = LoadTint()};
}

void LiveSetFieldRenderingEnabled(int enabled) {
  RoomRenderState state = LiveRoomRenderState();
  SetFieldRenderingEnabled(state, enabled != 0);
  isFieldRenderingEnabled = state.field_rendering_enabled ? 1 : 0;
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
  VS::Field::roomAnimationTicks = AdvanceRoomAnimationClock(
      areRoomUpdatesPaused != 0, VS::Field::roomAnimationTicks,
      VS::State::tickSpeed);
}

void LiveSetRoomRenderingEnabled(int enabled) {
  RoomDisplay display{.loaded = VS::Field::roomData.is_loaded != 0,
                      .displayed = VS::Field::roomData.display_flag != 0,
                      .owner_room_index = VS::Field::roomData.room_index};
  if (SetRoomRenderingEnabled(display, enabled != 0)) {
    VS::Field::roomData.display_flag =
        static_cast<char>(display.displayed ? 1 : 0);
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
  if (geometry == nullptr || geometry != VS::Field::roomData.geometry) {
    RefuseCall("Texture id offset", "geometry is not the loaded room's");
    return;
  }
  static_cast<void>(OffsetRoomTextureIds(RoomGeometry(), offset));
}

void LiveSetRoomTint(const VS::Render::ColorCode* color, int strength) {
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
  areRoomUpdatesPaused = static_cast<std::uint16_t>(state.updates_paused);
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
