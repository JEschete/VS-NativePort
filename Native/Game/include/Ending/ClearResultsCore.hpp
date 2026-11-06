// Hand-maintained (tools/port/owned.csv).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "Ending/DisplayHost.hpp"
#include "Render/TextureRegionDraw.hpp"

// ENDING.PRG's clear results: the progress a cleared game commits (titles,
// best time, the reset playthrough flags), the score and Riskbreaker rank, and
// the congratulations screen that counts them up. The progress arrives as a
// typed record and the atlas, rank tables and tints as spans and values;
// EndingLive.cpp binds them to the game's records and the module image.
namespace VS::Ending {
inline constexpr std::size_t kRoomFlagWords = 16;
inline constexpr std::size_t kChestCount = 64;
inline constexpr std::size_t kBossTrialCount = 8;
inline constexpr std::size_t kBossTitleCount = 6;
inline constexpr std::size_t kEnemyClassCount = 6;
inline constexpr std::size_t kWeaponCategoryCount = 10;
inline constexpr std::size_t kActionCount = 256;
inline constexpr std::size_t kRankCount = 16;
inline constexpr std::size_t kRankLabelRegions = 3;
// Ending_congratulationsTint's entries.
inline constexpr std::size_t kCongratulationsTints = 3;
// END.DIS (the results atlas), then ENDSCR00..15.DIS, one screen per rank.
inline constexpr std::size_t kResultsAtlasFile = 0;
inline constexpr std::size_t kResultsFileCount = kRankCount + 1;
// A boss time trial's first record holds this time until it is set.
inline constexpr std::uint32_t kUnsetTrialTime = 0x800000;
inline constexpr std::uint32_t kMaximumScore = 999999999;

struct PlayTimeRecord {
  std::int8_t hours = 0;
  std::int8_t minutes = 0;
};

// The State_ScoreData fields the ending reads or writes.
struct ScoreRecord {
  std::uint32_t titles = 0;
  std::array<std::uint16_t, kEnemyClassCount> enemy_kills{};
  std::array<std::uint16_t, kWeaponCategoryCount> weapon_attacks{};
  // Each boss's first time-trial record.
  std::array<std::uint32_t, kBossTrialCount> boss_trial_times{};
  std::uint16_t max_chain = 0;
  std::uint32_t clear_count = 0;
  std::uint32_t completion_time_minutes = 0;
  std::uint32_t map_completion = 0;
  std::uint32_t opened_chest_count = 0;
  std::uint32_t streak_score = 0;
  std::uint32_t misc_score = 0;
};

// The State_StateFlags fields the ending reads or writes.
struct PlaythroughFlags {
  std::uint8_t clear_count = 0;
  std::uint8_t difficulty = 0;
  std::uint8_t normal_mode_cleared = 0;
  bool magic_used = false;
  bool battle_abilities_used = false;
  bool break_arts_used = false;
  // Damascus Golem, Damascus Crab, Ravana, Dragon Zombie, Death and Ogre
  // Zombie, Asura.
  std::array<bool, kBossTitleCount> bosses_defeated{};
  bool gold_key_obtained = false;
  bool chest_key_obtained = false;
  std::array<std::uint8_t, kChestCount> chests_opened{};
};

struct ClearProgress {
  PlayTimeRecord play_time{};
  ScoreRecord score{};
  PlaythroughFlags flags{};
  std::array<std::uint32_t, kRoomFlagWords> visited_rooms{};
  std::array<bool, kActionCount> unlocked_actions{};
  std::uint16_t save_count = 0;
  std::uint8_t entry_zone = 0;
  std::uint8_t entry_room = 0;
};

// The results screen's load phase, score, rank and count-up animation.
struct ResultsState {
  int load_phase = 0;
  std::uint32_t rank = 0;
  int clear_count = 0;
  std::uint32_t final_score = 0;
  int map_completion = 0;
  std::uint32_t displayed_map_completion = 0;
  std::int32_t displayed_score = 0;
  int animation_frame = 0;
  bool is_leaving = false;
  bool is_score_incrementing = false;
  bool is_map_completion_incrementing = false;
};

using RegionColorPair = std::array<Render::RegionColor, 2>;

// Each draw's color pair; the draws set the left color's step to their fade.
struct ResultsTints {
  std::array<Render::RegionColor, kCongratulationsTints> congratulations{};
  RegionColorPair score{};
  RegionColorPair counting_score{};
  RegionColorPair map_completion{};
  RegionColorPair counting_map_completion{};
  RegionColorPair rank_heading{};
  RegionColorPair rank_label{};
};

// A rank's label: its region count, then up to three atlas regions.
using RankLabel = std::array<std::uint8_t, kRankLabelRegions + 1>;

struct ResultsTables {
  std::span<const std::uint32_t> countable_room_bits{};
  std::span<const Render::TextureRegion> atlas{};
  std::span<const RankLabel> rank_labels{};
  std::span<const std::int16_t> rank_title_thresholds{};
  std::span<const std::uint32_t> rank_score_thresholds{};
  std::uint8_t rank_heading_width = 0;
  ResultsTints tints{};
};

class ProgressHost {
 public:
  virtual ~ProgressHost() = default;
  // Clears the room visits and the playthrough flags after the first 16
  // bytes (the clear count, difficulty and normal-mode clear stay).
  virtual void ResetPlaythroughRecords() = 0;
};

// The results screen's display, its atlas quads (RegionQuadTarget::Draw), its
// sounds and its disc reads.
class ResultsHost : public DisplayHost, public Render::RegionQuadTarget {
 public:
  virtual void PlaySound(int sound) = 0;
  virtual void StopSound(int sound) = 0;
  // Allocates a buffer for results file `file` and queues its read.
  virtual void BeginResultsRead(std::size_t file) = 0;
  [[nodiscard]] virtual bool IsResultsReadComplete() = 0;
  // The read file's bytes; empty when no read has started.
  [[nodiscard]] virtual std::span<const std::byte> ResultsData() = 0;
  virtual void EndResultsRead() = 0;
  virtual void FreeResultsData() = 0;
};

void CommitClearProgress(ClearProgress& progress,
                         std::span<const std::uint32_t> countable_room_bits,
                         ProgressHost& host);
void UpdateClearProgress(ClearProgress& progress,
                         std::span<const std::uint32_t> countable_room_bits,
                         ProgressHost& host);
void AwardClearTitles(ClearProgress& progress,
                      std::span<const std::uint32_t> countable_room_bits);
void ShowClearResults(ResultsState& state, ClearProgress& progress,
                      const ResultsTables& tables, ResultsHost& host);
// One step of the atlas and rank-screen loads; true once both are uploaded.
bool LoadClearResultsStep(ResultsState& state, ClearProgress& progress,
                          const ResultsTables& tables, ResultsHost& host);
// One frame of the screen; true once it has faded out after a button press.
bool DrawClearResults(ResultsState& state, const ResultsTables& tables,
                      ResultsHost& host);
void DrawRankBackground(int fade_frame, DisplayHost& host);
void DrawCongratulations(ScreenPoint origin, int fade_frame,
                         const ResultsTables& tables, ResultsHost& host);
void DrawScore(ScreenPoint origin, int fade_frame, const ResultsState& state,
               const ResultsTables& tables, ResultsHost& host);
void DrawCountingScore(ScreenPoint origin, int animation_frame,
                       ResultsState& state, const ResultsTables& tables,
                       ResultsHost& host);
void DrawMapCompletion(ScreenPoint origin, int fade_frame,
                       const ResultsState& state, const ResultsTables& tables,
                       ResultsHost& host);
void DrawCountingMapCompletion(ScreenPoint origin, int fade_frame,
                               ResultsState& state, const ResultsTables& tables,
                               ResultsHost& host);
void DrawRiskbreakerRankHeading(ScreenPoint origin, int timer,
                                const ResultsTables& tables, ResultsHost& host);
void DrawRiskbreakerRank(ScreenPoint origin, int timer,
                         const ResultsState& state, const ResultsTables& tables,
                         ResultsHost& host);
// The first rank whose title and score thresholds the clear meets; the rank
// stays as it was when it meets none.
void DetermineRiskbreakerRank(ResultsState& state, const ScoreRecord& score,
                              const ResultsTables& tables);
void CalculateClearScore(ResultsState& state, const ScoreRecord& score);
}  // namespace VS::Ending
