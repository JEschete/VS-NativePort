#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Menu/MapMarkers.hpp"
#include "Menu/MapNavigation.hpp"

// The area map's drawing (MENU5.PRG 4D8.c:1081-1510 and 1965-2470): the
// visited rooms' projected fills, outlines and markers, the control legend
// with its backdrops, the compass and the queued label drain. The rules take
// typed room data, the menu state and the projection and depth-cue results
// a host answers, and hand typed packets and marker requests to the host;
// MapDrawingLive.hpp binds them to the GTE, the frame's packet buffer, the
// map markers and the copied map globals.
namespace VS::Menu::map {
struct Rgb {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;

  friend bool operator==(const Rgb&, const Rgb&) = default;
};

// _computeRotatedRect's corners (compassBounds): top-left, top-right,
// bottom-left and bottom-right before rotation.
struct CompassPoint {
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const CompassPoint&, const CompassPoint&) = default;
};
using CompassCorners = std::array<CompassPoint, kQuadCorners>;

// ---------------------------------------------------------------- rooms --

// An SVECTOR of the room's vertex table.
struct RoomVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t z = 0;
};

// RotTransPers' results: the screen position, the ordering-table depth it
// returns and the depth-cue interpolation it stores.
struct Projection {
  ScreenPoint screen;
  std::int32_t depth = 0;
  std::int32_t interpolation = 0;
};

// A row of the original's scratchpad vertex table (getScratchAddr(64)): the
// screen position and the low halfwords of the depth and interpolation.
struct ProjectedVertex {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t depth = 0;
  std::int16_t interpolation = 0;

  friend bool operator==(const ProjectedVertex&,
                         const ProjectedVertex&) = default;
};

// The rows a room's byte indices reach. Rows past the room's vertex count
// keep what an earlier room projected, as the original's scratchpad does.
inline constexpr std::size_t kRoomVertexCapacity = 256;
using RoomWorkspace = std::array<ProjectedVertex, kRoomVertexCapacity>;

// A gouraud triangle (POLY_G3) or quad (POLY_G4) in packet corner order.
inline constexpr std::size_t kTriangleCorners = 3;
struct RoomTriangle {
  std::array<ScreenPoint, kTriangleCorners> corners{};
  std::array<Rgb, kTriangleCorners> colors{};

  friend bool operator==(const RoomTriangle&, const RoomTriangle&) = default;
};

struct RoomQuad {
  std::array<ScreenPoint, kQuadCorners> corners{};
  std::array<Rgb, kQuadCorners> colors{};

  friend bool operator==(const RoomQuad&, const RoomQuad&) = default;
};

// A gouraud line (LINE_G2).
struct RoomEdge {
  ScreenPoint from;
  ScreenPoint to;
  Rgb from_color;
  Rgb to_color;
  bool semi_transparent = false;

  friend bool operator==(const RoomEdge&, const RoomEdge&) = default;
};

// A flat rectangle (TILE): half of a marker's cross.
struct RoomTile {
  ScreenPoint position;
  std::int16_t width = 0;
  std::int16_t height = 0;
  Rgb color;

  friend bool operator==(const RoomTile&, const RoomTile&) = default;
};

// What the drawn room's markers compare against: the room being drawn, the
// map's current room, the room the player stands in and the door they came
// through (vs_battle_doorEntered), the menu's frame counter, the room's
// zone and map and Ashley's 16 blink brightness steps.
struct RoomContext {
  std::int32_t room_index = 0;
  std::int32_t current_room = 0;
  std::int32_t scene_room = 0;
  std::int32_t entered_door = 0;
  std::int32_t frame_counter = 0;
  std::int32_t zone_id = 0;
  std::int32_t map_id = 0;
  std::span<const std::int32_t> brightness;
};

inline constexpr std::size_t kAshleyBrightnessSteps = 16;

enum class RoomDrawResult : std::uint8_t {
  kDrawn,
  // A vertex projected outside the drawn depth range; the original's 0.
  kOutOfDepthRange,
  // A count or table reaches past the room's bytes, the room has more
  // vertices than the workspace holds or the brightness steps are missing;
  // nothing is projected or drawn.
  kMalformed,
};

class RoomDrawHost {
 public:
  RoomDrawHost() = default;
  RoomDrawHost(const RoomDrawHost&) = delete;
  RoomDrawHost& operator=(const RoomDrawHost&) = delete;
  RoomDrawHost(RoomDrawHost&&) = delete;
  RoomDrawHost& operator=(RoomDrawHost&&) = delete;
  virtual ~RoomDrawHost() = default;

  // RotTransPers through the map's loaded view.
  [[nodiscard]] virtual Projection Project(const RoomVertex& vertex) = 0;
  // DpqColor.
  [[nodiscard]] virtual Rgb DepthCue(const Rgb& color,
                                     std::int32_t interpolation) = 0;
  // Each appends a packet at the primitive cursor, linked into the scene
  // ordering table at word `depth`.
  virtual void AddTriangle(const RoomTriangle& triangle,
                           std::int32_t depth) = 0;
  virtual void AddQuad(const RoomQuad& quad, std::int32_t depth) = 0;
  virtual void AddEdge(const RoomEdge& edge, std::int32_t depth) = 0;
  virtual void AddTile(const RoomTile& tile, std::int32_t depth) = 0;
  // func_8010839C, func_80107630, _renderIcon, _addMapLabel and
  // _renderDefaultIcon with the label ordering-table slot as its target.
  [[nodiscard]] virtual bool ShouldDrawDoorState(std::int32_t state_id,
                                                 std::int32_t zone_id,
                                                 std::int32_t map_id) = 0;
  virtual void DrawDoorState(std::int32_t x, std::int32_t y,
                             std::int32_t state_id) = 0;
  virtual void DrawFeature(std::int32_t icon, std::int32_t x,
                           std::int32_t y) = 0;
  virtual void QueueLabel(std::int32_t x, std::int32_t y, std::int32_t kind,
                          std::int32_t label_id) = 0;
  virtual void DrawAshley(std::int32_t position, std::int32_t brightness) = 0;
};

// _renderRoom: projects the room's vertices into `workspace`, stopping at
// the first one outside depths [0x40, 0x7FF), then draws its fills (G3 and
// G4), the four-pixel glow and core of its outlines, its dim edges and its
// markers. `room` starts at the vertex count.
[[nodiscard]] RoomDrawResult DrawRoom(RoomDrawHost& host,
                                      std::span<const std::byte> room,
                                      const RoomContext& context,
                                      RoomWorkspace& workspace);

// The menu's projection offset (vs_battle_geomOffset).
struct ScreenOffset {
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const ScreenOffset&, const ScreenOffset&) = default;
};

inline constexpr ScreenOffset kMapCenter{.x = 0x90, .y = 0x88};

class VisitedRoomsHost {
 public:
  VisitedRoomsHost() = default;
  VisitedRoomsHost(const VisitedRoomsHost&) = delete;
  VisitedRoomsHost& operator=(const VisitedRoomsHost&) = delete;
  VisitedRoomsHost(VisitedRoomsHost&&) = delete;
  VisitedRoomsHost& operator=(VisitedRoomsHost&&) = delete;
  virtual ~VisitedRoomsHost() = default;

  [[nodiscard]] virtual ScreenOffset GetScreenOffset() = 0;
  virtual void SetScreenOffset(const ScreenOffset& offset) = 0;
  // Empties the label queue (_mapLabelCount = 0).
  virtual void ClearLabels() = 0;
  // _renderRoom for the scene's room `index`.
  virtual void DrawRoom(std::size_t index, std::int32_t zone_id,
                        std::int32_t map_id) = 0;
  // renderMapLabels.
  virtual void DrawLabels() = 0;
};

// _renderMap: centres the projection on (geom_x + 0x90, geom_y + 0x88),
// empties the label queue, draws every visited room in order, drains the
// labels and restores the previous offset.
void DrawVisitedRooms(VisitedRoomsHost& host, std::int32_t geom_x,
                      std::int32_t geom_y, std::span<const MapRoom> rooms);

// --------------------------------------------------------------- labels --

class LabelDrainHost {
 public:
  LabelDrainHost() = default;
  LabelDrainHost(const LabelDrainHost&) = delete;
  LabelDrainHost& operator=(const LabelDrainHost&) = delete;
  LabelDrainHost(LabelDrainHost&&) = delete;
  LabelDrainHost& operator=(LabelDrainHost&&) = delete;
  virtual ~LabelDrainHost() = default;

  // _renderDoorName and _renderConnectingMapName.
  virtual void DrawDoorLabel(std::int32_t x, std::int32_t y,
                             std::int32_t door_id) = 0;
  virtual void DrawAreaLabel(std::int32_t x, std::int32_t y,
                             std::int32_t area_id) = 0;
};

// The commands state that hides the legend and the labels.
inline constexpr std::int32_t kHiddenCommandsState = 5;

// renderMapLabels: unless the legend is hidden, draws every queued door
// label and then every area label. False when `count` exceeds `labels`,
// where the original reads past _mapLabels; only `labels` are drawn.
[[nodiscard]] bool DrawLabels(LabelDrainHost& host, std::int32_t commands_state,
                              bool hide_ui, std::span<const MapLabel> labels,
                              std::int32_t count);

// -------------------------------------------------------------- legend --

// A backdrop's shaded line (LINE_G2) and its black shadow (POLY_F4), both
// linked into ordering-table slot 7.
struct BackdropLine {
  ScreenPoint from;
  ScreenPoint to;

  friend bool operator==(const BackdropLine&, const BackdropLine&) = default;
};

inline constexpr Rgb kBackdropLeftColor{.r = 0x40, .g = 0x38, .b = 0x20};
inline constexpr Rgb kBackdropRightColor{.r = 0x10, .g = 0x10, .b = 8};
// The rows a backdrop may draw; the legend's are 11.
inline constexpr std::int64_t kMaxBackdropLines = 256;

class BackdropHost {
 public:
  BackdropHost() = default;
  BackdropHost(const BackdropHost&) = delete;
  BackdropHost& operator=(const BackdropHost&) = delete;
  BackdropHost(BackdropHost&&) = delete;
  BackdropHost& operator=(BackdropHost&&) = delete;
  virtual ~BackdropHost() = default;

  virtual void AddBackdropLine(const BackdropLine& line) = 0;
  virtual void AddBackdropShadow(
      const std::array<ScreenPoint, kQuadCorners>& corners) = 0;
};

// _drawUIControlsBackground: rows y to y + h - 2, each a pixel shorter than
// the last from `w`, then the slanted shadow two pixels down and right.
// False, with nothing drawn, for more than kMaxBackdropLines rows, where the
// original writes them all.
[[nodiscard]] bool DrawControlBackdrop(BackdropHost& host, std::int32_t x,
                                       std::int32_t y, std::int32_t width,
                                       std::int32_t height);

// The legend's button glyphs (vs_mainmenu_renderButton's ids).
enum class ControlButton : std::uint8_t {
  kCircle = 2,
  kL1 = 4,
  kR1 = 5,
  kL2 = 6,
  kR2 = 7,
};

enum class ControlCaption : std::uint8_t {
  kBack,
  kNext,
  kRotation,
  kZoom,
  kMenu,
  kJamming,
};

inline constexpr std::uint32_t kCaptionBright = 0x808080;
inline constexpr std::uint32_t kCaptionDim = 0x202020;

// _drawUIControls' slide (uiAnimationState and its uiXOffset static).
struct ControlsState {
  std::int16_t animation_state = 0;
  std::int32_t x_offset = 0;
};

struct ControlsFrame {
  bool hide_ui = false;
  std::int16_t commands_state = 0;
  std::int32_t compass_angle = 0;
};

class ControlsHost {
 public:
  ControlsHost() = default;
  ControlsHost(const ControlsHost&) = delete;
  ControlsHost& operator=(const ControlsHost&) = delete;
  ControlsHost(ControlsHost&&) = delete;
  ControlsHost& operator=(ControlsHost&&) = delete;
  virtual ~ControlsHost() = default;

  // Moves text box 0's y and y indent by `delta` when it exists.
  virtual void ShiftTextBox(std::int32_t delta) = 0;
  // Whether the scene's map is jammed (map_paling); none refuses the frame.
  [[nodiscard]] virtual std::optional<bool> IsJammed() = 0;
  virtual void DrawButton(ControlButton button, std::int32_t x,
                          std::int32_t y) = 0;
  // vs_battle_renderTextRawColor with the packed position.
  virtual void DrawCaption(ControlCaption caption, std::int32_t packed_xy,
                           std::uint32_t color) = 0;
  virtual void DrawBackdrop(std::int32_t x, std::int32_t y, std::int32_t width,
                            std::int32_t height) = 0;
  // insertTPage(7, getTPage(2, 0, 192, 0)).
  virtual void AddTexturePage() = 0;
  virtual void DrawCompass(std::int32_t x, std::int32_t y,
                           std::int32_t angle) = 0;
};

// _drawUIControls: steps the legend's slide in from x = -128 and back out,
// moving text box 0 with it, then unless hidden draws either the jammed
// caption or the buttons, the captions shaded by the commands state, the
// backdrops, the texture page and the compass. False when the jamming state
// is refused.
[[nodiscard]] bool DrawControls(ControlsHost& host, ControlsState& state,
                                const ControlsFrame& frame);

// ------------------------------------------------------------- compass --

// _computeRotatedRect without the trigonometry: the corners of a
// `width` x `height` box rotated by the angle whose rsin and rcos are
// `sine` and `cosine`, halved, about (center_x, center_y).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
[[nodiscard]] CompassCorners ComputeCompassCorners(
    std::int32_t center_x, std::int32_t center_y, std::int32_t width,
    std::int32_t height, std::int32_t sine, std::int32_t cosine);

// A compass sprite (POLY_FT4 with setShadeTex) linked into ordering-table
// byte offset `slot_bytes`.
struct CompassQuad {
  MarkerQuad quad;
  bool semi_transparent = false;
  std::int32_t slot_bytes = 0;

  friend bool operator==(const CompassQuad&, const CompassQuad&) = default;
};

class CompassHost {
 public:
  CompassHost() = default;
  CompassHost(const CompassHost&) = delete;
  CompassHost& operator=(const CompassHost&) = delete;
  CompassHost(CompassHost&&) = delete;
  CompassHost& operator=(CompassHost&&) = delete;
  virtual ~CompassHost() = default;

  // rsin and rcos.
  [[nodiscard]] virtual std::int32_t Sine(std::int32_t angle) = 0;
  [[nodiscard]] virtual std::int32_t Cosine(std::int32_t angle) = 0;
  virtual void AddCompassQuad(const CompassQuad& quad) = 0;
};

// _renderCompass: the eight rose ticks, the four cardinal letters with their
// glows and the needle turned by angle - roll (cameraAngles.vz). Returns the
// needle's corners, which the original leaves in compassBounds.
[[nodiscard]] CompassCorners DrawCompass(CompassHost& host, std::int32_t x,
                                         std::int32_t y, std::int32_t angle,
                                         std::int32_t roll);
}  // namespace VS::Menu::map
