#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENU9.PRG score screen (gazette): the viewer camera, the shaded panels the
// overview, records and encyclopedia draw behind their text, and the
// overview's composition (238.c:1402-1408, 1474-1509, 1552-1717, 2146-2174;
// F02804, F02807, F02810-F02813, F02821). The cores hand typed primitives,
// each with the ordering-table slot the original linked it into, to a
// target, and reach the overview's text, projection offset and actor preview
// through a host; none of them touches the frame scratch, the primitive
// cursor or the copied globals (GazetteCameraLive.hpp binds them).

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
// Ordering-table slots relative to the frame's scene table
// (VS::Render::FrameScratchpad().ordering_table): the panels, footer and
// overview text
// (+0x1C), and the viewer's edge fade (+0x20).
inline constexpr std::int32_t kPanelSlot = 7;
inline constexpr std::int32_t kEdgeFadeSlot = 8;

struct PanelPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;

  friend bool operator==(const PanelPoint&, const PanelPoint&) = default;
};

struct PanelColor {
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;

  friend bool operator==(const PanelColor&, const PanelColor&) = default;
};

// Quad corners in the GPU's order: top left, top right, bottom left, bottom
// right.
inline constexpr std::size_t kPanelCorners = 4;
using PanelCorners = std::array<PanelPoint, kPanelCorners>;

// LINE_G2: a Gouraud-shaded line.
struct PanelLine {
  PanelPoint from{};
  PanelPoint to{};
  PanelColor from_color{};
  PanelColor to_color{};

  friend bool operator==(const PanelLine&, const PanelLine&) = default;
};

// POLY_F4: a flat quad.
struct PanelFlatQuad {
  PanelCorners corners{};
  PanelColor color{};

  friend bool operator==(const PanelFlatQuad&, const PanelFlatQuad&) = default;
};

// POLY_G4: a Gouraud-shaded quad, opaque or semi-transparent.
struct PanelShadedQuad {
  PanelCorners corners{};
  std::array<PanelColor, kPanelCorners> colors{};
  bool semi_transparent = false;

  friend bool operator==(const PanelShadedQuad&,
                         const PanelShadedQuad&) = default;
};

class PanelTarget {
 public:
  PanelTarget() = default;
  PanelTarget(const PanelTarget&) = delete;
  PanelTarget& operator=(const PanelTarget&) = delete;
  PanelTarget(PanelTarget&&) = delete;
  PanelTarget& operator=(PanelTarget&&) = delete;
  virtual ~PanelTarget() = default;

  // Each call queues one packet in front of ordering-table entry `slot`.
  virtual void DrawLine(std::int32_t slot, const PanelLine& line) = 0;
  virtual void DrawFlatQuad(std::int32_t slot, const PanelFlatQuad& quad) = 0;
  virtual void DrawShadedQuad(std::int32_t slot,
                              const PanelShadedQuad& quad) = 0;
  // _insertTPage (insertTPage.h): a dithered draw-mode command selecting
  // `texture_page`'s page and blending.
  virtual void SetTexturePage(std::int32_t slot,
                              std::uint16_t texture_page) = 0;
};

// The packet words after each primitive's tag, as the GPU reads them. The
// padding bytes after the second, third and fourth colours, which the
// original never writes, are 0.
inline constexpr std::size_t kPanelLineWords = 4;
inline constexpr std::size_t kPanelFlatQuadWords = 5;
inline constexpr std::size_t kPanelShadedQuadWords = 8;
inline constexpr std::size_t kPanelTexturePageWords = 2;
[[nodiscard]] std::array<std::uint32_t, kPanelLineWords> PackPanelLine(
    const PanelLine& line) noexcept;
[[nodiscard]] std::array<std::uint32_t, kPanelFlatQuadWords> PackPanelFlatQuad(
    const PanelFlatQuad& quad) noexcept;
[[nodiscard]] std::array<std::uint32_t, kPanelShadedQuadWords>
PackPanelShadedQuad(const PanelShadedQuad& quad) noexcept;
[[nodiscard]] std::array<std::uint32_t, kPanelTexturePageWords>
PackPanelTexturePage(std::uint16_t texture_page) noexcept;

enum class PanelDrawResult : std::uint8_t {
  kDrawn,
  // An input the original would have read past its palette or looped
  // without a bound with; nothing was drawn.
  kRefused,
};

// A rectangle in the original's int arguments, before 16-bit truncation.
struct PanelRect {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
};

// _bgColorStops0 and _bgColorStops1 (238.c:481, 484): the green and purple
// UI backgrounds' left and right colours.
struct PanelGradient {
  PanelColor left{};
  PanelColor right{};
};
inline constexpr std::int32_t kPanelGradients = 4;
using PanelGradients = std::array<PanelGradient, kPanelGradients>;
inline constexpr PanelGradients kPanelGradientStops{{
    {.left = {0, 65, 107}, .right = {0, 5, 51}},
    {.left = {25, 130, 108}, .right = {1, 40, 38}},
    {.left = {64, 48, 102}, .right = {8, 8, 32}},
    {.left = {64, 56, 32}, .right = {16, 16, 8}},
}};

// getTPage(0, 3, 0, 0): the opaque panel page; getTPage(0, 2, 0, 0): the
// additive page behind the footer and the edge fade.
inline constexpr std::uint16_t kPanelTexturePage = 0x60;
inline constexpr std::uint16_t kAdditiveTexturePage = 0x40;

// Native bound: the original queues one line per banner row with no limit.
// VRAM holds 512 lines, so no draw area shows a taller banner whole; a
// banner of more rows is refused instead of overrunning the primitives.
inline constexpr std::int64_t kMaxEnemyBannerLines = 512;

// The rows `rect` draws as banner lines: one per y from its top to the row
// before its last, none when that end (wrapped to a word) is not below the
// top.
[[nodiscard]] std::int64_t EnemyBannerLineCount(const PanelRect& rect) noexcept;

// decomp: MENU9.PRG _renderEnemyInfoBg (src/MENU/MENU9.PRG/238.c:1552). A
// banner of gradient lines, each a pixel shorter than the one above, so its
// right edge slants; then a black flat quad two pixels down and right whose
// bottom right corner follows the slant, and the opaque panel page. A
// gradient outside the palette or more than kMaxEnemyBannerLines lines are
// refused.
PanelDrawResult DrawEnemyBanner(const PanelRect& rect, std::int32_t gradient,
                                PanelTarget& target);

// decomp: MENU9.PRG _renderGradientQuad (src/MENU/MENU9.PRG/238.c:1594). A
// left-to-right gradient quad over `rect`, its black shadow two pixels down
// and right, and the opaque panel page. A gradient outside the palette is
// refused.
PanelDrawResult DrawGradientPanel(const PanelRect& rect, std::int32_t gradient,
                                  PanelTarget& target);

// decomp: MENU9.PRG _renderFooterBg (src/MENU/MENU9.PRG/238.c:1645). An
// eight-pixel additive fade from `top` down across the 320-pixel screen,
// black below it to y 240, and the additive page.
void DrawFooterBackground(std::int32_t top, PanelTarget& target);

// decomp: MENU9.PRG _renderScreenEdgeFade (src/MENU/MENU9.PRG/238.c:2146).
// Additive 64-pixel fades in from the screen's left and right edges, and the
// additive page, in the edge fade slot.
void DrawViewerEdgeFade(PanelTarget& target);

// VS::Render::Camera's position and angles VECTORs: x, y, z and the pad word.
inline constexpr std::size_t kCameraVectorWords = 4;
using CameraVector = std::array<std::int32_t, kCameraVectorWords>;
struct CameraPose {
  CameraVector position{};
  CameraVector angles{};

  friend bool operator==(const CameraPose&, const CameraPose&) = default;
};

// decomp: MENU9.PRG _restoreCamera (src/MENU/MENU9.PRG/238.c:1402). The
// frame camera's position and angles become the backups the score screen
// took on entry (_cameraPositionBackup, _cameraAnglesBackup), all four words
// of each, pad included.
void RestoreCamera(const CameraPose& backup, CameraPose& camera) noexcept;

// _cameraAngles (pitch about x, yaw about y, roll about z), _cameraDistance
// and _cameraHeightOffset.
struct ViewerCameraSettings {
  std::int16_t pitch = 0;
  std::int16_t yaw = 0;
  std::int16_t roll = 0;
  std::int32_t distance = 0;
  std::int16_t height_offset = 0;
};

struct ViewerCamera {
  // The eye, each component a signed halfword shifted to Q12, as the frame
  // camera's position holds it.
  Core::Math::Vec3 position{};
  // The frame camera's angles: pitch, yaw and roll.
  Core::Math::Vec3 angles{};
  // The view matrix: the pitch rotation times the yaw rotation, translated
  // by that rotation of the negated eye.
  Core::Math::Transform3 view{};

  friend bool operator==(const ViewerCamera&, const ViewerCamera&) = default;
};

// decomp: MENU9.PRG _buildCameraMatrix (src/MENU/MENU9.PRG/238.c:1474). The
// eye orbits `distance` from the origin: x is sin(yaw) * distance then *
// cos(pitch), z the same with cos(yaw) and -distance, each product divided
// by 4096 with truncation in 32 bits, and y is sin(pitch) * -distance / 4096
// less half the height offset; each component keeps its low 16 bits.
[[nodiscard]] ViewerCamera BuildViewerCamera(
    const ViewerCameraSettings& settings) noexcept;

// The overview's projection centre: x slides in with the animation step.
struct ProjectionOffset {
  std::int32_t x = 0;
  std::int32_t y = 0;

  friend bool operator==(const ProjectionOffset&,
                         const ProjectionOffset&) = default;
};

class OverviewHost {
 public:
  OverviewHost() = default;
  OverviewHost(const OverviewHost&) = delete;
  OverviewHost& operator=(const OverviewHost&) = delete;
  OverviewHost(OverviewHost&&) = delete;
  OverviewHost& operator=(OverviewHost&&) = delete;
  virtual ~OverviewHost() = default;

  // _renderScreenEdgeFade.
  virtual void DrawViewerEdgeFade() = 0;
  // getGeomOffset and setGeomOffset: the GTE projection's screen centre.
  [[nodiscard]] virtual ProjectionOffset GetProjectionOffset() = 0;
  virtual void SetProjectionOffset(const ProjectionOffset& offset) = 0;
  // _buildCameraMatrix: builds the viewer camera and returns its matrix.
  [[nodiscard]] virtual Core::Math::Transform3 BuildViewerCamera() = 0;
  // func_800F9EB8: draws Ashley's preview through `camera`.
  virtual void DrawActorPreview(const Core::Math::Transform3& camera) = 0;
  // vs_battle_renderTextRawColor in front of the panel slot.
  virtual void DrawText(std::string_view text, std::uint32_t position,
                        std::uint32_t color) = 0;
  // _renderGradientQuad.
  virtual void DrawGradientPanel(const PanelRect& rect,
                                 std::int32_t gradient) = 0;
  // _renderFooterBg.
  virtual void DrawFooterBackground(std::int32_t top) = 0;
};

// vs_getRGB888(128, 128, 128): the overview text's gray.
inline constexpr std::uint32_t kOverviewTextColor = 0x808080;
// The overview slides in over eight steps.
inline constexpr std::int32_t kOverviewSteps = 8;

// sprintf("#%ld", _score): the score as a signed long, which on the PS1 is
// the 32-bit word.
[[nodiscard]] std::string OverviewScoreText(std::uint32_t score);

// decomp: MENU9.PRG _renderTopMenu (src/MENU/MENU9.PRG/238.c:1679). Below
// kOverviewSteps the edge fade first; then, with the projection centred at
// x 0x80 - ((8 - step) << 5) and y 0xF0 and restored afterwards, Ashley's
// preview through the viewer camera; then "SCORE" and the score in the top
// panel and the rank caption in the bottom one, sliding in by 16 pixels a
// step, and the footer rising by 6 pixels a step from y 236. All arithmetic
// wraps in 32 bits.
void RenderOverview(OverviewHost& host, std::int32_t animation_step,
                    std::uint32_t score);
}  // namespace VS::Menu::Score
