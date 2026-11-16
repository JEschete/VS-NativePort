// Hand-maintained (tools/port/owned.csv).
#include "Menu/Score/GazetteCamera.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Menu/Framework/Framework.hpp"
#include "Render/UiPackets.hpp"

namespace VS::Menu::Score {
namespace {
using Core::Math::R3000Quotient;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrappingSubtract;
using Core::Math::WrapSigned16;

// The GPU command codes: LINE_G2, POLY_F4 and POLY_G4, whose code's bit 1
// makes it semi-transparent (setSemiTrans).
constexpr std::uint32_t kLineCode = 0x50;
constexpr std::uint32_t kFlatQuadCode = 0x28;
constexpr std::uint32_t kShadedQuadCode = 0x38;
constexpr std::uint32_t kSemiTransparentBit = 0x02;
// _insertTPage's draw-mode command: dithering on, and the page's nine bits.
constexpr std::uint32_t kDrawModeCommand = 0xE1000200;
constexpr std::uint32_t kTexturePageMask = 0x1FF;

constexpr PanelColor kBlack{};
constexpr PanelColor kHalfWhite{128, 128, 128};

// The quads two pixels down and right of a panel.
constexpr std::int32_t kShadowOffset = 2;
// PanelCorners' last corner.
constexpr std::size_t kBottomRightCorner = kPanelCorners - 1;
// The footer's and edge fade's screen: 320 by 240.
constexpr std::int16_t kScreenWidth = 320;
constexpr std::int16_t kScreenHeight = 240;
constexpr std::int32_t kFooterFadeHeight = 8;
constexpr std::int16_t kEdgeFadeWidth = 64;
constexpr std::int16_t kRightFadeLeft = kScreenWidth - kEdgeFadeWidth;

// ONE: the Q12 unit the camera's products are divided by.
constexpr std::int32_t kQ12One = 4096;
constexpr std::int32_t kQ12Shift = 12;

// The overview's projection centre and text rows (238.c:1693-1716).
constexpr std::int32_t kOverviewCentreX = 0x80;
constexpr std::int32_t kOverviewCentreY = 0xF0;
constexpr std::int32_t kOverviewSlideShift = 5;
constexpr std::int32_t kOverviewTextStep = 16;
constexpr std::int32_t kOverviewCaptionLeft = 120;
constexpr std::uint32_t kOverviewTopRow = 16;
constexpr std::uint32_t kOverviewBottomRow = 184;
constexpr std::int32_t kOverviewPanelLeft = 128;
constexpr std::int32_t kOverviewPanelWidth = 136;
constexpr std::int32_t kOverviewPanelHeight = 12;
constexpr std::int32_t kOverviewScoreGradient = 2;
constexpr std::int32_t kOverviewRankGradient = 1;
constexpr std::int32_t kOverviewFooterTop = 236;
constexpr std::int32_t kOverviewFooterStep = 6;
constexpr std::uint32_t kPackedLowMask = 0xFFFF;
constexpr int kPackedHighShift = 16;

std::uint32_t PackColor(std::uint32_t code, const PanelColor& color) noexcept {
  return Render::PackColorCommand(code, color.red, color.green, color.blue);
}

std::uint32_t PackPoint(const PanelPoint& point) noexcept {
  return static_cast<std::uint32_t>(Render::PackScreenWord(point.x, point.y));
}

PanelPoint Point(std::int32_t x, std::int32_t y) noexcept {
  return {.x = WrapSigned16(static_cast<std::uint32_t>(x)),
          .y = WrapSigned16(static_cast<std::uint32_t>(y))};
}

// The corners of a w by h quad at (x, y), its right and bottom edges
// inclusive.
PanelCorners RectCorners(const PanelRect& rect) noexcept {
  const std::int32_t right =
      WrappingSubtract(WrappingAdd(rect.x, rect.width), 1);
  const std::int32_t bottom =
      WrappingSubtract(WrappingAdd(rect.y, rect.height), 1);
  return {Point(rect.x, rect.y), Point(right, rect.y), Point(rect.x, bottom),
          Point(right, bottom)};
}

PanelRect Shadow(const PanelRect& rect) noexcept {
  return {.x = WrappingAdd(rect.x, kShadowOffset),
          .y = WrappingAdd(rect.y, kShadowOffset),
          .width = rect.width,
          .height = rect.height};
}

bool InPalette(std::int32_t gradient) noexcept {
  return gradient >= 0 && gradient < kPanelGradients;
}

// ((v * ONE-scaled factor) / ONE): a 32-bit product divided with truncation.
std::int32_t ScaleQ12(std::int32_t value, std::int32_t factor) noexcept {
  return R3000Quotient(WrappingMultiply(value, factor), kQ12One);
}

std::int32_t ShiftLeft(std::int32_t value, std::int32_t shift) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(value) << shift);
}
}  // namespace

std::array<std::uint32_t, kPanelLineWords> PackPanelLine(
    const PanelLine& line) noexcept {
  return {PackColor(kLineCode, line.from_color), PackPoint(line.from),
          PackColor(0, line.to_color), PackPoint(line.to)};
}

std::array<std::uint32_t, kPanelFlatQuadWords> PackPanelFlatQuad(
    const PanelFlatQuad& quad) noexcept {
  std::array<std::uint32_t, kPanelFlatQuadWords> words{
      PackColor(kFlatQuadCode, quad.color)};
  for (std::size_t corner = 0; corner < kPanelCorners; ++corner) {
    words[corner + 1] = PackPoint(quad.corners[corner]);
  }
  return words;
}

std::array<std::uint32_t, kPanelShadedQuadWords> PackPanelShadedQuad(
    const PanelShadedQuad& quad) noexcept {
  const std::uint32_t code =
      kShadedQuadCode | (quad.semi_transparent ? kSemiTransparentBit : 0);
  std::array<std::uint32_t, kPanelShadedQuadWords> words{};
  for (std::size_t corner = 0; corner < kPanelCorners; ++corner) {
    words[corner * 2] = PackColor(corner == 0 ? code : 0, quad.colors[corner]);
    words[(corner * 2) + 1] = PackPoint(quad.corners[corner]);
  }
  return words;
}

std::array<std::uint32_t, kPanelTexturePageWords> PackPanelTexturePage(
    std::uint16_t texture_page) noexcept {
  return {kDrawModeCommand | (texture_page & kTexturePageMask), 0};
}

std::int64_t EnemyBannerLineCount(const PanelRect& rect) noexcept {
  const std::int32_t end =
      WrappingSubtract(WrappingAdd(rect.y, rect.height), 1);
  return end > rect.y ? std::int64_t{end} - rect.y : 0;
}

// decomp: MENU9.PRG _renderEnemyInfoBg (src/MENU/MENU9.PRG/238.c:1552)
PanelDrawResult DrawEnemyBanner(const PanelRect& rect, std::int32_t gradient,
                                PanelTarget& target) {
  const std::int64_t lines = EnemyBannerLineCount(rect);
  if (!InPalette(gradient) || lines > kMaxEnemyBannerLines) {
    return PanelDrawResult::kRefused;
  }
  const PanelGradient& stops =
      kPanelGradientStops[static_cast<std::size_t>(gradient)];
  // Each row's line is a pixel shorter than the one above it.
  for (std::int32_t row = 0; row < lines; ++row) {
    const std::int32_t y = WrappingAdd(rect.y, row);
    const std::int32_t right = WrappingSubtract(
        WrappingAdd(rect.x, WrappingSubtract(rect.width, row)), 1);
    target.DrawLine(kPanelSlot, {.from = Point(rect.x, y),
                                 .to = Point(right, y),
                                 .from_color = stops.left,
                                 .to_color = stops.right});
  }
  // The shadow's bottom right corner follows the slant: height pixels in.
  const PanelRect shadow = Shadow(rect);
  PanelCorners corners = RectCorners(shadow);
  PanelPoint& bottom_right = corners[kBottomRightCorner];
  bottom_right = Point(
      WrappingSubtract(WrappingAdd(shadow.x, shadow.width), shadow.height),
      bottom_right.y);
  target.DrawFlatQuad(kPanelSlot, {.corners = corners, .color = kBlack});
  target.SetTexturePage(kPanelSlot, kPanelTexturePage);
  return PanelDrawResult::kDrawn;
}

// decomp: MENU9.PRG _renderGradientQuad (src/MENU/MENU9.PRG/238.c:1594)
PanelDrawResult DrawGradientPanel(const PanelRect& rect, std::int32_t gradient,
                                  PanelTarget& target) {
  if (!InPalette(gradient)) {
    return PanelDrawResult::kRefused;
  }
  const PanelGradient& stops =
      kPanelGradientStops[static_cast<std::size_t>(gradient)];
  target.DrawShadedQuad(kPanelSlot, {.corners = RectCorners(rect),
                                     .colors = {stops.left, stops.right,
                                                stops.left, stops.right}});
  target.DrawFlatQuad(kPanelSlot,
                      {.corners = RectCorners(Shadow(rect)), .color = kBlack});
  target.SetTexturePage(kPanelSlot, kPanelTexturePage);
  return PanelDrawResult::kDrawn;
}

// decomp: MENU9.PRG _renderFooterBg (src/MENU/MENU9.PRG/238.c:1645)
void DrawFooterBackground(std::int32_t top, PanelTarget& target) {
  const std::int32_t fade_bottom = WrappingAdd(top, kFooterFadeHeight);
  target.DrawShadedQuad(
      kPanelSlot,
      {.corners = {Point(0, top), Point(kScreenWidth, top),
                   Point(0, fade_bottom), Point(kScreenWidth, fade_bottom)},
       .colors = {kBlack, kBlack, kHalfWhite, kHalfWhite},
       .semi_transparent = true});
  target.DrawFlatQuad(
      kPanelSlot,
      {.corners = {Point(0, fade_bottom), Point(kScreenWidth, fade_bottom),
                   Point(0, kScreenHeight), Point(kScreenWidth, kScreenHeight)},
       .color = kBlack});
  target.SetTexturePage(kPanelSlot, kAdditiveTexturePage);
}

// decomp: MENU9.PRG _renderScreenEdgeFade (src/MENU/MENU9.PRG/238.c:2146)
void DrawViewerEdgeFade(PanelTarget& target) {
  target.DrawShadedQuad(
      kEdgeFadeSlot,
      {.corners = {PanelPoint{0, 0}, PanelPoint{kEdgeFadeWidth, 0},
                   PanelPoint{0, kScreenHeight},
                   PanelPoint{kEdgeFadeWidth, kScreenHeight}},
       .colors = {kHalfWhite, kBlack, kHalfWhite, kBlack},
       .semi_transparent = true});
  target.DrawShadedQuad(
      kEdgeFadeSlot,
      {.corners = {PanelPoint{kRightFadeLeft, 0}, PanelPoint{kScreenWidth, 0},
                   PanelPoint{kRightFadeLeft, kScreenHeight},
                   PanelPoint{kScreenWidth, kScreenHeight}},
       .colors = {kBlack, kHalfWhite, kBlack, kHalfWhite},
       .semi_transparent = true});
  target.SetTexturePage(kEdgeFadeSlot, kAdditiveTexturePage);
}

// decomp: MENU9.PRG _restoreCamera (src/MENU/MENU9.PRG/238.c:1402)
void RestoreCamera(const CameraPose& backup, CameraPose& camera) noexcept {
  camera.position = backup.position;
  camera.angles = backup.angles;
}

// decomp: MENU9.PRG _buildCameraMatrix (src/MENU/MENU9.PRG/238.c:1474)
ViewerCamera BuildViewerCamera(const ViewerCameraSettings& settings) noexcept {
  const std::int32_t distance = settings.distance;
  const std::int32_t negative_distance = WrappingNegate(distance);
  const std::int32_t cos_pitch = Core::Math::Cosine(settings.pitch);
  const std::int16_t eye_x = WrapSigned16(static_cast<std::uint32_t>(
      ScaleQ12(ScaleQ12(Core::Math::Sine(settings.yaw), distance), cos_pitch)));
  const std::int16_t eye_z = WrapSigned16(static_cast<std::uint32_t>(
      ScaleQ12(ScaleQ12(Core::Math::Cosine(settings.yaw), negative_distance),
               cos_pitch)));
  // The height offset is halved toward zero before it lowers the eye.
  const std::int16_t eye_y =
      WrapSigned16(static_cast<std::uint32_t>(WrappingSubtract(
          ScaleQ12(Core::Math::Sine(settings.pitch), negative_distance),
          settings.height_offset / 2)));

  ViewerCamera camera;
  camera.position = {.x = ShiftLeft(eye_x, kQ12Shift),
                     .y = ShiftLeft(eye_y, kQ12Shift),
                     .z = ShiftLeft(eye_z, kQ12Shift)};
  camera.angles = {.x = settings.pitch, .y = settings.yaw, .z = settings.roll};
  // func_80041C68(&pitch, camera): the yaw rotation, then the pitch.
  camera.view.rotation = Core::Math::MultiplyMatrixIntoRight(
      Framework::BuildPitchMatrix(settings.pitch),
      Framework::BuildYawMatrix(settings.yaw));
  // ApplyMatrix: the rotation of the negated eye, as signed halfwords.
  camera.view.translation = Core::Math::RotateTranslate(
      {.rotation = camera.view.rotation, .translation = {}},
      {.x = WrapSigned16(static_cast<std::uint32_t>(-eye_x)),
       .y = WrapSigned16(static_cast<std::uint32_t>(-eye_y)),
       .z = WrapSigned16(static_cast<std::uint32_t>(-eye_z))});
  return camera;
}

std::string OverviewScoreText(std::uint32_t score) {
  return "#" + std::to_string(static_cast<std::int32_t>(score));
}

// decomp: MENU9.PRG _renderTopMenu (src/MENU/MENU9.PRG/238.c:1679)
void RenderOverview(OverviewHost& host, std::int32_t animation_step,
                    std::uint32_t score) {
  if (animation_step < kOverviewSteps) {
    host.DrawViewerEdgeFade();
  }
  const ProjectionOffset saved = host.GetProjectionOffset();
  host.SetProjectionOffset(
      {.x = WrappingSubtract(
           kOverviewCentreX,
           ShiftLeft(WrappingSubtract(kOverviewSteps, animation_step),
                     kOverviewSlideShift)),
       .y = kOverviewCentreY});
  const Core::Math::Transform3 view = host.BuildViewerCamera();
  host.DrawActorPreview(view);
  host.SetProjectionOffset(saved);

  const std::string score_text = OverviewScoreText(score);
  const std::int32_t slide =
      WrappingMultiply(animation_step, kOverviewTextStep);
  const std::uint32_t caption_x = static_cast<std::uint32_t>(WrappingSubtract(
                                      slide, kOverviewCaptionLeft)) &
                                  kPackedLowMask;
  const std::uint32_t score_x =
      static_cast<std::uint32_t>(slide) & kPackedLowMask;
  const std::int32_t panel_x = WrappingSubtract(slide, kOverviewPanelLeft);
  host.DrawText("SCORE", caption_x | (kOverviewTopRow << kPackedHighShift),
                kOverviewTextColor);
  host.DrawText(score_text, score_x | (kOverviewTopRow << kPackedHighShift),
                kOverviewTextColor);
  host.DrawGradientPanel({.x = panel_x,
                          .y = static_cast<std::int32_t>(kOverviewTopRow),
                          .width = kOverviewPanelWidth,
                          .height = kOverviewPanelHeight},
                         kOverviewScoreGradient);
  host.DrawText("RISK   BREAKER   RANK",
                caption_x | (kOverviewBottomRow << kPackedHighShift),
                kOverviewTextColor);
  host.DrawGradientPanel({.x = panel_x,
                          .y = static_cast<std::int32_t>(kOverviewBottomRow),
                          .width = kOverviewPanelWidth,
                          .height = kOverviewPanelHeight},
                         kOverviewRankGradient);
  host.DrawFooterBackground(
      WrappingSubtract(kOverviewFooterTop,
                       WrappingMultiply(animation_step, kOverviewFooterStep)));
}
}  // namespace VS::Menu::Score
