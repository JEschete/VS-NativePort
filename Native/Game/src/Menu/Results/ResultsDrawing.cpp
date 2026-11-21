// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/ResultsDrawing.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Render/ScreenOverlay.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "Render/UiTexturedQuads.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

constexpr std::int32_t kDigitRise = 3;
constexpr std::int32_t kPrimeRise = 2;
constexpr std::int32_t kGlyphAdvance = 12;
constexpr std::int32_t kPrimeAdvance = 6;
constexpr std::size_t kPrimeAfter = 1;
constexpr std::size_t kDoublePrimeAfter = 3;
constexpr std::uint32_t kTimeByteMask = 0xFF;
constexpr int kTimeHighShift = 16;
constexpr int kTimeMiddleShift = 8;
constexpr std::uint32_t kDecimal = 10;
constexpr std::size_t kMaxByteDigits = 3;

constexpr std::int32_t kPromptOffset = 288;
constexpr std::int32_t kPromptSlideStep = 6;
constexpr std::int32_t kPromptTextIndent = 18;
constexpr std::int32_t kPromptLineHeight = 10;
constexpr std::int32_t kPromptIconDrop = 22;
constexpr std::uint32_t kBounceMask = 0xF;
constexpr std::uint32_t kBounceTurn = 9;
constexpr std::int32_t kBounceRiseBase = 16;
constexpr std::int32_t kBounceFallBase = 10;
constexpr int kBounceRiseShift = 1;
constexpr int kBounceFallShift = 2;
constexpr std::int32_t kIconTop = 2;
constexpr std::int32_t kIconSize = 16;
constexpr std::uint32_t kHalfMask = 0xFFFF;
constexpr int kHalfShift = 16;

std::int16_t Half(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(value) & kHalfMask));
}

std::int32_t PackedXy(std::int32_t x, std::int32_t y) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(x) & kHalfMask) |
      (static_cast<std::uint32_t>(y) << kHalfShift));
}

ResultsDrawResult DrawFading(std::span<const Render::TextureRegion> atlas,
                             std::int32_t x, std::int32_t y,
                             std::int32_t texture_index,
                             std::int32_t brightness,
                             std::optional<std::int32_t> clut,
                             ResultsDrawTarget& target) {
  if (brightness <= 0) return ResultsDrawResult::kHidden;
  if (texture_index < 0 ||
      static_cast<std::size_t>(texture_index) >= atlas.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto& texture = atlas[static_cast<std::size_t>(texture_index)];
  const auto quad = FadingTextureQuad(texture, {.x = x, .y = y}, brightness,
                                      clut.has_value() ? *clut : texture.clut);
  if (!quad.has_value()) return ResultsDrawResult::kHidden;
  return target.DrawQuad(*quad) ? ResultsDrawResult::kDrawn
                                : ResultsDrawResult::kFull;
}

// Appends `value` with at least two digits ("%02d").
void AppendByte(std::array<char, kResultsTimeGlyphs + kMaxByteDigits>& out,
                std::size_t& used, std::uint32_t value) {
  std::array<char, kMaxByteDigits> digits{};
  std::size_t count = 0;
  do {
    digits.at(count++) = static_cast<char>('0' + (value % kDecimal));
    value /= kDecimal;
  } while (value != 0);
  if (count < 2) digits.at(count++) = '0';
  while (count != 0 && used < out.size()) out.at(used++) = digits.at(--count);
}
}  // namespace

std::optional<Render::UiTexturedQuad> FadingTextureQuad(
    const Render::TextureRegion& texture, ResultsPoint origin,
    std::int32_t brightness, std::int32_t clut) {
  const auto [x, y] = origin;
  if (brightness <= 0) return std::nullopt;
  Render::UiTexturedQuad quad{
      .top_left = {.x = Half(x), .y = Half(y)},
      .bottom_right = {.x = Half(math::WrappingAdd(x, texture.width)),
                       .y = Half(math::WrappingAdd(y, texture.height))},
      .texture = texture,
      .tint = Render::QuadColor{.r = kResultsFullBrightness,
                                .g = kResultsFullBrightness,
                                .b = kResultsFullBrightness},
      .raw_texture = false,
      .translucent = true};
  quad.texture.clut = static_cast<std::uint16_t>(clut);
  // Still fading in: dimmer, additive, and one palette along.
  if (brightness < kResultsFadeSteps) {
    const auto level = static_cast<std::uint8_t>(
        static_cast<std::uint32_t>(brightness * kResultsFadeBrightnessScale));
    quad.tint = Render::QuadColor{.r = level, .g = level, .b = level};
    quad.texture.texture_page = static_cast<std::uint16_t>(
        texture.texture_page | kResultsAdditiveBlend);
    quad.texture.clut =
        static_cast<std::uint16_t>(quad.texture.clut + kResultsFadePaletteStep);
  }
  return quad;
}

// decomp: MENUF.PRG renderTextureFadeIn (src/MENU/MENUF.PRG/3B8.c:1889; was
// _renderTextureFadeIn).
ResultsDrawResult DrawFadingTexture(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t texture_index, std::int32_t brightness,
    ResultsDrawTarget& target) {
  return DrawFading(atlas, x, y, texture_index, brightness, std::nullopt,
                    target);
}

// decomp: MENUF.PRG renderTextureFadeInClut (src/MENU/MENUF.PRG/3B8.c:1926; was
// _renderTextureFadeInClut).
ResultsDrawResult DrawFadingTextureClut(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t texture_index, std::int32_t brightness,
    std::int32_t clut, ResultsDrawTarget& target) {
  return DrawFading(atlas, x, y, texture_index, brightness, clut, target);
}

std::array<char, kResultsTimeGlyphs> TimeGlyphs(std::int32_t time) {
  std::array<char, kResultsTimeGlyphs> glyphs{};
  if (time == kResultsNoTime) {
    glyphs.fill('-');
    return glyphs;
  }
  const auto bits = static_cast<std::uint32_t>(time);
  std::array<char, kResultsTimeGlyphs + kMaxByteDigits> text{};
  std::size_t used = 0;
  AppendByte(text, used, (bits >> kTimeHighShift) & kTimeByteMask);
  AppendByte(text, used, (bits >> kTimeMiddleShift) & kTimeByteMask);
  AppendByte(text, used, bits & kTimeByteMask);
  std::copy_n(text.begin(), glyphs.size(), glyphs.begin());
  return glyphs;
}

// decomp: MENUF.PRG renderTime (src/MENU/MENUF.PRG/3B8.c:1963; was
// _renderTime).
ResultsDrawResult DrawTime(std::span<const Render::TextureRegion> atlas,
                           std::int32_t x, std::int32_t y, std::int32_t timer,
                           std::int32_t time, bool is_new_record,
                           ResultsDrawTarget& target) {
  if (atlas.size() <= kResultsTimeDash) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const std::int32_t clut =
      is_new_record ? kResultsNewRecordClut : kResultsTimeClut;
  auto result = ResultsDrawResult::kHidden;
  const auto draw = [&](std::int32_t at_x, std::int32_t at_y,
                        std::size_t index) {
    const auto drawn = DrawFadingTextureClut(atlas, at_x, at_y,
                                             static_cast<std::int32_t>(index),
                                             timer, clut, target);
    if (drawn == ResultsDrawResult::kFull) {
      result = ResultsDrawResult::kFull;
    } else if (drawn == ResultsDrawResult::kDrawn &&
               result == ResultsDrawResult::kHidden) {
      result = ResultsDrawResult::kDrawn;
    }
  };
  const auto glyphs = TimeGlyphs(time);
  for (std::size_t i = 0; i < glyphs.size(); ++i) {
    const char glyph = glyphs.at(i);
    if (glyph == '-') {
      draw(x, y, kResultsTimeDash);
    } else {
      draw(x, math::WrappingSubtract(y, kDigitRise),
           static_cast<std::size_t>(glyph - '0'));
    }
    x = math::WrappingAdd(x, kGlyphAdvance);
    if (i == kPrimeAfter || i == kDoublePrimeAfter) {
      draw(x, math::WrappingSubtract(y, kPrimeRise),
           i == kPrimeAfter ? kResultsTimePrime : kResultsTimeDoublePrime);
      x = math::WrappingAdd(x, kPrimeAdvance);
    }
  }
  return result;
}

std::optional<PressButtonPromptLayout> PressButtonPrompt(ResultsPoint origin,
                                                         std::int32_t slide,
                                                         std::int32_t bounce) {
  auto [x, y] = origin;
  if (slide < 0) return std::nullopt;
  if (slide > kPromptMaxSlide) slide = kPromptMaxSlide;
  x = math::WrappingSubtract(math::WrappingAdd(x, kPromptOffset),
                             slide * kPromptSlideStep);
  const auto text_x = math::WrappingAdd(x, kPromptTextIndent);
  PressButtonPromptLayout layout{
      .press_xy = PackedXy(text_x, y),
      .button_xy = PackedXy(text_x, math::WrappingAdd(y, kPromptLineHeight))};
  y = math::WrappingAdd(y, kPromptIconDrop);
  const auto step = static_cast<std::uint32_t>(bounce) & kBounceMask;
  // Half a pixel a step up for nine steps, then a quarter back down.
  if (step < kBounceTurn) {
    y = math::WrappingAdd(math::WrappingSubtract(y, kBounceRiseBase),
                          static_cast<std::int32_t>(step >> kBounceRiseShift));
  } else {
    y = math::WrappingSubtract(
        math::WrappingSubtract(y, kBounceFallBase),
        static_cast<std::int32_t>(step >> kBounceFallShift));
  }
  const auto top = math::WrappingSubtract(y, kIconTop);
  layout.icon = Render::UiTexturedQuad{
      .top_left = {.x = Half(x), .y = Half(top)},
      .bottom_right = {.x = Half(math::WrappingAdd(x, kIconSize)),
                       .y = Half(math::WrappingAdd(top, kIconSize))},
      .texture = kPromptButtonTexture};
  return layout;
}

// decomp: MENUF.PRG renderPressButtonPrompt (src/MENU/MENUF.PRG/3B8.c:2128; was
// _renderPressButtonPrompt).
ResultsDrawResult DrawPressButtonPrompt(std::int32_t x, std::int32_t y,
                                        std::int32_t slide, std::int32_t bounce,
                                        ResultsDrawTarget& target) {
  const auto layout = PressButtonPrompt({.x = x, .y = y}, slide, bounce);
  if (!layout.has_value()) return ResultsDrawResult::kHidden;
  target.DrawText(kPromptPressText, layout->press_xy, kPromptTextColor);
  target.DrawText(kPromptButtonText, layout->button_xy, kPromptTextColor);
  return target.DrawQuad(layout->icon) ? ResultsDrawResult::kDrawn
                                       : ResultsDrawResult::kFull;
}
}  // namespace VS::Menu::Results
