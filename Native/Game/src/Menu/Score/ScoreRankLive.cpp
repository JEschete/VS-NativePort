// Hand-maintained (tools/port/owned.csv).
// Binds the typed score screen policies (ScoreRank.cpp) to the live globals
// the generated MENU9.PRG functions used.
#include "Menu/Score/ScoreRankLive.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>

#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/StatusEffects.h"
#include "Core/Memory/WordCopyLive.hpp"
#include "Core/Scratch.hpp"
#include "Field/MapProgress.h"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Score/Records.h"
#include "Menu/Score/ScoreRank.hpp"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Render.hpp"
#include "State/Progress.h"
#include "State/StateFlags.h"

namespace VS::Menu::Score {
namespace {
static_assert(std::size(Battle_actionDefinitions) >= kTitleActionCount);
static_assert(std::size(State_roomVisitProgress.roomFlags) == kRoomFlagWords);
static_assert(std::size(Field_countableRoomMask) == kRoomFlagWords);
static_assert(std::size(VS::State::stateFlags.chestsOpened) == kChestFlagCount);
static_assert(std::size(VS::State::scoreData.enemyKills) ==
              kScoreEnemyClassCount);
static_assert(std::size(VS::State::scoreData.weaponAttacks) ==
              kScoreWeaponClassCount);
static_assert(std::size(VS::State::scoreData.bossTimeTrialScores) ==
              kBossTrialCount);

constexpr std::size_t kBestTrialRank = 0;
constexpr unsigned kTrialRoundShift = 24;
constexpr unsigned kTrialDifficultyShift = 31;

std::uint32_t BestTrialWord(std::size_t boss) {
  // The original reads each boss's rank 0 record as a word (lw, then masks the
  // 24-bit time).
  const State_TrialScore& score =
      VS::State::scoreData.bossTimeTrialScores[boss][kBestTrialRank];
  return static_cast<std::uint32_t>(score.time) |
         (static_cast<std::uint32_t>(score.round) << kTrialRoundShift) |
         (static_cast<std::uint32_t>(score.difficulty)
          << kTrialDifficultyShift);
}

bool IsSet(char flag) { return static_cast<std::uint8_t>(flag) != 0; }

class LiveProgressHost final : public ProgressHost {
 public:
  void ResetScreen() override {
    Menu_Score_menuState = 0;
    Menu_Score_hideUi = 0;
  }
  void ClearCursorMemory() override {
    Core::Memory::ZeroLiveAlignedWords(
        &Battle_unusedMenu9CursorMemory,
        static_cast<std::int32_t>(sizeof Battle_unusedMenu9CursorMemory));
  }
  void BackupCamera() override {
    auto* scratch = static_cast<VS::Render::FrameScratch*>(Core::ScratchBase());
    Core::Memory::CopyLiveAlignedWords(&Menu_Score_cameraPositionBackup,
                                       &scratch->camera.position,
                                       sizeof Menu_Score_cameraPositionBackup);
    Core::Memory::CopyLiveAlignedWords(&Menu_Score_cameraAnglesBackup,
                                       &scratch->camera.angles.vx,
                                       sizeof Menu_Score_cameraAnglesBackup);
  }
};

ProgressWorld ReadLiveProgressWorld() {
  ProgressWorld world;
  for (std::size_t i = 0; i < kRoomFlagWords; ++i) {
    world.room_flags.at(i) =
        static_cast<std::uint32_t>(State_roomVisitProgress.roomFlags[i]);
    world.countable_room_masks.at(i) =
        static_cast<std::uint32_t>(Field_countableRoomMask[i]);
  }
  for (std::size_t i = 0; i < kChestFlagCount; ++i) {
    world.chests_opened.at(i) =
        static_cast<std::uint8_t>(VS::State::stateFlags.chestsOpened[i]);
  }
  world.clear_count =
      static_cast<std::uint8_t>(VS::State::stateFlags.clearCount);
  world.cursor_memory = IsSet(VS::Input::settings.cursorMemory);
  world.conditions = ReadLiveTitleConditions();
  world.record = ReadLiveScoreRecord();
  return world;
}
}  // namespace

ScoreRecord ReadLiveScoreRecord() {
  const State_ScoreData& data = VS::State::scoreData;
  ScoreRecord record;
  record.titles = static_cast<std::uint32_t>(data.titles);
  for (std::size_t i = 0; i < kScoreEnemyClassCount; ++i) {
    record.enemy_kills.at(i) = data.enemyKills[i];
  }
  for (std::size_t i = 0; i < kScoreWeaponClassCount; ++i) {
    record.weapon_attacks.at(i) = data.weaponAttacks[i];
  }
  for (std::size_t i = 0; i < kBossTrialCount; ++i) {
    record.boss_trial_words.at(i) = BestTrialWord(i);
  }
  record.max_chain = data.maxChain;
  record.completion_time_minutes = data.completionTimeMinutes;
  record.map_completion = data.mapCompletion;
  record.opened_chest_count = data.openedChestCount;
  record.streak_score = data.streakScore;
  record.misc_score = data.miscScore;
  return record;
}

TitleConditions ReadLiveTitleConditions() {
  const State_StateFlags& flags = VS::State::stateFlags;
  TitleConditions conditions;
  conditions.bosses_defeated = {IsSet(flags.damascusGolemDefeated),
                                IsSet(flags.damascusCrabDefeated),
                                IsSet(flags.ravanaDefeated),
                                IsSet(flags.dragonZombieDefeated),
                                IsSet(flags.deathAndOgreZombieDefeated),
                                IsSet(flags.asuraDefeated)};
  conditions.gold_key = IsSet(flags.goldKeyObtained);
  conditions.chest_key = IsSet(flags.chestKeyObtained);
  for (std::size_t i = 0; i < kTitleActionCount; ++i) {
    conditions.actions_unlocked.at(i) =
        Battle_actionDefinitions[i].unlocked != 0;
  }
  return conditions;
}

// decomp: MENU9.PRG _setTitleFlags (src/MENU/MENU9.PRG/238.c:2662).
void AwardLiveTitles() {
  const std::uint32_t titles = AwardTitles(
      ReadLiveScoreRecord(), Menu_Score_clearCount, ReadLiveTitleConditions());
  VS::State::scoreData.titles = static_cast<int>(titles);
}

// decomp: MENU9.PRG _calculateScore (src/MENU/MENU9.PRG/238.c:2842).
void CalculateLiveScore() {
  Menu_Score_score =
      CalculateScore(ReadLiveScoreRecord(), Menu_Score_clearCount);
}

// decomp: MENU9.PRG _determineCharacterRank (src/MENU/MENU9.PRG/238.c:2633).
void DetermineLiveRiskbreakerRank() {
  const auto rank = DetermineRiskbreakerRank(
      static_cast<std::uint32_t>(VS::State::scoreData.titles),
      static_cast<std::uint32_t>(Menu_Score_score));
  if (rank.has_value()) {
    Menu_Score_characterRank = *rank;
  }
}

// decomp: MENU9.PRG _calculateProgress (src/MENU/MENU9.PRG/238.c:418).
int CalculateLiveProgress() {
  ProgressWorld world = ReadLiveProgressWorld();
  LiveProgressHost host;
  const ScoreScreenProgress progress = CalculateProgress(world, host);

  State_roomVisitProgress.roomFlags[1] = static_cast<int>(world.room_flags[1]);
  Menu_Score_mapCompletion = progress.map_completion;
  Menu_Score_openedChestCount = progress.opened_chest_count;
  Menu_Score_clearCount = progress.clear_count;
  VS::State::scoreData.mapCompletion = world.record.map_completion;
  VS::State::scoreData.openedChestCount = world.record.opened_chest_count;
  VS::State::scoreData.titles = static_cast<int>(world.record.titles);
  Menu_Score_score = progress.score;
  if (progress.rank.has_value()) {
    Menu_Score_characterRank = *progress.rank;
  }
  return 1;
}
}  // namespace VS::Menu::Score
