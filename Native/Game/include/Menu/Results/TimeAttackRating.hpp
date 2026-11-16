#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENUF.PRG time-attack results: the "TIME ATTACK RESULTS" heading, the rating
// chosen from the completed time (faded or wiped in), the completed time
// (shown or counting up), the "NEW RECORD" label and the course's three
// ranked records (F03032-F03039). The draws turn atlas widths, a reveal timer,
// the completed time and the course's trial scores into flash-fade, fade,
// wipe and time draws for a TimeAttackResultsDrawHost, so the cores reach no
// global state.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Render/TextureRegionDraw.hpp"

namespace VS::Menu::Results {
// Atlas entries (disIndexTime0*, 3B8.c).
inline constexpr std::size_t kTimeAttackNewRecordLabel = 69;
// "TIME ATTACK" and "RESULTS".
inline constexpr std::size_t kTimeAttackResultsHeading = 70;
inline constexpr std::size_t kTimeAttackTimeLabel = 72;
// "1ST", "2ND" and "3RD".
inline constexpr std::size_t kTimeAttackFirstPlaceLabel = 75;
// The five ratings, best first.
inline constexpr std::size_t kTimeAttackFirstRating = 78;
inline constexpr std::size_t kTimeAttackRatings = 5;

// A time is minutes << 16 | seconds << 8 | hundredths. The rating improves
// below 30 seconds, one minute, 1:30 and two minutes (signed comparisons on
// the packed word; each byte may exceed its unit).
inline constexpr std::int32_t kTimeAttackExcellentBelow = 0x1E00;
inline constexpr std::int32_t kTimeAttackSecondRatingMost = 0xFFFF;
inline constexpr std::int32_t kTimeAttackThirdRatingMost = 0x11DFF;
inline constexpr std::int32_t kTimeAttackFourthRatingMost = 0x1FFFF;

// The rating wipe's edge moves eight pixels a step from x.
inline constexpr std::int32_t kTimeAttackRatingWipeStride = 8;

// The counting time stays at zero for 16 steps, then rises linearly to the
// completed time over the remaining 48.
inline constexpr std::int32_t kTimeAttackCountingDelay = 16;
inline constexpr std::int32_t kTimeAttackCountingSteps = 48;

// Each course keeps three ranked trial scores, fastest first.
inline constexpr std::size_t kTimeAttackRanks = 3;
using TrialScoreRow = std::array<std::uint32_t, kTimeAttackRanks>;

// The time-attack draws plus the rating wipe and the ranked time.
class TimeAttackResultsDrawHost : public TimeAttackDrawHost {
 public:
  // _renderTextureWipe.
  virtual void DrawWipe(ResultsPoint origin, std::size_t texture_index,
                        const TimeAttackColors& colors,
                        std::int32_t wipe_edge_x) = 0;
  // _renderTime, in the new-record palette when `is_new_record`.
  virtual void DrawRankedTime(ResultsPoint origin, std::int32_t timer,
                              std::int32_t time, bool is_new_record) = 0;
};

// The rating (0 best to 4) for the completed `time`.
[[nodiscard]] std::size_t TimeAttackRatingFor(std::int32_t time);

// `time` scaled by the steps of `timer` past 16 out of 48 and renormalised
// to minutes, seconds and hundredths; zero below 16. `timer` is clamped to
// [0, 64] first.
[[nodiscard]] std::int32_t CountingTimeAttackTime(std::int32_t time,
                                                  std::int32_t timer);

// "TIME ATTACK" and "RESULTS" flashed in side by side with the same neutral
// colors, centred on `x`. kHidden at a timer of zero or less; kInvalidTexture
// for an atlas without the heading.
ResultsDrawResult DrawTimeAttackResultsHeading(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, TimeAttackResultsDrawHost& host);

// The rating for `time` faded in at (x, y).
ResultsDrawResult DrawTimeAttackRating(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host);

// The rating for `time` wiped in from x, its edge eight pixels further each
// step.
ResultsDrawResult DrawTimeAttackRatingWipe(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host);

// "TIME" faded in, then `time` after it.
ResultsDrawResult DrawTimeAttackTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host);

// "TIME" faded in, then CountingTimeAttackTime(time, timer) after it.
ResultsDrawResult DrawCountingTimeAttackTime(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t timer, std::int32_t time,
    TimeAttackResultsDrawHost& host);

// "NEW RECORD" faded in at (x, y).
ResultsDrawResult DrawNewRecord(std::span<const Render::TextureRegion> atlas,
                                std::int32_t x, std::int32_t y,
                                std::int32_t timer,
                                TimeAttackResultsDrawHost& host);

// One ranked record of `course` from `course_scores` (a row per course): the
// time right of x - 84 and the place label ending at x - 88, 20 pixels a rank
// down from y, both in the new-record palette when `slot` is `new_slot`.
// kHidden at a timer of zero or less; a slot outside the three ranks, a course
// without scores or an atlas without the place labels refuses with
// kInvalidTexture before drawing.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
ResultsDrawResult DrawLeaderboardEntry(
    std::span<const Render::TextureRegion> atlas, std::int32_t x,
    std::int32_t y, std::int32_t slot, std::int32_t timer, std::uint8_t course,
    std::int32_t new_slot, std::span<const TrialScoreRow> course_scores,
    TimeAttackResultsDrawHost& host);

// The three ranked records of `course` in rank order, as DrawLeaderboardEntry.
ResultsDrawResult DrawLeaderboard(std::span<const Render::TextureRegion> atlas,
                                  std::int32_t x, std::int32_t y,
                                  std::int32_t timer, std::uint8_t course,
                                  std::int32_t new_slot,
                                  std::span<const TrialScoreRow> course_scores,
                                  TimeAttackResultsDrawHost& host);
}  // namespace VS::Menu::Results
