// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapMarkersLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "Core/Log.hpp"
#include "Field/RoomSwitchesLive.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Menu/Map.hpp"
#include "Menu/MapMarkers.hpp"
#include "Render/Render.hpp"
#include "Render/Sprites.hpp"
#include "Render/SpritesLive.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

using VS::Legacy::AddPrim;
using VS::Legacy::LINE_F2;
using VS::Legacy::POLY_FT4;
using VS::Legacy::setLineF2;
using VS::Legacy::setPolyFT4;
using VS::Legacy::setRGB0;
using VS::Legacy::setShadeTex;
using VS::Legacy::setUV4;
using VS::Legacy::setXY2;
using VS::Legacy::setXY4;

namespace VS::Menu::map {
namespace {
// The state flags func_80107630 and func_8010839C index by door state.
constexpr std::size_t kDoorStateFlagsOffset = 0x33F;
// Ordering-table byte offset of the icon quads and word slot of the lines.
constexpr std::size_t kMarkerQuadSlotBytes = 28;
constexpr std::size_t kLabelLineSlot = 8;

static_assert(offsetof(VS::State::StateFlags, door_lock_states) ==
              kDoorStateFlagsOffset);

std::uint64_t refusedCalls = 0;

void Refuse(std::string_view call, std::string_view reason) {
  if (std::has_single_bit(++refusedCalls)) {
    VS::Core::LogWarn("menu", "map markers: {} refused: {} ({} refusals)", call,
                      reason, refusedCalls);
  }
}

[[nodiscard]] std::span<MapLabel> Labels() { return mapLabels; }

// The state flag byte for `state_id`, read unsigned as the original's lbu;
// false when it lies outside the state flags.
[[nodiscard]] bool DoorStateFlagOf(std::int32_t state_id, std::uint8_t& flag) {
  const std::span<const std::uint8_t> flags{
      reinterpret_cast<const std::uint8_t*>(&VS::State::stateFlags),
      sizeof(VS::State::StateFlags)};
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
      (sizeof(Primitive) + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
  return reinterpret_cast<Primitive*>(
      Render::ReserveLivePrimitiveWords(kWords));
}

class LiveMapMarkerHost final : public MapMarkerHost {
 public:
  explicit LiveMapMarkerHost(std::string_view call,
                             std::uint32_t* caller = nullptr)
      : call_(call), caller_(caller) {}

  void AddMarkerQuad(const MarkerQuad& quad) override {
    auto* poly = ReservePrimitive<POLY_FT4>();
    if (poly == nullptr ||
        VS::Render::FrameScratchpad().ordering_table == nullptr) {
      Refuse(call_, "no room in the frame primitive buffer");
      return;
    }
    setPolyFT4(poly);
    setShadeTex(poly, 1);
    const auto& corner = quad.corners;
    setXY4(poly, corner[0].x, corner[0].y, corner[1].x, corner[1].y,
           corner[2].x, corner[2].y, corner.back().x, corner.back().y);
    const auto& texture = quad.texture;
    setUV4(poly, texture[0].u, texture[0].v, texture[1].u, texture[1].v,
           texture[2].u, texture[2].v, texture.back().u, texture.back().v);
    poly->tpage = quad.texture_page;
    poly->clut = quad.clut;
    AddPrim(static_cast<std::uint8_t*>(
                VS::Render::FrameScratchpad().ordering_table) +
                kMarkerQuadSlotBytes,
            poly);
    VS::Render::FrameScratchpad().primitive_cursor = poly + 1;
  }

  void AddLabelLine(const LabelLine& line) override {
    auto* packet = ReservePrimitive<LINE_F2>();
    if (packet == nullptr ||
        VS::Render::FrameScratchpad().ordering_table == nullptr) {
      Refuse(call_, "no room in the frame primitive buffer");
      return;
    }
    setLineF2(packet);
    setXY2(packet, line.from.x, line.from.y, line.to.x, line.to.y);
    setRGB0(packet, kLabelLineShade, kLabelLineShade, kLabelLineShade);
    AddPrim(static_cast<std::uint32_t*>(
                VS::Render::FrameScratchpad().ordering_table) +
                kLabelLineSlot,
            packet);
    VS::Render::FrameScratchpad().primitive_cursor = packet + 1;
  }

  void AddAshleyMarker(const AshleyMarkerSprite& sprite) override {
    std::uint32_t* target =
        sprite.list == SpriteList::kCaller
            ? caller_
            : static_cast<std::uint32_t*>(
                  VS::Render::FrameScratchpad().overlay_ordering_table);
    std::uint32_t* packet = Render::QueueLiveSpriteWithTexturePage(
        sprite.brightness_transparency, sprite.packed_position,
        sprite.packed_size, target);
    // The original writes the colour through the cursor 16 bytes back, the
    // linked packet's colour word; a dropped packet's discard storage keeps
    // the write in bounds.
    packet[Render::kSpriteTextureWord] = sprite.texture_word;
    packet[Render::kSpriteColorWord] = sprite.color_word;
  }

  void DrawLabelText(const LabelText& text) override {
    std::uint16_t* table =
        text.table == LabelTextTable::kDoorNames ? doors : connectingMaps;
    Framework::MenuRow* row = Framework::SetLiveMenuItem(
        0, text.x, text.y, text.width, 0,
        reinterpret_cast<char*>(&table[text.text_offset]));
    row->background_width = text.background_width;
    Framework::DrawLiveMenuItem(row);
    Framework::LiveMenuItem(0)->state = Framework::RowState::kInactive;
  }

  bool IsSigilDoorUnlocked(std::int32_t zone_id, std::int32_t map_id,
                           std::uint32_t kind) override {
    return VS::Field::IsRoomSigilDoorUnlockedEntry(zone_id, map_id, kind) != 0;
  }

 private:
  std::string_view call_;
  std::uint32_t* caller_;
};
}  // namespace

// decomp: MENU5.PRG renderDefaultIcon (src/MENU/MENU5.PRG/4D8.c:116; was
// _renderDefaultIcon).
void LiveDrawAshleyMarker(int id, int xy, int brightness,
                          std::uint32_t* next_prim) {
  LiveMapMarkerHost host("DrawAshleyMarker", next_prim);
  DrawAshleyMarker(
      host, id, xy, brightness,
      next_prim == nullptr ? SpriteList::kOverlay : SpriteList::kCaller);
}

// decomp: MENU5.PRG renderIcon (src/MENU/MENU5.PRG/4D8.c:2163; was
// _renderIcon).
void LiveDrawFeatureMarker(int id, int x, int y) {
  constexpr std::string_view kCall = "DrawFeatureMarker";
  LiveMapMarkerHost host(kCall);
  if (!DrawFeatureMarker(host, kIconRects, id, x, y)) {
    Refuse(kCall, "icon outside _iconRects");
  }
}

// decomp: MENU5.PRG func_80107630 (src/MENU/MENU5.PRG/4D8.c:2185).
void LiveDrawDoorStateMarker(int x, int y, int id) {
  constexpr std::string_view kCall = "DrawDoorStateMarker";
  std::uint8_t flag = 0;
  if (!DoorStateFlagOf(id, flag)) {
    Refuse(kCall, "door state outside the state flags");
    return;
  }
  LiveMapMarkerHost host(kCall);
  if (!DrawDoorStateMarker(host, kDoorMarkerUpperRects, kDoorMarkerLowerRects,
                           Labels(), mapLabelCount, x, y, id, flag)) {
    Refuse(kCall, "label queue full");
  }
}

// decomp: MENU5.PRG renderDoorName (src/MENU/MENU5.PRG/4D8.c:2232; was
// _renderDoorName).
void LiveDrawDoorLabel(int x, int y, int door_id) {
  constexpr std::string_view kCall = "DrawDoorLabel";
  LiveMapMarkerHost host(kCall);
  if (!DrawDoorLabel(host, doors, mapLabelY, x, y, door_id)) {
    Refuse(kCall, "door name outside _doors");
  }
}

// decomp: MENU5.PRG insertLine (src/MENU/MENU5.PRG/4D8.c:2263; was
// _insertLine).
void LiveAppendLabelLine(int x0, int y0, int x1, int y1) {
  LiveMapMarkerHost host("AppendLabelLine");
  AppendLabelLine(host, x0, y0, x1, y1);
}

// decomp: MENU5.PRG renderConnectingMapName (src/MENU/MENU5.PRG/4D8.c:2408; was
// _renderConnectingMapName).
void LiveDrawConnectingAreaLabel(int x, int y, int map_id) {
  constexpr std::string_view kCall = "DrawConnectingAreaLabel";
  std::array<std::uint32_t, std::size(VS::State::roomVisitProgress.area_flags)>
      flags{};
  std::memcpy(flags.data(), VS::State::roomVisitProgress.area_flags,
              sizeof(flags));
  LiveMapMarkerHost host(kCall);
  if (!DrawConnectingAreaLabel(host, connectingMaps, flags, mapLabelY, x, y,
                               map_id)) {
    Refuse(kCall, "area outside the visit flags or _connectingMaps");
  }
}

// decomp: MENU5.PRG addMapLabel (src/MENU/MENU5.PRG/4D8.c:2446; was
// _addMapLabel).
void LiveQueueLabel(int x, int y, int type, int label_id) {
  if (!QueueLabel(Labels(), mapLabelCount, x, y, type, label_id)) {
    Refuse("QueueLabel", "label queue full");
  }
}

// decomp: MENU5.PRG func_8010839C (src/MENU/MENU5.PRG/4D8.c:2476).
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
