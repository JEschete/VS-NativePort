// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapMarkersLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Audio/Seq/Channel.h"
#include "Core/Log.hpp"
#include "Field/RoomSwitchesLive.hpp"
#include "Legacy/Psx.h"
#include "Menu/Framework/Framework.h"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Map.h"
#include "Menu/MapMarkers.hpp"
#include "Render/Render.h"
#include "Render/Sprites.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
#include "State/StateFlags.h"

namespace VS::Menu::map {
namespace {
// _iconRects, D_80108D04 and D_80108D14's entries.
constexpr std::size_t kFeatureIcons = 4;
constexpr std::size_t kDoorStateVariants = 2;
// The state flags func_80107630 and func_8010839C index by door state.
constexpr std::size_t kDoorStateFlagsOffset = 0x33F;
// Ordering-table byte offset of the icon quads and word slot of the lines.
constexpr std::size_t kMarkerQuadSlotBytes = 28;
constexpr std::size_t kLabelLineSlot = 8;

static_assert(sizeof(Menu_Map_MapLabel) == sizeof(MapLabel));
static_assert(offsetof(State_StateFlags, opaque33F) == kDoorStateFlagsOffset);
static_assert(sizeof(u_long) == sizeof(std::uint32_t));

std::uint64_t refused_calls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refused_calls)) {
    VS::Core::LogWarn("menu", "map markers: {} refused: {} ({} refusals)", call,
                      reason, refused_calls);
  }
}

[[nodiscard]] MarkerRect RectOf(const RECT& rect) {
  return {.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h};
}

[[nodiscard]] std::array<MarkerRect, kDoorStateVariants> VariantsOf(
    const RECT* rects) {
  const std::span<const RECT, kDoorStateVariants> table{rects,
                                                        kDoorStateVariants};
  return {RectOf(table[0]), RectOf(table[1])};
}

[[nodiscard]] std::span<MapLabel> Labels() {
  return {reinterpret_cast<MapLabel*>(Menu_Map_mapLabels), kMapLabelCapacity};
}

// The state flag byte for `state_id`, read unsigned as the original's lbu;
// false when it lies outside the state flags.
[[nodiscard]] bool DoorStateFlagOf(std::int32_t state_id, std::uint8_t& flag) {
  const std::span<const std::uint8_t> flags{
      reinterpret_cast<const std::uint8_t*>(&Audio_Seq_unusedStateFlags),
      sizeof(State_StateFlags)};
  const auto offset = static_cast<std::int64_t>(kDoorStateFlagsOffset) +
                      static_cast<std::int64_t>(state_id);
  if (offset < 0 || static_cast<std::uint64_t>(offset) >= flags.size()) {
    return false;
  }
  flag = flags[static_cast<std::size_t>(offset)];
  return true;
}

template <typename Primitive>
[[nodiscard]] Primitive* ReservePrimitive() {
  constexpr std::size_t kWords =
      (sizeof(Primitive) + sizeof(u_long) - 1) / sizeof(u_long);
  return reinterpret_cast<Primitive*>(
      Render::ReserveLivePrimitiveWords(kWords));
}

class LiveMapMarkerHost final : public MapMarkerHost {
 public:
  explicit LiveMapMarkerHost(std::string_view call, u_long* caller = nullptr)
      : call_(call), caller_(caller) {}

  void AddMarkerQuad(const MarkerQuad& quad) override {
    auto* poly = ReservePrimitive<POLY_FT4>();
    if (poly == nullptr || Render_frameScratch.orderingTable == nullptr) {
      Refuse(call_, "no room in the frame primitive buffer");
      return;
    }
    setPolyFT4(poly);
    setShadeTex(poly, 1);
    const auto& corner = quad.corners;
    setXY4(poly, corner[0].x, corner[0].y, corner[1].x, corner[1].y,
           corner[2].x, corner[2].y, corner[3].x, corner[3].y);
    const auto& texture = quad.texture;
    setUV4(poly, texture[0].u, texture[0].v, texture[1].u, texture[1].v,
           texture[2].u, texture[2].v, texture[3].u, texture[3].v);
    poly->tpage = quad.texture_page;
    poly->clut = quad.clut;
    AddPrim(static_cast<u_char*>(Render_frameScratch.orderingTable) +
                kMarkerQuadSlotBytes,
            poly);
    Render_frameScratch.primitiveCursor = poly + 1;
  }

  void AddLabelLine(const LabelLine& line) override {
    auto* packet = ReservePrimitive<LINE_F2>();
    if (packet == nullptr || Render_frameScratch.orderingTable == nullptr) {
      Refuse(call_, "no room in the frame primitive buffer");
      return;
    }
    setLineF2(packet);
    setXY2(packet, line.from.x, line.from.y, line.to.x, line.to.y);
    setRGB0(packet, kLabelLineShade, kLabelLineShade, kLabelLineShade);
    AddPrim(static_cast<u_long*>(Render_frameScratch.orderingTable) +
                kLabelLineSlot,
            packet);
    Render_frameScratch.primitiveCursor = packet + 1;
  }

  void AddAshleyMarker(const AshleyMarkerSprite& sprite) override {
    u_long* target =
        sprite.list == SpriteList::kCaller
            ? caller_
            : static_cast<u_long*>(Render_frameScratch.overlayOrderingTable);
    u_long* packet = Render::QueueLiveSpriteWithTexturePage(
        sprite.brightness_transparency, sprite.packed_position,
        sprite.packed_size, target);
    // The original writes the colour through the cursor 16 bytes back, the
    // linked packet's colour word; a dropped packet's discard storage keeps
    // the write in bounds.
    packet[Render::kSpriteTextureWord] = sprite.texture_word;
    packet[Render::kSpriteColorWord] = sprite.color_word;
  }

  void DrawLabelText(const LabelText& text) override {
    u_short* table = text.table == LabelTextTable::kDoorNames
                         ? Menu_Map_doors
                         : Menu_Map_connectingMaps;
    Menu_Framework_MenuRow* row = Framework::SetLiveMenuItem(
        0, text.x, text.y, text.width, 0,
        reinterpret_cast<char*>(&table[text.text_offset]));
    row->backgroundWidth = text.background_width;
    Framework::DrawLiveMenuItem(row);
    Framework::LiveMenuItem(0)->state = 0;
  }

  bool IsSigilDoorUnlocked(std::int32_t zone_id, std::int32_t map_id,
                           std::uint32_t kind) override {
    return VS::Field::IsRoomSigilDoorUnlockedEntry(zone_id, map_id, kind) != 0;
  }

 private:
  std::string_view call_;
  u_long* caller_;
};
}  // namespace

// decomp: MENU5.PRG _renderDefaultIcon (4D8.c:106).
void LiveDrawAshleyMarker(int id, int xy, int brightness, u_long* next_prim) {
  LiveMapMarkerHost host("DrawAshleyMarker", next_prim);
  DrawAshleyMarker(
      host, id, xy, brightness,
      next_prim == nullptr ? SpriteList::kOverlay : SpriteList::kCaller);
}

// decomp: MENU5.PRG _renderIcon (4D8.c:2158).
void LiveDrawFeatureMarker(int id, int x, int y) {
  constexpr std::string_view kCall = "DrawFeatureMarker";
  const std::span<const RECT, kFeatureIcons> rects{Menu_Map_iconRects,
                                                   kFeatureIcons};
  std::array<MarkerRect, kFeatureIcons> icons{};
  for (std::size_t icon = 0; icon < kFeatureIcons; ++icon) {
    icons.at(icon) = RectOf(rects[icon]);
  }
  LiveMapMarkerHost host(kCall);
  if (!DrawFeatureMarker(host, icons, id, x, y)) {
    Refuse(kCall, "icon outside _iconRects");
  }
}

// decomp: MENU5.PRG func_80107630 (4D8.c:2180).
void LiveDrawDoorStateMarker(int x, int y, int id) {
  constexpr std::string_view kCall = "DrawDoorStateMarker";
  std::uint8_t flag = 0;
  if (!DoorStateFlagOf(id, flag)) {
    Refuse(kCall, "door state outside the state flags");
    return;
  }
  LiveMapMarkerHost host(kCall);
  if (!DrawDoorStateMarker(host, VariantsOf(Menu_Map_kDoorMarkerUpperRects),
                           VariantsOf(Menu_Map_kDoorMarkerLowerRects), Labels(),
                           Menu_Map_mapLabelCount, x, y, id, flag)) {
    Refuse(kCall, "label queue full");
  }
}

// decomp: MENU5.PRG _renderDoorName (4D8.c:2227).
void LiveDrawDoorLabel(int x, int y, int door_id) {
  constexpr std::string_view kCall = "DrawDoorLabel";
  LiveMapMarkerHost host(kCall);
  if (!DrawDoorLabel(host, Menu_Map_doors, Menu_Map_mapLabelY, x, y, door_id)) {
    Refuse(kCall, "door name outside _doors");
  }
}

// decomp: MENU5.PRG _insertLine (4D8.c:2258).
void LiveAppendLabelLine(int x0, int y0, int x1, int y1) {
  LiveMapMarkerHost host("AppendLabelLine");
  AppendLabelLine(host, x0, y0, x1, y1);
}

// decomp: MENU5.PRG _renderConnectingMapName (4D8.c:2403).
void LiveDrawConnectingAreaLabel(int x, int y, int map_id) {
  constexpr std::string_view kCall = "DrawConnectingAreaLabel";
  std::array<std::uint32_t, std::size(State_roomVisitProgress.areaFlags)>
      flags{};
  std::memcpy(flags.data(), State_roomVisitProgress.areaFlags, sizeof(flags));
  LiveMapMarkerHost host(kCall);
  if (!DrawConnectingAreaLabel(host, Menu_Map_connectingMaps, flags,
                               Menu_Map_mapLabelY, x, y, map_id)) {
    Refuse(kCall, "area outside the visit flags or _connectingMaps");
  }
}

// decomp: MENU5.PRG _addMapLabel (4D8.c:2441).
void LiveQueueLabel(int x, int y, int type, int label_id) {
  if (!QueueLabel(Labels(), Menu_Map_mapLabelCount, x, y, type, label_id)) {
    Refuse("QueueLabel", "label queue full");
  }
}

// decomp: MENU5.PRG func_8010839C (4D8.c:2471).
int LiveShouldDrawDoorStateMarker(int door_state_id, int zone_id, int map_id) {
  constexpr std::string_view kCall = "ShouldDrawDoorStateMarker";
  std::uint8_t flag = 0;
  if (!DoorStateFlagOf(door_state_id, flag)) {
    Refuse(kCall, "door state outside the state flags");
    return 0;
  }
  LiveMapMarkerHost host(kCall);
  return ShouldDrawDoorStateMarker(host, door_state_id, zone_id, map_id, flag)
             ? 1
             : 0;
}
}  // namespace VS::Menu::map
