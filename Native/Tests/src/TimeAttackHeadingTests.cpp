#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Menu/Results/TimeAttackHeading.hpp"
#include "Menu/Results/TimeAttackHeadingLive.hpp"
#include "Render/DisplayControl.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "TestSupport/TimeAttackDrawRecorder.hpp"
extern "C" {
#include "Audio/Seq/Channel.h"
#include "Menu/Results/CubePuzzle.h"
#include "Menu/Results/Results.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Images.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"
#include "State/Progress.h"
#include "State/StateFlags.h"
}

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;
using VS::test_support::time_attack::Kind;
using VS::test_support::time_attack::LiveTimeAttackFixture;
using VS::test_support::time_attack::RecordHost;
using VS::test_support::time_attack::Replay;

class RecordClut final : public r::ResultsClutTarget {
 public:
  std::vector<render::ScreenRect> areas;
  std::vector<std::uint16_t> uploaded;

  void Upload(const render::ScreenRect& area,
              std::span<const std::uint16_t> colors) override {
    areas.push_back(area);
    uploaded.assign(colors.begin(), colors.end());
  }
};

// Every entry is 16 wide and 16 high except the ones the layouts centre on.
std::vector<render::TextureRegion> Atlas() {
  std::vector<render::TextureRegion> atlas(127, {.width = 16, .height = 16});
  atlas[r::kTimeAttackCourseLabel].width = 50;
  atlas[r::kTimeAttackFirstBossName].width = 30;
  atlas[r::kTimeAttackFirstBossName + 2].width = 41;
  atlas[r::kTimeAttackDeath].width = 44;
  atlas[r::kTimeAttackOgreZombie].width = 70;
  atlas[r::kTimeAttackBestTimeLabel].width = 60;
  atlas[r::kTimeAttackCountdownThree].width = 21;
  atlas[r::kTimeAttackCountdownThree].height = 33;
  atlas[r::kTimeAttackCountdownThree + 3].width = 50;
  atlas[r::kTimeAttackCountdownThree + 3].height = 24;
  atlas[r::kTimeAttackHeading].width = 40;
  atlas[r::kTimeAttackHeading + 1].width = 60;
  return atlas;
}

constexpr std::array<std::uint32_t, r::kTimeAttackCourses> MakeScores() {
  return {0x0001'0203, 0xFF12'3456, 0x8000'0000, 0x0080'0000,
          0x0000'0000, 0x7F05'0607, 0x0010'2030, 0xFFFF'FFFF};
}
constexpr auto kScores = MakeScores();

// ---------------------------------------------------------------- F03018

TEST_CASE(
    "F03018 heading flashes both halves centred with the dark third color",
    "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackHeading(atlas, 160, 48, 30, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  const auto& left = host.calls[0];
  const auto& right = host.calls[1];
  CHECK((left.kind == Kind::kFlash && left.At(110, 48, 100)));
  CHECK((left.colors[0].r == 128 && left.colors[0].step == 30));
  CHECK((left.colors[1].r == 200 && left.colors[1].step == 30));
  CHECK((right.kind == Kind::kFlash && right.At(150, 48, 101)));
  CHECK((right.colors[0].g == 180 && right.colors[0].step == 30));
  CHECK((right.colors[1].b == 64 && right.colors[1].step == 0));
}

TEST_CASE("F03018 heading clamps the timer to 64 and hides at zero or less",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackHeading(atlas, 0, 0, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawTimeAttackHeading(atlas, 0, 0, INT32_MIN, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
  CHECK(r::DrawTimeAttackHeading(atlas, 0, 0, INT32_MAX, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].colors[0].step == 64);
  CHECK(host.calls[1].colors[0].step == 64);
}

TEST_CASE("F03018 heading refuses an atlas without both heading entries",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawTimeAttackHeading(std::span(atlas).first(101), 0, 0, 5, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
  CHECK(r::DrawTimeAttackHeading(std::span(atlas).first(102), 0, 0, 5, host) ==
        r::ResultsDrawResult::kDrawn);
}

// ---------------------------------------------------------------- F03019

TEST_CASE("F03019 course fades the label then the digit in the course palette",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawCourseNumber(atlas, 160, 92, 90, 3, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  // 160 - (50 + 24) / 2 = 123; the digit sits 12 past the label, a pixel up.
  CHECK((host.calls[0].kind == Kind::kFade && host.calls[0].At(123, 92, 84)));
  CHECK(host.calls[0].brightness == 64);
  CHECK(
      (host.calls[1].kind == Kind::kFadeClut && host.calls[1].At(185, 91, 4)));
  CHECK((host.calls[1].brightness == 64 && host.calls[1].clut == 0x7FF4));
}

TEST_CASE("F03019 course refuses a digit past the atlas only while revealed",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawCourseNumber(atlas, 0, 0, 1, 125, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[1].index == 126);
  host.calls.clear();
  CHECK(r::DrawCourseNumber(atlas, 0, 0, 1, 126, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawCourseNumber(atlas, 0, 0, 1, 255, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawCourseNumber(atlas, 0, 0, 0, 255, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawCourseNumber(std::span(atlas).first(84), 0, 0, 1, 0, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03020

TEST_CASE("F03020 boss name centres the course's name entry",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawBossName(atlas, 160, 120, 7, 0, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawBossName(atlas, 160, 120, 7, 2, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawBossName(atlas, 160, 120, 7, 7, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 3);
  CHECK((host.calls[0].At(145, 120, 85) && host.calls[0].brightness == 7));
  // An odd width rounds the half down.
  CHECK(host.calls[1].At(140, 120, 87));
  CHECK(host.calls[2].At(152, 120, 92));
}

TEST_CASE("F03020 boss name draws Death and the Ogre Zombie for course six",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawBossName(atlas, 160, 120, 100, 6, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  // 160 - (44 + 70) / 2 = 103, then 44 further right.
  CHECK((host.calls[0].At(103, 120, 91) && host.calls[0].brightness == 64));
  CHECK((host.calls[1].At(147, 120, 93) && host.calls[1].brightness == 64));
}

TEST_CASE("F03020 boss name refuses a name entry past the atlas",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawBossName(atlas, 0, 0, 1, 41, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].index == 126);
  host.calls.clear();
  CHECK(r::DrawBossName(atlas, 0, 0, 1, 42, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawBossName(std::span(atlas).first(93), 0, 0, 1, 6, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawBossName(atlas, 0, 0, -3, 200, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03021

TEST_CASE("F03021 best time fades the label then the course's masked time",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawBestTime(atlas, 160, 156, 9, 1, kScores, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  // 160 - (60 + 96) / 2 = 82; the time starts 12 past the label, 2 down.
  CHECK((host.calls[0].kind == Kind::kFade && host.calls[0].At(82, 156, 95)));
  CHECK((host.calls[1].kind == Kind::kTime && host.calls[1].x == 154));
  CHECK((host.calls[1].y == 158 && host.calls[1].brightness == 9));
  CHECK(host.calls[1].time == 0x123456);
}

TEST_CASE("F03021 best time keeps the no-time sentinel and clamps the timer",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawBestTime(atlas, 0, 0, 1000, 3, kScores, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(r::DrawBestTime(atlas, 0, 0, 1, 7, kScores, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 4);
  CHECK((host.calls[1].time == r::kResultsNoTime &&
         host.calls[1].brightness == 64));
  CHECK(host.calls[3].time == 0xFFFFFF);
}

TEST_CASE("F03021 best time refuses a course without a score",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawBestTime(atlas, 0, 0, 5, 8, kScores, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawBestTime(atlas, 0, 0, 5, 255, kScores, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawBestTime(std::span(atlas).first(95), 0, 0, 5, 0, kScores,
                        host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawBestTime(atlas, 0, 0, 0, 8, kScores, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03022

TEST_CASE("F03022 countdown shows a new number every 15 frames",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  for (const std::int32_t timer : {1, 14, 16, 59}) {
    CHECK(r::DrawStartCountdown(atlas, 160, 120, timer, host) ==
          r::ResultsDrawResult::kDrawn);
  }
  REQUIRE(host.calls.size() == 4);
  // Three is 21x33, the default entries 16x16 and the fourth 50x24.
  CHECK((host.calls[0].At(150, 104, 96) && host.calls[0].brightness == 1));
  CHECK((host.calls[1].At(150, 104, 96) && host.calls[1].brightness == 14));
  CHECK((host.calls[2].At(152, 112, 97) && host.calls[2].brightness == 1));
  CHECK((host.calls[3].At(135, 108, 99) && host.calls[3].brightness == 14));
}

TEST_CASE("F03022 countdown hides on each number's first frame",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  for (const std::int32_t timer : {0, 15, 30, 45, -1, -16}) {
    CHECK(r::DrawStartCountdown(atlas, 160, 120, timer, host) ==
          r::ResultsDrawResult::kHidden);
  }
  CHECK(host.calls.empty());
}

TEST_CASE("F03022 countdown refuses a number outside the atlas",
          "[timeattack-heading]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawStartCountdown(atlas, 0, 0, 464, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK((host.calls[0].index == 126 && host.calls[0].brightness == 14));
  host.calls.clear();
  CHECK(r::DrawStartCountdown(atlas, 0, 0, 465, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawStartCountdown(atlas, 0, 0, INT32_MAX, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  // -1454 / 15 truncates to -96: entry 0 with a negative remainder.
  CHECK(r::DrawStartCountdown(atlas, 0, 0, -1454, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawStartCountdown(atlas, 0, 0, -1455, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawStartCountdown(atlas, 0, 0, INT32_MIN, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

// ---------------------------------------------------------------- F03023

std::array<std::uint16_t, r::kResultsClutWords> PulseClut() {
  std::array<std::uint16_t, r::kResultsClutWords> clut{};
  for (std::size_t i = 0; i < clut.size(); ++i) {
    clut.at(i) = static_cast<std::uint16_t>(0x1111 * (i % 7) + i);
  }
  return clut;
}

TEST_CASE("F03023 pulse at full and zero weight copies one row with the mask",
          "[timeattack-heading]") {
  auto clut = PulseClut();
  clut[32] = 0x8000 | 0x7FFF;
  clut[96] = 0x0421;
  clut[33] = 0x0000;
  clut[97] = 0xFFFF;
  RecordClut target;
  r::PulseTimeAttackClut(clut, 16, target);
  CHECK(clut[128] == 0xFFFF);
  CHECK(clut[129] == 0x0000);
  r::PulseTimeAttackClut(clut, 0, target);
  CHECK(clut[128] == 0x8421);
  // The fourth row's mask bit never reaches the fifth.
  CHECK(clut[129] == 0x7FFF);
}

TEST_CASE("F03023 pulse mixes each component and leaves the other rows",
          "[timeattack-heading]") {
  auto clut = PulseClut();
  const auto before = clut;
  // Red 31, green 0, blue 16 against red 1, green 31, blue 0.
  clut[40] = 0x401F;
  clut[104] = 0x03E1;
  RecordClut target;
  r::PulseTimeAttackClut(clut, 4, target);
  // Red (31*4 + 1*12) / 16 = 8, green (31*12) / 16 = 23, blue (16*4) / 16 = 4.
  CHECK(clut[136] == (8 | (23 << 5) | (4 << 10)));
  for (std::size_t i = 0; i < r::kTimeAttackPulseTargetRow; ++i) {
    if (i != 40 && i != 104) {
      CHECK(clut.at(i) == before.at(i));
    }
  }
  REQUIRE(target.areas.size() == 1);
  CHECK(target.areas[0] == r::kResultsClutArea);
  CHECK(std::ranges::equal(target.uploaded, clut));
}

TEST_CASE("F03023 pulse truncates a negative weighted sum toward zero",
          "[timeattack-heading]") {
  auto clut = PulseClut();
  clut[32] = 0x001F;
  clut[96] = 0x0000;
  clut[33] = 0x0000;
  clut[97] = 0x0001;
  RecordClut target;
  // Red 31 * -1 = -31: -31 / 16 is -1, so all five bits; a floor shift
  // would give 0x1E.
  r::PulseTimeAttackClut(clut, -1, target);
  CHECK(clut[128] == 0x001F);
  // Red 1 * 17 = 17 / 16 = 1.
  CHECK(clut[129] == 0x0001);
}

// ---------------------------------------------------------------- Live

TEST_CASE("F03018 live heading flashes the results atlas heading entries",
          "[timeattack-heading-live]") {
  LiveTimeAttackFixture frame;
  r::LiveDrawTimeAttackHeading(160, 48, 12);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawTimeAttackHeading(r::LiveResultsAtlas(), 160, 48, 12, host);
  REQUIRE(host.calls.size() == 2);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  r::LiveDrawTimeAttackHeading(160, 48, 0);
  CHECK(frame.Empty());
}

TEST_CASE("F03019 live course reads the time-attack course and refuses past it",
          "[timeattack-heading-live]") {
  LiveTimeAttackFixture frame;
  Audio_Seq_unusedStateFlags.timeAttackBoss = 4;
  r::LiveDrawCourseNumber(160, 92, 20);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawCourseNumber(r::LiveResultsAtlas(), 160, 92, 20, 4, host);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[1].index == 5);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  Audio_Seq_unusedStateFlags.timeAttackBoss = 200;
  r::LiveDrawCourseNumber(160, 92, 20);
  CHECK(frame.Empty());
}

TEST_CASE("F03020 live boss name reads the course and refuses past the atlas",
          "[timeattack-heading-live]") {
  LiveTimeAttackFixture frame;
  Audio_Seq_unusedStateFlags.timeAttackBoss = 6;
  r::LiveDrawBossName(160, 120, 40);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawBossName(r::LiveResultsAtlas(), 160, 120, 40, 6, host);
  REQUIRE(host.calls.size() == 2);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  Audio_Seq_unusedStateFlags.timeAttackBoss = 42;
  r::LiveDrawBossName(160, 120, 40);
  CHECK(frame.Empty());
}

TEST_CASE("F03021 live best time reads the course's first-place score",
          "[timeattack-heading-live]") {
  LiveTimeAttackFixture frame;
  Audio_Seq_unusedStateFlags.timeAttackBoss = 2;
  Audio_Seq_unusedScoredata.bossTimeTrialScores[2][0] = {
      .time = 0x010203, .round = 5, .difficulty = 1};
  Audio_Seq_unusedScoredata.bossTimeTrialScores[2][1] = {
      .time = 0x040506, .round = 0, .difficulty = 0};
  r::LiveDrawBestTime(160, 156, 64);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  std::array<std::uint32_t, r::kTimeAttackCourses> scores{};
  scores[2] = 0x8501'0203;
  r::DrawBestTime(r::LiveResultsAtlas(), 160, 156, 64, 2, scores, host);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[1].time == 0x010203);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  Audio_Seq_unusedStateFlags.timeAttackBoss = 8;
  r::LiveDrawBestTime(160, 156, 64);
  CHECK(frame.Empty());
}

TEST_CASE("F03022 live countdown draws the number and refuses past the atlas",
          "[timeattack-heading-live]") {
  LiveTimeAttackFixture frame;
  r::LiveDrawStartCountdown(160, 120, 20);
  const auto live = frame.Frame();
  REQUIRE(!frame.Empty());
  RecordHost host;
  r::DrawStartCountdown(r::LiveResultsAtlas(), 160, 120, 20, host);
  REQUIRE(host.calls.size() == 1);
  frame.Reset();
  Replay(host);
  CHECK(frame.Frame() == live);
  frame.Reset();
  r::LiveDrawStartCountdown(160, 120, 465);
  r::LiveDrawStartCountdown(160, 120, 30);
  CHECK(frame.Empty());
}

TEST_CASE("F03023 live pulse blends clutBuf and uploads it to the palette row",
          "[timeattack-heading-live]") {
  std::array<std::uint16_t, r::kResultsClutWords> saved_clut{};
  std::ranges::copy(Menu_Results_clutBuf, saved_clut.begin());
  RECT area{768, 511, 160, 1};
  std::array<std::uint16_t, r::kResultsClutWords> saved_vram{};
  StoreImage(&area, reinterpret_cast<u_long*>(saved_vram.data()));

  const auto clut = PulseClut();
  std::ranges::copy(clut, std::begin(Menu_Results_clutBuf));
  Menu_Results_clutBuf[32] = 0x001F;
  Menu_Results_clutBuf[96] = 0x7C00;
  r::LivePulseTimeAttackClut(8);
  // Red 31 * 8 / 16 = 15, blue 31 * 8 / 16 = 15.
  CHECK(Menu_Results_clutBuf[128] == (15 | (15 << 10)));
  std::array<std::uint16_t, r::kResultsClutWords> vram{};
  StoreImage(&area, reinterpret_cast<u_long*>(vram.data()));
  CHECK(std::ranges::equal(vram, Menu_Results_clutBuf));

  LoadImage(&area, reinterpret_cast<u_long*>(saved_vram.data()));
  std::ranges::copy(saved_clut, std::begin(Menu_Results_clutBuf));
}
}  // namespace
