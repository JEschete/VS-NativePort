// Hand-maintained (tools/port/owned.csv).
// Binds the typed time-attack results draws (TimeAttackRating.cpp) to
// kDisMap, the completed time, the new record's rank, the
// time-attack course and its trial scores and the live atlas draws.
#include "Menu/Results/TimeAttackRatingLive.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

#include "Core/Log.hpp"
#include "Legacy/Psx.hpp"
#include "Menu/Results/Results.hpp"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/TimeAttack.hpp"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Menu/Results/TimeAttackHeadingLive.hpp"
#include "Menu/Results/TimeAttackRating.hpp"
#include "Render/Images.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

using VS::Core::Gte::CVECTOR;

namespace VS::Menu::Results {
namespace {
constexpr std::uint32_t kTrialRoundShift = 24;
constexpr std::uint32_t kTrialDifficultyShift = 31;
static_assert(std::size(VS::State::scoreData.boss_time_trial_scores) ==
              kTimeAttackCourses);
static_assert(std::size(VS::State::scoreData.boss_time_trial_scores[0]) ==
              kTimeAttackRanks);

std::uint64_t refusals = 0;

void Report(ResultsDrawResult result) {
  if (result == ResultsDrawResult::kInvalidTexture &&
      std::has_single_bit(++refusals)) {
    VS::Core::LogWarn(
        "menu.results",
        "Time-attack results drawing refused a course without trial "
        "scores, a rank outside the leaderboard or a short atlas ({} "
        "refusals)",
        refusals);
  }
}

// The shared time-attack draws plus the live wipe and ranked time.
class LiveResultsHost final : public TimeAttackResultsDrawHost {
 public:
  void DrawFlashFade(ResultsPoint origin, std::size_t texture_index,
                     const TimeAttackColors& colors) override {
    base_.DrawFlashFade(origin, texture_index, colors);
  }
  void DrawFade(ResultsPoint origin, std::size_t texture_index,
                std::int32_t brightness) override {
    base_.DrawFade(origin, texture_index, brightness);
  }
  void DrawFadeClut(ResultsPoint origin, std::size_t texture_index,
                    std::int32_t brightness, std::int32_t clut) override {
    base_.DrawFadeClut(origin, texture_index, brightness, clut);
  }
  void DrawTime(ResultsPoint origin, std::int32_t timer,
                std::int32_t time) override {
    base_.DrawTime(origin, timer, time);
  }
  void DrawWipe(ResultsPoint origin, std::size_t texture_index,
                const TimeAttackColors& colors,
                std::int32_t wipe_edge_x) override {
    const std::array<CVECTOR, 2> vectors{{
        {colors[0].r, colors[0].g, colors[0].b, colors[0].step},
        {colors[1].r, colors[1].g, colors[1].b, colors[1].step},
    }};
    Render::DrawLiveTextureWipe(kDisMap, origin.x, origin.y,
                                static_cast<int>(texture_index), vectors,
                                wipe_edge_x);
  }
  void DrawRankedTime(ResultsPoint origin, std::int32_t timer,
                      std::int32_t time, bool is_new_record) override {
    LiveDrawTime(origin.x, origin.y, timer, time, is_new_record ? 1 : 0);
  }

 private:
  TimeAttackDrawHost& base_ = LiveTimeAttackDrawHost();
};

std::uint8_t LiveCourse() { return VS::State::stateFlags.time_attack_boss; }

// Every course's trial scores, packed as the original words.
std::array<TrialScoreRow, kTimeAttackCourses> LiveCourseScores() {
  std::array<TrialScoreRow, kTimeAttackCourses> rows{};
  for (std::size_t course = 0; course < rows.size(); ++course) {
    for (std::size_t rank = 0; rank < kTimeAttackRanks; ++rank) {
      const VS::State::TrialScore& score =
          VS::State::scoreData.boss_time_trial_scores[course][rank];
      rows.at(course).at(rank) =
          score.time |
          (static_cast<std::uint32_t>(score.round) << kTrialRoundShift) |
          (static_cast<std::uint32_t>(score.difficulty)
           << kTrialDifficultyShift);
    }
  }
  return rows;
}
}  // namespace

// decomp: MENUF.PRG _renderTimeAttackResultsHeader
// (src/MENU/MENUF.PRG/3B8.c:1654), 0x8010559C
void LiveDrawTimeAttackResultsHeading(int x, int y, int timer) {
  LiveResultsHost host;
  Report(DrawTimeAttackResultsHeading(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderTimeAttackRating (src/MENU/MENUF.PRG/3B8.c:1678),
// 0x8010564C
void LiveDrawTimeAttackRating(int x, int y, int fade_timer) {
  LiveResultsHost host;
  Report(DrawTimeAttackRating(LiveResultsAtlas(), x, y, fade_timer,
                              timeTrialTime, host));
}

// decomp: MENUF.PRG _renderTimeAttackRatingWipe
// (src/MENU/MENUF.PRG/3B8.c:1708), 0x801056E8
void LiveDrawTimeAttackRatingWipe(int x, int y, int timer) {
  LiveResultsHost host;
  Report(DrawTimeAttackRatingWipe(LiveResultsAtlas(), x, y, timer,
                                  timeTrialTime, host));
}

// decomp: MENUF.PRG _renderHeaderAndTime (src/MENU/MENUF.PRG/3B8.c:1743),
// 0x80105790
void LiveDrawTimeAttackTime(int x, int y, int fade_timer) {
  LiveResultsHost host;
  Report(DrawTimeAttackTime(LiveResultsAtlas(), x, y, fade_timer, timeTrialTime,
                            host));
}

// decomp: MENUF.PRG _renderHeaderAndIncrementalTime
// (src/MENU/MENUF.PRG/3B8.c:1762), 0x8010581C
void LiveDrawCountingTimeAttackTime(int x, int y, int timer) {
  LiveResultsHost host;
  Report(DrawCountingTimeAttackTime(LiveResultsAtlas(), x, y, timer,
                                    timeTrialTime, host));
}

// decomp: MENUF.PRG _renderNewRecord (src/MENU/MENUF.PRG/3B8.c:1801),
// 0x801059B8
void LiveDrawNewRecord(int x, int y, int timer) {
  LiveResultsHost host;
  Report(DrawNewRecord(LiveResultsAtlas(), x, y, timer, host));
}

// decomp: MENUF.PRG _renderAllStoredTimes (src/MENU/MENUF.PRG/3B8.c:1816),
// 0x801059FC
void LiveDrawLeaderboard(int x, int y, int timer) {
  LiveResultsHost host;
  const auto scores = LiveCourseScores();
  Report(DrawLeaderboard(LiveResultsAtlas(), x, y, timer, LiveCourse(),
                         newTimeSlot, scores, host));
}

// decomp: MENUF.PRG _renderStoredTime (src/MENU/MENUF.PRG/3B8.c:1849),
// 0x80105B30
void LiveDrawLeaderboardEntry(int x, int y, int slot, int timer) {
  LiveResultsHost host;
  const auto scores = LiveCourseScores();
  Report(DrawLeaderboardEntry(LiveResultsAtlas(), x, y, slot, timer,
                              LiveCourse(), newTimeSlot, scores, host));
}
}  // namespace VS::Menu::Results
