#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Audio/Seq/Channel.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/TimeAttack.h"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Menu/Results/TimeAttackRating.hpp"
#include "Menu/Results/TimeAttackRatingLive.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "State/Progress.h"
#include "State/StateFlags.h"
#include "TestSupport/TimeAttackDrawRecorder.hpp"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;
using VS::test_support::time_attack::Kind;
using VS::test_support::time_attack::LiveTimeAttackFixture;
using VS::test_support::time_attack::RecordHost;
using VS::test_support::time_attack::Replay;

constexpr std::size_t kAtlasEntries = 127;

std::vector<render::TextureRegion> Atlas() {
  std::vector<render::TextureRegion> atlas(kAtlasEntries,
                                           {.width = 16, .height = 16});
  atlas[r::kTimeAttackResultsHeading].width = 90;
  atlas[r::kTimeAttackResultsHeading + 1].width = 71;
  atlas[r::kTimeAttackTimeLabel].width = 37;
  atlas[r::kTimeAttackFirstPlaceLabel].width = 24;
  atlas[r::kTimeAttackFirstPlaceLabel + 1].width = 29;
  atlas[r::kTimeAttackFirstPlaceLabel + 2].width = 27;
  return atlas;
}

std::span<const render::TextureRegion> Short(
    const std::vector<render::TextureRegion>& atlas, std::size_t entries) {
  return std::span<const render::TextureRegion>(atlas).first(entries);
}

constexpr std::int32_t Time(std::int32_t minutes, std::int32_t seconds,
                            std::int32_t hundredths) {
  return (minutes << 16) | (seconds << 8) | hundredths;
}

using Scores = std::array<r::TrialScoreRow, r::kTimeAttackCourses>;

Scores CourseScores() {
  Scores scores{};
  for (std::size_t course = 0; course < scores.size(); ++course) {
    for (std::size_t rank = 0; rank < r::kTimeAttackRanks; ++rank) {
      scores.at(course).at(rank) =
          static_cast<std::uint32_t>(Time(static_cast<std::int32_t>(course),
                                          static_cast<std::int32_t>(rank) + 1,
                                          7)) |
          (3U << 24) | (rank == 1 ? 0x80000000U : 0U);
    }
  }
  return scores;
}

// ---------------------------------------------------------------- F03032

TEST_CASE("F03032 results heading centres both halves on x",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackResultsHeading(atlas, 160, 20, 12, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  // (90 + 71) >> 1 = 80.
  CHECK(host.calls[0].At(80, 20, r::kTimeAttackResultsHeading));
  CHECK(host.calls[1].At(170, 20, r::kTimeAttackResultsHeading + 1));
  for (const auto& call : host.calls) {
    CHECK(call.kind == Kind::kFlash);
    CHECK(call.colors[0].r == 128);
    CHECK(call.colors[0].step == 12);
    CHECK(call.colors[1].b == 128);
    CHECK(call.colors[1].step == 0);
  }
}

TEST_CASE("F03032 results heading clamps the timer and hides at zero",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackResultsHeading(atlas, 160, 20, 500, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].colors[0].step == 64);
  host.calls.clear();
  CHECK(r::DrawTimeAttackResultsHeading(atlas, 160, 20, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawTimeAttackResultsHeading(atlas, 160, 20, -9, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

TEST_CASE("F03032 results heading refuses an atlas without the second half",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackResultsHeading(
            Short(atlas, r::kTimeAttackResultsHeading + 1), 160, 20, 12,
            host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03033

TEST_CASE("F03033 rating changes at 30 seconds, 1, 1.5 and 2 minutes",
          "[timeattack-rating]") {
  CHECK(r::TimeAttackRatingFor(0) == 0);
  CHECK(r::TimeAttackRatingFor(Time(0, 29, 255)) == 0);
  CHECK(r::TimeAttackRatingFor(Time(0, 30, 0)) == 1);
  CHECK(r::TimeAttackRatingFor(Time(0, 255, 255)) == 1);
  CHECK(r::TimeAttackRatingFor(Time(1, 0, 0)) == 2);
  CHECK(r::TimeAttackRatingFor(Time(1, 29, 255)) == 2);
  CHECK(r::TimeAttackRatingFor(Time(1, 30, 0)) == 3);
  CHECK(r::TimeAttackRatingFor(Time(1, 255, 255)) == 3);
  CHECK(r::TimeAttackRatingFor(Time(2, 0, 0)) == 4);
  // The comparisons are signed: a negative word rates best.
  CHECK(r::TimeAttackRatingFor(-1) == 0);
}

TEST_CASE("F03033 rating fades in the chosen rating entry",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackRating(atlas, 200, 130, 70, Time(1, 45, 3), host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].kind == Kind::kFade);
  CHECK(host.calls[0].At(200, 130, r::kTimeAttackFirstRating + 3));
  CHECK(host.calls[0].brightness == 64);
}

TEST_CASE("F03033 rating hides at zero and refuses a short atlas",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackRating(atlas, 200, 130, 0, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawTimeAttackRating(
            Short(atlas, r::kTimeAttackFirstRating + r::kTimeAttackRatings - 1),
            200, 130, 5, 0, host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03034

TEST_CASE("F03034 rating wipe moves its edge eight pixels a step",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackRatingWipe(atlas, 100, 140, 9, Time(0, 59, 99),
                                    host) == r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].kind == Kind::kWipe);
  CHECK(host.calls[0].At(100, 140, r::kTimeAttackFirstRating + 1));
  CHECK(host.calls[0].edge == 172);
  for (const auto& color : host.calls[0].colors) {
    CHECK(color.r == 128);
    CHECK(color.g == 128);
    CHECK(color.step == 0);
  }
}

TEST_CASE("F03034 rating wipe stops its edge at 64 steps",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackRatingWipe(atlas, 100, 140, 1000, Time(3, 0, 0),
                                    host) == r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].index == r::kTimeAttackFirstRating + 4);
  CHECK(host.calls[0].edge == 100 + (64 * 8));
}

TEST_CASE("F03034 rating wipe hides at zero and refuses a short atlas",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackRatingWipe(atlas, 100, 140, -1, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawTimeAttackRatingWipe(Short(atlas, r::kTimeAttackFirstRating),
                                    100, 140, 5, 0, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03035

TEST_CASE("F03035 time fades the label in at the origin",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackTime(atlas, 60, 90, 20, Time(2, 13, 48), host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].kind == Kind::kFade);
  CHECK(host.calls[0].At(60, 90, r::kTimeAttackTimeLabel));
  CHECK(host.calls[0].brightness == 20);
}

TEST_CASE("F03035 time draws the time two pixels lower after the label",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  const auto time = Time(2, 13, 48);
  r::DrawTimeAttackTime(atlas, 60, 90, 20, time, host);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[1].kind == Kind::kTime);
  CHECK(host.calls[1].x == 97);
  CHECK(host.calls[1].y == 92);
  CHECK(host.calls[1].time == time);
  CHECK(!host.calls[1].record);
}

TEST_CASE("F03035 time clamps its timer for both draws",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackTime(atlas, 60, 90, 99, 1, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].brightness == 64);
  CHECK(host.calls[1].brightness == 64);
}

TEST_CASE("F03035 time hides at zero and refuses an atlas without the label",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackTime(atlas, 60, 90, 0, 1, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawTimeAttackTime(Short(atlas, r::kTimeAttackTimeLabel), 60, 90, 4,
                              1,
                              host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03036

TEST_CASE("F03036 counting time waits 16 steps then rises over 48",
          "[timeattack-rating]") {
  const auto time = Time(1, 30, 60);  // 9060 hundredths
  CHECK(r::CountingTimeAttackTime(time, 15) == 0);
  CHECK(r::CountingTimeAttackTime(time, 16) == 0);
  // 9060 * 24 / 48 = 4530 = 0:45.30.
  CHECK(r::CountingTimeAttackTime(time, 40) == Time(0, 45, 30));
  CHECK(r::CountingTimeAttackTime(time, 64) == time);
  CHECK(r::CountingTimeAttackTime(time, 200) == time);
}

TEST_CASE("F03036 counting time renormalises out-of-range fields",
          "[timeattack-rating]") {
  // 0:99.150 is 9900 + 150 = 10050 hundredths = 1:40.50.
  CHECK(r::CountingTimeAttackTime(Time(0, 99, 150), 64) == Time(1, 40, 50));
  // The largest packed fields stay within 32 bits.
  CHECK(r::CountingTimeAttackTime(Time(255, 255, 255), 64) ==
        Time(259, 17, 55));
}

TEST_CASE("F03036 counting time draws the label and the counted time",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawCountingTimeAttackTime(atlas, 60, 90, 40, Time(1, 30, 60),
                                      host) == r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].At(60, 90, r::kTimeAttackTimeLabel));
  CHECK(host.calls[1].time == Time(0, 45, 30));
  CHECK(host.calls[1].brightness == 40);
  host.calls.clear();
  CHECK(r::DrawCountingTimeAttackTime(atlas, 60, 90, 0, 1, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03037

TEST_CASE("F03037 new record fades in its label", "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawNewRecord(atlas, 230, 170, 31, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].kind == Kind::kFade);
  CHECK(host.calls[0].At(230, 170, r::kTimeAttackNewRecordLabel));
  CHECK(host.calls[0].brightness == 31);
}

TEST_CASE("F03037 new record clamps the timer and hides at zero",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawNewRecord(atlas, 230, 170, 65, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].brightness == 64);
  CHECK(r::DrawNewRecord(atlas, 230, 170, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.size() == 1);
}

TEST_CASE("F03037 new record refuses an atlas without the label",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawNewRecord(Short(atlas, r::kTimeAttackNewRecordLabel), 230, 170,
                         3, host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03038

TEST_CASE("F03038 leaderboard draws the three times in rank order",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  const auto scores = CourseScores();
  RecordHost host;
  CHECK(r::DrawLeaderboard(atlas, 250, 100, 30, 5, -1, scores, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 6);
  for (std::size_t rank = 0; rank < 3; ++rank) {
    const auto& time = host.calls.at(rank * 2);
    CHECK(time.kind == Kind::kTime);
    CHECK(time.At(166, 102 + (static_cast<std::int32_t>(rank) * 20), 0));
    // The round and difficulty bits never reach the time.
    CHECK(time.time == Time(5, static_cast<std::int32_t>(rank) + 1, 7));
    CHECK(!time.record);
  }
}

TEST_CASE("F03038 leaderboard ends each place label 88 pixels left of x",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  const auto scores = CourseScores();
  RecordHost host;
  r::DrawLeaderboard(atlas, 250, 100, 30, 5, -1, scores, host);
  REQUIRE(host.calls.size() == 6);
  for (std::size_t rank = 0; rank < 3; ++rank) {
    const auto& label = host.calls.at((rank * 2) + 1);
    const auto index = r::kTimeAttackFirstPlaceLabel + rank;
    CHECK(label.kind == Kind::kFadeClut);
    CHECK(label.At(250 - atlas[index].width - 88,
                   100 + (static_cast<std::int32_t>(rank) * 20), index));
    CHECK(label.clut == r::kResultsTimeClut);
  }
}

TEST_CASE("F03038 leaderboard highlights only the new record's rank",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  auto scores = CourseScores();
  // Tied times and the dashes sentinel draw as stored.
  scores[2] = {0x00010203U, 0x00010203U, 0x00800000U};
  RecordHost host;
  CHECK(r::DrawLeaderboard(atlas, 250, 100, 64, 2, 1, scores, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 6);
  CHECK(!host.calls[0].record);
  CHECK(host.calls[1].clut == r::kResultsTimeClut);
  CHECK(host.calls[2].record);
  CHECK(host.calls[3].clut == r::kResultsNewRecordClut);
  CHECK(!host.calls[4].record);
  CHECK(host.calls[4].time == 0x800000);
  CHECK(host.calls[0].time == host.calls[2].time);
}

TEST_CASE("F03038 leaderboard refuses a course without scores",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  const auto scores = CourseScores();
  RecordHost host;
  CHECK(r::DrawLeaderboard(atlas, 250, 100, 30, 8, 0, scores, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawLeaderboard(Short(atlas, r::kTimeAttackFirstPlaceLabel + 2), 250,
                           100, 30, 0, 0, scores,
                           host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawLeaderboard(atlas, 250, 100, 0, 8, 0, scores, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03039

TEST_CASE("F03039 leaderboard entry draws one rank row",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  const auto scores = CourseScores();
  RecordHost host;
  CHECK(r::DrawLeaderboardEntry(atlas, 250, 100, 2, 70, 7, 2, scores, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].x == 166);
  CHECK(host.calls[0].y == 142);
  CHECK(host.calls[0].brightness == 64);
  CHECK(host.calls[0].time == Time(7, 3, 7));
  CHECK(host.calls[0].record);
  CHECK(
      host.calls[1].At(250 - 27 - 88, 140, r::kTimeAttackFirstPlaceLabel + 2));
  CHECK(host.calls[1].clut == r::kResultsNewRecordClut);
}

TEST_CASE("F03039 leaderboard entry keeps the time palette off the new rank",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  const auto scores = CourseScores();
  RecordHost host;
  CHECK(r::DrawLeaderboardEntry(atlas, 250, 100, 0, 10, 1, 1, scores, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(!host.calls[0].record);
  CHECK(host.calls[0].time == Time(1, 1, 7));
  CHECK(host.calls[1].clut == r::kResultsTimeClut);
}

TEST_CASE("F03039 leaderboard entry refuses a rank or course outside the board",
          "[timeattack-rating]") {
  const auto atlas = Atlas();
  const auto scores = CourseScores();
  RecordHost host;
  CHECK(r::DrawLeaderboardEntry(atlas, 250, 100, 3, 10, 1, 1, scores, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawLeaderboardEntry(atlas, 250, 100, -1, 10, 1, 1, scores, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawLeaderboardEntry(atlas, 250, 100, 0, 10, 8, 1, scores, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawLeaderboardEntry(atlas, 250, 100, 3, 0, 1, 1, scores, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- Live

// Stores `scores` as the live trial scores.
void SetLiveScores(const Scores& scores) {
  for (std::size_t course = 0; course < scores.size(); ++course) {
    for (std::size_t rank = 0; rank < r::kTimeAttackRanks; ++rank) {
      const auto word = scores.at(course).at(rank);
      auto& score = Audio_Seq_unusedScoredata.bossTimeTrialScores[course][rank];
      score.time = word & 0xFFFFFFU;
      score.round = (word >> 24) & 0x7FU;
      score.difficulty = word >> 31;
    }
  }
}

TEST_CASE("F03032 live results heading matches the typed draw",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  r::LiveDrawTimeAttackResultsHeading(160, 20, 12);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawTimeAttackResultsHeading(r::LiveResultsAtlas(), 160, 20, 12, host);
  REQUIRE(host.calls.size() == 2);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  r::LiveDrawTimeAttackResultsHeading(160, 20, 0);
  CHECK(frame.Empty());
}

TEST_CASE("F03033 live rating reads the completed time",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  Menu_Results_timeTrialTime = Time(1, 10, 0);
  r::LiveDrawTimeAttackRating(200, 130, 30);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawTimeAttackRating(r::LiveResultsAtlas(), 200, 130, 30, Time(1, 10, 0),
                          host);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].index == r::kTimeAttackFirstRating + 2);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
}

TEST_CASE("F03034 live rating wipe reads the completed time",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  Menu_Results_timeTrialTime = Time(0, 12, 0);
  r::LiveDrawTimeAttackRatingWipe(100, 140, 6);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawTimeAttackRatingWipe(r::LiveResultsAtlas(), 100, 140, 6,
                              Time(0, 12, 0), host);
  REQUIRE(host.calls.size() == 1);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  r::LiveDrawTimeAttackRatingWipe(100, 140, 0);
  CHECK(frame.Empty());
}

TEST_CASE("F03035 live time reads the completed time",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  Menu_Results_timeTrialTime = Time(2, 3, 4);
  r::LiveDrawTimeAttackTime(60, 90, 50);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawTimeAttackTime(r::LiveResultsAtlas(), 60, 90, 50, Time(2, 3, 4), host);
  REQUIRE(host.calls.size() == 2);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
}

TEST_CASE("F03036 live counting time counts the completed time",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  Menu_Results_timeTrialTime = Time(1, 30, 60);
  r::LiveDrawCountingTimeAttackTime(60, 90, 40);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawCountingTimeAttackTime(r::LiveResultsAtlas(), 60, 90, 40,
                                Time(1, 30, 60), host);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[1].time == Time(0, 45, 30));
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
}

TEST_CASE("F03037 live new record matches the typed draw",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  r::LiveDrawNewRecord(230, 170, 31);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawNewRecord(r::LiveResultsAtlas(), 230, 170, 31, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  r::LiveDrawNewRecord(230, 170, -4);
  CHECK(frame.Empty());
}

TEST_CASE("F03038 live leaderboard reads the course, scores and new rank",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  const auto scores = CourseScores();
  SetLiveScores(scores);
  Audio_Seq_unusedStateFlags.timeAttackBoss = 3;
  Menu_Results_newTimeSlot = 2;
  r::LiveDrawLeaderboard(250, 100, 40);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawLeaderboard(r::LiveResultsAtlas(), 250, 100, 40, 3, 2, scores, host);
  REQUIRE(host.calls.size() == 6);
  CHECK(host.calls[4].record);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  Audio_Seq_unusedStateFlags.timeAttackBoss = 8;
  r::LiveDrawLeaderboard(250, 100, 40);
  CHECK(frame.Empty());
}

TEST_CASE("F03039 live leaderboard entry refuses a rank outside the board",
          "[timeattack-rating-live]") {
  LiveTimeAttackFixture frame;
  const auto scores = CourseScores();
  SetLiveScores(scores);
  Audio_Seq_unusedStateFlags.timeAttackBoss = 6;
  Menu_Results_newTimeSlot = 1;
  r::LiveDrawLeaderboardEntry(250, 100, 1, 25);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawLeaderboardEntry(r::LiveResultsAtlas(), 250, 100, 1, 25, 6, 1, scores,
                          host);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].record);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  r::LiveDrawLeaderboardEntry(250, 100, 3, 25);
  CHECK(frame.Empty());
}
}  // namespace
