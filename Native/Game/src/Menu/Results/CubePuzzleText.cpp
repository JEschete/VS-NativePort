// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/CubePuzzleText.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

constexpr std::int32_t kSecondsPerMinute = 60;
constexpr std::int32_t kHundredthsPerSecond = 100;
// kSecondsPerMinute * kHundredthsPerSecond.
constexpr std::int32_t kHundredthsPerMinute = 6000;
// Packed times keep minutes in bits 16-23, seconds in 8-15 and hundredths in
// 0-7.
constexpr std::int32_t kMinutesShift = 16;
constexpr std::int32_t kSecondsShift = 8;

// The highest fixed atlas entry each draw reads, plus one.
constexpr std::size_t kGivingUpAtlasEntries = kCubeTextGivingUp + 2;
constexpr std::size_t kChickenAtlasEntries = kCubeTextChicken + 2;
constexpr std::size_t kEvolveAtlasEntries = kCubeTextEvolveOrDie + 2;
constexpr std::size_t kFoodChainAtlasEntries = kCubeTextFoodChain + 2;
constexpr std::size_t kAverageAtlasEntries = kCubeTextAverageTime + 1;
constexpr std::size_t kTimeLabelAtlasEntries = kCubeTextTimeLabel + 1;

// The functions' static CVECTOR colors (3B8.c:2642, 2666, 2714) with their
// initial zero step. Each function stores the clamped timer in the steps it
// uses; the rest stay zero.
constexpr Render::RegionColor kBannerDark{.r = 128, .g = 96, .b = 64};
constexpr Render::RegionColor kBannerLight{.r = 200, .g = 180, .b = 160};
constexpr Render::RegionColor kRankGrey{.r = 128, .g = 128, .b = 128};

[[nodiscard]] constexpr Render::RegionColor Stepped(Render::RegionColor color,
                                                    std::int32_t step) {
  color.step = static_cast<std::uint8_t>(step);
  return color;
}

[[nodiscard]] constexpr std::int32_t ClampTimer(std::int32_t timer) {
  return std::clamp(timer, 0, kCubeTextRevealSteps);
}

[[nodiscard]] std::int32_t Width(std::span<const Render::TextureRegion> atlas,
                                 std::size_t index) {
  return atlas[index].width;
}

// Both quit messages: two halves faded in side by side (3B8.c:2610-2638).
ResultsDrawResult DrawQuitMessage(std::span<const Render::TextureRegion> atlas,
                                  std::size_t first, std::size_t entries,
                                  std::int32_t y, std::int32_t timer,
                                  CubePuzzleTextHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < entries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawFadingTexture({.x = 0, .y = y}, first, timer);
  host.DrawFadingTexture({.x = kCubeTextQuitSecondHalfX, .y = y}, first + 1,
                         timer);
  return ResultsDrawResult::kDrawn;
}

// Both banners: two halves flashed in, centred on `x`. The second half draws
// with &colors[1]: the light color, then the dark third color whose step is
// never set (3B8.c:2640-2685).
ResultsDrawResult DrawBanner(std::span<const Render::TextureRegion> atlas,
                             std::size_t first, std::size_t entries,
                             ResultsPoint centre, std::int32_t timer,
                             CubePuzzleTextHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < entries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto dark = Stepped(kBannerDark, timer);
  const auto light = Stepped(kBannerLight, timer);
  const auto left = Width(atlas, first);
  const auto x =
      math::WrappingSubtract(centre.x, (left + Width(atlas, first + 1)) >> 1);
  host.DrawFlashFade({.x = x, .y = centre.y}, first, {dark, light});
  host.DrawFlashFade({.x = math::WrappingAdd(x, left), .y = centre.y},
                     first + 1, {light, kBannerDark});
  return ResultsDrawResult::kDrawn;
}
}  // namespace

std::int32_t PackedParTime(std::int16_t par_seconds) {
  const std::int32_t minutes = par_seconds / kSecondsPerMinute;
  // The seconds are shifted up a half-word and arithmetically back by a byte,
  // so a negative remainder sign-extends over the minutes (3B8.c:2706-2708).
  const std::int32_t seconds = par_seconds % kSecondsPerMinute;
  const auto packed_minutes = static_cast<std::uint32_t>(minutes)
                              << kMinutesShift;
  const std::int32_t packed_seconds = seconds * (1 << kSecondsShift);
  return static_cast<std::int32_t>(packed_minutes |
                                   static_cast<std::uint32_t>(packed_seconds));
}

std::int32_t CountedCubePuzzleTime(std::int32_t timer, CubePuzzleClock clock) {
  if (timer < kCubeTimeCountingStart) {
    return 0;
  }
  // At most 255 * 6000 + 255 * 100 + 255 times 48: no 32-bit overflow.
  const std::int32_t total = (clock.minutes * kHundredthsPerMinute) +
                             (clock.seconds * kHundredthsPerSecond) +
                             clock.hundredths;
  const std::int32_t shown =
      total * (timer - kCubeTimeCountingStart) / kCubeTimeCountingSteps;
  const std::int32_t minutes = shown / kHundredthsPerMinute;
  const std::int32_t rest = shown % kHundredthsPerMinute;
  const std::int32_t seconds = rest / kHundredthsPerSecond;
  const std::int32_t hundredths = rest % kHundredthsPerSecond;
  return (minutes << kMinutesShift) | (seconds << kSecondsShift) | hundredths;
}

// decomp: MENUF.PRG _renderGivingUp (src/MENU/MENUF.PRG/3B8.c:2610), 0x801084F4
ResultsDrawResult DrawGivingUp(std::span<const Render::TextureRegion> atlas,
                               std::int32_t y, std::int32_t timer,
                               CubePuzzleTextHost& host) {
  return DrawQuitMessage(atlas, kCubeTextGivingUp, kGivingUpAtlasEntries, y,
                         timer, host);
}

// decomp: MENUF.PRG _renderChicken (src/MENU/MENUF.PRG/3B8.c:2624), 0x80108564
ResultsDrawResult DrawChicken(std::span<const Render::TextureRegion> atlas,
                              std::int32_t y, std::int32_t timer,
                              CubePuzzleTextHost& host) {
  return DrawQuitMessage(atlas, kCubeTextChicken, kChickenAtlasEntries, y,
                         timer, host);
}

// decomp: MENUF.PRG _renderEvolveOrDie (src/MENU/MENUF.PRG/3B8.c:2640),
// 0x801085D4
ResultsDrawResult DrawEvolveOrDie(std::span<const Render::TextureRegion> atlas,
                                  std::int32_t x, std::int32_t y,
                                  std::int32_t timer,
                                  CubePuzzleTextHost& host) {
  return DrawBanner(atlas, kCubeTextEvolveOrDie, kEvolveAtlasEntries,
                    {.x = x, .y = y}, timer, host);
}

// decomp: MENUF.PRG _renderFoodchain (src/MENU/MENUF.PRG/3B8.c:2664),
// 0x80108688
ResultsDrawResult DrawFoodChain(std::span<const Render::TextureRegion> atlas,
                                std::int32_t x, std::int32_t y,
                                std::int32_t timer, CubePuzzleTextHost& host) {
  return DrawBanner(atlas, kCubeTextFoodChain, kFoodChainAtlasEntries,
                    {.x = x, .y = y}, timer, host);
}

// decomp: MENUF.PRG _renderAverageTimeHeader (src/MENU/MENUF.PRG/3B8.c:2687),
// 0x8010873C
ResultsDrawResult DrawAverageTimeHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, CubePuzzleTextHost& host) {
  // Only the upper bound is clamped; a negative timer still draws nothing.
  timer = std::min(timer, kCubeTextRevealSteps);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kAverageAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawFadingTexture(
      {.x = math::WrappingSubtract(x, Width(atlas, kCubeTextAverageTime) >> 1),
       .y = y},
      kCubeTextAverageTime, timer);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderCubePuzzleTime (src/MENU/MENUF.PRG/3B8.c:2699),
// 0x80108784
ResultsDrawResult DrawCubePuzzleParTime(std::int32_t x, std::int32_t y,
                                        std::int32_t timer,
                                        std::int16_t par_seconds,
                                        CubePuzzleTextHost& host) {
  timer = std::min(timer, kCubeTextRevealSteps);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  host.DrawTime({.x = x, .y = y}, timer, PackedParTime(par_seconds));
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderCubePuzzleRank (src/MENU/MENUF.PRG/3B8.c:2712),
// 0x8010880C
ResultsDrawResult DrawCubePuzzleRank(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
    std::int32_t y, std::int32_t timer, std::int32_t rank,
    CubePuzzleTextHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  // The original reads _disMap[rank + 107] for any rank; the title must lie
  // inside the atlas.
  const auto index =
      math::WrappingAdd(rank, static_cast<std::int32_t>(kCubeTextFirstRank));
  if (index < 0 || static_cast<std::size_t>(index) >= atlas.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto title = static_cast<std::size_t>(index);
  host.DrawFlashFade(
      {.x = math::WrappingSubtract(x, Width(atlas, title) >> 1), .y = y}, title,
      {Stepped(kRankGrey, timer), kRankGrey});
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderCubePuzzleIncrementalTime
// (src/MENU/MENUF.PRG/3B8.c:2732), 0x8010887C
ResultsDrawResult DrawCountingCubePuzzleTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, CubePuzzleClock clock,
    CubePuzzleTextHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) {
    return ResultsDrawResult::kHidden;
  }
  if (atlas.size() < kTimeLabelAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawFadingTexture({.x = x, .y = y}, kCubeTextTimeLabel, timer);
  host.DrawTime({.x = math::WrappingAdd(x, Width(atlas, kCubeTextTimeLabel)),
                 .y = math::WrappingAdd(y, kCubeTimeDrop)},
                timer, CountedCubePuzzleTime(timer, clock));
  return ResultsDrawResult::kDrawn;
}
}  // namespace VS::Menu::Results
