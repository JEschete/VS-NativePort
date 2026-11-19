#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/StatusEffects.h"
#include "Core/Scratch.hpp"
#include "Field/MapProgress.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Results/Results.h"
#include "Menu/Results/ResultsRankLive.hpp"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreRank.hpp"
#include "Menu/Score/ScoreRankLive.hpp"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Render.hpp"
#include "State/Progress.h"
#include "State/StateFlags.h"

namespace {
namespace s = VS::Menu::Score;

s::ScoreRecord Uncleared() {
  s::ScoreRecord record;
  record.boss_trial_words.fill(s::kUnclearedTrialTime);
  record.completion_time_minutes = 1000;
  return record;
}

std::uint32_t ChainScore(std::uint16_t chain) {
  s::ScoreRecord record = Uncleared();
  record.max_chain = chain;
  return s::CalculateScore(record, 0);
}

std::uint32_t ClearScore(std::uint16_t clear_count, std::uint32_t minutes) {
  s::ScoreRecord record = Uncleared();
  record.completion_time_minutes = minutes;
  return s::CalculateScore(record, clear_count);
}

std::uint32_t Titles(const s::ScoreRecord& record,
                     const s::TitleConditions& conditions = {},
                     std::uint16_t clear_count = 0) {
  return s::AwardTitles(record, clear_count, conditions);
}

constexpr std::uint32_t Bit(unsigned index) {
  return std::uint32_t{1} << index;
}

s::TitleConditions WithActions(std::size_t first, std::size_t end) {
  s::TitleConditions conditions;
  for (std::size_t i = first; i < end; ++i) {
    conditions.actions_unlocked.at(i) = true;
  }
  return conditions;
}

std::uint32_t RankOrMax(std::uint32_t titles, std::uint32_t score) {
  return s::DetermineRiskbreakerRank(titles, score).value_or(0xFFFF);
}

std::int32_t ResultsRankOrMax(std::uint32_t titles, std::uint32_t score) {
  const auto rank = s::RiskbreakerRankIndex(titles, score);
  return rank.has_value() ? static_cast<std::int32_t>(*rank) : -1;
}

class RecordingHost final : public s::ProgressHost {
 public:
  std::vector<std::string> calls;
  void ResetScreen() override { calls.emplace_back("reset"); }
  void ClearCursorMemory() override { calls.emplace_back("cursor"); }
  void BackupCamera() override { calls.emplace_back("camera"); }
};

s::ProgressWorld SampleWorld() {
  s::ProgressWorld world;
  world.room_flags[0] = 0xFF;
  world.countable_room_masks[0] = 0x0F;
  world.room_flags[1] = 0x800000;
  world.countable_room_masks[1] = 0xFFFFFFFF;
  world.chests_opened[0] = 1;
  world.chests_opened[5] = 0x80;
  world.chests_opened[63] = 2;
  world.clear_count = 2;
  world.record = Uncleared();
  world.record.map_completion = 5;
  world.record.opened_chest_count = 10;
  return world;
}

// Saves and restores every global the live entries read or write.
class LiveGlobals {
 public:
  LiveGlobals()
      : scoredata_(VS::State::scoreData),
        flags_(VS::State::stateFlags),
        rooms_(State_roomVisitProgress),
        settings_(VS::Input::settings),
        cursor_(Battle_unusedMenu9CursorMemory),
        position_(Menu_Score_cameraPositionBackup),
        angles_(Menu_Score_cameraAnglesBackup) {
    std::memcpy(masks_.data(), Field_countableRoomMask,
                sizeof Field_countableRoomMask);
    std::memcpy(actions_.data(), Battle_actionDefinitions,
                sizeof Battle_actionDefinitions);
    std::memcpy(scratch_.data(), VS::Core::ScratchBase(), scratch_.size());
    VS::State::scoreData = {};
    VS::State::stateFlags = {};
    State_roomVisitProgress = {};
    std::memset(Field_countableRoomMask, 0, sizeof Field_countableRoomMask);
    for (Battle_ActionDefinition& action : Battle_actionDefinitions) {
      action.unlocked = 0;
    }
    for (State_TrialScore(&boss)[3] :
         VS::State::scoreData.bossTimeTrialScores) {
      boss[0].time = s::kUnclearedTrialTime;
    }
  }
  LiveGlobals(const LiveGlobals&) = delete;
  LiveGlobals& operator=(const LiveGlobals&) = delete;
  LiveGlobals(LiveGlobals&&) = delete;
  LiveGlobals& operator=(LiveGlobals&&) = delete;
  ~LiveGlobals() {
    VS::State::scoreData = scoredata_;
    VS::State::stateFlags = flags_;
    State_roomVisitProgress = rooms_;
    VS::Input::settings = settings_;
    Battle_unusedMenu9CursorMemory = cursor_;
    Menu_Score_cameraPositionBackup = position_;
    Menu_Score_cameraAnglesBackup = angles_;
    std::memcpy(Field_countableRoomMask, masks_.data(),
                sizeof Field_countableRoomMask);
    std::memcpy(Battle_actionDefinitions, actions_.data(),
                sizeof Battle_actionDefinitions);
    std::memcpy(VS::Core::ScratchBase(), scratch_.data(), scratch_.size());
  }

 private:
  State_ScoreData scoredata_;
  State_StateFlags flags_;
  State_RoomVisitFlags rooms_;
  State_Settings settings_;
  Menu_Score_CursorMemory cursor_;
  VECTOR position_;
  VECTOR angles_;
  std::array<std::byte, sizeof Field_countableRoomMask> masks_{};
  std::array<std::byte, sizeof Battle_actionDefinitions> actions_{};
  std::array<std::byte, 0x400> scratch_{};
};

void CheckChainTiers() {
  CHECK(ChainScore(5) == 50);
  CHECK(ChainScore(6) == 120);
  CHECK(ChainScore(30) == 9600);
  CHECK(ChainScore(31) == 19840);
  CHECK(ChainScore(50) == 32000);
  CHECK(ChainScore(51) == 65280);
  CHECK(ChainScore(0xFFFF) == 0xFFFFU * 1280U);
}

void CheckTimeMultiplier() {
  CHECK(ClearScore(0, 0) == 0);
  CHECK(ClearScore(1, 600) == 100000);
  CHECK(ClearScore(1, 599) == 125000);
  CHECK(ClearScore(1, 540) == 125000);
  CHECK(ClearScore(1, 539) == 150000);
  CHECK(ClearScore(1, 300) == 150000);
  CHECK(ClearScore(1, 299) == 200000);
}

void CheckThresholdTitles() {
  s::ScoreRecord record = Uncleared();
  record.opened_chest_count = 51;
  record.map_completion = 360;
  record.max_chain = 29;
  CHECK(Titles(record) == 0);
  record.opened_chest_count = 52;
  record.map_completion = 361;
  record.max_chain = 30;
  CHECK(Titles(record) == (Bit(2) | Bit(3) | Bit(11)));
  CHECK(Titles(Uncleared(), {}, 1) == Bit(0));
}

void CheckConditionTitles() {
  for (std::size_t boss = 0; boss < s::kTitleBossCount; ++boss) {
    s::TitleConditions conditions;
    conditions.bosses_defeated.at(boss) = true;
    CHECK(Titles(Uncleared(), conditions) ==
          Bit(4 + static_cast<unsigned>(boss)));
  }
  s::TitleConditions keys;
  keys.gold_key = true;
  keys.chest_key = true;
  CHECK(Titles(Uncleared(), keys) == (Bit(12) | Bit(13)));
}

void CheckActionTitles() {
  CHECK(Titles(Uncleared(), WithActions(184, 224)) == Bit(14));
  CHECK(Titles(Uncleared(), WithActions(184, 223)) == 0);
  s::TitleConditions early = WithActions(22, 54);
  for (const std::size_t skipped : std::array<std::size_t, 4>{33, 36, 38, 39}) {
    early.actions_unlocked.at(skipped) = false;
  }
  CHECK(Titles(Uncleared(), early) == Bit(15));
  early.actions_unlocked.at(53) = false;
  CHECK(Titles(Uncleared(), early) == 0);
}

void CheckCountTitles() {
  s::ScoreRecord record = Uncleared();
  record.enemy_kills.fill(5000);
  record.weapon_attacks.fill(5000);
  CHECK(Titles(record) == (Bit(20) | Bit(21) | 0xFFC00000U));
  record.enemy_kills[5] = 4999;
  record.weapon_attacks.fill(499);
  record.weapon_attacks[0] = 500;
  CHECK(Titles(record) == Bit(31));
  record.weapon_attacks[0] = 0;
  record.weapon_attacks[9] = 500;
  CHECK(Titles(record) == Bit(30));
}

void CheckRankRows(std::uint32_t (*rank)(std::uint32_t, std::uint32_t)) {
  CHECK(rank(0xFFFF, 100'000'000) == 0);
  CHECK(rank(0xFFFF, 99'999'999) == 1);
  CHECK(rank(0x0FFF, 75'000'000) == 1);
  CHECK(rank(0x07FF, 75'000'000) == 2);
  CHECK(rank(0xFFFF0000U, 100'000'000) == 3);
  CHECK(rank(0, 999'999) == 14);
  CHECK(rank(0, 499'999) == 15);
}

std::uint32_t ResultsRankRow(std::uint32_t titles, std::uint32_t score) {
  return static_cast<std::uint32_t>(ResultsRankOrMax(titles, score));
}

void SetLiveScoreInputs() {
  VS::State::scoreData.titles = 0x00070000;
  for (u_short& kills : VS::State::scoreData.enemyKills) {
    kills = 5000;
  }
  VS::State::scoreData.bossTimeTrialScores[3][0].time = 0x123;
  VS::State::scoreData.bossTimeTrialScores[4][0].round = 5;
  VS::State::scoreData.maxChain = 30;
  VS::State::scoreData.completionTimeMinutes = 299;
  VS::State::scoreData.streakScore = 7;
  VS::State::scoreData.miscScore = 11;
  VS::State::stateFlags.asuraDefeated = 1;
  VS::State::stateFlags.goldKeyObtained = static_cast<char>(0x80);
}

// 5000 kills of each class (1600000), one cleared trial (20000), a 30 chain
// (9600) and one clear (100000), doubled for 299 minutes, then streak 7 and
// misc 11.
constexpr std::uint32_t kLiveScore = 3'459'218;
// The kept 0x70000 plus the clear, Asura, chain, gold key and kills titles.
constexpr std::uint32_t kLiveTitles = 0x00171A01;

void CheckProgressStatics() {
  CHECK(Menu_Score_mapCompletion == 6);
  CHECK(Menu_Score_openedChestCount == 3);
  CHECK(Menu_Score_clearCount == 2);
  CHECK(Menu_Score_menuState == 0);
  CHECK(Menu_Score_hideUi == 0);
  CHECK(State_roomVisitProgress.roomFlags[1] == 0xC00000);
  CHECK(VS::State::scoreData.mapCompletion == 6);
  CHECK(VS::State::scoreData.openedChestCount == 10);
}

void SetLiveProgressInputs() {
  const s::ProgressWorld world = SampleWorld();
  State_roomVisitProgress.roomFlags[0] = static_cast<int>(world.room_flags[0]);
  State_roomVisitProgress.roomFlags[1] = static_cast<int>(world.room_flags[1]);
  Field_countableRoomMask[0] = static_cast<int>(world.countable_room_masks[0]);
  Field_countableRoomMask[1] = -1;
  VS::State::stateFlags.chestsOpened[0] = 1;
  VS::State::stateFlags.chestsOpened[5] = static_cast<char>(0x80);
  VS::State::stateFlags.chestsOpened[63] = 2;
  VS::State::stateFlags.clearCount = 2;
  VS::State::scoreData.completionTimeMinutes = 1000;
  VS::State::scoreData.mapCompletion = 5;
  VS::State::scoreData.openedChestCount = 10;
  Menu_Score_menuState = 7;
  Menu_Score_hideUi = 1;
}
}  // namespace

TEST_CASE("F02835 score sums kills attacks and cleared trials",
          "[score-rank][F02835]") {
  s::ScoreRecord record = Uncleared();
  record.enemy_kills.fill(1);
  record.weapon_attacks.fill(1);
  record.boss_trial_words[2] = 0x0F800000;
  record.boss_trial_words[6] = 0x123;
  CHECK(s::CalculateScore(record, 0) == 320 + 640 + 20000);
  CHECK(s::CalculateScore(s::ScoreRecord{}, 0) == 8 * 20000);
}

TEST_CASE("F02835 chain bonus tiers switch below 6 11 16 21 26 31 and 51",
          "[score-rank][F02835]") {
  CheckChainTiers();
}

TEST_CASE("F02835 clear bonus applies the completion time multiplier",
          "[score-rank][F02835]") {
  CheckTimeMultiplier();
}

TEST_CASE("F02835 score wraps at 32 bits and clamps to 999999999",
          "[score-rank][F02835]") {
  CHECK(ClearScore(42950, 1000) == 32704);
  CHECK(ClearScore(65535, 1000) == s::kMaxScore);
  s::ScoreRecord record = Uncleared();
  record.streak_score = 0xFFFFFFFF;
  record.misc_score = 2;
  CHECK(s::CalculateScore(record, 0) == 1);
  record.streak_score = s::kMaxScore;
  record.misc_score = 1;
  CHECK(s::CalculateScore(record, 0) == s::kMaxScore);
}

TEST_CASE("F03061 results score is the shared policy on the int clear count",
          "[score-rank][F03061]") {
  s::ScoreRecord record = Uncleared();
  record.max_chain = 51;
  record.completion_time_minutes = 299;
  CHECK(s::ScoreFromRecord(record, 1) == (51U * 1280U + 100000U) * 2U);
  record.completion_time_minutes = 1000;
  CHECK(s::ScoreFromRecord(record, 42950) == 51U * 1280U + 32704U);
  CHECK(s::ScoreFromRecord(record, 0xFFFFFFFFU) == s::kMaxScore);
}

TEST_CASE("F02833 count thresholds and the clear earn titles",
          "[score-rank][F02833]") {
  CheckThresholdTitles();
}

TEST_CASE("F02833 bosses and keys earn titles", "[score-rank][F02833]") {
  CheckConditionTitles();
}

TEST_CASE("F02833 action sets earn titles with the skipped early actions",
          "[score-rank][F02833]") {
  CheckActionTitles();
}

TEST_CASE("F02833 kill and weapon counts earn titles", "[score-rank][F02833]") {
  CheckCountTitles();
}

TEST_CASE("F02833 earned titles keep the existing bits and uncleared trials",
          "[score-rank][F02833]") {
  s::ScoreRecord record = Uncleared();
  record.titles = 0x00030000;
  CHECK(Titles(record) == 0x00030000);
  record.boss_trial_words.fill(0x7FFFFF);
  CHECK(Titles(record) == (0x00030000U | Bit(10)));
  record.boss_trial_words[7] = 0xFF800000;
  CHECK(Titles(record) == 0x00030000);
}

TEST_CASE("F02832 rank rows need both the title count and the score",
          "[score-rank][F02832]") {
  CheckRankRows(RankOrMax);
}

TEST_CASE("F03060 results rank rows need both the title count and the score",
          "[score-rank][F03060]") {
  CheckRankRows(ResultsRankRow);
}

TEST_CASE("F04157 progress counts rooms and chests and raises the record",
          "[score-rank][F04157]") {
  s::ProgressWorld world = SampleWorld();
  RecordingHost host;
  const s::ScoreScreenProgress progress = s::CalculateProgress(world, host);
  CHECK(progress.map_completion == 6);
  CHECK(progress.opened_chest_count == 3);
  CHECK(progress.clear_count == 2);
  CHECK(world.room_flags[1] == 0xC00000);
  CHECK(world.record.map_completion == 6);
  CHECK(world.record.opened_chest_count == 10);
  CHECK(host.calls == std::vector<std::string>{"reset", "cursor", "camera"});
}

TEST_CASE("F04157 progress awards titles before the score and rank",
          "[score-rank][F04157]") {
  s::ProgressWorld world = SampleWorld();
  world.cursor_memory = true;
  world.chests_opened.fill(1);
  world.record.opened_chest_count = 0;
  RecordingHost host;
  const s::ScoreScreenProgress progress = s::CalculateProgress(world, host);
  CHECK(world.record.opened_chest_count == 64);
  CHECK(world.record.titles == (Bit(0) | Bit(2)));
  CHECK(progress.score == 200000);
  CHECK(progress.rank == std::optional<std::uint16_t>{15});
  CHECK(host.calls == std::vector<std::string>{"reset", "camera"});
}

TEST_CASE("F04157 progress keeps unaliased room flags and a larger record",
          "[score-rank][F04157]") {
  s::ProgressWorld world = SampleWorld();
  world.room_flags[1] = 0x400000;
  world.record.map_completion = 0xFFFFFFFF;
  RecordingHost host;
  const s::ScoreScreenProgress progress = s::CalculateProgress(world, host);
  CHECK(world.room_flags[1] == 0x400000);
  CHECK(progress.map_completion == 5);
  CHECK(world.record.map_completion == 0xFFFFFFFF);
}

TEST_CASE("F02833 F02835 F02832 live entries bind the score screen globals",
          "[score-rank-live][F02833][F02835][F02832]") {
  const LiveGlobals globals;
  SetLiveScoreInputs();
  Menu_Score_clearCount = 1;
  Menu_Score_characterRank = 0xFF;
  VS::Menu::Score::AwardLiveTitles();
  CHECK(static_cast<std::uint32_t>(VS::State::scoreData.titles) == kLiveTitles);
  VS::Menu::Score::CalculateLiveScore();
  CHECK(Menu_Score_score == kLiveScore);
  VS::Menu::Score::DetermineLiveRiskbreakerRank();
  CHECK(Menu_Score_characterRank == 11);
}

TEST_CASE("F03061 F03060 live entries bind the results screen globals",
          "[score-rank-live][F03061][F03060]") {
  const LiveGlobals globals;
  SetLiveScoreInputs();
  VS::State::scoreData.titles = static_cast<int>(kLiveTitles);
  Menu_Results_clearCount = 1;
  Menu_Results_rank = 99;
  VS::Menu::Results::CalculateLiveScore();
  CHECK(Menu_Results_score == kLiveScore);
  VS::Menu::Results::DetermineLiveRiskbreakerRank();
  CHECK(Menu_Results_rank == 11);
}

TEST_CASE("F04157 live progress binds the room chest and score globals",
          "[score-rank-live][F04157]") {
  const LiveGlobals globals;
  SetLiveProgressInputs();
  VS::Input::settings.cursorMemory = 0;
  std::memset(&Battle_unusedMenu9CursorMemory, 0xA5,
              sizeof Battle_unusedMenu9CursorMemory);
  auto* scratch =
      static_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
  scratch->camera.position.vy = 1234;
  scratch->camera.angles.vz = -56;
  CHECK(VS::Menu::Score::CalculateLiveProgress() == 1);
  CheckProgressStatics();
  CHECK(Battle_unusedMenu9CursorMemory.titlePage == 0);
  CHECK(Battle_unusedMenu9CursorMemory.gazetteRow == 0);
  CHECK(Menu_Score_cameraPositionBackup.vy == 1234);
  CHECK(Menu_Score_cameraAnglesBackup.vz == -56);
  CHECK(Menu_Score_score == 200000);
}
