// Hand-maintained (tools/port/owned.csv).
#include "Menu/Results/TimeAttackRating.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include "Core/Math/ScalarMath.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
namespace {
namespace math = Core::Math;

constexpr std::int32_t kTimeDrop = 2;
constexpr std::int32_t kRankSpacing = 20;
constexpr std::int32_t kRankTimeInset = 84;
constexpr std::int32_t kRankLabelGap = 88;

constexpr std::int32_t kHundredthsPerSecond = 100;
constexpr std::int32_t kHundredthsPerMinute = 6000;
constexpr int kMinutesShift = 16;
constexpr int kSecondsShift = 8;
constexpr std::int32_t kTimeByte = 0xFF;

constexpr std::size_t kHeadingAtlasEntries = kTimeAttackResultsHeading + 2;
constexpr auto kRatingEntries = kTimeAttackFirstRating + kTimeAttackRatings;
constexpr std::size_t kTimeAtlasEntries = kTimeAttackTimeLabel + 1;
constexpr std::size_t kNewRecordAtlasEntries = kTimeAttackNewRecordLabel + 1;
constexpr auto kRankEntries = kTimeAttackFirstPlaceLabel + kTimeAttackRanks;

// The heading's and the wipe's static CVECTORs (3B8.c:1656, 1710): neutral
// grey. Only the heading's first step is ever set.
constexpr std::uint8_t kNeutral = 128;
constexpr Render::RegionColor kNeutralColor{kNeutral, kNeutral, kNeutral, 0};

[[nodiscard]] constexpr std::int32_t ClampTimer(std::int32_t timer) {
  return std::clamp(timer, 0, kTimeAttackRevealSteps);
}

[[nodiscard]] std::int32_t Width(std::span<const Render::TextureRegion> atlas,
                                 std::size_t index) {
  return atlas[index].width;
}

// The time label and the time (counted up when `counts`) two pixels lower
// right after it.
ResultsDrawResult DrawLabelledTime(std::span<const Render::TextureRegion> atlas,
                                   std::int32_t x, std::int32_t y,
                                   std::int32_t timer, std::int32_t time,
                                   bool counts,
                                   TimeAttackResultsDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kTimeAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  if (counts) time = CountingTimeAttackTime(time, timer);
  host.DrawFade({.x = x, .y = y}, kTimeAttackTimeLabel, timer);
  host.DrawTime({.x = math::WrappingAdd(x, Width(atlas, kTimeAttackTimeLabel)),
                 .y = math::WrappingAdd(y, kTimeDrop)},
                timer, time);
  return ResultsDrawResult::kDrawn;
}

// One leaderboard row after the checks; `score` is the packed trial score.
void DrawRankRow(std::span<const Render::TextureRegion> atlas, std::int32_t x,
                 std::int32_t y, std::size_t slot, std::int32_t timer,
                 bool is_new_record, std::uint32_t score,
                 TimeAttackResultsDrawHost& host) {
  const auto row_y = math::WrappingAdd(
      y, math::WrappingMultiply(static_cast<std::int32_t>(slot), kRankSpacing));
  host.DrawRankedTime({.x = math::WrappingSubtract(x, kRankTimeInset),
                       .y = math::WrappingAdd(row_y, kTimeDrop)},
                      timer,
                      static_cast<std::int32_t>(score & kTrialScoreTimeMask),
                      is_new_record);
  const std::size_t label = kTimeAttackFirstPlaceLabel + slot;
  host.DrawFadeClut(
      {.x = math::WrappingSubtract(
           math::WrappingSubtract(x, Width(atlas, label)), kRankLabelGap),
       .y = row_y},
      label, timer, is_new_record ? kResultsNewRecordClut : kResultsTimeClut);
}
}  // namespace

std::size_t TimeAttackRatingFor(std::int32_t time) {
  // The highest packed time of each rating but the last, best first.
  static constexpr std::array<std::int32_t, kTimeAttackRatings - 1> kMost{
      kTimeAttackExcellentBelow - 1, kTimeAttackSecondRatingMost,
      kTimeAttackThirdRatingMost, kTimeAttackFourthRatingMost};
  return static_cast<std::size_t>(std::ranges::count_if(
      kMost, [time](std::int32_t most) { return time > most; }));
}

std::int32_t CountingTimeAttackTime(std::int32_t time, std::int32_t timer) {
  timer = ClampTimer(timer);
  if (timer < kTimeAttackCountingDelay) return 0;
  // At most 255 * 6000 + 255 * 100 + 255 = 1555755 hundredths, times 48:
  // the product never leaves 32 bits.
  const std::int32_t total =
      ((time >> kMinutesShift) & kTimeByte) * kHundredthsPerMinute +
      ((time >> kSecondsShift) & kTimeByte) * kHundredthsPerSecond +
      (time & kTimeByte);
  std::int32_t shown =
      total * (timer - kTimeAttackCountingDelay) / kTimeAttackCountingSteps;
  const std::int32_t minutes = shown / kHundredthsPerMinute;
  shown %= kHundredthsPerMinute;
  const std::int32_t seconds = shown / kHundredthsPerSecond;
  shown %= kHundredthsPerSecond;
  return (minutes << kMinutesShift) | (seconds << kSecondsShift) | shown;
}

// decomp: MENUF.PRG _renderTimeAttackResultsHeader
// (src/MENU/MENUF.PRG/3B8.c:1654), 0x8010559C
ResultsDrawResult DrawTimeAttackResultsHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, TimeAttackResultsDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kHeadingAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  // Both halves draw with the same static pair; only its first step is set.
  auto first = kNeutralColor;
  first.step = static_cast<std::uint8_t>(timer);
  const TimeAttackColors colors{first, kNeutralColor};
  const auto left = Width(atlas, kTimeAttackResultsHeading);
  x = math::WrappingSubtract(
      x, (left + Width(atlas, kTimeAttackResultsHeading + 1)) >> 1);
  host.DrawFlashFade({.x = x, .y = y}, kTimeAttackResultsHeading, colors);
  host.DrawFlashFade({.x = math::WrappingAdd(x, left), .y = y},
                     kTimeAttackResultsHeading + 1, colors);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderTimeAttackRating (src/MENU/MENUF.PRG/3B8.c:1678),
// 0x8010564C
ResultsDrawResult DrawTimeAttackRating(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kRatingEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawFade({.x = x, .y = y},
                kTimeAttackFirstRating + TimeAttackRatingFor(time), timer);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderTimeAttackRatingWipe
// (src/MENU/MENUF.PRG/3B8.c:1708), 0x801056E8
ResultsDrawResult DrawTimeAttackRatingWipe(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kRatingEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawWipe({.x = x, .y = y},
                kTimeAttackFirstRating + TimeAttackRatingFor(time),
                {kNeutralColor, kNeutralColor},
                math::WrappingAdd(x, timer * kTimeAttackRatingWipeStride));
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderHeaderAndTime (src/MENU/MENUF.PRG/3B8.c:1743),
// 0x80105790
ResultsDrawResult DrawTimeAttackTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host) {
  return DrawLabelledTime(atlas, x, y, timer, time, false, host);
}

// decomp: MENUF.PRG _renderHeaderAndIncrementalTime
// (src/MENU/MENUF.PRG/3B8.c:1762), 0x8010581C
ResultsDrawResult DrawCountingTimeAttackTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host) {
  return DrawLabelledTime(atlas, x, y, timer, time, true, host);
}

// decomp: MENUF.PRG _renderNewRecord (src/MENU/MENUF.PRG/3B8.c:1801),
// 0x801059B8
ResultsDrawResult DrawNewRecord(std::span<const Render::TextureRegion> atlas,
                                std::int32_t x, std::int32_t y,
                                std::int32_t timer,
                                TimeAttackResultsDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kNewRecordAtlasEntries) {
    return ResultsDrawResult::kInvalidTexture;
  }
  host.DrawFade({.x = x, .y = y}, kTimeAttackNewRecordLabel, timer);
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderAllStoredTimes (src/MENU/MENUF.PRG/3B8.c:1816),
// 0x801059FC
ResultsDrawResult DrawLeaderboard(std::span<const Render::TextureRegion> atlas,
                                  std::int32_t x, std::int32_t y,
                                  std::int32_t timer, std::uint8_t course,
                                  std::int32_t new_slot,
                                  std::span<const TrialScoreRow> course_scores,
                                  TimeAttackResultsDrawHost& host) {
  // The original draws all three rows even at timer 0; every fade is then
  // invisible.
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kRankEntries || course >= course_scores.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto& scores = course_scores[course];
  for (std::size_t slot = 0; slot < kTimeAttackRanks; ++slot) {
    DrawRankRow(atlas, x, y, slot, timer, std::cmp_equal(new_slot, slot),
                scores.at(slot), host);
  }
  return ResultsDrawResult::kDrawn;
}

// decomp: MENUF.PRG _renderStoredTime (src/MENU/MENUF.PRG/3B8.c:1849),
// 0x80105B30
ResultsDrawResult DrawLeaderboardEntry(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t slot, std::int32_t timer, std::uint8_t course,
    std::int32_t new_slot, std::span<const TrialScoreRow> course_scores,
    TimeAttackResultsDrawHost& host) {
  timer = ClampTimer(timer);
  if (timer <= 0) return ResultsDrawResult::kHidden;
  if (atlas.size() < kRankEntries || slot < 0 ||
      std::cmp_greater_equal(slot, kTimeAttackRanks) ||
      course >= course_scores.size()) {
    return ResultsDrawResult::kInvalidTexture;
  }
  const auto rank = static_cast<std::size_t>(slot);
  DrawRankRow(atlas, x, y, rank, timer, new_slot == slot,
              course_scores[course].at(rank), host);
  return ResultsDrawResult::kDrawn;
}
}  // namespace VS::Menu::Results
