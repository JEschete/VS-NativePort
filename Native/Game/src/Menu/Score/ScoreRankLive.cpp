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
#include "Field/MapProgress.hpp"
#include "Input/ControllerLiveState.hpp"
#include "Legacy/Psx.h"
#include "Menu/Score/Records.hpp"
#include "Menu/Score/ScoreRank.hpp"
#include "Menu/Score/ScoreScreen.hpp"
#include "Render/Render.hpp"
#include "State/Progress.hpp"
#include "State/StateFlags.hpp"

namespace VS::Menu::Score {
namespace {
static_assert(std::size(Battle_actionDefinitions) >= kTitleActionCount);
static_assert(std::size(VS::State::roomVisitProgress.room_flags) ==
              kRoomFlagWords);
static_assert(std::size(VS::Field::countableRoomMask) == kRoomFlagWords);
static_assert(std::size(VS::State::stateFlags.chests_opened) ==
              kChestFlagCount);
static_assert(std::size(VS::State::scoreData.enemy_kills) ==
              kScoreEnemyClassCount);
static_assert(std::size(VS::State::scoreData.weapon_attacks) ==
              kScoreWeaponClassCount);
static_assert(std::size(VS::State::scoreData.boss_time_trial_scores) ==
              kBossTrialCount);

constexpr std::size_t kBestTrialRank = 0;
constexpr unsigned kTrialRoundShift = 24;
constexpr unsigned kTrialDifficultyShift = 31;

std::uint32_t BestTrialWord(std::size_t boss) {
  // The original reads each boss's rank 0 record as a word (lw, then masks the
  // 24-bit time).
  const VS::State::TrialScore& score =
      VS::State::scoreData.boss_time_trial_scores[boss][kBestTrialRank];
  return static_cast<std::uint32_t>(score.time) |
         (static_cast<std::uint32_t>(score.round) << kTrialRoundShift) |
         (static_cast<std::uint32_t>(score.difficulty)
          << kTrialDifficultyShift);
}

bool IsSet(std::uint8_t flag) { return flag != 0; }

class LiveProgressHost final : public ProgressHost {
 public:
  void ResetScreen() override {
    menuState = 0;
    hideUi = 0;
  }
  void ClearCursorMemory() override {
    Core::Memory::ZeroLiveAlignedWords(
        &Battle_unusedMenu9CursorMemory,
        static_cast<std::int32_t>(sizeof Battle_unusedMenu9CursorMemory));
  }
  void BackupCamera() override {
    auto* scratch = static_cast<VS::Render::FrameScratch*>(Core::ScratchBase());
    Core::Memory::CopyLiveAlignedWords(&cameraPositionBackup,
                                       &scratch->camera.position,
                                       sizeof cameraPositionBackup);
    Core::Memory::CopyLiveAlignedWords(&cameraAnglesBackup,
                                       &scratch->camera.angles.vx,
                                       sizeof cameraAnglesBackup);
  }
};

ProgressWorld ReadLiveProgressWorld() {
  ProgressWorld world;
  for (std::size_t i = 0; i < kRoomFlagWords; ++i) {
    world.room_flags.at(i) =
        static_cast<std::uint32_t>(VS::State::roomVisitProgress.room_flags[i]);
    world.countable_room_masks.at(i) = VS::Field::countableRoomMask[i];
  }
  for (std::size_t i = 0; i < kChestFlagCount; ++i) {
    world.chests_opened.at(i) =
        static_cast<std::uint8_t>(VS::State::stateFlags.chests_opened[i]);
  }
  world.clear_count =
      static_cast<std::uint8_t>(VS::State::stateFlags.clear_count);
  world.cursor_memory = IsSet(VS::Input::settings.cursor_memory);
  world.conditions = ReadLiveTitleConditions();
  world.record = ReadLiveScoreRecord();
  return world;
}
}  // namespace

ScoreRecord ReadLiveScoreRecord() {
  const VS::State::ScoreData& data = VS::State::scoreData;
  ScoreRecord record;
  record.titles = static_cast<std::uint32_t>(data.titles);
  for (std::size_t i = 0; i < kScoreEnemyClassCount; ++i) {
    record.enemy_kills.at(i) = data.enemy_kills[i];
  }
  for (std::size_t i = 0; i < kScoreWeaponClassCount; ++i) {
    record.weapon_attacks.at(i) = data.weapon_attacks[i];
  }
  for (std::size_t i = 0; i < kBossTrialCount; ++i) {
    record.boss_trial_words.at(i) = BestTrialWord(i);
  }
  record.max_chain = data.max_chain;
  record.completion_time_minutes = data.completion_time_minutes;
  record.map_completion = data.map_completion;
  record.opened_chest_count = data.opened_chest_count;
  record.streak_score = data.streak_score;
  record.misc_score = data.misc_score;
  return record;
}

TitleConditions ReadLiveTitleConditions() {
  const VS::State::StateFlags& flags = VS::State::stateFlags;
  TitleConditions conditions;
  conditions.bosses_defeated = {IsSet(flags.damascus_golem_defeated),
                                IsSet(flags.damascus_crab_defeated),
                                IsSet(flags.ravana_defeated),
                                IsSet(flags.dragon_zombie_defeated),
                                IsSet(flags.death_and_ogre_zombie_defeated),
                                IsSet(flags.asura_defeated)};
  conditions.gold_key = IsSet(flags.gold_key_obtained);
  conditions.chest_key = IsSet(flags.chest_key_obtained);
  for (std::size_t i = 0; i < kTitleActionCount; ++i) {
    conditions.actions_unlocked.at(i) =
        Battle_actionDefinitions[i].unlocked != 0;
  }
  return conditions;
}

// decomp: MENU9.PRG _setTitleFlags (src/MENU/MENU9.PRG/238.c:2662).
void AwardLiveTitles() {
  const std::uint32_t titles =
      AwardTitles(ReadLiveScoreRecord(), clearCount, ReadLiveTitleConditions());
  VS::State::scoreData.titles = static_cast<int>(titles);
}

// decomp: MENU9.PRG _calculateScore (src/MENU/MENU9.PRG/238.c:2842).
void CalculateLiveScore() {
  VS::Menu::Score::score = CalculateScore(ReadLiveScoreRecord(), clearCount);
}

// decomp: MENU9.PRG _determineCharacterRank (src/MENU/MENU9.PRG/238.c:2633).
void DetermineLiveRiskbreakerRank() {
  const auto rank = DetermineRiskbreakerRank(
      static_cast<std::uint32_t>(VS::State::scoreData.titles),
      VS::Menu::Score::score);
  if (rank.has_value()) {
    characterRank = *rank;
  }
}

// decomp: MENU9.PRG _calculateProgress (src/MENU/MENU9.PRG/238.c:418).
int CalculateLiveProgress() {
  ProgressWorld world = ReadLiveProgressWorld();
  LiveProgressHost host;
  const ScoreScreenProgress progress = CalculateProgress(world, host);

  VS::State::roomVisitProgress.room_flags[1] =
      static_cast<int>(world.room_flags[1]);
  mapCompletion = progress.map_completion;
  openedChestCount = progress.opened_chest_count;
  clearCount = progress.clear_count;
  VS::State::scoreData.map_completion = world.record.map_completion;
  VS::State::scoreData.opened_chest_count = world.record.opened_chest_count;
  VS::State::scoreData.titles = static_cast<int>(world.record.titles);
  VS::Menu::Score::score = progress.score;
  if (progress.rank.has_value()) {
    characterRank = *progress.rank;
  }
  return 1;
}
}  // namespace VS::Menu::Score
