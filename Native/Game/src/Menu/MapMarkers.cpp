// Hand-maintained (tools/port/owned.csv).
#include "Menu/MapMarkers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::map {
namespace {
using Core::Math::WrappingAdd;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

// getTPage(0, 0, 768, 0).
constexpr std::uint16_t kIconTexturePage = 0x0C;
// getClut(848, 223), getClut(976, 223) and getClut(864, 223).
constexpr std::uint16_t kFeatureIconClut = 0x37F5;
constexpr std::uint16_t kDoorStateIconClut = 0x37FD;
constexpr std::uint16_t kDoorStateCaptionClut = 0x37F6;
constexpr std::int32_t kFeatureIconHeight = 8;

constexpr std::int32_t kAshleyBrightness = 128;
// vs_getWH(16, 16).
constexpr std::int32_t kAshleySize = 0x00100010;
constexpr std::uint32_t kAshleyFirstIcon = 4;
constexpr unsigned kAshleyIconShift = 4;
// getClut(976, 223) << 16 | v = 0x80.
constexpr std::uint32_t kAshleyClutAndRow = 0x37FD8000U;
constexpr std::uint32_t kAshleySpriteCommand = 0x64000000U;

constexpr std::int32_t kLabelRowX = 194;
constexpr std::int32_t kLabelLineDrop = 8;
constexpr std::int32_t kLabelRowStep = 16;
constexpr std::int32_t kDoorPivotX = 186;
constexpr std::int32_t kDoorRowWidth = 128;
constexpr std::uint8_t kDoorRowBackground = 16;
constexpr std::int32_t kAreaPivotX = 170;
constexpr std::int32_t kAreaRowX = 178;
constexpr std::int32_t kAreaRowWidth = 144;
constexpr std::uint8_t kAreaRowBackground = 8;
constexpr unsigned kQuarterShift = 2;
constexpr unsigned kAreaWordShift = 5;
constexpr std::uint32_t kAreaBitMask = 0x1F;
constexpr std::int32_t kDoorNameMask = 0x7F;
constexpr std::uint8_t kHiddenStateFrom = 2;

[[nodiscard]] std::int16_t Wrap(std::int32_t value) {
  return WrapSigned16(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::uint8_t LowByte(std::int32_t value) {
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(value));
}

// The quad `rect` covers centred on `x` from `y` to `y + height`: the left
// edge rounds half the width toward zero and the right edge adds the width.
[[nodiscard]] MarkerQuad QuadOf(const MarkerRect& rect, std::int32_t x,
                                std::int32_t y, std::int32_t height,
                                std::uint16_t clut) {
  const std::int32_t left = WrappingSubtract(x, rect.w / 2);
  const std::int32_t right = WrappingAdd(rect.w, left);
  const std::int32_t bottom = WrappingAdd(height, y);
  const std::uint8_t u0 = LowByte(rect.x);
  const std::uint8_t v0 = LowByte(rect.y);
  const auto u1 = static_cast<std::uint8_t>(u0 + LowByte(rect.w));
  const auto v1 = static_cast<std::uint8_t>(v0 + LowByte(height));
  return {.corners = {{{Wrap(left), Wrap(y)},
                       {Wrap(right), Wrap(y)},
                       {Wrap(left), Wrap(bottom)},
                       {Wrap(right), Wrap(bottom)}}},
          .texture = {{{u0, v0}, {u1, v0}, {u0, v1}, {u1, v1}}},
          .clut = clut,
          .texture_page = kIconTexturePage};
}

// The leader line's bend: a quarter of the way from `x` to `pivot_x`, or
// `pivot_x` itself when `x` lies right of it. The original rounds a negative
// distance toward zero before testing the quarter's sign, but a negative
// quarter and the zero quarter it can round to both give `pivot_x`.
[[nodiscard]] std::int32_t LeaderBendOf(std::int32_t x, std::int32_t pivot_x) {
  const std::int32_t offset = WrappingSubtract(pivot_x, x);
  if (offset < 0) return pivot_x;
  return WrappingSubtract(pivot_x, offset >> kQuarterShift);
}

void DrawLeader(MapMarkerHost& host, std::int32_t x, std::int32_t y,
                std::int32_t bend, std::int32_t label_y) {
  const std::int32_t row_y = WrappingAdd(label_y, kLabelLineDrop);
  AppendLabelLine(host, x, y, bend, row_y);
  AppendLabelLine(host, bend, row_y, kLabelRowX, row_y);
}

// The text offset entry `id` of a self-indexed name table names, when both
// lie inside it.
[[nodiscard]] bool NameOffsetOf(std::span<const std::uint16_t> names,
                                std::int32_t id, std::size_t& offset) {
  if (id < 0 || static_cast<std::size_t>(id) >= names.size()) return false;
  offset = names[static_cast<std::size_t>(id)];
  return offset < names.size();
}
}  // namespace

// decomp: MENU5.PRG _renderDefaultIcon (4D8.c:106).
void DrawAshleyMarker(MapMarkerHost& host, std::int32_t id,
                      std::int32_t position, std::int32_t brightness,
                      SpriteList list) {
  const std::uint32_t icon = static_cast<std::uint32_t>(id) + kAshleyFirstIcon;
  host.AddAshleyMarker(
      {.brightness_transparency = kAshleyBrightness,
       .packed_position = position,
       .packed_size = kAshleySize,
       .texture_word = (icon << kAshleyIconShift) | kAshleyClutAndRow,
       .color_word =
           static_cast<std::uint32_t>(brightness) | kAshleySpriteCommand,
       .list = list});
}

// decomp: MENU5.PRG _renderIcon (4D8.c:2158).
bool DrawFeatureMarker(MapMarkerHost& host, std::span<const MarkerRect> icons,
                       std::int32_t id, std::int32_t x, std::int32_t y) {
  if (id < 0 || static_cast<std::size_t>(id) >= icons.size()) return false;
  host.AddMarkerQuad(QuadOf(icons[static_cast<std::size_t>(id)], x, y,
                            kFeatureIconHeight, kFeatureIconClut));
  return true;
}

// decomp: MENU5.PRG _addMapLabel (4D8.c:2441).
bool QueueLabel(std::span<MapLabel> labels, std::int32_t& count, std::int32_t x,
                std::int32_t y, std::int32_t kind, std::int32_t id) {
  if (count < 0 || static_cast<std::size_t>(count) >= labels.size()) {
    return false;
  }
  labels[static_cast<std::size_t>(count)] = {
      .x = Wrap(x), .y = Wrap(y), .kind = Wrap(kind), .id = Wrap(id)};
  ++count;
  return true;
}

// decomp: MENU5.PRG func_80107630 (4D8.c:2180).
bool DrawDoorStateMarker(MapMarkerHost& host,
                         const std::array<MarkerRect, 2>& upper,
                         const std::array<MarkerRect, 2>& lower,
                         std::span<MapLabel> labels, std::int32_t& label_count,
                         std::int32_t x, std::int32_t y, std::int32_t state_id,
                         std::uint8_t state_flag) {
  const std::size_t variant = state_flag & 1U;
  const MarkerRect& icon = upper.at(variant);
  host.AddMarkerQuad(QuadOf(icon, x, y, icon.h, kDoorStateIconClut));
  const std::int32_t caption_y = WrappingAdd(y, icon.h);
  const MarkerRect& caption = lower.at(variant);
  host.AddMarkerQuad(
      QuadOf(caption, x, caption_y, caption.h, kDoorStateCaptionClut));
  const std::int32_t door_name = WrappingSubtract(state_id, 1) & kDoorNameMask;
  return QueueLabel(labels, label_count, x, caption_y, kDoorLabelKind,
                    door_name);
}

// decomp: MENU5.PRG _insertLine (4D8.c:2258).
void AppendLabelLine(MapMarkerHost& host, std::int32_t x0, std::int32_t y0,
                     std::int32_t x1, std::int32_t y1) {
  host.AddLabelLine({.from = {Wrap(x0), Wrap(y0)}, .to = {Wrap(x1), Wrap(y1)}});
}

// decomp: MENU5.PRG _renderDoorName (4D8.c:2227).
bool DrawDoorLabel(MapMarkerHost& host,
                   std::span<const std::uint16_t> door_names,
                   std::int32_t& label_y, std::int32_t x, std::int32_t y,
                   std::int32_t door_id) {
  std::size_t text_offset = 0;
  if (!NameOffsetOf(door_names, door_id, text_offset)) return false;
  DrawLeader(host, x, y, LeaderBendOf(x, kDoorPivotX), label_y);
  host.DrawLabelText({.x = kLabelRowX,
                      .y = label_y,
                      .width = kDoorRowWidth,
                      .background_width = kDoorRowBackground,
                      .table = LabelTextTable::kDoorNames,
                      .text_offset = text_offset});
  label_y = WrappingSubtract(label_y, kLabelRowStep);
  return true;
}

// decomp: MENU5.PRG _renderConnectingMapName (4D8.c:2403).
bool DrawConnectingAreaLabel(MapMarkerHost& host,
                             std::span<const std::uint16_t> area_names,
                             std::span<const std::uint32_t> area_flags,
                             std::int32_t& label_y, std::int32_t x,
                             std::int32_t y, std::int32_t area_id) {
  const std::int32_t word = area_id >> kAreaWordShift;
  if (word < 0 || static_cast<std::size_t>(word) >= area_flags.size()) {
    return false;
  }
  const std::uint32_t bit =
      1U << (static_cast<std::uint32_t>(area_id) & kAreaBitMask);
  const bool visited = (area_flags[static_cast<std::size_t>(word)] & bit) != 0;
  std::size_t text_offset = 0;
  if (!NameOffsetOf(area_names, visited ? area_id : kUnvisitedAreaName,
                    text_offset)) {
    return false;
  }
  DrawLeader(host, x, y, LeaderBendOf(x, kAreaPivotX), label_y);
  host.DrawLabelText({.x = kAreaRowX,
                      .y = label_y,
                      .width = kAreaRowWidth,
                      .background_width = kAreaRowBackground,
                      .table = LabelTextTable::kAreaNames,
                      .text_offset = text_offset});
  label_y = WrappingSubtract(label_y, kLabelRowStep);
  return true;
}

// decomp: MENU5.PRG func_8010839C (4D8.c:2471).
bool ShouldDrawDoorStateMarker(MapMarkerHost& host, std::int32_t state_id,
                               std::int32_t zone_id, std::int32_t map_id,
                               std::uint8_t state_flag) {
  const std::uint32_t sigil_kind = static_cast<std::uint32_t>(
      WrappingSubtract(state_id, kFirstSigilDoorState));
  if (sigil_kind < kSigilDoorKinds &&
      host.IsSigilDoorUnlocked(zone_id, map_id, sigil_kind)) {
    return false;
  }
  return state_flag < kHiddenStateFrom;
}
}  // namespace VS::Menu::map
