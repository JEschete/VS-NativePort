// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapDrawingLive.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Battle/StatusEffects.hpp"
#include "Core/Gte.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Field/Doors.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/MinimapScene.hpp"
#include "Field/Room.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Framework/TextBoxes.hpp"
#include "Menu/Map.hpp"
#include "Menu/MapDrawing.hpp"
#include "Menu/MapMarkersLive.hpp"
#include "Menu/MapSceneStorageLive.hpp"
#include "Menu/TextBoxStateLive.hpp"
#include "Render/DepthCueLive.hpp"
#include "Render/GameFontLive.hpp"
#include "Render/Render.hpp"
#include "Render/RoomDrawLive.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPacketsLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/StateFlags.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::RotTransPers;
using VS::Core::Gte::SVECTOR;

using VS::Legacy::AddPrim;
using VS::Legacy::LINE_G2;
using VS::Legacy::POLY_F4;
using VS::Legacy::POLY_FT4;
using VS::Legacy::POLY_G3;
using VS::Legacy::POLY_G4;
using VS::Legacy::setlen;
using VS::Legacy::setLineG2;
using VS::Legacy::setPolyF4;
using VS::Legacy::setPolyFT4;
using VS::Legacy::setPolyG3;
using VS::Legacy::setPolyG4;
using VS::Legacy::setRGB0;
using VS::Legacy::setRGB1;
using VS::Legacy::setRGB2;
using VS::Legacy::setRGB3;
using VS::Legacy::setSemiTrans;
using VS::Legacy::setShadeTex;
using VS::Legacy::setTile;
using VS::Legacy::setUV4;
using VS::Legacy::setWH;
using VS::Legacy::setXY0;
using VS::Legacy::setXY2;
using VS::Legacy::setXY3;
using VS::Legacy::setXY4;
using VS::Legacy::TILE;

namespace VS::Menu::map {
namespace {
constexpr std::size_t kWordBytes = 4;
constexpr std::size_t kDataOffset = 4;
// The state flags' map_paling bytes.
constexpr int kMapPalingScenes = 32;
// The ordering-table words the original links into.
constexpr int kBackdropSlot = 7;
constexpr int kLegendSlot = 6;
constexpr int kAshleySlot = 8;
constexpr int kTexturePageSlot = 7;
// getTPage(2, 0, 192, 0).
constexpr std::uint32_t kLegendTexturePage = 0x103;
// insertTPage's draw-mode command with dithering enabled.
constexpr std::uint32_t kTexturePageCommand = 0xE1000200;
constexpr int kTexturePageWords = 2;

struct LegendTexturePage {
  std::uint32_t tag;
  std::uint32_t mode;
  std::uint32_t padding;
};
// The legend's slide starts and stops here (uiXOffset).
constexpr int kHiddenLegendOffset = -128;

static_assert(offsetof(Field::MapRoomRecord, data_offset) == kDataOffset);
static_assert(std::size(VS::State::StateFlags{}.map_paling) ==
              kMapPalingScenes);

std::uint64_t refusedCalls = 0;
int legendOffset = 0;
RoomWorkspace roomWorkspace{};

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusedCalls)) {
    VS::Core::LogWarn("menu", "map drawing: {} refused: {} ({} refusals)", call,
                      reason, refusedCalls);
  }
}

template <typename Primitive>
[[nodiscard]] Primitive* ReservePrimitive() {
  constexpr std::size_t kWords =
      (sizeof(Primitive) + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
  return reinterpret_cast<Primitive*>(
      Render::ReserveLivePrimitiveWords(kWords));
}

// Reserves a packet for ordering-table word `slot`; null, and refused, when
// the entry lies outside the frame's table or the buffer is full.
template <typename Primitive>
[[nodiscard]] Primitive* PacketFor(std::string_view call, int slot,
                                   std::uint32_t*& entry) {
  entry =
      Render::LiveFrameOrderingEntry(Render::LiveOrderingBase::kScene, slot);
  if (entry == nullptr) {
    Refuse(call, "ordering-table entry outside the frame's table");
    return nullptr;
  }
  auto* packet = ReservePrimitive<Primitive>();
  if (packet == nullptr) {
    Refuse(call, "no room in the frame primitive buffer");
  }
  return packet;
}

template <typename Primitive>
void Link(std::uint32_t* entry, Primitive* packet) {
  AddPrim(entry, packet);
  VS::Render::FrameScratchpad().primitive_cursor = packet + 1;
}

[[nodiscard]] std::span<const MapLabel> Labels() { return mapLabels; }

void StoreCompassBounds(const CompassCorners& corners) {
  std::ranges::copy(corners, compassBounds);
}

// The buffer `scene` was loaded into: the ARM scene the menu loaded or the
// current room's minimap scene (MapNavigationLive.cpp).
[[nodiscard]] std::optional<std::span<const std::byte>> SceneBytes(
    const void* scene) {
  if (scene == nullptr) return std::nullopt;
  if (scene == VS::Battle::sceneBuffer) {
    if (const auto loaded = BoundLiveMapScene(scene)) return *loaded;
    if (currentScene < 0 || currentScene >= kSceneCount) {
      return std::nullopt;
    }
    const int size = kSceneArmFiles[currentScene].size;
    if (size < 0) return std::nullopt;
    return std::span{static_cast<const std::byte*>(VS::Battle::sceneBuffer),
                     static_cast<std::size_t>(size)};
  }
  if (scene == VS::Field::roomData.minimap_scene) {
    return std::span{
        reinterpret_cast<const std::byte*>(VS::Field::roomData.minimap_scene),
        static_cast<std::size_t>(
            VS::Field::roomData.header.minimap_scene_length)};
  }
  return std::nullopt;
}

// The bytes from `data` to the end of the loaded scene that holds it.
[[nodiscard]] std::optional<std::span<const std::byte>> RoomBytes(
    const void* data) {
  for (const void* scene :
       {static_cast<const void*>(VS::Battle::sceneBuffer),
        static_cast<const void*>(VS::Field::roomData.minimap_scene)}) {
    const auto bytes = SceneBytes(scene);
    if (!bytes || bytes->empty()) continue;
    const std::less_equal<const void*> not_after;
    const std::less<const void*> before;
    if (not_after(bytes->data(), data) &&
        before(data, bytes->data() + bytes->size())) {
      const auto offset = static_cast<std::size_t>(
          static_cast<const std::byte*>(data) - bytes->data());
      return bytes->subspan(offset);
    }
  }
  return std::nullopt;
}

class LiveRoomHost final : public RoomDrawHost {
 public:
  Projection Project(const RoomVertex& vertex) override {
    SVECTOR input{.vx = vertex.x, .vy = vertex.y, .vz = vertex.z, .pad = 0};
    VS::Core::Gte::Word screen = 0;
    VS::Core::Gte::Word interpolation = 0;
    VS::Core::Gte::Word flag = 0;
    const VS::Core::Gte::Word depth =
        RotTransPers(&input, &screen, &interpolation, &flag);
    const auto packed = static_cast<std::uint32_t>(screen);
    constexpr std::uint32_t kHalfShift = 16;
    return {.screen = {.x = Core::Math::WrapSigned16(packed),
                       .y = Core::Math::WrapSigned16(packed >> kHalfShift)},
            .depth = static_cast<std::int32_t>(depth),
            .interpolation = static_cast<std::int32_t>(interpolation)};
  }

  Rgb DepthCue(const Rgb& color, std::int32_t interpolation) override {
    const CVECTOR input{.r = color.r, .g = color.g, .b = color.b, .cd = 0};
    CVECTOR output{};
    Render::ApplyLiveDepthCueColor(&input, interpolation, &output);
    return {.r = output.r, .g = output.g, .b = output.b};
  }

  void AddTriangle(const RoomTriangle& triangle, std::int32_t depth) override {
    std::uint32_t* entry = nullptr;
    auto* poly = PacketFor<POLY_G3>(kCall, depth, entry);
    if (poly == nullptr) return;
    setPolyG3(poly);
    const auto& [a, b, c] = triangle.corners;
    setXY3(poly, a.x, a.y, b.x, b.y, c.x, c.y);
    const auto& [ca, cb, cc] = triangle.colors;
    setRGB0(poly, ca.r, ca.g, ca.b);
    setRGB1(poly, cb.r, cb.g, cb.b);
    setRGB2(poly, cc.r, cc.g, cc.b);
    Link(entry, poly);
  }

  void AddQuad(const RoomQuad& quad, std::int32_t depth) override {
    std::uint32_t* entry = nullptr;
    auto* poly = PacketFor<POLY_G4>(kCall, depth, entry);
    if (poly == nullptr) return;
    setPolyG4(poly);
    const auto& [a, b, c, d] = quad.corners;
    setXY4(poly, a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    const auto& [ca, cb, cc, cd] = quad.colors;
    setRGB0(poly, ca.r, ca.g, ca.b);
    setRGB1(poly, cb.r, cb.g, cb.b);
    setRGB2(poly, cc.r, cc.g, cc.b);
    setRGB3(poly, cd.r, cd.g, cd.b);
    Link(entry, poly);
  }

  void AddEdge(const RoomEdge& edge, std::int32_t depth) override {
    std::uint32_t* entry = nullptr;
    auto* line = PacketFor<LINE_G2>(kCall, depth, entry);
    if (line == nullptr) return;
    setLineG2(line);
    setSemiTrans(line, edge.semi_transparent ? 1 : 0);
    setXY2(line, edge.from.x, edge.from.y, edge.to.x, edge.to.y);
    setRGB0(line, edge.from_color.r, edge.from_color.g, edge.from_color.b);
    setRGB1(line, edge.to_color.r, edge.to_color.g, edge.to_color.b);
    Link(entry, line);
  }

  void AddTile(const RoomTile& tile, std::int32_t depth) override {
    std::uint32_t* entry = nullptr;
    auto* packet = PacketFor<TILE>(kCall, depth, entry);
    if (packet == nullptr) return;
    setTile(packet);
    setXY0(packet, tile.position.x, tile.position.y);
    setWH(packet, tile.width, tile.height);
    setRGB0(packet, tile.color.r, tile.color.g, tile.color.b);
    Link(entry, packet);
  }

  bool ShouldDrawDoorState(std::int32_t state_id, std::int32_t zone_id,
                           std::int32_t map_id) override {
    return LiveShouldDrawDoorStateMarker(state_id, zone_id, map_id) != 0;
  }

  void DrawDoorState(std::int32_t x, std::int32_t y,
                     std::int32_t state_id) override {
    LiveDrawDoorStateMarker(x, y, state_id);
  }

  void DrawFeature(std::int32_t icon, std::int32_t x, std::int32_t y) override {
    LiveDrawFeatureMarker(icon, x, y);
  }

  void QueueLabel(std::int32_t x, std::int32_t y, std::int32_t kind,
                  std::int32_t label_id) override {
    LiveQueueLabel(x, y, kind, label_id);
  }

  void DrawAshley(std::int32_t position, std::int32_t brightness) override {
    std::uint32_t* entry = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kAshleySlot);
    if (entry == nullptr) {
      Refuse(kCall, "ordering-table entry outside the frame's table");
      return;
    }
    LiveDrawAshleyMarker(0, position, brightness, entry);
  }

 private:
  static constexpr std::string_view kCall = "DrawRoom";
};

[[nodiscard]] int DrawRoomBytes(std::span<const std::byte> room, int room_index,
                                int zone_id, int map_id) {
  const RoomContext context{.room_index = room_index,
                            .current_room = currentRoomIndex,
                            .scene_room = isCurrentScene,
                            .entered_door = VS::Field::enteredDoorId,
                            .frame_counter = menuFramesCounter,
                            .zone_id = zone_id,
                            .map_id = map_id,
                            .brightness = std::span{kAshleyIconBrightness}};
  LiveRoomHost host;
  switch (DrawRoom(host, room, context, roomWorkspace)) {
    case RoomDrawResult::kDrawn:
      return 1;
    case RoomDrawResult::kOutOfDepthRange:
      return 0;
    case RoomDrawResult::kMalformed:
      Refuse("DrawRoom", "room tables reach past the scene");
      return 0;
  }
  return 0;
}

class LiveVisitedRoomsHost final : public VisitedRoomsHost {
 public:
  explicit LiveVisitedRoomsHost(std::span<const std::byte> scene)
      : scene_(scene) {}

  ScreenOffset GetScreenOffset() override {
    const VS::Render::ScreenOffset offset =
        Render::GetLiveProjectionScreenOffset();
    return {.x = offset.x, .y = offset.y};
  }

  void SetScreenOffset(const ScreenOffset& offset) override {
    Render::SetLiveProjectionScreenOffset({.x = offset.x, .y = offset.y});
  }

  void ClearLabels() override { mapLabelCount = 0; }

  void DrawRoom(std::size_t index, std::int32_t zone_id,
                std::int32_t map_id) override {
    const std::size_t record =
        kWordBytes + (index * Field::kMapSceneRoomRecordBytes);
    // The record's on-disc pointer, resolved to an offset from the record.
    std::uint32_t relative = 0;
    std::memcpy(&relative, scene_.subspan(record + kDataOffset).data(),
                sizeof(relative));
    if (relative >= scene_.size() - record) {
      Refuse("DrawVisitedRooms", "room data outside the scene");
      return;
    }
    static_cast<void>(DrawRoomBytes(scene_.subspan(record + relative),
                                    static_cast<int>(index), zone_id, map_id));
  }

  void DrawLabels() override { LiveDrawLabels(); }

 private:
  std::span<const std::byte> scene_;
};

class LiveLabelHost final : public LabelDrainHost {
 public:
  void DrawDoorLabel(std::int32_t x, std::int32_t y,
                     std::int32_t door_id) override {
    LiveDrawDoorLabel(x, y, door_id);
  }

  void DrawAreaLabel(std::int32_t x, std::int32_t y,
                     std::int32_t area_id) override {
    LiveDrawConnectingAreaLabel(x, y, area_id);
  }
};

class LiveBackdropHost final : public BackdropHost {
 public:
  void AddBackdropLine(const BackdropLine& line) override {
    std::uint32_t* entry = nullptr;
    auto* packet = PacketFor<LINE_G2>(kCall, kBackdropSlot, entry);
    if (packet == nullptr) return;
    setLineG2(packet);
    setXY2(packet, line.from.x, line.from.y, line.to.x, line.to.y);
    setRGB0(packet, kBackdropLeftColor.r, kBackdropLeftColor.g,
            kBackdropLeftColor.b);
    setRGB1(packet, kBackdropRightColor.r, kBackdropRightColor.g,
            kBackdropRightColor.b);
    Link(entry, packet);
  }

  void AddBackdropShadow(
      const std::array<ScreenPoint, kQuadCorners>& corners) override {
    std::uint32_t* entry = nullptr;
    auto* poly = PacketFor<POLY_F4>(kCall, kBackdropSlot, entry);
    if (poly == nullptr) return;
    setPolyF4(poly);
    const auto& [a, b, c, d] = corners;
    setXY4(poly, a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    setRGB0(poly, 0, 0, 0);
    Link(entry, poly);
  }

 private:
  static constexpr std::string_view kCall = "DrawControlBackdrop";
};

class LiveCompassHost final : public CompassHost {
 public:
  std::int32_t Sine(std::int32_t angle) override {
    return Core::Math::Sine(angle);
  }

  std::int32_t Cosine(std::int32_t angle) override {
    return Core::Math::Cosine(angle);
  }

  void AddCompassQuad(const CompassQuad& compass) override {
    std::uint32_t* entry = nullptr;
    auto* poly = PacketFor<POLY_FT4>(
        "DrawCompass",
        compass.slot_bytes / static_cast<std::int32_t>(sizeof(std::uint32_t)),
        entry);
    if (poly == nullptr) return;
    setPolyFT4(poly);
    setSemiTrans(poly, compass.semi_transparent ? 1 : 0);
    setShadeTex(poly, 1);
    const MarkerQuad& quad = compass.quad;
    const auto& [a, b, c, d] = quad.corners;
    setXY4(poly, a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    const auto& [ta, tb, tc, td] = quad.texture;
    setUV4(poly, ta.u, ta.v, tb.u, tb.v, tc.u, tc.v, td.u, td.v);
    poly->tpage = quad.texture_page;
    poly->clut = quad.clut;
    Link(entry, poly);
  }
};

class LiveControlsHost final : public ControlsHost {
 public:
  void ShiftTextBox(std::int32_t delta) override {
    TextBoxState* box = LiveTextBox(0);
    if (box == nullptr) return;
    box->y =
        Core::Math::WrapSigned16(static_cast<std::uint32_t>(box->y + delta));
    box->y_indent = Core::Math::WrapSigned16(
        static_cast<std::uint32_t>(box->y_indent + delta));
  }

  std::optional<bool> IsJammed() override {
    if (currentScene < 0 || currentScene >= kMapPalingScenes) {
      Refuse(kCall, "scene outside the map paling flags");
      return std::nullopt;
    }
    return VS::State::stateFlags.map_paling[currentScene] != 0;
  }

  void DrawButton(Framework::ButtonIcon button, std::int32_t x,
                  std::int32_t y) override {
    if (std::uint32_t* entry = LegendEntry(); entry != nullptr) {
      Framework::DrawLiveButtonIcon(button, x, y, entry);
    }
  }

  void DrawCaption(ControlCaption caption, std::int32_t packed_xy,
                   std::uint32_t color) override {
    constexpr std::array<const char*, 6> kTexts{"BACK", "NEXT", "ROTATION",
                                                "ZOOM", "MENU", "JAMMING"};
    if (std::uint32_t* entry = LegendEntry(); entry != nullptr) {
      Render::DrawLiveCompactTextColor(
          kTexts.at(static_cast<std::size_t>(caption)), packed_xy,
          static_cast<int>(color), entry);
    }
  }

  void DrawBackdrop(std::int32_t x, std::int32_t y, std::int32_t width,
                    std::int32_t height) override {
    LiveDrawControlBackdrop(x, y, width, height);
  }

  // The original's inline insertTPage: a two-word packet whose second word
  // is zero, not AppendLiveTexturePage's one-word DR_TPAGE.
  void AddTexturePage() override {
    std::uint32_t* entry = nullptr;
    auto* packet =
        PacketFor<LegendTexturePage>("DrawControls", kTexturePageSlot, entry);
    if (packet == nullptr) return;
    packet->mode = kTexturePageCommand | kLegendTexturePage;
    packet->padding = 0;
    setlen(packet, kTexturePageWords);
    Link(entry, packet);
  }

  void DrawCompass(std::int32_t x, std::int32_t y,
                   std::int32_t angle) override {
    LiveDrawCompass(x, y, angle);
  }

 private:
  static constexpr std::string_view kCall = "DrawControls";

  [[nodiscard]] static std::uint32_t* LegendEntry() {
    std::uint32_t* entry = Render::LiveFrameOrderingEntry(
        Render::LiveOrderingBase::kScene, kLegendSlot);
    if (entry == nullptr) {
      Refuse(kCall, "ordering-table entry outside the frame's table");
    }
    return entry;
  }
};
}  // namespace

// decomp: MENU5.PRG renderMap (src/MENU/MENU5.PRG/4D8.c:1086; was _renderMap).
void LiveDrawVisitedRooms(Field::MapScene* scene) {
  constexpr std::string_view kCall = "DrawVisitedRooms";
  const auto bytes = SceneBytes(scene);
  if (!bytes || bytes->size() < kWordBytes) {
    Refuse(kCall, "scene is not the menu's loaded scene");
    return;
  }
  std::int32_t count = 0;
  std::memcpy(&count, bytes->data(), sizeof(count));
  const std::size_t rooms = count <= 0 ? 0 : static_cast<std::size_t>(count);
  if (rooms > (bytes->size() - kWordBytes) / Field::kMapSceneRoomRecordBytes) {
    Refuse(kCall, "room records reach past the scene");
    return;
  }
  std::vector<MapRoom> records(rooms);
  for (std::size_t room = 0; room < rooms; ++room) {
    Field::MapRoomRecord record{};
    std::memcpy(
        &record,
        bytes->subspan(kWordBytes + (room * Field::kMapSceneRoomRecordBytes))
            .data(),
        sizeof(record));
    records[room] = {.visited = record.visited,
                     .zone_id = record.zone_id,
                     .map_id = record.map_id};
  }
  LiveVisitedRoomsHost host(*bytes);
  DrawVisitedRooms(host, geomOffsetX, geomOffsetY, records);
}

// decomp: MENU5.PRG renderRoom (src/MENU/MENU5.PRG/4D8.c:1117; was
// _renderRoom).
int LiveDrawRoom(int room_index, void* room_data, int zone_id, int map_id) {
  const auto room = RoomBytes(room_data);
  if (!room) {
    Refuse("DrawRoom", "room data outside the menu's loaded scene");
    return 0;
  }
  return DrawRoomBytes(*room, room_index, zone_id, map_id);
}

// decomp: MENU5.PRG drawUIControlsBackground (src/MENU/MENU5.PRG/4D8.c:1970;
// was _drawUIControlsBackground).
void LiveDrawControlBackdrop(int x, int y, int w, int h) {
  LiveBackdropHost host;
  if (!DrawControlBackdrop(host, x, y, w, h)) {
    Refuse("DrawControlBackdrop", "more rows than a backdrop draws");
  }
}

// decomp: MENU5.PRG drawUIControls (src/MENU/MENU5.PRG/4D8.c:1996; was
// _drawUIControls).
void LiveDrawControls() {
  ControlsState state{.animation_state = uiAnimationState,
                      .x_offset = legendOffset};
  const ControlsFrame frame{.hide_ui = triggerHideUi != 0,
                            .commands_state = menuCommandsState,
                            .compass_angle = cameraAngles.vy};
  LiveControlsHost host;
  static_cast<void>(DrawControls(host, state, frame));
  uiAnimationState = state.animation_state;
  legendOffset = state.x_offset;
}

// decomp: MENU5.PRG renderCompass (src/MENU/MENU5.PRG/4D8.c:2277; was
// _renderCompass).
void LiveDrawCompass(int x, int y, int angle) {
  LiveCompassHost host;
  StoreCompassBounds(DrawCompass(host, x, y, angle, cameraAngles.vz));
}

// decomp: MENU5.PRG computeRotatedRect (src/MENU/MENU5.PRG/4D8.c:2382; was
// _computeRotatedRect).
void LiveComputeCompassCorners(int center_x, int center_y, int width,
                               int height, int angle) {
  const std::int32_t sine = Core::Math::Sine(angle);
  const std::int32_t cosine = Core::Math::Cosine(angle);
  StoreCompassBounds(
      ComputeCompassCorners(center_x, center_y, width, height, sine, cosine));
}

// decomp: MENU5.PRG renderMapLabels (src/MENU/MENU5.PRG/4D8.c:2455).
void LiveDrawLabels() {
  LiveLabelHost host;
  if (!DrawLabels(host, menuCommandsState, triggerHideUi != 0, Labels(),
                  mapLabelCount)) {
    Refuse("DrawLabels", "label count past _mapLabels");
  }
}

void ResetMapDrawingLive() {
  legendOffset = kHiddenLegendOffset;
  roomWorkspace = {};
}

void ResetMapDrawingModuleData() { legendOffset = 0; }
}  // namespace VS::Menu::map
