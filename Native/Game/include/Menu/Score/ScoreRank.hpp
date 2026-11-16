#pragma once
// Hand-maintained (tools/port/owned.csv).
// MENU9.PRG score screen (gazette): the progress tally, the title awards, the
// score and the Riskbreaker rank (F04157, F02833, F02835, F02832). The score
// and rank policies are also MENUF.PRG's results screen _calculateScore and
// _determineRank (F03061, F03060): ScoreFromRecord and RiskbreakerRankIndex
// are those functions, bound by Menu/Results/ResultsRankLive.hpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Menu::Score {  // NOLINT(readability-identifier-naming)
inline constexpr std::size_t kScoreEnemyClassCount = 6;
inline constexpr std::size_t kScoreWeaponClassCount = 10;
inline constexpr std::size_t kBossTrialCount = 8;
inline constexpr std::size_t kTitleBossCount = 6;
inline constexpr std::size_t kRoomFlagWords = 16;
inline constexpr std::size_t kChestFlagCount = 64;
inline constexpr std::size_t kRiskbreakerRankCount = 16;
// Actions 0-223 are the ones the title checks read (vs_main_actions).
inline constexpr std::size_t kTitleActionCount = 224;
// A boss time trial's 24-bit time while the trial is uncleared.
inline constexpr std::uint32_t kUnclearedTrialTime = 0x800000;
inline constexpr std::uint32_t kTrialTimeMask = 0xFFFFFF;
inline constexpr std::uint32_t kMaxScore = 999'999'999;

// The fields of vs_main_scoredata (main.h:202) the score screen and results
// screen read. boss_trial_words are each boss's best (rank 0) trial words;
// only their low 24 bits (the time) count.
struct ScoreRecord {
  std::uint32_t titles = 0;                                            // 0x00
  std::array<std::uint16_t, kScoreEnemyClassCount> enemy_kills{};      // 0x04
  std::array<std::uint16_t, kScoreWeaponClassCount> weapon_attacks{};  // 0x14
  std::array<std::uint32_t, kBossTrialCount> boss_trial_words{};       // 0x28
  std::uint16_t max_chain = 0;                                         // 0x88
  std::uint32_t completion_time_minutes = 0;                           // 0x90
  std::uint32_t map_completion = 0;                                    // 0x94
  std::uint32_t opened_chest_count = 0;                                // 0x98
  std::uint32_t streak_score = 0;                                      // 0x104
  std::uint32_t misc_score = 0;                                        // 0x108
};

// The game-state conditions _setTitleFlags reads besides the score record:
// the six story bosses' defeat bytes (vs_main_stateFlags 0xCC-0xD1), the gold
// and chest keys (0x383, 0x387) and each action's unlocked bit.
struct TitleConditions {
  std::array<bool, kTitleBossCount> bosses_defeated{};
  bool gold_key = false;
  bool chest_key = false;
  std::array<bool, kTitleActionCount> actions_unlocked{};
};

// What _calculateProgress reads from the room visit flags, the countable
// room masks, the chest flags, the clear count byte and the cursor memory
// setting, plus the score record it raises.
struct ProgressWorld {
  std::array<std::uint32_t, kRoomFlagWords> room_flags{};
  std::array<std::uint32_t, kRoomFlagWords> countable_room_masks{};
  std::array<std::uint8_t, kChestFlagCount> chests_opened{};
  std::uint8_t clear_count = 0;
  bool cursor_memory = false;
  TitleConditions conditions{};
  ScoreRecord record{};
};

// The score screen statics _calculateProgress leaves. rank is nothing when no
// rank row matched (the original then keeps the previous rank).
struct ScoreScreenProgress {
  std::uint16_t map_completion = 0;
  std::uint16_t opened_chest_count = 0;
  std::uint16_t clear_count = 0;
  std::uint32_t score = 0;
  std::optional<std::uint16_t> rank;
};

// The non-policy steps of _calculateProgress, in the original's order.
class ProgressHost {
 public:
  ProgressHost() = default;
  ProgressHost(const ProgressHost&) = delete;
  ProgressHost& operator=(const ProgressHost&) = delete;
  ProgressHost(ProgressHost&&) = delete;
  ProgressHost& operator=(ProgressHost&&) = delete;
  virtual ~ProgressHost() = default;
  // _menuState = _hideUi = 0.
  virtual void ResetScreen() = 0;
  // Clears vs_battle_menu9CursorMemory (only with cursor memory off).
  virtual void ClearCursorMemory() = 0;
  // Copies the scratchpad camera position and angles to the backups.
  virtual void BackupCamera() = 0;
};

// decomp: MENUF.PRG _calculateScore (3B8.c:2801), also the body of MENU9.PRG
// _calculateScore (238.c:2842). The shared score policy: kills and weapon
// attacks by class, cleared boss trials, the chain bonus, clear_count (the
// 32-bit value the original multiplies) times 100000, the completion-time
// multiplier, then the streak and misc scores, in wrapping unsigned 32-bit
// arithmetic, clamped to kMaxScore.
[[nodiscard]] std::uint32_t ScoreFromRecord(const ScoreRecord& record,
                                            std::uint32_t clear_count) noexcept;

// decomp: MENUF.PRG _determineRank (3B8.c:2774), also the body of MENU9.PRG
// _determineCharacterRank (238.c:2633). The shared rank policy: the first of
// the sixteen rows whose title count (titles bits 0-15) and unsigned score
// thresholds are met.
[[nodiscard]] std::optional<std::size_t> RiskbreakerRankIndex(
    std::uint32_t titles, std::uint32_t score) noexcept;

// decomp: MENU9.PRG _determineCharacterRank (238.c:2633).
[[nodiscard]] std::optional<std::uint16_t> DetermineRiskbreakerRank(
    std::uint32_t titles, std::uint32_t score) noexcept;

// decomp: MENU9.PRG _setTitleFlags (238.c:2662). Returns record.titles with
// every earned title bit set.
[[nodiscard]] std::uint32_t AwardTitles(
    const ScoreRecord& record, std::uint16_t clear_count,
    const TitleConditions& conditions) noexcept;

// decomp: MENU9.PRG _calculateScore (238.c:2842).
[[nodiscard]] std::uint32_t CalculateScore(const ScoreRecord& record,
                                           std::uint16_t clear_count) noexcept;

// decomp: MENU9.PRG _calculateProgress (238.c:418). Updates world's room
// flags and record (best map completion and chest count, then the titles)
// and returns the statics it computes.
ScoreScreenProgress CalculateProgress(ProgressWorld& world, ProgressHost& host);
}  // namespace VS::Menu::Score
