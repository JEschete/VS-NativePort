#include "Battle/FloatingLabelRendering.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;
constexpr std::int32_t kCaptionVerticalRadius = 6;
constexpr std::int32_t kSignVerticalRadius = 5;
constexpr std::int32_t kNumberAdvance = 6;
constexpr std::int32_t kCaptionGap = 12;
constexpr std::int32_t kSignGap = 4;
constexpr std::int32_t kSignWidth = 8;
constexpr std::uint16_t kCaptionPage = 0x17;
constexpr std::uint16_t kCaptionPalette = 0x373D;
constexpr std::uint16_t kCompactPage = 0x0C;
constexpr std::uint16_t kCompactPalette = 0x37F6;
constexpr std::int16_t kSignU = 154;
constexpr std::int16_t kSignV = 76;
constexpr std::int16_t kSignHeight = 10;
constexpr std::uint16_t kNegativeValue = 0x8000;
constexpr std::uint8_t kOpeningFrames = 4;
constexpr std::uint8_t kLockedClosingFrame = 27;
constexpr std::int32_t kLockedLastFrame = 30;
constexpr std::int32_t kLockedHalfWidth = 16;
constexpr std::size_t kLockedPieces = 4;
constexpr std::int32_t kScreenRight = 320;
constexpr std::int32_t kMarkerHalfWidth = 8;
constexpr std::int32_t kMarkerWidth = 16;
constexpr std::int32_t kMarkerFrameStride = 16;
constexpr std::int32_t kMarkerU = 64;
constexpr std::int16_t kMarkerV = 48;
constexpr std::uint16_t kMarkerPalette = 0x373E;
constexpr std::uint8_t kTimingClosingFrame = 42;
constexpr std::int32_t kTimingLastFrame = 45;
constexpr std::uint16_t kTimingSuccess = 2;
constexpr std::uint32_t kSuccessColor = 0x408040;
constexpr std::uint32_t kChainColor = 0x1040A0;
constexpr std::uint8_t kChainClosingFrame = 87;
constexpr std::int32_t kChainLastFrame = 90;
constexpr std::size_t kChainTextBytes = 16;
constexpr std::int32_t kCueHalfWidth = 11;
constexpr std::int32_t kCueHeight = 32;
constexpr std::int16_t kCueU = 192;
constexpr std::uint8_t kCueWidth = 22;
constexpr std::uint16_t kCuePalette = 0x377C;
constexpr Render::QuadColor kCueTint{240, 64, 32};
constexpr std::array<std::int16_t, kOpeningFrames> kSlide{64, 16, 4, 1};

Render::ScreenVertex ScreenPoint(std::int32_t x, std::int32_t y) {
  return {math::WrapSigned16(static_cast<std::uint32_t>(x)),
          math::WrapSigned16(static_cast<std::uint32_t>(y))};
}

FloatingLabelQuad Quad(const FloatingLabelAtlasRect& atlas,
                       FloatingLabelScreenPoint top_left,
                       FloatingLabelScreenPoint bottom_right,
                       std::uint16_t palette, std::uint16_t page) {
  return {.top_left = ScreenPoint(top_left.x, top_left.y),
          .bottom_right = ScreenPoint(bottom_right.x, bottom_right.y),
          .texture = {.u = static_cast<std::uint8_t>(atlas.u),
                      .v = static_cast<std::uint8_t>(atlas.v),
                      .width = static_cast<std::uint8_t>(atlas.width),
                      .height = static_cast<std::uint8_t>(atlas.height),
                      .texture_page = page,
                      .clut = palette}};
}

bool Slide(FloatingLabelScreenPoint& origin, std::int32_t step) {
  if (step < 0 || static_cast<std::size_t>(step) >= kSlide.size()) {
    return false;
  }
  origin.x =
      math::WrappingSubtract(origin.x, kSlide[static_cast<std::size_t>(step)]);
  return true;
}
}  // namespace

// decomp: F00723 func_80094E18 (src/BATTLE/BATTLE.PRG/2842C.c:2138).
std::optional<std::int32_t> DrawValueLabelCaption(
    FloatingLabelRenderWorld& world, FloatingLabelScreenPoint origin,
    const FloatingLabelAppearance& label) {
  const auto caption = world.ValueCaption(label.raw_mode);
  if (!caption) return std::nullopt;
  const auto& atlas = caption.value();
  const std::int32_t width =
      (label.text_length * kNumberAdvance + atlas.width + kCaptionGap) / 2;
  origin.x = math::WrappingSubtract(origin.x, width);
  const auto right = math::WrappingAdd(origin.x, atlas.width);
  if (!world.DrawQuad(Quad(
          {.u = atlas.u,
           .v = atlas.v,
           .width = atlas.width,
           .height = kCaptionVerticalRadius * 2},
          {origin.x, math::WrappingSubtract(origin.y, kCaptionVerticalRadius)},
          {right, math::WrappingAdd(origin.y, kCaptionVerticalRadius)},
          kCaptionPalette, kCaptionPage))) {
    return std::nullopt;
  }
  origin.x = math::WrappingAdd(right, kSignGap);
  const auto sign = (label.value & kNegativeValue) != 0 ? 0 : kSignWidth;
  const FloatingLabelAtlasRect sign_atlas{
      static_cast<std::int16_t>(kSignU + sign), kSignV, kSignWidth,
      kSignHeight};
  if (!world.DrawQuad(Quad(
          sign_atlas,
          {origin.x, math::WrappingSubtract(origin.y, kSignVerticalRadius)},
          {math::WrappingAdd(origin.x, kSignWidth),
           math::WrappingAdd(origin.y, kSignVerticalRadius)},
          kCompactPalette, kCompactPage))) {
    return std::nullopt;
  }
  return width;
}

// decomp: F00724 func_8009506C (src/BATTLE/BATTLE.PRG/2842C.c:2167).
bool DrawLockedFloatingLabel(FloatingLabelRenderWorld& world,
                             FloatingLabelScreenPoint origin,
                             const FloatingLabelAppearance& label) {
  std::int32_t step = kOpeningFrames;
  if (label.elapsed_frames < kOpeningFrames) {
    step = label.elapsed_frames;
  } else if (label.elapsed_frames >= kLockedClosingFrame) {
    step = kLockedLastFrame - label.elapsed_frames;
  }
  const auto half_height = world.LockedHalfHeight(step);
  if (!half_height) return false;
  origin.x = math::WrappingSubtract(origin.x, kLockedHalfWidth);
  const auto top = math::WrappingSubtract(origin.y, half_height.value());
  const auto bottom = math::WrappingAdd(origin.y, half_height.value());
  for (std::size_t piece = 0; piece < kLockedPieces; ++piece) {
    const auto rectangle = world.LockedPiece(piece);
    if (!rectangle) return false;
    auto atlas = rectangle.value();
    const auto right = math::WrappingAdd(origin.x, atlas.width);
    if (origin.x < kScreenRight && right >= 0) {
      atlas.height = kSignHeight;
      auto command = Quad(atlas, {origin.x, top}, {right, bottom},
                          kCompactPalette, kCompactPage);
      command.translucent = true;
      if (!world.DrawQuad(command)) return false;
    }
    origin.x = right;
  }
  return true;
}

// decomp: F00726 func_800957D0 (src/BATTLE/BATTLE.PRG/2842C.c:2312).
bool DrawStatusRemovalMarker(FloatingLabelRenderWorld& world,
                             FloatingLabelScreenPoint origin,
                             std::int32_t animation_step) {
  const auto texture_u = math::WrappingAdd(
      math::WrappingMultiply(animation_step, kMarkerFrameStride), kMarkerU);
  const FloatingLabelAtlasRect atlas{
      math::WrapSigned16(static_cast<std::uint32_t>(texture_u)), kMarkerV,
      kMarkerWidth, kSignHeight};
  origin.x = math::WrappingSubtract(origin.x, kMarkerHalfWidth);
  const auto command = Quad(
      atlas, {origin.x, math::WrappingSubtract(origin.y, kSignVerticalRadius)},
      {math::WrappingAdd(origin.x, kMarkerWidth),
       math::WrappingAdd(origin.y, kSignVerticalRadius)},
      kMarkerPalette, kCaptionPage);
  return world.DrawQuad(command);
}

// decomp: F00727 func_800958A4 (src/BATTLE/BATTLE.PRG/2842C.c:2332).
bool DrawAbilityTimingResult(FloatingLabelRenderWorld& world,
                             FloatingLabelScreenPoint origin,
                             const FloatingLabelAppearance& label) {
  constexpr std::array<std::string_view, 5> kTiming{
      "$TOO   FAST!", "$FAST!", "", "$SLOW!", "$TOO   SLOW!"};
  constexpr std::array<std::string_view, 8> kSuccess{
      "$COOL!",    "$GOOD!",         "$EXCELLENT!", "$RIGHT   ON!",
      "$PERFECT!", "$WELL   TIMED!", "$NICE!",      "$GREAT!"};
  constexpr std::array<std::uint32_t, kTiming.size()> kTimingColors{
      0x808000, 0x608020, 0x408040, 0x208060, 0x008080};
  if (label.elapsed_frames < kOpeningFrames) {
    if (!Slide(origin, label.elapsed_frames)) return false;
  } else if (label.elapsed_frames >= kTimingClosingFrame) {
    if (!Slide(origin, kTimingLastFrame - label.elapsed_frames)) return false;
  }
  if (label.value != kTimingSuccess) {
    if (label.value >= kTiming.size()) return false;
    return world.DrawText(kTiming[label.value], origin,
                          kTimingColors[label.value]);
  }
  if (label.text_length >= kSuccess.size()) return false;
  return world.DrawText(kSuccess[label.text_length], origin, kSuccessColor);
}

// decomp: F00728 func_80095A4C (src/BATTLE/BATTLE.PRG/2842C.c:2359).
bool DrawChainCount(FloatingLabelRenderWorld& world,
                    FloatingLabelScreenPoint origin,
                    const FloatingLabelAppearance& label) {
  if (label.elapsed_frames < kOpeningFrames && label.value == 1) {
    if (!Slide(origin, label.elapsed_frames)) return false;
  } else if (label.elapsed_frames >= kChainClosingFrame) {
    if (!Slide(origin, kChainLastFrame - label.elapsed_frames)) return false;
  }
  if (label.value == 1) {
    return world.DrawText("$1 CHAIN", origin, kChainColor);
  }
  std::array<char, kChainTextBytes> text{};
  text[0] = '$';
  const auto formatted =
      std::to_chars(text.data() + 1, text.data() + text.size(), label.value);
  if (formatted.ec != std::errc{}) return false;
  constexpr std::string_view kPlural = " CHAINS";
  const auto digits_end = static_cast<std::size_t>(formatted.ptr - text.data());
  if (digits_end + kPlural.size() >= text.size()) return false;
  kPlural.copy(text.data() + digits_end, kPlural.size());
  return world.DrawText({text.data(), digits_end + kPlural.size()}, origin,
                        kChainColor);
}

// decomp: F00741 func_800970BC (src/BATTLE/BATTLE.PRG/2842C.c:2864).
bool DrawAbilityTimingCue(FloatingLabelRenderWorld& world,
                          FloatingLabelScreenPoint origin) {
  auto command = Quad({kCueU, 0, kCueWidth, kCueHeight},
                      {math::WrappingSubtract(origin.x, kCueHalfWidth),
                       math::WrappingSubtract(origin.y, kCueHeight)},
                      {math::WrappingAdd(origin.x, kCueHalfWidth), origin.y},
                      kCuePalette, kCaptionPage);
  command.raw_texture = false;
  command.tint = kCueTint;
  return world.DrawQuad(command);
}
}  // namespace VS::Battle
