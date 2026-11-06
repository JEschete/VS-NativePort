#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Content/ModuleData.h"
#include "Ending/ClearResultsCore.hpp"
#include "Ending/EndingLive.hpp"
#include "Render/TextureRegionDraw.hpp"
#include "TestSupport/ChannelWarnings.hpp"
#include "TestSupport/EndingDisplayRecorder.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Ending/Results.h"
#include "Input/ControllerLiveState.h"
#include "State/Location.h"
#include "State/PlayTime.h"
#include "State/Progress.h"
#include "State/StateFlags.h"
}

namespace {
namespace Ending = VS::Ending;
namespace Render = VS::Render;
using VS::test_support::ChannelWarnings;
using VS::test_support::EndingDisplayRecorder;
using VS::test_support::TimBlockSpec;
using VS::test_support::TimBytes;

constexpr std::size_t kAtlasRegions = 30;
constexpr int kRegionTexelStride = 8;
constexpr std::uint8_t kHeadingWidth = 40;
constexpr std::uint32_t kAllRooms = 0xFFFFFFFFU;
constexpr std::uint32_t kPointsPerRung = 1000;
constexpr std::size_t kAtlasImageStride = 0x8220;
constexpr std::int16_t kAtlasRows = 0xFF;
constexpr Ending::ScreenPoint kScoreOrigin{0xA0, 0x50};
constexpr Ending::ScreenPoint kMapOrigin{0xA0, 0x66};

constexpr Render::RegionColor kRed{200, 10, 10, 0};
constexpr Render::RegionColor kGreen{10, 200, 10, 0};
constexpr Render::RegionColor kBlue{10, 10, 200, 0};

// Results tables over a test atlas whose region n starts at texel u = 8n and
// is 6 + n % 5 texels wide. Rank n needs 15 - n titles and (15 - n) * 1000
// points and its label shows regions n and n + 1.
struct ResultsFixture {
  std::array<std::uint32_t, Ending::kRoomFlagWords> rooms{};
  std::array<Render::TextureRegion, kAtlasRegions> atlas{};
  std::array<Ending::RankLabel, Ending::kRankCount> labels{};
  std::array<std::int16_t, Ending::kRankCount> title_thresholds{};
  std::array<std::uint32_t, Ending::kRankCount> score_thresholds{};
  Ending::ResultsTables tables;

  ResultsFixture() {
    rooms.fill(kAllRooms);
    for (std::size_t id = 0; id < atlas.size(); ++id) {
      atlas[id] = {.u = static_cast<std::uint8_t>(id * kRegionTexelStride),
                   .v = static_cast<std::uint8_t>(id),
                   .width = static_cast<std::uint8_t>(6 + (id % 5)),
                   .height = 8,
                   .texture_page = static_cast<std::uint16_t>(0x100 + id),
                   .clut = static_cast<std::uint16_t>(0x200 + id)};
    }
    for (std::size_t rank = 0; rank < Ending::kRankCount; ++rank) {
      const std::size_t rung = Ending::kRankCount - 1 - rank;
      title_thresholds[rank] = static_cast<std::int16_t>(rung);
      score_thresholds[rank] =
          static_cast<std::uint32_t>(rung) * kPointsPerRung;
      labels[rank] = {2, static_cast<std::uint8_t>(rank),
                      static_cast<std::uint8_t>(rank + 1), 0};
    }
    tables = {.countable_room_bits = rooms,
              .atlas = atlas,
              .rank_labels = labels,
              .rank_title_thresholds = title_thresholds,
              .rank_score_thresholds = score_thresholds,
              .rank_heading_width = kHeadingWidth,
              .tints = {.congratulations = {kRed, kGreen, kBlue},
                        .score = {kRed, kGreen},
                        .counting_score = {kGreen, kBlue},
                        .map_completion = {kBlue, kRed},
                        .counting_map_completion = {kRed, kBlue},
                        .rank_heading = {kGreen, kRed},
                        .rank_label = {kBlue, kGreen}}};
  }
  ResultsFixture(const ResultsFixture&) = delete;
  ResultsFixture& operator=(const ResultsFixture&) = delete;
  ResultsFixture(ResultsFixture&&) = delete;
  ResultsFixture& operator=(ResultsFixture&&) = delete;
  ~ResultsFixture() = default;
};

class FakeProgressHost final : public Ending::ProgressHost {
 public:
  int resets = 0;

  void ResetPlaythroughRecords() override { ++resets; }
};

class FakeResultsHost final
    : public EndingDisplayRecorder<Ending::ResultsHost> {
 public:
  std::vector<Render::RegionQuad> quads;
  std::vector<int> played;
  std::vector<int> stopped;
  std::vector<std::size_t> reads;
  std::map<std::size_t, std::vector<std::byte>> files;
  // IsResultsReadComplete reports each read pending this many times.
  int read_polls = 0;
  int ended_reads = 0;
  int freed = 0;

  void Draw(const Render::RegionQuad& quad) override { quads.push_back(quad); }
  void PlaySound(int sound) override { played.push_back(sound); }
  void StopSound(int sound) override { stopped.push_back(sound); }
  void BeginResultsRead(std::size_t file) override {
    reads.push_back(file);
    data_ = files[file];
    pending_polls_ = read_polls;
  }
  [[nodiscard]] bool IsResultsReadComplete() override {
    if (pending_polls_ == 0) return true;
    --pending_polls_;
    return false;
  }
  [[nodiscard]] std::span<const std::byte> ResultsData() override {
    return data_;
  }
  void EndResultsRead() override { ++ended_reads; }
  void FreeResultsData() override { ++freed; }

 private:
  std::vector<std::byte> data_;
  int pending_polls_ = 0;
};

// The atlas region each queued quad starts in.
std::vector<int> RegionsDrawn(const FakeResultsHost& host) {
  std::vector<int> regions;
  std::ranges::transform(host.quads, std::back_inserter(regions),
                         [](const Render::RegionQuad& quad) {
                           return quad.u_left / kRegionTexelStride;
                         });
  return regions;
}

std::vector<int> QuadLefts(const FakeResultsHost& host) {
  std::vector<int> lefts;
  std::ranges::transform(host.quads, std::back_inserter(lefts),
                         &Render::RegionQuad::left);
  return lefts;
}

std::vector<int> QuadTops(const FakeResultsHost& host) {
  std::vector<int> tops;
  std::ranges::transform(host.quads, std::back_inserter(tops),
                         &Render::RegionQuad::top);
  return tops;
}

// Five and a half hours, two earlier clears, eight rooms of the first word
// visited and one chest opened.
Ending::ClearProgress ClearedProgress() {
  Ending::ClearProgress progress;
  progress.play_time = {.hours = 5, .minutes = 30};
  progress.flags.clear_count = 2;
  progress.flags.chests_opened[3] = 1;
  progress.visited_rooms[0] = 0xF0F0U;
  progress.save_count = 3;
  progress.entry_zone = 9;
  progress.entry_room = 4;
  return progress;
}

struct RetainedFlags {
  int clear_count = 0;
  int difficulty = 0;
  int normal_mode_cleared = 0;
};

// A cleared playthrough keeps only the clear count, difficulty and normal-mode
// clear.
void CheckRetainedFlags(const Ending::PlaythroughFlags& flags,
                        RetainedFlags expected) {
  CHECK(flags.clear_count == expected.clear_count);
  CHECK(flags.difficulty == expected.difficulty);
  CHECK(flags.normal_mode_cleared == expected.normal_mode_cleared);
  CHECK_FALSE(flags.magic_used);
  CHECK(std::ranges::none_of(flags.chests_opened,
                             [](std::uint8_t opened) { return opened != 0; }));
}

// A score whose boss time trials are all unset.
Ending::ScoreRecord UntimedScore() {
  Ending::ScoreRecord score;
  score.boss_trial_times.fill(Ending::kUnsetTrialTime);
  return score;
}

std::uint32_t ScoreOf(const Ending::ScoreRecord& score, int clear_count) {
  Ending::ResultsState state{.clear_count = clear_count};
  Ending::CalculateClearScore(state, score);
  return state.final_score;
}

// END.DIS: three TIMs 0x8220 bytes apart, the first with a 128-colour CLUT.
std::vector<std::byte> AtlasFile() {
  std::vector<std::byte> bytes =
      TimBytes({.area = {0, 0, 2, kAtlasRows}, .fill = std::byte{0x11}},
               TimBlockSpec{.area = {0, 0, 0x80, 1}, .fill = std::byte{0xAB}});
  for (std::size_t index = 1; index < 3; ++index) {
    bytes.resize(index * kAtlasImageStride);
    const auto tim =
        TimBytes({.area = {0, 0, 1, kAtlasRows}, .fill = std::byte{0x22}});
    bytes.insert(bytes.end(), tim.begin(), tim.end());
  }
  return bytes;
}

bool LoadStep(Ending::ResultsState& state, const ResultsFixture& fixture,
              FakeResultsHost& host) {
  Ending::ClearProgress progress;
  progress.score = UntimedScore();
  return Ending::LoadClearResultsStep(state, progress, fixture.tables, host);
}

std::string RegionRefusal(int region) {
  return VS::test_support::Refused("Results region " + std::to_string(region),
                                   "the atlas has 30 regions");
}
}  // namespace

TEST_CASE(
    "Committing a first clear records its time and resets the playthrough",
    "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  Ending::ClearProgress progress = ClearedProgress();
  progress.flags.magic_used = true;
  FakeProgressHost host;
  Ending::CommitClearProgress(progress, fixture.rooms, host);
  CHECK(progress.score.completion_time_minutes == 330);
  CHECK(progress.score.clear_count == 2);
  CHECK(progress.score.map_completion == 8);
  CHECK(progress.score.opened_chest_count == 1);
  CHECK(host.resets == 1);
  CHECK(progress.visited_rooms == decltype(progress.visited_rooms){});
  CheckRetainedFlags(progress.flags, {.clear_count = 3, .difficulty = 1});
  CHECK((progress.entry_zone == 1 && progress.entry_room == 0));
}

TEST_CASE("Committing a slower clear keeps the record and marks normal mode",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeProgressHost host;
  Ending::ClearProgress slower = ClearedProgress();
  slower.score.completion_time_minutes = 330;
  slower.score.clear_count = 1;
  slower.flags.difficulty = 2;
  Ending::CommitClearProgress(slower, fixture.rooms, host);
  CHECK(slower.score.completion_time_minutes == 330);
  CHECK(slower.score.clear_count == 1);
  CheckRetainedFlags(
      slower.flags,
      {.clear_count = 3, .difficulty = 2, .normal_mode_cleared = 1});
  Ending::ClearProgress faster = ClearedProgress();
  faster.score.completion_time_minutes = 331;
  Ending::CommitClearProgress(faster, fixture.rooms, host);
  CHECK(faster.score.completion_time_minutes == 330);
  CHECK(faster.score.clear_count == 2);
}

TEST_CASE("UpdateClearProgress commits a clear and stops counting at 99",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  Ending::ClearProgress committed = ClearedProgress();
  committed.flags.clear_count = 99;
  Ending::ClearProgress updated = committed;
  FakeProgressHost commit_host;
  FakeProgressHost update_host;
  Ending::CommitClearProgress(committed, fixture.rooms, commit_host);
  Ending::UpdateClearProgress(updated, fixture.rooms, update_host);
  CHECK(update_host.resets == 1);
  CHECK(updated.flags.clear_count == 99);
  CHECK(updated.score.clear_count == 99);
  CHECK(updated.score.titles == committed.score.titles);
  CHECK(updated.score.completion_time_minutes == 330);
  CHECK(updated.score.map_completion == committed.score.map_completion);
  CHECK(updated.flags.difficulty == committed.flags.difficulty);
}

TEST_CASE("AwardClearTitles awards the restraint, boss, key and action titles",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  Ending::ClearProgress progress;
  progress.score.completion_time_minutes = 599;
  progress.unlocked_actions.fill(true);
  progress.flags.bosses_defeated = {true, false, true, false, false, true};
  progress.flags.gold_key_obtained = true;
  progress.flags.chest_key_obtained = true;
  Ending::AwardClearTitles(progress, fixture.rooms);
  // Under ten hours (1), bosses 0, 2 and 5 (4, 6, 9), both keys (12, 13), both
  // action sets (14, 15), never saved (16) and no magic, abilities or break
  // arts (17-19).
  CHECK(progress.score.titles == 0xFF252U);
}

TEST_CASE("AwardClearTitles withholds titles whose conditions fail",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  Ending::ClearProgress progress;
  progress.score.titles = 1U << 20U;
  progress.score.completion_time_minutes = 600;
  progress.unlocked_actions.fill(true);
  progress.unlocked_actions[223] = false;
  progress.unlocked_actions[22] = false;
  progress.save_count = 1;
  progress.flags.magic_used = true;
  progress.flags.battle_abilities_used = true;
  progress.flags.break_arts_used = true;
  Ending::AwardClearTitles(progress, fixture.rooms);
  CHECK(progress.score.titles == 1U << 20U);
}

TEST_CASE("AwardClearTitles skips the four actions the lower title excludes",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  constexpr std::uint32_t kUpperActions = 1U << 14U;
  constexpr std::uint32_t kLowerActions = 1U << 15U;
  Ending::ClearProgress progress;
  std::fill_n(progress.unlocked_actions.begin() + 22, 32, true);
  for (const std::size_t excluded : {33U, 36U, 38U, 39U}) {
    progress.unlocked_actions[excluded] = false;
  }
  Ending::AwardClearTitles(progress, fixture.rooms);
  CHECK((progress.score.titles & kLowerActions) != 0);
  CHECK((progress.score.titles & kUpperActions) == 0);
  progress.score.titles = 0;
  progress.unlocked_actions[53] = false;
  std::fill_n(progress.unlocked_actions.begin() + 184, 40, true);
  Ending::AwardClearTitles(progress, fixture.rooms);
  CHECK((progress.score.titles & kLowerActions) == 0);
  CHECK((progress.score.titles & kUpperActions) != 0);
}

TEST_CASE(
    "AwardClearTitles raises the exploration records without lowering "
    "them",
    "[ending][ending-results-credits]") {
  std::array<std::uint32_t, Ending::kRoomFlagWords> countable{};
  countable[0] = 0x0FU;
  countable[15] = 0x80000000U;
  Ending::ClearProgress progress;
  progress.visited_rooms[0] = 0xFFU;
  progress.visited_rooms[15] = kAllRooms;
  progress.flags.chests_opened[0] = 1;
  progress.flags.chests_opened[63] = 2;
  progress.score.map_completion = 3;
  progress.score.opened_chest_count = 10;
  Ending::AwardClearTitles(progress, countable);
  CHECK(progress.score.map_completion == 5);
  CHECK(progress.score.opened_chest_count == 10);
  progress.score.map_completion = 9;
  progress.score.opened_chest_count = 0;
  Ending::AwardClearTitles(progress, countable);
  CHECK(progress.score.map_completion == 9);
  CHECK(progress.score.opened_chest_count == 2);
}

TEST_CASE("AwardClearTitles counts only the room words a short table holds",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  std::array<std::uint32_t, 4> countable{};
  countable.fill(kAllRooms);
  Ending::ClearProgress progress;
  progress.visited_rooms.fill(1U);
  Ending::AwardClearTitles(progress, countable);
  CHECK(progress.score.map_completion == 4);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Countable rooms table has 4 of 16 words; counting those"});
}

TEST_CASE("CalculateClearScore adds kill, attack, boss trial and chain points",
          "[ending][ending-results-credits]") {
  constexpr std::array<std::uint32_t, 6> kKillPoints{20, 20, 40, 80, 100, 60};
  constexpr std::array<std::uint32_t, 10> kAttackPoints{20,  40, 20,  100, 60,
                                                        100, 60, 100, 80,  60};
  for (std::size_t index = 0; index < kKillPoints.size(); ++index) {
    Ending::ScoreRecord score = UntimedScore();
    score.enemy_kills[index] = 3;
    CHECK(ScoreOf(score, 0) == 3 * kKillPoints[index]);
  }
  for (std::size_t index = 0; index < kAttackPoints.size(); ++index) {
    Ending::ScoreRecord score = UntimedScore();
    score.weapon_attacks[index] = 2;
    CHECK(ScoreOf(score, 0) == 2 * kAttackPoints[index]);
  }
  Ending::ScoreRecord score = UntimedScore();
  score.boss_trial_times[0] = 0;
  score.boss_trial_times[7] = 1234;
  score.max_chain = 5;
  score.streak_score = 7;
  score.misc_score = 3;
  CHECK(ScoreOf(score, 0) == 40000 + 50 + 10);
}

TEST_CASE("CalculateClearScore multiplies the longest chain by its tier",
          "[ending][ending-results-credits]") {
  constexpr std::array<std::array<std::uint32_t, 2>, 15> kTiers{{{5, 10},
                                                                 {6, 20},
                                                                 {10, 20},
                                                                 {11, 40},
                                                                 {15, 40},
                                                                 {16, 80},
                                                                 {20, 80},
                                                                 {21, 160},
                                                                 {25, 160},
                                                                 {26, 320},
                                                                 {30, 320},
                                                                 {31, 640},
                                                                 {50, 640},
                                                                 {51, 1280},
                                                                 {999, 1280}}};
  for (const auto& [chain, points] : kTiers) {
    Ending::ScoreRecord score = UntimedScore();
    score.max_chain = static_cast<std::uint16_t>(chain);
    CHECK(ScoreOf(score, 0) == chain * points);
  }
}

TEST_CASE("CalculateClearScore adds clear points and the fast clear bonus",
          "[ending][ending-results-credits]") {
  constexpr std::array<std::array<std::uint32_t, 2>, 6> kBonuses{{{600, 200000},
                                                                  {540, 250000},
                                                                  {539, 300000},
                                                                  {300, 300000},
                                                                  {299, 400000},
                                                                  {0, 400000}}};
  for (const auto& [minutes, expected] : kBonuses) {
    Ending::ScoreRecord score = UntimedScore();
    score.completion_time_minutes = minutes;
    CHECK(ScoreOf(score, 2) == expected);
  }
  Ending::ScoreRecord first = UntimedScore();
  first.completion_time_minutes = 100;
  first.max_chain = 1;
  CHECK(ScoreOf(first, 0) == 10);
}

TEST_CASE("CalculateClearScore caps the score at 999999999",
          "[ending][ending-results-credits]") {
  Ending::ScoreRecord score = UntimedScore();
  score.completion_time_minutes = 600;
  score.misc_score = 99;
  CHECK(ScoreOf(score, 9999) == 999900099);
  score.completion_time_minutes = 100;
  CHECK(ScoreOf(score, 9999) == Ending::kMaximumScore);
}

TEST_CASE("DetermineRiskbreakerRank picks the first rank both thresholds allow",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  Ending::ScoreRecord score;
  Ending::ResultsState state{.final_score = 50000};
  score.titles = 0xFFFF0007U;
  Ending::DetermineRiskbreakerRank(state, score, fixture.tables);
  CHECK(state.rank == 12);
  score.titles = 0xFFFFU;
  state.final_score = 15000;
  Ending::DetermineRiskbreakerRank(state, score, fixture.tables);
  CHECK(state.rank == 0);
  state.final_score = 14999;
  Ending::DetermineRiskbreakerRank(state, score, fixture.tables);
  CHECK(state.rank == 1);
  score.titles = 0;
  state.final_score = 0;
  Ending::DetermineRiskbreakerRank(state, score, fixture.tables);
  CHECK(state.rank == 15);
}

TEST_CASE("DetermineRiskbreakerRank keeps the rank when none qualifies",
          "[ending][ending-results-credits]") {
  ResultsFixture fixture;
  fixture.title_thresholds[15] = 1;
  Ending::ResultsState state{.rank = 7};
  Ending::DetermineRiskbreakerRank(state, {}, fixture.tables);
  CHECK(state.rank == 7);
  const ChannelWarnings warnings("ending");
  Ending::ResultsTables tables = fixture.tables;
  tables.rank_title_thresholds = std::span(fixture.title_thresholds).first(4);
  Ending::ScoreRecord score;
  score.titles = 0xFFFFU;
  state.final_score = 0;
  Ending::DetermineRiskbreakerRank(state, score, tables);
  CHECK(state.rank == 7);
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Rank tables hold 4 of 16 ranks; checking those"});
}

TEST_CASE("LoadClearResultsStep uploads the atlas once its read completes",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  host.files[0] = AtlasFile();
  host.read_polls = 1;
  Ending::ResultsState state;
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK(state.load_phase == 1);
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK(host.loaded_areas ==
        std::vector<Render::ScreenRect>{{832, 256, 2, 255},
                                        {0x300, 0x1FF, 0x80, 1},
                                        {896, 256, 1, 255},
                                        {960, 256, 1, 255}});
  std::vector<std::byte> palette(0x100, std::byte{0xAB});
  std::fill_n(palette.begin(), 4, std::byte{0});
  CHECK(host.loaded_pixels.at(1) == palette);
  CHECK((host.reads == std::vector<std::size_t>{0} && host.ended_reads == 1));
  CHECK(state.load_phase == 2);
}

TEST_CASE("LoadClearResultsStep scores the clear and reads its rank screen",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ClearProgress progress;
  progress.flags.clear_count = 1;
  progress.score = UntimedScore();
  progress.score.map_completion = 180;
  progress.score.titles = 0x7;
  progress.score.completion_time_minutes = 600;
  Ending::ResultsState state{.load_phase = 2,
                             .displayed_map_completion = 9,
                             .displayed_score = 9,
                             .animation_frame = 9,
                             .is_leaving = true};
  CHECK_FALSE(
      Ending::LoadClearResultsStep(state, progress, fixture.tables, host));
  CHECK((state.final_score == 100000 && state.rank == 12));
  CHECK((state.map_completion == 49 && state.clear_count == 1));
  CHECK((state.displayed_score == 0 && state.displayed_map_completion == 0));
  CHECK((state.animation_frame == 0 && !state.is_leaving));
  CHECK(host.reads == std::vector<std::size_t>{13});
  CHECK(host.freed == 1);
  CHECK(state.load_phase == 3);
}

TEST_CASE("LoadClearResultsStep uploads the rank screen and then finishes",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  host.files[4] = TimBytes({.area = {5, 6, 4, 2}, .fill = std::byte{0x33}});
  host.BeginResultsRead(4);
  Ending::ResultsState state{.load_phase = 3};
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK(host.loaded_areas == std::vector<Render::ScreenRect>{{0, 0x100, 4, 2}});
  CHECK(host.loaded_pixels.at(0) ==
        std::vector<std::byte>(16, std::byte{0x33}));
  CHECK(host.ended_reads == 1);
  CHECK(state.load_phase == 4);
  CHECK(LoadStep(state, fixture, host));
  CHECK(state.load_phase == 0);
  CHECK(host.freed == 1);
}

TEST_CASE("LoadClearResultsStep refuses a rank past the rank screens",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  ResultsFixture fixture;
  fixture.title_thresholds[15] = 1;
  FakeResultsHost host;
  Ending::ResultsState state{.load_phase = 2, .rank = 16};
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK(state.load_phase == 4);
  CHECK(host.reads.empty());
  CHECK(warnings.messages ==
        std::vector<std::string>{
            "Rank screen 16 refused: there are 16 rank screens"});
}

TEST_CASE("LoadClearResultsStep refuses images that are not whole TIMs",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  const ResultsFixture fixture;
  FakeResultsHost host;
  host.files[0] =
      TimBytes({.area = {0, 0, 1, 1}},
               TimBlockSpec{.area = {0, 0, 16, 1}, .fill = std::byte{1}});
  Ending::ResultsState state;
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK_FALSE(LoadStep(state, fixture, host));
  state.load_phase = 3;
  host.files[1] = {};
  host.BeginResultsRead(1);
  CHECK_FALSE(LoadStep(state, fixture, host));
  CHECK(host.loaded_areas.empty());
  CHECK(host.ended_reads == 2);
  const std::string image_outside =
      "Results image refused: 510 bytes at 64 lie outside the 66-byte file";
  CHECK(warnings.messages ==
        std::vector<std::string>{
            image_outside,
            "Results atlas palette refused: it lies outside the file",
            "Results atlas image 1 refused: no TIM at 33312",
            "Results atlas image 2 refused: no TIM at 66624",
            "Rank screen refused: its 0 bytes hold no TIM"});
}

TEST_CASE(
    "ShowClearResults loads the screens and draws until a button fades "
    "it out",
    "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  host.files[0] = AtlasFile();
  host.files[16] = TimBytes({.area = {0, 0, 1, 1}});
  host.buttons = 1;
  Ending::ClearProgress progress;
  progress.score = UntimedScore();
  Ending::ResultsState state;
  Ending::ShowClearResults(state, progress, fixture.tables, host);
  CHECK(host.calls.front() == "display on");
  CHECK(host.reads == std::vector<std::size_t>{0, 16});
  CHECK(host.presented_frames == 14);
  CHECK(host.controller_updates == 14);
  CHECK(host.vertical_blank_waits == 9);
  CHECK(std::ranges::all_of(host.drawn_slots,
                            [](int slot) { return slot == 0x7FF; }));
  CHECK((state.is_leaving && state.animation_frame == 8));
}

TEST_CASE("DrawClearResults plays the rank reveal sounds at frame 0xD0",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.rank = 3, .animation_frame = 0xD0};
  CHECK_FALSE(Ending::DrawClearResults(state, fixture.tables, host));
  CHECK(host.played == std::vector<int>{0x77, 0x78});
  CHECK(host.textured_quads.size() == 5);
  // Congratulations, the counted score and map completion, and the heading;
  // the rank label starts on the next frame.
  CHECK(RegionsDrawn(host) == std::vector<int>{22, 23,  0x12, 0x1A, 0,    0,
                                               0,  0xA, 0,    0,    0,    0xA,
                                               0,  0,   0,    0x14, 0x15, 0x1A,
                                               0,  0,   0,    0x13, 12});
  CHECK(state.animation_frame == 0xD1);
}

TEST_CASE("DrawClearResults holds its frame and starts leaving on a button",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.animation_frame = 0x1005};
  CHECK_FALSE(Ending::DrawClearResults(state, fixture.tables, host));
  CHECK(state.animation_frame == 0x1001);
  CHECK_FALSE(state.is_leaving);
  host.buttons = 1;
  CHECK_FALSE(Ending::DrawClearResults(state, fixture.tables, host));
  CHECK(state.is_leaving);
  CHECK(state.animation_frame == 1);
  CHECK(host.played.empty());
}

TEST_CASE("DrawClearResults fades every part out after the button press",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.rank = 2,
                             .final_score = 123,
                             .map_completion = 45,
                             .animation_frame = 4,
                             .is_leaving = true};
  CHECK_FALSE(Ending::DrawClearResults(state, fixture.tables, host));
  CHECK(host.textured_quads.size() == 5);
  CHECK(host.textured_quads[0].brightness == 8);
  // Congratulations (2), score (14), map completion (6), heading (1) and the
  // rank label (2).
  CHECK(host.quads.size() == 25);
  CHECK(state.animation_frame == 5);
  host.quads.clear();
  state.animation_frame = 8;
  CHECK(Ending::DrawClearResults(state, fixture.tables, host));
  CHECK(host.quads.empty());
  CHECK(state.animation_frame == 8);
}

TEST_CASE("DrawRankBackground draws five panels lit by the clamped fade",
          "[ending][ending-results-credits]") {
  EndingDisplayRecorder<Ending::DisplayHost> host;
  Ending::DrawRankBackground(10, host);
  REQUIRE(host.textured_quads.size() == 5);
  CHECK(host.textured_quads[0].texture_page == (16 | 0x180));
  CHECK(host.textured_quads[4] ==
        Ending::TexturedQuadDraw{
            .ordering_slot = 0x7FE,
            .corners = {{{256, 8}, {320, 8}, {256, 232}, {320, 232}}},
            .texels = {{{0, 0}, {64, 0}, {0, 224}, {64, 224}}},
            .texture_page = 20 | 0x180,
            .brightness = 20});
  host.textured_quads.clear();
  Ending::DrawRankBackground(100, host);
  CHECK(host.textured_quads.at(0).brightness == 128);
  host.textured_quads.clear();
  Ending::DrawRankBackground(0, host);
  Ending::DrawRankBackground(-3, host);
  CHECK(host.textured_quads.empty());
}

TEST_CASE("DrawCongratulations centres its two regions on the origin",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::DrawCongratulations({0xA0, 0x28}, 64, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{22, 23});
  CHECK(QuadLefts(host) == std::vector<int>{152, 160});
  CHECK(QuadTops(host) == std::vector<int>{0x28, 0x28});
  CHECK(host.quads.at(0).left_color == Render::QuadColor{200, 10, 10});
  CHECK(host.quads.at(1).right_color == Render::QuadColor{10, 10, 200});
  host.quads.clear();
  Ending::DrawCongratulations({0xA0, 0x28}, 8, fixture.tables, host);
  CHECK(host.quads.at(0).left_color == Render::QuadColor{192, 192, 192});
  host.quads.clear();
  Ending::DrawCongratulations({0xA0, 0x28}, 0, fixture.tables, host);
  CHECK(host.quads.empty());
}

TEST_CASE("DrawScore lays out the label, digits, separators and unit",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  const Ending::ResultsState state{.final_score = 123456789};
  Ending::DrawScore(kScoreOrigin, 64, state, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{0x12, 0x1A, 1, 2, 3, 0xA, 4, 5,
                                               6, 0xA, 7, 8, 9, 0x14});
  CHECK(QuadLefts(host) == std::vector<int>{85, 94, 103, 115, 127, 139, 148,
                                            160, 172, 184, 193, 205, 217, 229});
  CHECK(QuadTops(host) == std::vector<int>{80, 87, 83, 83, 83, 94, 83, 83, 83,
                                           94, 83, 83, 83, 88});
  CHECK(host.quads.at(0).left_color == Render::QuadColor{200, 10, 10});
  host.quads.clear();
  Ending::DrawScore(kScoreOrigin, 4, state, fixture.tables, host);
  CHECK(host.quads.at(0).left_color == Render::QuadColor{100, 5, 5});
  CHECK(host.quads.at(0).clut == 0x213);
  host.quads.clear();
  Ending::DrawScore(kScoreOrigin, 0, state, fixture.tables, host);
  CHECK(host.quads.empty());
}

TEST_CASE("DrawMapCompletion lays out three digits and the percent sign",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.map_completion = 42};
  Ending::DrawMapCompletion(kMapOrigin, 64, state, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{0x15, 0x1A, 0, 4, 2, 0x13});
  CHECK(QuadLefts(host) == std::vector<int>{129, 136, 145, 157, 169, 181});
  CHECK(QuadTops(host) == std::vector<int>{102, 109, 105, 105, 105, 110});
  CHECK(host.quads.at(0).left_color == Render::QuadColor{10, 10, 200});
  host.quads.clear();
  state.map_completion = 100;
  Ending::DrawMapCompletion(kMapOrigin, 64, state, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{0x15, 0x1A, 1, 0, 0, 0x13});
  host.quads.clear();
  Ending::DrawMapCompletion(kMapOrigin, -1, state, fixture.tables, host);
  CHECK(host.quads.empty());
}

TEST_CASE("DrawCountingScore counts toward the final score with its sounds",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.final_score = 3200};
  const auto draw = [&](int frame) {
    Ending::DrawCountingScore(kScoreOrigin, frame, state, fixture.tables, host);
  };
  draw(31);
  CHECK((host.played.empty() && state.displayed_score == 0));
  draw(32);
  draw(33);
  CHECK(state.displayed_score == 100);
  draw(63);
  CHECK(state.displayed_score == 3100);
  draw(64);
  draw(90);
  draw(91);
  CHECK(state.displayed_score == 3200);
  CHECK(host.played == std::vector<int>{0x72, 0x72, 0x73});
  CHECK(host.stopped == std::vector<int>{0x72});
  CHECK_FALSE(state.is_score_incrementing);
}

TEST_CASE("DrawCountingScore wipes the number in from the left",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state;
  Ending::DrawCountingScore(kScoreOrigin, 1, state, fixture.tables, host);
  // The wipe edge is 8 pixels right of the origin: the label and colon are
  // whole, digits short of the edge are feathered strips.
  CHECK(QuadLefts(host) ==
        std::vector<int>{85, 94, 103, 115, 127, 139, 148, 160});
  CHECK(host.quads.back().left_color == Render::QuadColor{1, 25, 1});
  CHECK(host.quads.back().clut == 0x201);
  CHECK(host.played.empty());
  host.quads.clear();
  Ending::DrawCountingScore(kScoreOrigin, 0, state, fixture.tables, host);
  CHECK(host.quads.empty());
}

TEST_CASE("DrawCountingMapCompletion steps a small completion each frame",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.map_completion = 3};
  for (int frame = 32; frame < 40; ++frame) {
    Ending::DrawCountingMapCompletion(kMapOrigin, frame, state, fixture.tables,
                                      host);
  }
  CHECK(state.displayed_map_completion == 3);
  CHECK(host.played == std::vector<int>{0x72, 0x73});
  CHECK(host.stopped == std::vector<int>{0x72});
  CHECK(RegionsDrawn(host).back() == 0x13);
}

TEST_CASE("DrawCountingMapCompletion scales a large completion by the fade",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::ResultsState state{.map_completion = 64};
  const auto draw = [&](int frame) {
    Ending::DrawCountingMapCompletion(kMapOrigin, frame, state, fixture.tables,
                                      host);
  };
  draw(48);
  CHECK(state.displayed_map_completion == 32);
  draw(64);
  CHECK(state.displayed_map_completion == 64);
  draw(70);
  CHECK(host.played == std::vector<int>{0x72, 0x73});
  Ending::ResultsState empty;
  draw(0);
  Ending::DrawCountingMapCompletion(kMapOrigin, 64, empty, fixture.tables,
                                    host);
  CHECK(empty.displayed_map_completion == 0);
  CHECK(host.played.size() == 2);
}

TEST_CASE("DrawRiskbreakerRankHeading centres the heading by its table width",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  Ending::DrawRiskbreakerRankHeading({0xA0, 0x8A}, 64, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{12});
  CHECK(QuadLefts(host) == std::vector<int>{140});
  CHECK(QuadTops(host) == std::vector<int>{0x8A});
  CHECK(host.quads.at(0).left_color == Render::QuadColor{10, 200, 10});
  host.quads.clear();
  Ending::DrawRiskbreakerRankHeading({0xA0, 0x8A}, 0, fixture.tables, host);
  CHECK(host.quads.empty());
}

TEST_CASE("DrawRiskbreakerRank centres the rank's label regions",
          "[ending][ending-results-credits]") {
  const ResultsFixture fixture;
  FakeResultsHost host;
  const Ending::ResultsState state{.rank = 5};
  Ending::DrawRiskbreakerRank({0xA0, 0x9A}, 64, state, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{5, 6});
  CHECK(QuadLefts(host) == std::vector<int>{154, 160});
  CHECK(QuadTops(host) == std::vector<int>{0x9A, 0x9A});
  CHECK(host.quads.at(0).left_color == Render::QuadColor{10, 10, 200});
  host.quads.clear();
  Ending::DrawRiskbreakerRank({0xA0, 0x9A}, 0, state, fixture.tables, host);
  CHECK(host.quads.empty());
}

TEST_CASE("DrawRiskbreakerRank refuses missing labels and clamps long ones",
          "[ending][ending-results-credits]") {
  const ChannelWarnings warnings("ending");
  ResultsFixture fixture;
  fixture.labels[1] = {5, 1, 2, 3};
  fixture.labels[2] = {1, 200, 0, 0};
  FakeResultsHost host;
  Ending::ResultsState state{.rank = 16};
  Ending::DrawRiskbreakerRank({0xA0, 0x9A}, 64, state, fixture.tables, host);
  state.rank = 1;
  Ending::DrawRiskbreakerRank({0xA0, 0x9A}, 64, state, fixture.tables, host);
  CHECK(RegionsDrawn(host) == std::vector<int>{1, 2, 3});
  state.rank = 2;
  Ending::DrawRiskbreakerRank({0xA0, 0x9A}, 64, state, fixture.tables, host);
  CHECK(host.quads.size() == 3);
  CHECK(warnings.messages ==
        std::vector<std::string>{"Rank 16 label refused: there are 16 labels",
                                 "Rank 1 label names 5 regions; drawing 3",
                                 RegionRefusal(200), RegionRefusal(200),
                                 RegionRefusal(200)});
}

namespace {
// Saves the records LiveCommitClearProgress reads and writes, clears them and
// restores them afterwards.
class LiveClearRecords {
 public:
  LiveClearRecords() {
    Audio_Seq_unusedScoredata = {};
    Audio_Seq_unusedStateFlags = {};
    State_playTime = {};
    State_roomVisitProgress = {};
    State_savedEntryDoor = {};
    Input_settings = {};
    std::ranges::fill(Battle_actionDefinitions, Battle_ActionDefinition{});
    std::ranges::fill(Ending_countableRoomBits, 0);
  }
  LiveClearRecords(const LiveClearRecords&) = delete;
  LiveClearRecords& operator=(const LiveClearRecords&) = delete;
  LiveClearRecords(LiveClearRecords&&) = delete;
  LiveClearRecords& operator=(LiveClearRecords&&) = delete;
  ~LiveClearRecords() {
    Audio_Seq_unusedScoredata = score_;
    Audio_Seq_unusedStateFlags = flags_;
    State_playTime = play_time_;
    State_roomVisitProgress = rooms_;
    State_savedEntryDoor = door_;
    Input_settings = settings_;
    std::ranges::copy(actions_, std::begin(Battle_actionDefinitions));
    std::ranges::copy(countable_, std::begin(Ending_countableRoomBits));
  }

 private:
  State_ScoreData score_ = Audio_Seq_unusedScoredata;
  State_StateFlags flags_ = Audio_Seq_unusedStateFlags;
  State_PlayTime play_time_ = State_playTime;
  State_RoomVisitFlags rooms_ = State_roomVisitProgress;
  Field_MpdDoorRecord door_ = State_savedEntryDoor;
  State_Settings settings_ = Input_settings;
  decltype(std::to_array(Battle_actionDefinitions)) actions_ =
      std::to_array(Battle_actionDefinitions);
  decltype(std::to_array(Ending_countableRoomBits)) countable_ =
      std::to_array(Ending_countableRoomBits);
};

// A 2:05 clear on the second difficulty with magic used, Asura defeated, one
// chest opened and two of four visited rooms countable.
void CommitLiveClear() {
  State_playTime.t.hours = 2;
  State_playTime.t.minutes = 5;
  State_StateFlags& flags = Audio_Seq_unusedStateFlags;
  flags.clearCount = 4;
  flags.difficulty = 1;
  flags.magicUsed = 1;
  flags.asuraDefeated = 1;
  flags.chestsOpened[0] = 1;
  State_roomVisitProgress.roomFlags[0] = 0xF;
  Ending_countableRoomBits[0] = 0x3;
  State_savedEntryDoor.destinationZoneId = 9;
  VS::Ending::LiveCommitClearProgress();
}
}  // namespace

TEST_CASE("LiveCommitClearProgress records the clear in the game's score",
          "[ending][ending-results-credits]") {
  const LiveClearRecords records;
  CommitLiveClear();
  const State_ScoreData& score = Audio_Seq_unusedScoredata;
  CHECK(score.completionTimeMinutes == 125);
  CHECK(score.clearCount == 4);
  // Under ten hours (1), Asura (9), never saved (16) and no battle abilities or
  // break arts (18, 19); magic was used.
  CHECK(static_cast<std::uint32_t>(score.titles) == 0xD0202U);
  CHECK(score.mapCompletion == 2);
  CHECK(score.openedChestCount == 1);
}

TEST_CASE("LiveCommitClearProgress resets the game's playthrough records",
          "[ending][ending-results-credits]") {
  const LiveClearRecords records;
  CommitLiveClear();
  const State_StateFlags& flags = Audio_Seq_unusedStateFlags;
  CHECK(flags.clearCount == 5);
  CHECK((flags.difficulty == 1 && flags.normalModeCleared == 1));
  CHECK((flags.magicUsed == 0 && flags.asuraDefeated == 0));
  CHECK(flags.chestsOpened[0] == 0);
  CHECK(State_roomVisitProgress.roomFlags[0] == 0);
  CHECK(State_savedEntryDoor.destinationZoneId == 1);
  CHECK(State_savedEntryDoor.destinationRoomId == 0);
}

TEST_CASE("Promoted clear results functions leave no module address bindings",
          "[ending][ending-results-credits]") {
  // No module image stores these ENDING.PRG addresses, so their entries are
  // retired with the generated code.
  constexpr auto kAddresses = std::to_array<unsigned>(
      {0x8006B760, 0x8006B910, 0x8006B9FC, 0x8006BD78, 0x8006BE04, 0x8006C214,
       0x8006C3CC, 0x8006C514, 0x8006C5C8, 0x8006C744, 0x8006C9A8, 0x8006CAF4,
       0x8006CD38, 0x8006CD94, 0x8006DFD0, 0x8006E074});
  const auto symbols =
      std::span(Content_moduleSymbols, Content_moduleSymbolCount);
  for (const unsigned address : kAddresses) {
    CHECK(VS::Content::ResolveModuleAddress(symbols, "ENDING/ENDING.PRG",
                                            address) == nullptr);
  }
}
