#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Legacy/Psx.h"
#include "Menu/Results/GameClearScore.hpp"
#include "Menu/Results/GameClearScoreLive.hpp"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsDrawing.hpp"
#include "Menu/Results/ResultsDrawingLive.hpp"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Images.h"
#include "Render/Render.h"
#include "Render/TextureRegionDraw.hpp"
#include "Render/TextureRegionDrawLive.hpp"
#include "Render/UiPrimitives.h"

namespace {
namespace r = VS::Menu::Results;
namespace render = VS::Render;

enum class Kind : std::uint8_t { kTint, kFlash, kWipe, kPlay, kStop };

struct Call {
  Kind kind = Kind::kTint;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::size_t index = 0;
  r::GameClearColors colors{};
  std::int32_t edge = 0;

  [[nodiscard]] bool At(std::int32_t at_x, std::int32_t at_y,
                        std::size_t at_index) const {
    return x == at_x && y == at_y && index == at_index;
  }
  [[nodiscard]] bool Steps(std::uint8_t left, std::uint8_t right) const {
    return colors[0].step == left && colors[1].step == right;
  }
};

class RecordHost final : public r::GameClearDrawHost {
 public:
  std::vector<Call> calls;

  void DrawTintFade(r::ResultsPoint origin, std::size_t texture_index,
                    const r::GameClearColors& colors) override {
    calls.push_back({.kind = Kind::kTint,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .colors = colors});
  }
  void DrawFlashFade(r::ResultsPoint origin, std::size_t texture_index,
                     const r::GameClearColors& colors) override {
    calls.push_back({.kind = Kind::kFlash,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .colors = colors});
  }
  void DrawWipe(r::ResultsPoint origin, std::size_t texture_index,
                const r::GameClearColors& colors,
                std::int32_t wipe_edge_x) override {
    calls.push_back({.kind = Kind::kWipe,
                     .x = origin.x,
                     .y = origin.y,
                     .index = texture_index,
                     .colors = colors,
                     .edge = wipe_edge_x});
  }
  void PlaySfx(std::int32_t group, std::int32_t effect) override {
    calls.push_back({.kind = Kind::kPlay,
                     .x = group,
                     .y = effect,
                     .index = 0,
                     .colors = {}});
  }
  void StopSfx(std::int32_t group, std::int32_t effect) override {
    calls.push_back({.kind = Kind::kStop,
                     .x = group,
                     .y = effect,
                     .index = 0,
                     .colors = {}});
  }

  [[nodiscard]] std::vector<std::size_t> Indices() const {
    std::vector<std::size_t> indices;
    for (const auto& call : calls) {
      indices.push_back(call.index);
    }
    return indices;
  }
  [[nodiscard]] std::vector<Kind> Sounds() const {
    std::vector<Kind> sounds;
    for (const auto& call : calls) {
      if (call.kind == Kind::kPlay || call.kind == Kind::kStop) {
        sounds.push_back(call.kind);
      }
    }
    return sounds;
  }
};

// Every entry is 16 wide except the ones the layouts centre on.
std::vector<render::TextureRegion> Atlas() {
  std::vector<render::TextureRegion> atlas(127, {.width = 16});
  atlas[r::kGameClearComma].width = 4;
  atlas[r::kGameClearRankHeading].width = 70;
  atlas[r::kGameClearScoreLabel].width = 40;
  atlas[r::kGameClearPercent].width = 10;
  atlas[r::kGameClearPoints].width = 20;
  atlas[r::kGameClearMapLabel].width = 60;
  atlas[r::kGameClearCongratulations].width = 50;
  atlas[r::kGameClearCongratulations + 1].width = 30;
  atlas[r::kGameClearColon].width = 6;
  atlas[30].width = 25;
  atlas[31].width = 10;
  return atlas;
}

constexpr std::array<r::RiskbreakerRankRow, 4> kRanks{{
    {2, 30, 31, 0},
    {0, 0, 0, 0},
    {4, 30, 31, 30},
    {1, 127, 0, 0},
}};

TEST_CASE("F03025 congratulations flash in both halves centred on x",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawCongratulations(atlas, 200, 30, 90, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  const auto& left = host.calls[0];
  const auto& right = host.calls[1];
  CHECK((left.kind == Kind::kFlash && left.At(160, 30, 22)));
  CHECK((left.Steps(64, 64) && left.colors[1].r == 200));
  CHECK((right.At(210, 30, 23) && right.Steps(64, 0)));
  CHECK((right.colors[0].g == 180 && right.colors[1].b == 64));
}

TEST_CASE("F03025 congratulations stay hidden at a timer of zero or less",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawCongratulations(atlas, 0, 0, 0, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawCongratulations(atlas, 0, 0, INT32_MIN, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawCongratulations(std::span(atlas).first(23), 0, 0, 5, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

TEST_CASE("F03026 score groups nine digits with commas between label and pts",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawScore(atlas, 200, 50, 7, 12345678, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 14);
  CHECK(host.Indices() == std::vector<std::size_t>{18, 26, 0, 1, 2, 10, 3, 4, 5,
                                                   10, 6, 7, 8, 20});
  // call, x, y, texture index.
  constexpr std::array<std::array<std::int32_t, 4>, 7> kExpected{{
      {0, 105, 50, 18},
      {1, 145, 57, 26},
      {2, 153, 53, 0},
      {5, 189, 64, 10},
      {6, 196, 53, 3},
      {10, 239, 53, 6},
      {13, 275, 58, 20},
  }};
  for (const auto& [call, x, y, index] : kExpected) {
    CHECK(host.calls[static_cast<std::size_t>(call)].At(x, y, index));
  }
  CHECK((host.calls[13].kind == Kind::kTint && host.calls[13].Steps(7, 0)));
}

TEST_CASE("F03026 score past nine digits draws its first nine",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawScore(atlas, 0, 0, 64, 1234567890, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(host.Indices() == std::vector<std::size_t>{18, 26, 1, 2, 3, 10, 4, 5, 6,
                                                   10, 7, 8, 9, 20});
}

TEST_CASE("F03026 score that prints a minus sign or a short atlas refuses",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawScore(atlas, 0, 0, 10, 0x80000000U, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawScore(std::span(atlas).first(26), 0, 0, 10, 5, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawScore(atlas, 0, 0, -4, 0x80000000U, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

TEST_CASE("F03027 counting score holds its readout before step 32",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout{.shown = 7, .counting = 0};
  CHECK(r::DrawCountingScore(atlas, 200, 50, 31, 3200, readout, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(readout.shown == 7);
  CHECK(host.Sounds().empty());
  REQUIRE(host.calls.size() == 14);
  CHECK(host.calls[0].kind == Kind::kWipe);
  CHECK(host.calls[0].At(105, 50, 18));
  CHECK(host.calls[0].edge == 200 + 31 * 8);
  CHECK(host.calls[12].At(263, 53, 7));
  CHECK(host.calls[13].Steps(0, 0));
}

TEST_CASE("F03027 counting score climbs and plays its sounds across reentry",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout;
  r::DrawCountingScore(atlas, 0, 0, 32, 3200, readout, host);
  CHECK(readout.shown == 0);
  CHECK(readout.counting == 1);
  r::DrawCountingScore(atlas, 0, 0, 33, 3200, readout, host);
  CHECK(readout.shown == 100);
  r::DrawCountingScore(atlas, 0, 0, 40, 3200, readout, host);
  CHECK(readout.shown == 800);
  r::DrawCountingScore(atlas, 0, 0, 99, 3200, readout, host);
  CHECK(readout.shown == 3200);
  CHECK(host.Sounds() == std::vector<Kind>{Kind::kPlay, Kind::kPlay});
}

TEST_CASE("F03027 counting score plays the counted sound once at the target",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout{.shown = 3200, .counting = 1};
  r::DrawCountingScore(atlas, 0, 0, 64, 3200, readout, host);
  r::DrawCountingScore(atlas, 0, 0, 64, 3200, readout, host);
  CHECK(readout.counting == 0);
  CHECK(host.Sounds() == std::vector<Kind>{Kind::kStop, Kind::kPlay});
  const auto& counted = host.calls[host.calls.size() - 29];
  CHECK(
      (counted.kind == Kind::kPlay && counted.x == 0x7E && counted.y == 0x73));
}

TEST_CASE("F03027 counting score of zero stays silent", "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout{.shown = 0, .counting = 1};
  CHECK(r::DrawCountingScore(atlas, 0, 0, 64, 0, readout, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(host.Sounds().empty());
  CHECK(readout.counting == 1);
  CHECK(readout.shown == 0);
}

TEST_CASE("F03027 counting score that prints negative counts but draws nothing",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout{.shown = 5, .counting = 1};
  CHECK(r::DrawCountingScore(atlas, 0, 0, 64, 0x80000005U, readout, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(readout.shown == static_cast<std::int32_t>(0x80000005U));
  CHECK(host.calls.empty());
  CHECK(r::DrawCountingScore(std::span(atlas).first(26), 0, 0, 64, 9, readout,
                             host) == r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawCountingScore(atlas, 0, 0, 0, 9, readout, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(readout.shown == static_cast<std::int32_t>(0x80000005U));
  CHECK(host.calls.empty());
}

TEST_CASE("F03028 map completion draws three digits then the percent sign",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawMapCompletion(atlas, 200, 40, 3, 87, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 6);
  CHECK(host.Indices() == std::vector<std::size_t>{21, 26, 0, 8, 7, 19});
  CHECK(host.calls[0].At(143, 40, 21));
  CHECK(host.calls[1].At(203, 47, 26));
  CHECK(host.calls[2].At(211, 43, 0));
  CHECK(host.calls[4].At(235, 43, 7));
  CHECK(host.calls[5].At(247, 48, 19));
  CHECK(host.calls[5].Steps(3, 0));
}

TEST_CASE("F03028 map completion past three digits draws its first three",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawMapCompletion(atlas, 0, 0, 64, 98765, host) ==
        r::ResultsDrawResult::kDrawn);
  CHECK(host.Indices() == std::vector<std::size_t>{21, 26, 9, 8, 7, 19});
  host.calls.clear();
  CHECK(r::DrawMapCompletion(atlas, 0, 0, 64, 0xFFFFFFFFU, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.empty());
}

TEST_CASE("F03029 small map completion counts up one a frame from step 32",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout;
  r::DrawCountingMapCompletion(atlas, 0, 0, 31, 3, readout, host);
  CHECK(readout.shown == 0);
  CHECK(host.Sounds().empty());
  for (int frame = 0; frame < 5; ++frame) {
    r::DrawCountingMapCompletion(atlas, 0, 0, 40, 3, readout, host);
  }
  CHECK(readout.shown == 3);
  CHECK(readout.counting == 0);
  CHECK(host.Sounds() ==
        std::vector<Kind>{Kind::kPlay, Kind::kStop, Kind::kPlay});
  CHECK(host.calls.back().kind == Kind::kWipe);
  CHECK(host.calls.back().edge == 320);
}

TEST_CASE("F03029 large map completion scales with the elapsed steps",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  r::CountingReadout readout{.shown = 50, .counting = 1};
  r::DrawCountingMapCompletion(atlas, 200, 40, 48, 100, readout, host);
  CHECK(readout.shown == 50);
  CHECK(host.Indices() == std::vector<std::size_t>{21, 26, 0, 5, 0, 19});
  CHECK(host.calls[0].At(143, 40, 21));
  r::DrawCountingMapCompletion(atlas, 0, 0, 64, 100, readout, host);
  CHECK(readout.shown == 100);
  r::DrawCountingMapCompletion(atlas, 0, 0, 64, 0x08000001U, readout, host);
  CHECK(readout.shown == 1);
  CHECK(host.Sounds().empty());
}

TEST_CASE("F03030 rank heading tint-fades centred on x", "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawRiskbreakerRankHeading(atlas, 200, 12, 1, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 1);
  CHECK(host.calls[0].kind == Kind::kTint);
  CHECK(host.calls[0].At(165, 12, 12));
  CHECK(host.calls[0].Steps(1, 0));
  CHECK(host.calls[0].colors[0].g == 80);
  CHECK(r::DrawRiskbreakerRankHeading(atlas, 0, 0, -1, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawRiskbreakerRankHeading(std::span(atlas).first(12), 0, 0, 1,
                                      host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(host.calls.size() == 1);
}

TEST_CASE("F03030 rank heading clamps its reveal step to sixty-four",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  for (const std::int32_t timer : {63, 64, 65, 1000, INT32_MAX}) {
    CHECK(r::DrawRiskbreakerRankHeading(atlas, 200, 12, timer, host) ==
          r::ResultsDrawResult::kDrawn);
  }
  REQUIRE(host.calls.size() == 5);
  CHECK(host.calls[0].Steps(63, 0));
  for (std::size_t call = 1; call < host.calls.size(); ++call) {
    CHECK(host.calls[call].Steps(64, 0));
    CHECK(host.calls[call].At(165, 12, 12));
  }
  CHECK(r::DrawRiskbreakerRankHeading(atlas, 200, 12, INT32_MIN, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.size() == 5);
}

TEST_CASE("F03031 rank words flash in side by side centred on x",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawRiskbreakerRank(atlas, 200, 90, 70, 0, kRanks, host) ==
        r::ResultsDrawResult::kDrawn);
  REQUIRE(host.calls.size() == 2);
  CHECK(host.calls[0].kind == Kind::kFlash);
  CHECK(host.calls[0].At(183, 90, 30));
  CHECK(host.calls[1].At(208, 90, 31));
  CHECK(host.calls[1].Steps(64, 0));
  CHECK(host.calls[1].colors[0].r == 128);
}

TEST_CASE("F03031 rank refuses rows and words outside its tables",
          "[gameclear-score]") {
  const auto atlas = Atlas();
  RecordHost host;
  CHECK(r::DrawRiskbreakerRank(atlas, 0, 0, 5, 1, kRanks, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(r::DrawRiskbreakerRank(atlas, 0, 0, 5, 2, kRanks, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawRiskbreakerRank(atlas, 0, 0, 5, 3, kRanks, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawRiskbreakerRank(atlas, 0, 0, 5, 4, kRanks, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawRiskbreakerRank(atlas, 0, 0, 5, -1, kRanks, host) ==
        r::ResultsDrawResult::kInvalidTexture);
  CHECK(r::DrawRiskbreakerRank(atlas, 0, 0, 0, 99, kRanks, host) ==
        r::ResultsDrawResult::kHidden);
  CHECK(host.calls.empty());
}

constexpr std::size_t kLivePrimitiveWords = 0x20000 / sizeof(u_long);
constexpr std::size_t kLiveOrderingWords = 32;
constexpr std::size_t kLiveSceneHead = 16;

// A frame whose primitives land in `storage`, and the game-clear statics the
// Live entries read, restored afterwards.
struct LiveGameClearFixture {
  Render_FrameScratch saved_frame = Render_frameScratch;
  void* saved_buffer = Render_legacyPrimitiveBuffers[0];
  int saved_framebuffer = Render_frameBuf;
  u_int saved_score = Menu_Results_score;
  u_int saved_map = Menu_Results_mapCompletion;
  int saved_rank = Menu_Results_rank;
  int saved_shown_score = Menu_Results_onScreenScore;
  int saved_counting_score = Menu_Results_incrementingScore;
  int saved_shown_map = Menu_Results_onScreenMapCompletion;
  int saved_counting_map = Menu_Results_incrementingMapCompletion;
  std::vector<u_long> storage = std::vector<u_long>(kLivePrimitiveWords, 0);
  std::array<u_long, kLiveOrderingWords> ordering{};

  LiveGameClearFixture() { Reset(); }
  ~LiveGameClearFixture() {
    Render_frameScratch = saved_frame;
    Render_legacyPrimitiveBuffers[0] = saved_buffer;
    Render_frameBuf = saved_framebuffer;
    Menu_Results_score = saved_score;
    Menu_Results_mapCompletion = saved_map;
    Menu_Results_rank = saved_rank;
    Menu_Results_onScreenScore = saved_shown_score;
    Menu_Results_incrementingScore = saved_counting_score;
    Menu_Results_onScreenMapCompletion = saved_shown_map;
    Menu_Results_incrementingMapCompletion = saved_counting_map;
  }
  LiveGameClearFixture(const LiveGameClearFixture&) = delete;
  LiveGameClearFixture& operator=(const LiveGameClearFixture&) = delete;
  LiveGameClearFixture(LiveGameClearFixture&&) = delete;
  LiveGameClearFixture& operator=(LiveGameClearFixture&&) = delete;

  void Reset() {
    std::ranges::fill(storage, 0);
    ClearOTagR(ordering.data(), static_cast<int>(ordering.size()));
    Render_frameBuf = 0;
    Render_legacyPrimitiveBuffers[0] = storage.data();
    Render_frameScratch.primitiveCursor = storage.data();
    Render_frameScratch.orderingTable = ordering.data() + kLiveSceneHead;
  }
  [[nodiscard]] std::vector<u_long> Used() const {
    const auto used =
        static_cast<u_long*>(Render_frameScratch.primitiveCursor) -
        storage.data();
    return {storage.begin(), storage.begin() + used};
  }
};

// Replays recorded draws through the live atlas draws.
void Replay(const RecordHost& host) {
  for (const auto& call : host.calls) {
    const std::array<CVECTOR, 2> colors{{
        {call.colors[0].r, call.colors[0].g, call.colors[0].b,
         call.colors[0].step},
        {call.colors[1].r, call.colors[1].g, call.colors[1].b,
         call.colors[1].step},
    }};
    const auto index = static_cast<int>(call.index);
    if (call.kind == Kind::kTint) {
      render::DrawLiveTextureTintFade(Menu_Results_disMap, call.x, call.y,
                                      index, colors);
    } else if (call.kind == Kind::kFlash) {
      render::DrawLiveTextureFlashFade(Menu_Results_disMap, call.x, call.y,
                                       index, colors);
    } else if (call.kind == Kind::kWipe) {
      render::DrawLiveTextureWipe(Menu_Results_disMap, call.x, call.y, index,
                                  colors, call.edge);
    }
  }
}

TEST_CASE("Live score draws the results score through the atlas",
          "[gameclear-score-live]") {
  LiveGameClearFixture frame;
  Menu_Results_score = 987654321;
  r::LiveDrawScore(160, 100, 64, 0);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawScore(r::LiveResultsAtlas(), 160, 100, 64, 987654321, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}

TEST_CASE("Live counting score advances the results readout statics",
          "[gameclear-score-live]") {
  LiveGameClearFixture frame;
  Menu_Results_score = 3200;
  Menu_Results_onScreenScore = 5;
  Menu_Results_incrementingScore = 1;
  r::LiveDrawCountingScore(160, 100, 40, 0);
  CHECK(Menu_Results_onScreenScore == 800);
  CHECK(Menu_Results_incrementingScore == 1);
  CHECK(!frame.Used().empty());
  frame.Reset();
  Menu_Results_score = 0x80000000U;
  r::LiveDrawCountingScore(160, 100, 64, 0);
  CHECK(Menu_Results_onScreenScore == INT32_MIN);
  CHECK(frame.Used().empty());
}

TEST_CASE("Live map completion draws through the atlas and counts its readout",
          "[gameclear-score-live]") {
  LiveGameClearFixture frame;
  Menu_Results_mapCompletion = 0;
  Menu_Results_onScreenMapCompletion = 0;
  Menu_Results_incrementingMapCompletion = 0;
  r::LiveDrawCountingMapCompletion(160, 120, 50, 0);
  CHECK(Menu_Results_onScreenMapCompletion == 0);
  CHECK(!frame.Used().empty());
  frame.Reset();
  Menu_Results_mapCompletion = 100;
  r::LiveDrawMapCompletion(160, 120, 20, 0);
  const auto live = frame.Used();
  RecordHost host;
  r::DrawMapCompletion(r::LiveResultsAtlas(), 160, 120, 20, 100, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
}

TEST_CASE("Live rank draws the results rank row and refuses a rank outside it",
          "[gameclear-score-live]") {
  LiveGameClearFixture frame;
  Menu_Results_rank = 0;
  r::LiveDrawRiskbreakerRank(160, 140, 64);
  const auto live = frame.Used();
  RecordHost host;
  const std::array<r::RiskbreakerRankRow, 1> first{
      {{Menu_Results_riskbreakerRanks[0][0],
        Menu_Results_riskbreakerRanks[0][1],
        Menu_Results_riskbreakerRanks[0][2],
        Menu_Results_riskbreakerRanks[0][3]}}};
  r::DrawRiskbreakerRank(r::LiveResultsAtlas(), 160, 140, 64, 0, first, host);
  CHECK(host.calls.size() == 3);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
  frame.Reset();
  Menu_Results_rank = 16;
  r::LiveDrawRiskbreakerRank(160, 140, 64);
  CHECK(frame.Used().empty());
}

TEST_CASE("Live congratulations and heading draw the banner entries",
          "[gameclear-score-live]") {
  LiveGameClearFixture frame;
  r::LiveDrawCongratulations(160, 30, 12);
  r::LiveDrawRiskbreakerRankHeading(160, 60, 12);
  const auto live = frame.Used();
  REQUIRE(!live.empty());
  RecordHost host;
  r::DrawCongratulations(r::LiveResultsAtlas(), 160, 30, 12, host);
  r::DrawRiskbreakerRankHeading(r::LiveResultsAtlas(), 160, 60, 12, host);
  frame.Reset();
  Replay(host);
  CHECK(frame.Used() == live);
  frame.Reset();
  r::LiveDrawCongratulations(160, 30, 0);
  CHECK(frame.Used().empty());
}
}  // namespace
