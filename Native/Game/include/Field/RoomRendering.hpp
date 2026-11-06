#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Field/RoomState.hpp"
#include "Render/RoomDraw.hpp"

// The room rendering rules (BATTLE.PRG 146C.c:15521-16010 and 16412-16449):
// the field and room rendering switches, the room tint, the room palettes,
// the camera's view-trigger occlusion and the order a frame draws the room
// scene and its minimap in. Every geometry, palette and trigger record is
// read through a bounded span; RoomRenderingLive.cpp binds the session's
// room globals.
namespace VS::Field {
// The room's switches and tint (D_800F190C, D_800F1BA4 and D_800F1BB0).
struct RoomTintState {
  Render::RoomTint parameters{};
  bool active = false;
};

struct RoomRenderState {
  bool field_rendering_enabled = false;
  bool updates_paused = false;
  RoomTintState tint{};
};

// decomp: BATTLE.PRG func_8008A4DC (146C.c:15521)
void SetFieldRenderingEnabled(RoomRenderState& state, bool enabled);
// decomp: BATTLE.PRG func_8008B4BC (146C.c:16010)
// Pauses or resumes the room's animations and effects.
void SetRoomAnimationsPaused(RoomRenderState& state, bool paused);

// A colour code's channels (Render_ColorCode without the GPU code byte).
struct TintColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
};

// decomp: BATTLE.PRG func_8008B430 (146C.c:15995)
// A black colour at zero strength switches the tint off and keeps its last
// parameters; any other turns it on. Each channel becomes a signed offset
// and the strength keeps its low halfword, as the original stored them.
void SetRoomTint(RoomTintState& tint, const TintColor& color, int strength);
// decomp: BATTLE.PRG func_8008B4B0 (146C.c:16008)
void ClearRoomTint(RoomTintState& tint);

// decomp: BATTLE.PRG func_8008B28C (146C.c:15939)
// The room animation clock after a frame: unchanged while paused, else
// advanced by the tick speed and kept to 16 bits.
[[nodiscard]] std::uint16_t AdvanceRoomAnimationClock(bool paused,
                                                      std::uint16_t ticks,
                                                      int tick_speed);

// decomp: BATTLE.PRG func_8008B2C0 (146C.c:15946)
// Shows or hides the room when one is loaded; false when none is.
bool SetRoomRenderingEnabled(RoomDisplay& display, bool enabled);

// The room palette copy (18.c:595 allocates 0x800 bytes): four rows of 256
// colours. Rows 0-2 are the room's CLUT rows; the gimmick palette table
// starts at row 3.
inline constexpr std::size_t kRoomPaletteRowColors = 256;
inline constexpr std::size_t kRoomPaletteRows = 4;
inline constexpr std::size_t kRoomPaletteColors(
    kRoomPaletteRowColors* kRoomPaletteRows);
inline constexpr int kUploadedRoomPaletteRows = 3;
inline constexpr std::size_t kGimmickPaletteFirstColor(
    kRoomPaletteRowColors* kUploadedRoomPaletteRows);

// decomp: BATTLE.PRG func_8008B2E0 (146C.c:15953)
// Copies `colors` into palette `row` from colour `first`. The original's
// word copy moves whole pairs, so an odd last colour is not copied. False
// (nothing copied) when the colours would leave the palettes.
bool CopyZonePaletteRow(std::span<std::uint16_t> palettes,
                        std::span<const std::uint16_t> colors, int row,
                        int first);

class RoomPaletteHost {
 public:
  RoomPaletteHost() = default;
  RoomPaletteHost(const RoomPaletteHost&) = delete;
  RoomPaletteHost& operator=(const RoomPaletteHost&) = delete;
  RoomPaletteHost(RoomPaletteHost&&) = delete;
  RoomPaletteHost& operator=(RoomPaletteHost&&) = delete;
  virtual ~RoomPaletteHost() = default;

  // Loads one row of colours into CLUT row `row` from its first colour.
  virtual void UploadClutRow(std::span<const std::uint16_t> colors,
                             int row) = 0;
  // The palettes the room's gimmicks animate from.
  virtual void SetGimmickPalettes(std::span<std::uint16_t> palettes) = 0;
};

// decomp: BATTLE.PRG func_8008B320 (146C.c:15958)
// Uploads rows 0-2 and hands row 3 on to the gimmicks; false (nothing done)
// when the palettes are shorter than the room's four rows.
bool UploadRoomPalettes(std::span<std::uint16_t> palettes,
                        RoomPaletteHost& host);

// The texture id each room polygon carries (Field_MpdTriangle and
// Field_MpdQuad +0x1E).
inline constexpr std::size_t kRoomPolygonTextureIdOffset = 0x1E;
// The room loader's texture page offset (146C.c:15663 passes 11).
inline constexpr int kRoomTextureIdOffset = 11;

// decomp: BATTLE.PRG func_8008B390 (146C.c:15966)
// Adds `offset` to every polygon's texture id, wrapping at 16 bits. A group
// whose polygon block leaves the geometry is skipped and logged; the number
// of groups rewritten is returned.
int OffsetRoomTextureIds(std::span<std::byte> geometry, int offset);

// decomp: BATTLE.PRG func_8008C40C (146C.c:16449)
// Re-enables every geometry group the occlusion disabled.
void ClearGeometryOcclusion(std::span<std::byte> geometry);

// The camera's eighth of a turn the view triggers compare (146C.c:16412).
[[nodiscard]] int CameraViewDirection(int camera_yaw);

// Where the camera stands and how far it is (camera_distance_mode).
struct OcclusionQuery {
  int tile_x = 0;
  int tile_z = 0;
  int floor_layer = 0;
  int distance_mode = 0;
  int camera_yaw = 0;
};

// decomp: BATTLE.PRG func_8008C2C0 (146C.c:16412)
// With view triggers: clears the occlusion, then disables the geometry
// groups of the first trigger matching the query and reports whether one
// did. A truncated trigger record ends the walk and is logged.
bool ApplyCameraGeometryOcclusion(std::span<std::byte> geometry,
                                  std::span<const std::byte> view_triggers,
                                  const OcclusionQuery& query);

class RoomGeometryHost {
 public:
  RoomGeometryHost() = default;
  RoomGeometryHost(const RoomGeometryHost&) = delete;
  RoomGeometryHost& operator=(const RoomGeometryHost&) = delete;
  RoomGeometryHost(RoomGeometryHost&&) = delete;
  RoomGeometryHost& operator=(RoomGeometryHost&&) = delete;
  virtual ~RoomGeometryHost() = default;

  // The geometry is whole within its section extent.
  [[nodiscard]] virtual bool GeometryValid() = 0;
  // Loads the frame's view matrix as the rotation and translation.
  virtual void LoadViewTransform() = 0;
  virtual void UpdateGroupTransforms() = 0;
  // Draws the groups and records the primitives the draw wrote.
  virtual void DrawGroups() = 0;
  // Tints the primitives the last DrawGroups wrote.
  virtual void TintDrawnPrimitives(const Render::RoomTint& tint) = 0;
};

// decomp: BATTLE.PRG func_8008B1FC (146C.c:15926)
// Draws the room geometry and tints it when the tint is on; false (logged,
// nothing drawn) when the geometry is refused.
bool DrawRoomGeometry(const RoomTintState& tint, RoomGeometryHost& host);

// The first-person facing bits: the camera's eighth of a turn and its two
// neighbours (146C.c:15791).
[[nodiscard]] std::uint8_t RoomViewFacingMask(int camera_yaw);

// The minimap's slide in and out (Field_minimapSlideFrame).
inline constexpr std::int16_t kMinimapSlideFrames = 8;

// The slide frame after one frame: in by one while the minimap is visible,
// up to kMinimapSlideFrames, else out by two down to zero. Both steps wrap
// at 16 bits first, as the original's halfword did.
[[nodiscard]] std::int16_t AdvanceMinimapSlide(std::int16_t frame,
                                               bool visible);

struct MinimapArea {
  std::int16_t x = 0;
  std::int16_t y = 0;
  std::int16_t width = 0;
  std::int16_t height = 0;

  friend bool operator==(const MinimapArea&, const MinimapArea&) = default;
};

// Where the minimap draws: its clip area on the display buffer being drawn,
// the projection centre sliding in from the right and the map's turn.
struct MinimapLayout {
  MinimapArea area{};
  int offset_x = 0;
  int offset_y = 0;
  int map_yaw = 0;

  friend bool operator==(const MinimapLayout&, const MinimapLayout&) = default;
};

// The layout for a slide frame, the drawn display's clip x and the camera.
[[nodiscard]] MinimapLayout MinimapLayoutFor(std::int16_t slide_frame,
                                             int display_clip_x,
                                             int camera_yaw);

struct FrameColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const FrameColor&, const FrameColor&) = default;
};

// The minimap frame: the four corners of its top, right and bottom edges
// (a LINE_G4 from the top left) and the two ends of its left edge.
inline constexpr std::size_t kMinimapBorderCorners = 4;
inline constexpr std::size_t kMinimapEdgeEnds = 2;

struct MinimapFrameColors {
  std::array<FrameColor, kMinimapBorderCorners> border{};
  std::array<FrameColor, kMinimapEdgeEnds> edge{};

  friend bool operator==(const MinimapFrameColors&,
                         const MinimapFrameColors&) = default;
};

// The frame pulses red while a weapon is drawn, advancing `pulse_phase` by
// half the tick speed; otherwise it is green and the phase restarts.
[[nodiscard]] MinimapFrameColors AdvanceMinimapFrameColors(bool weapon_drawn,
                                                           int& pulse_phase,
                                                           int tick_speed);

// The player's minimap settings and the room's scene map.
struct MinimapSettings {
  bool shown = false;
  bool framed = false;
  bool has_scene = false;
};

class RoomSceneHost {
 public:
  RoomSceneHost() = default;
  RoomSceneHost(const RoomSceneHost&) = delete;
  RoomSceneHost& operator=(const RoomSceneHost&) = delete;
  RoomSceneHost(RoomSceneHost&&) = delete;
  RoomSceneHost& operator=(RoomSceneHost&&) = delete;
  virtual ~RoomSceneHost() = default;

  [[nodiscard]] virtual int TickSpeed() = 0;
  [[nodiscard]] virtual bool RoomUpdatesPaused() = 0;
  virtual void LoadViewTransform() = 0;
  // The passes ahead of the room geometry, in the original's order.
  virtual void DrawTriggerMarkers() = 0;
  virtual void DrawActorMarkers() = 0;
  virtual void UpdateGimmickAnimations() = 0;
  virtual void UpdateRoomEffects() = 0;
  virtual void DrawPlatformShadows() = 0;
  virtual void UpdateWeather() = 0;
  // A room is loaded and shown.
  [[nodiscard]] virtual bool RoomShown() = 0;
  [[nodiscard]] virtual bool HasGeometry() = 0;
  [[nodiscard]] virtual int CameraYaw() = 0;
  // Replaces the low byte of the first-person facing bits.
  virtual void SetViewFacing(std::uint8_t mask) = 0;
  virtual void DrawGeometry() = 0;
  [[nodiscard]] virtual MinimapSettings Minimap() = 0;
  [[nodiscard]] virtual bool MinimapVisible() = 0;
  [[nodiscard]] virtual std::int16_t MinimapSlideFrame() = 0;
  virtual void SetMinimapSlideFrame(std::int16_t frame) = 0;
  [[nodiscard]] virtual int DisplayClipX() = 0;
  virtual void DrawMinimap(const MinimapLayout& layout) = 0;
  [[nodiscard]] virtual bool WeaponDrawn() = 0;
  [[nodiscard]] virtual int BorderPulsePhase() = 0;
  virtual void SetBorderPulsePhase(int phase) = 0;
  virtual void DrawMinimapFrame(const MinimapFrameColors& colors) = 0;
};

// decomp: BATTLE.PRG func_8008AC78 (146C.c:15791)
// Draws one frame of the room: markers, gimmicks, a room effect step per two
// ticks unless paused, shadows and weather, then the shown room's geometry
// and its sliding minimap with the optional frame.
void DrawRoomScene(RoomSceneHost& host);
}  // namespace VS::Field
