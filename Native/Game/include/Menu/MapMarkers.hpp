#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// The area map's markers and labels (MENU5.PRG 4D8.c:106-117, 2158-2270 and
// 2403-2479): Ashley's position sprite, the feature and door-state icons, the
// door and connecting-area name labels with their leader lines, the label
// queue the projected rooms fill, and whether a door's state marker is shown.
// The rules take screen positions the room projection already produced and
// hand typed quads, lines, sprites and text rows to a MapMarkerHost;
// MapMarkersLive.hpp binds them to the frame's packet buffer, the menu rows
// and the copied map globals. Nothing here projects a vertex.
namespace VS::Menu::map {
// A RECT from _iconRects, D_80108D04 or D_80108D14: the icon's texture origin
// and size.
struct MarkerRect {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t w = 0;
  std::int16_t h = 0;
};

// A screen position as the packet's halfwords store it.
struct ScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const ScreenPoint&, const ScreenPoint&) = default;
};

// A texture coordinate as the packet's bytes store it.
struct TexturePoint {
  std::uint8_t u = 0;
  std::uint8_t v = 0;

  friend bool operator==(const TexturePoint&, const TexturePoint&) = default;
};

inline constexpr std::size_t kQuadCorners = 4;

// A shade-free textured quad (POLY_FT4 with setShadeTex) for the icon
// ordering-table slot: corners and texture points in top-left, top-right,
// bottom-left, bottom-right order.
struct MarkerQuad {
  std::array<ScreenPoint, kQuadCorners> corners{};
  std::array<TexturePoint, kQuadCorners> texture{};
  std::uint16_t clut = 0;
  std::uint16_t texture_page = 0;

  friend bool operator==(const MarkerQuad&, const MarkerQuad&) = default;
};

// A flat grey line (LINE_F2, colour 128) for the label ordering-table slot.
struct LabelLine {
  ScreenPoint from;
  ScreenPoint to;

  friend bool operator==(const LabelLine&, const LabelLine&) = default;
};

inline constexpr std::uint8_t kLabelLineShade = 128;

// Which name table a label's text comes from.
enum class LabelTextTable : std::uint8_t {
  kDoorNames,
  kAreaNames,
};

// A menu-row label: row 0 placed at (x, y) with `width`, its background
// width set after placement and its state cleared once drawn. The text is
// the name table entry `text_offset` halfwords from the table's start.
struct LabelText {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::uint8_t background_width = 0;
  LabelTextTable table = LabelTextTable::kDoorNames;
  std::size_t text_offset = 0;

  friend bool operator==(const LabelText&, const LabelText&) = default;
};

// Where _renderDefaultIcon links its sprite.
enum class SpriteList : std::uint8_t {
  // The frame's overlay ordering table (vs_scratch.unk8), for a null target.
  kOverlay,
  // The caller's primitive.
  kCaller,
};

// Ashley's position sprite: vs_battle_setSpriteDefaultTexPage's arguments and
// the texture and colour words written into its packet afterwards.
struct AshleyMarkerSprite {
  std::int32_t brightness_transparency = 0;
  std::int32_t packed_position = 0;
  std::int32_t packed_size = 0;
  std::uint32_t texture_word = 0;
  std::uint32_t color_word = 0;
  SpriteList list = SpriteList::kOverlay;

  friend bool operator==(const AshleyMarkerSprite&,
                         const AshleyMarkerSprite&) = default;
};

// An entry of _mapLabels, each field a stored halfword.
struct MapLabel {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t kind = 0;
  std::int16_t id = 0;

  friend bool operator==(const MapLabel&, const MapLabel&) = default;
};

// _mapLabels' capacity; the count word follows the array in the module, so
// the original's ninth append overwrites it.
inline constexpr std::size_t kMapLabelCapacity = 8;
inline constexpr std::int32_t kDoorLabelKind = 0;
inline constexpr std::int32_t kAreaLabelKind = 1;
// The connecting-area name shown for an area not yet visited.
inline constexpr std::int32_t kUnvisitedAreaName = 32;
// The first and number of sigil door states func_8008E7BC answers for.
inline constexpr std::int32_t kFirstSigilDoorState = 0x41;
inline constexpr std::uint32_t kSigilDoorKinds = 7;

class MapMarkerHost {
 public:
  MapMarkerHost() = default;
  MapMarkerHost(const MapMarkerHost&) = delete;
  MapMarkerHost& operator=(const MapMarkerHost&) = delete;
  MapMarkerHost(MapMarkerHost&&) = delete;
  MapMarkerHost& operator=(MapMarkerHost&&) = delete;
  virtual ~MapMarkerHost() = default;

  // Appends a quad at the primitive cursor, linked into ordering-table
  // slot 7.
  virtual void AddMarkerQuad(const MarkerQuad& quad) = 0;
  // Appends a line at the primitive cursor, linked into slot 8.
  virtual void AddLabelLine(const LabelLine& line) = 0;
  virtual void AddAshleyMarker(const AshleyMarkerSprite& sprite) = 0;
  // vs_battle_setMenuItem, the background width, vs_battle_renderMenuItem
  // and clearing row 0's state.
  virtual void DrawLabelText(const LabelText& text) = 0;
  // func_8008E7BC: whether the room's sigil door `kind` is unlocked.
  [[nodiscard]] virtual bool IsSigilDoorUnlocked(std::int32_t zone_id,
                                                 std::int32_t map_id,
                                                 std::uint32_t kind) = 0;
};

// _renderDefaultIcon: Ashley's sprite `id` at packed `position`, 16x16 from
// the default texture page with CLUT (976, 223), coloured by `brightness`.
void DrawAshleyMarker(MapMarkerHost& host, std::int32_t id,
                      std::int32_t position, std::int32_t brightness,
                      SpriteList list);

// _renderIcon: icon `id` of `icons` centred on `x` from `y`, eight pixels
// tall with CLUT (848, 223). False, with nothing drawn, for an id outside
// `icons`, where the original reads past _iconRects.
[[nodiscard]] bool DrawFeatureMarker(MapMarkerHost& host,
                                     std::span<const MarkerRect> icons,
                                     std::int32_t id, std::int32_t x,
                                     std::int32_t y);

// _addMapLabel: appends a label, each field's low halfword stored. False, with
// nothing written, when `count` is outside [0, labels.size()), where the
// original writes past _mapLabels.
[[nodiscard]] bool QueueLabel(std::span<MapLabel> labels, std::int32_t& count,
                              std::int32_t x, std::int32_t y, std::int32_t kind,
                              std::int32_t id);

// func_80107630: the door state icon (`upper`, CLUT (976, 223)) and its
// caption (`lower`, CLUT (864, 223)) below it, both chosen by bit 0 of the
// state flag, then queues a door label at the caption with the door name
// (state_id - 1) & 0x7F. False when the queue is full; the quads are drawn.
[[nodiscard]] bool DrawDoorStateMarker(
    MapMarkerHost& host, const std::array<MarkerRect, 2>& upper,
    const std::array<MarkerRect, 2>& lower, std::span<MapLabel> labels,
    std::int32_t& label_count, std::int32_t x, std::int32_t y,
    std::int32_t state_id, std::uint8_t state_flag);

// _insertLine: a grey line from (x0, y0) to (x1, y1).
void AppendLabelLine(MapMarkerHost& host, std::int32_t x0, std::int32_t y0,
                     std::int32_t x1, std::int32_t y1);

// _renderDoorName: a leader line from (x, y) bending at a quarter of the way
// to x = 186 toward the row at (194, label_y), the door's name and the next
// row 16 higher. False, with nothing drawn, when the name is outside
// `door_names`.
[[nodiscard]] bool DrawDoorLabel(MapMarkerHost& host,
                                 std::span<const std::uint16_t> door_names,
                                 std::int32_t& label_y, std::int32_t x,
                                 std::int32_t y, std::int32_t door_id);

// _renderConnectingMapName: as DrawDoorLabel bending toward x = 170, with the
// row at (178, label_y); an area whose bit in `area_flags` is clear shows
// the unvisited name. False, with nothing drawn, when the area's flag word or
// name is outside the tables.
[[nodiscard]] bool DrawConnectingAreaLabel(
    MapMarkerHost& host, std::span<const std::uint16_t> area_names,
    std::span<const std::uint32_t> area_flags, std::int32_t& label_y,
    std::int32_t x, std::int32_t y, std::int32_t area_id);

// func_8010839C: whether a door's state marker is drawn: not for an unlocked
// sigil door (states 0x41-0x47), otherwise when its state flag is below 2.
[[nodiscard]] bool ShouldDrawDoorStateMarker(MapMarkerHost& host,
                                             std::int32_t state_id,
                                             std::int32_t zone_id,
                                             std::int32_t map_id,
                                             std::uint8_t state_flag);
}  // namespace VS::Menu::map
