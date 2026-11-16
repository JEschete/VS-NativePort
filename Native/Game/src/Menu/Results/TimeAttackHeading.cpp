// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/TimeAttackHeading.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

// The course and best-time layouts centre the label and these many pixels of
// gap and value; the value starts this far past the label.
constexpr std::int32_t kCoursePadding = 24;
constexpr std::int32_t kCourseDigitGap = 12;
constexpr std::int32_t kCourseDigitRise = 1;
constexpr std::int32_t kBestTimePadding = 96;
constexpr std::int32_t kBestTimeGap = 12;
constexpr std::int32_t kBestTimeDrop = 2;

constexpr std::size_t kHeadingAtlasEntries = kTimeAttackHeading + 2;
constexpr std::size_t kCourseAtlasEntries = kTimeAttackCourseLabel + 1;
constexpr std::size_t kPairedNameAtlasEntries = kTimeAttackOgreZombie + 1;
constexpr std::size_t kBestTimeAtlasEntries = kTimeAttackBestTimeLabel + 1;

struct Rgb {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
};

// _renderTimeAttackHeader's static CVECTORs (3B8.c:1148): dark, light, dark.
constexpr Rgb kHeadingDark{.r = 128, .g = 96, .b = 64};
constexpr Rgb kHeadingLight{.r = 200, .g = 180, .b = 160};

[[nodiscard]] constexpr Render::RegionColor WithStep(Rgb color,
                                                     std::int32_t step) {
  return {.r = color.r,
          .g = color.g,
          .b = color.b,
          .step = static_cast<std::uint8_t>(step)};
}

[[nodiscard]] std::int32_t Width(std::span<const Render::TextureRegion> atlas,
                                 std::size_t index) {
  return atlas[index].width;
}
}  // namespace

// decomp: MENUF.PRG _renderTimeAttackHeader (src/MENU/MENUF.PRG/3B8.c:1146),
// 0x8010459C
ResultsDrawResult DrawTimeAttackHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, TimeAttackDrawHost& host) {
  timer = std::clamp(timer, 0, kTimeAttackRevealSteps);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kHeadingAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  // The static colors keep the last timer in the first two steps; the second
  // half draws with &colors[1], whose third color's step is never set.
  const auto dark = WithStep(kHeadingDark, timer);
  const auto light = WithStep(kHeadingLight, timer);
  const auto left = Width(atlas, kTimeAttackHeading);
  x = math::WrappingSubtract(
      x, (left + Width(atlas, kTimeAttackHeading + 1)) >> 1);
  host.DrawFlashFade({.x = x, .y = y}, kTimeAttackHeading, {dark, light});
  host.DrawFlashFade({.x = math::WrappingAdd(x, left), .y = y},
                     kTimeAttackHeading + 1,
                     {light, WithStep(kHeadingDark, 0)});
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderCourse (src/MENU/MENUF.PRG/3B8.c:1175), 0x80104650
ResultsDrawResult DrawCourseNumber(std::span<const Render::TextureRegion> atlas,
                                   std::int32_t x, std::int32_t y,
                                   std::int32_t timer, std::uint8_t course,
                                   TimeAttackDrawHost& host) {
  timer = std::min(timer, kTimeAttackRevealSteps);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  const std::size_t digit = static_cast<std::size_t>(course) + 1;
  if (atlas.size() < kCourseAtlasEntries || digit >= atlas.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto label = Width(atlas, kTimeAttackCourseLabel);
  x = math::WrappingSubtract(x, (label + kCoursePadding) >> 1);
  host.DrawFade({.x = x, .y = y}, kTimeAttackCourseLabel, timer);
  host.DrawFadeClut(
      {.x = math::WrappingAdd(math::WrappingAdd(x, kCourseDigitGap), label),
       .y = math::WrappingSubtract(y, kCourseDigitRise)},
      digit, timer, kTimeAttackCourseClut);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderBossName (src/MENU/MENUF.PRG/3B8.c:1195), 0x801046F8
ResultsDrawResult DrawBossName(std::span<const Render::TextureRegion> atlas,
                               std::int32_t x, std::int32_t y,
                               std::int32_t timer, std::uint8_t course,
                               TimeAttackDrawHost& host) {
  timer = std::min(timer, kTimeAttackRevealSteps);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (course == kTimeAttackPairedCourse) {
    if (atlas.size() < kPairedNameAtlasEntries) {
      return ResultsDrawResult::kInvalidTexture;
    }
    const auto first = Width(atlas, kTimeAttackDeath);
    x = math::WrappingSubtract(
        x, (first + Width(atlas, kTimeAttackOgreZombie)) >> 1);
    host.DrawFade({.x = x, .y = y}, kTimeAttackDeath, timer);
    host.DrawFade({.x = math::WrappingAdd(x, first), .y = y},
                  kTimeAttackOgreZombie, timer);
    return ResultsDrawResult::kDrawn;
  }
  const std::size_t name = kTimeAttackFirstBossName + course;
  if (name >= atlas.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawFade(
      {.x = math::WrappingSubtract(x, Width(atlas, name) >> 1), .y = y}, name,
      timer);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderBestTimeHeader (src/MENU/MENUF.PRG/3B8.c:1226),
// 0x801047D4
ResultsDrawResult DrawBestTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::uint8_t course,
    std::span<const std::uint32_t> first_place_scores,
    TimeAttackDrawHost& host) {
  timer = std::min(timer, kTimeAttackRevealSteps);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kBestTimeAtlasEntries ||
      course >= first_place_scores.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto label = Width(atlas, kTimeAttackBestTimeLabel);
  x = math::WrappingSubtract(x, (label + kBestTimePadding) >> 1);
  host.DrawFade({.x = x, .y = y}, kTimeAttackBestTimeLabel, timer);
  const auto time = static_cast<std::int32_t>(first_place_scores[course] &
                                              kTrialScoreTimeMask);
  host.DrawTime(
      {.x = math::WrappingAdd(math::WrappingAdd(x, kBestTimeGap), label),
       .y = math::WrappingAdd(y, kBestTimeDrop)},
      timer, time);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderStartCountdown (src/MENU/MENUF.PRG/3B8.c:1248),
// 0x8010489C
ResultsDrawResult DrawStartCountdown(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, TimeAttackDrawHost& host) {
  const std::int64_t number =
      static_cast<std::int64_t>(kTimeAttackCountdownThree) +
      timer / kTimeAttackCountdownFrames;
  if (number < 0 || std::cmp_greater_equal(number, atlas.size())) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const std::int32_t brightness = timer % kTimeAttackCountdownFrames;
  if (brightness <= 0) {
    return ResultsDrawResult::kHidden;
  }
  const auto index = static_cast<std::size_t>(number);
  const auto& texture = atlas[index];
  host.DrawFade({.x = math::WrappingSubtract(x, texture.width >> 1),
                 .y = math::WrappingSubtract(y, texture.height >> 1)},
                index, brightness);
  return ResultsDrawResult::kDrawn;
}
}  // namespace VS::Menu::Results
