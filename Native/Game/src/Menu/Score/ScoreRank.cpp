// Hand-maintained (tools/port/owned.csv).
#include "Menu/Score/ScoreRank.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace VS::Menu::Score {
namespace {
constexpr std::uint32_t kClearedTrialPoints = 20000;
constexpr std::uint32_t kClearPoints = 100000;

constexpr std::uint32_t kLongChainPoints = 1280;

// A clear under 600 minutes multiplies the score: +1/4 from 540 minutes,
// +1/2 from 300, otherwise twice.
constexpr std::uint32_t kFastClearMinutes = 600;
constexpr std::uint32_t kQuarterBonusMinutes = 540;
constexpr std::uint32_t kHalfBonusMinutes = 300;

constexpr std::uint32_t kRankTitleMask = 0xFFFF;

// Title bits and thresholds (238.c:2662-2799).
constexpr unsigned kClearedTitle = 0;
constexpr unsigned kChestsTitle = 2;
constexpr unsigned kMapTitle = 3;
constexpr unsigned kFirstBossTitle = 4;
constexpr unsigned kTrialsTitle = 10;
constexpr unsigned kChainTitle = 11;
constexpr unsigned kGoldKeyTitle = 12;
constexpr unsigned kChestKeyTitle = 13;
constexpr unsigned kLateActionsTitle = 14;
constexpr unsigned kEarlyActionsTitle = 15;
constexpr unsigned kAllKillsTitle = 20;
constexpr unsigned kAllAttacksTitle = 21;
constexpr unsigned kFirstWeaponClassTitle = 22;
constexpr unsigned kFirstWeaponClassMasteryTitle = 31;
constexpr std::uint32_t kChestTitleCount = 52;
constexpr std::uint32_t kMapTitleRooms = 360;
constexpr std::uint16_t kChainTitleLength = 30;
constexpr std::uint16_t kVeteranCount = 5000;
constexpr std::uint16_t kWeaponMasteryAttacks = 500;
constexpr std::size_t kLateActionsFirst = 184;
constexpr std::size_t kEarlyActionsFirst = 22;
constexpr std::size_t kEarlyActionsEnd = 54;
// Actions 33, 36, 38 and 39 are skipped by the early-actions title.
constexpr std::array<std::size_t, 4> kSkippedEarlyActions{33, 36, 38, 39};

constexpr std::uint32_t kRoomFlagWithAlias = 0x800000;
constexpr std::uint32_t kRoomFlagAlias = 0x400000;
constexpr std::size_t kRoomFlagAliasWord = 1;

constexpr std::uint32_t Bit(unsigned index) {
  return std::uint32_t{1} << index;
}

// The chain bonus multiplies the longest chain by the first tier it is below.
struct ChainTier {
  std::uint32_t below;
  std::uint32_t points;
};

std::uint32_t ChainPoints(std::uint16_t chain) {
  static constexpr std::array<ChainTier, 7> kChainTiers{
      {{6, 10}, {11, 20}, {16, 40}, {21, 80}, {26, 160}, {31, 320}, {51, 640}}};
  for (const ChainTier& tier : kChainTiers) {
    if (chain < tier.below) {
      return tier.points;
    }
  }
  return kLongChainPoints;
}

bool AllUnlocked(const TitleConditions& conditions, std::size_t first,
                 std::size_t end) {
  for (std::size_t i = first; i < end; ++i) {
    bool skipped = false;
    if (first == kEarlyActionsFirst) {
      for (const std::size_t skip : kSkippedEarlyActions) {
        skipped = skipped || i == skip;
      }
    }
    if (!skipped && !conditions.actions_unlocked.at(i)) {
      return false;
    }
  }
  return true;
}

template <std::size_t N>
bool AllReach(const std::array<std::uint16_t, N>& counts,
              std::uint16_t threshold) {
  for (const std::uint16_t count : counts) {
    if (count < threshold) {
      return false;
    }
  }
  return true;
}

bool AllTrialsCleared(const ScoreRecord& record) {
  for (const std::uint32_t word : record.boss_trial_words) {
    if ((word & kTrialTimeMask) == kUnclearedTrialTime) {
      return false;
    }
  }
  return true;
}

std::uint32_t RecordTitles(const ScoreRecord& record) {
  std::uint32_t titles = 0;
  if (record.opened_chest_count >= kChestTitleCount) {
    titles |= Bit(kChestsTitle);
  }
  if (record.map_completion > kMapTitleRooms) {
    titles |= Bit(kMapTitle);
  }
  if (AllTrialsCleared(record)) {
    titles |= Bit(kTrialsTitle);
  }
  if (record.max_chain >= kChainTitleLength) {
    titles |= Bit(kChainTitle);
  }
  if (AllReach(record.enemy_kills, kVeteranCount)) {
    titles |= Bit(kAllKillsTitle);
  }
  if (AllReach(record.weapon_attacks, kVeteranCount)) {
    titles |= Bit(kAllAttacksTitle);
  }
  // weapon_attacks[1..9] earn bits 22-30; weapon_attacks[0] earns bit 31.
  for (std::size_t i = 1; i < kScoreWeaponClassCount; ++i) {
    if (record.weapon_attacks.at(i) >= kWeaponMasteryAttacks) {
      titles |= Bit(kFirstWeaponClassTitle + static_cast<unsigned>(i) - 1);
    }
  }
  if (record.weapon_attacks[0] >= kWeaponMasteryAttacks) {
    titles |= Bit(kFirstWeaponClassMasteryTitle);
  }
  return titles;
}

std::uint32_t ConditionTitles(const TitleConditions& conditions) {
  std::uint32_t titles = 0;
  for (std::size_t i = 0; i < kTitleBossCount; ++i) {
    if (conditions.bosses_defeated.at(i)) {
      titles |= Bit(kFirstBossTitle + static_cast<unsigned>(i));
    }
  }
  if (conditions.gold_key) {
    titles |= Bit(kGoldKeyTitle);
  }
  if (conditions.chest_key) {
    titles |= Bit(kChestKeyTitle);
  }
  if (AllUnlocked(conditions, kLateActionsFirst, kTitleActionCount)) {
    titles |= Bit(kLateActionsTitle);
  }
  if (AllUnlocked(conditions, kEarlyActionsFirst, kEarlyActionsEnd)) {
    titles |= Bit(kEarlyActionsTitle);
  }
  return titles;
}

std::uint32_t TimeBonus(std::uint32_t score, std::uint32_t minutes) {
  if (minutes >= kQuarterBonusMinutes) {
    return score + (score >> 2);
  }
  if (minutes >= kHalfBonusMinutes) {
    return score + (score >> 1);
  }
  return score * 2;
}
}  // namespace

// decomp: MENUF.PRG _calculateScore (src/MENU/MENUF.PRG/3B8.c:2801),
// 0x80108AB0-0x80108E34; also the shared body of MENU9.PRG _calculateScore
// (src/MENU/MENU9.PRG/238.c:2842), 0x80107C34-0x80107FB8. Both compile to the
// same 226 instructions; only the clear count load (lw vs lhu) differs.
std::uint32_t ScoreFromRecord(const ScoreRecord& record,
                              std::uint32_t clear_count) noexcept {
  // Per-kill and per-attack points by class (238.c:2844-2845).
  static constexpr std::array<std::uint32_t, kScoreEnemyClassCount>
      kEnemyClassPoints{20, 20, 40, 80, 100, 60};
  static constexpr std::array<std::uint32_t, kScoreWeaponClassCount>
      kWeaponClassPoints{20, 40, 20, 100, 60, 100, 60, 100, 80, 60};
  std::uint32_t score = 0;
  for (std::size_t i = 0; i < kScoreEnemyClassCount; ++i) {
    score += std::uint32_t{record.enemy_kills.at(i)} * kEnemyClassPoints.at(i);
  }
  for (std::size_t i = 0; i < kScoreWeaponClassCount; ++i) {
    score +=
        std::uint32_t{record.weapon_attacks.at(i)} * kWeaponClassPoints.at(i);
  }
  for (const std::uint32_t word : record.boss_trial_words) {
    if ((word & kTrialTimeMask) != kUnclearedTrialTime) {
      score += kClearedTrialPoints;
    }
  }
  score += std::uint32_t{record.max_chain} * ChainPoints(record.max_chain);
  score += clear_count * kClearPoints;
  if (clear_count != 0 && record.completion_time_minutes < kFastClearMinutes) {
    score = TimeBonus(score, record.completion_time_minutes);
  }
  score += record.streak_score;
  score += record.misc_score;
  return score > kMaxScore ? kMaxScore : score;
}

// decomp: MENUF.PRG _determineRank (src/MENU/MENUF.PRG/3B8.c:2774),
// 0x80108A0C-0x80108AAC; also the shared body of MENU9.PRG
// _determineCharacterRank (src/MENU/MENU9.PRG/238.c:2633),
// 0x80107600-0x801076A0. Only the rank store width (sw vs sh) differs.
std::optional<std::size_t> RiskbreakerRankIndex(std::uint32_t titles,
                                                std::uint32_t score) noexcept {
  // Rank rows: titles needed (signed halfwords) and unsigned score needed.
  static constexpr std::array<std::int16_t, kRiskbreakerRankCount> kRankTitles{
      16, 12, 8};
  static constexpr std::array<std::uint32_t, kRiskbreakerRankCount> kRankScores{
      100'000'000, 75'000'000, 60'000'000, 40'000'000, 32'000'000, 24'000'000,
      16'000'000,  12'000'000, 8'000'000,  5'000'000,  4'000'000,  3'000'000,
      2'000'000,   1'000'000,  500'000,    0};
  const int title_count = std::popcount(titles & kRankTitleMask);
  for (std::size_t i = 0; i < kRiskbreakerRankCount; ++i) {
    if (title_count >= kRankTitles.at(i) && score >= kRankScores.at(i)) {
      return i;
    }
  }
  return std::nullopt;
}

// decomp: MENU9.PRG _determineCharacterRank (src/MENU/MENU9.PRG/238.c:2633),
// 0x80107600-0x801076A0.
std::optional<std::uint16_t> DetermineRiskbreakerRank(
    std::uint32_t titles, std::uint32_t score) noexcept {
  const auto rank = RiskbreakerRankIndex(titles, score);
  if (!rank.has_value()) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(*rank);
}

// decomp: MENU9.PRG _setTitleFlags (src/MENU/MENU9.PRG/238.c:2662),
// 0x801076A4-0x80107A94.
std::uint32_t AwardTitles(const ScoreRecord& record, std::uint16_t clear_count,
                          const TitleConditions& conditions) noexcept {
  std::uint32_t titles =
      record.titles | RecordTitles(record) | ConditionTitles(conditions);
  if (clear_count != 0) {
    titles |= Bit(kClearedTitle);
  }
  return titles;
}

// decomp: MENU9.PRG _calculateScore (src/MENU/MENU9.PRG/238.c:2842),
// 0x80107C34-0x80107FB8. The clear count is the screen's u_short copy (lhu).
std::uint32_t CalculateScore(const ScoreRecord& record,
                             std::uint16_t clear_count) noexcept {
  return ScoreFromRecord(record, std::uint32_t{clear_count});
}

// decomp: MENU9.PRG _calculateProgress (src/MENU/MENU9.PRG/238.c:418),
// 0x801031BC-0x80103378.
ScoreScreenProgress CalculateProgress(ProgressWorld& world,
                                      ProgressHost& host) {
  auto& alias_word = world.room_flags[kRoomFlagAliasWord];
  if ((alias_word & kRoomFlagWithAlias) != 0) {
    alias_word |= kRoomFlagAlias;
  }
  host.ResetScreen();
  if (!world.cursor_memory) {
    host.ClearCursorMemory();
  }

  ScoreScreenProgress progress;
  for (std::size_t i = 0; i < kRoomFlagWords; ++i) {
    progress.map_completion += static_cast<std::uint16_t>(std::popcount(
        world.room_flags.at(i) & world.countable_room_masks.at(i)));
  }
  for (const std::uint8_t chest : world.chests_opened) {
    if (chest != 0) {
      ++progress.opened_chest_count;
    }
  }
  progress.clear_count = world.clear_count;

  ScoreRecord& record = world.record;
  if (progress.map_completion > record.map_completion) {
    record.map_completion = progress.map_completion;
  }
  if (progress.opened_chest_count > record.opened_chest_count) {
    record.opened_chest_count = progress.opened_chest_count;
  }
  host.BackupCamera();

  record.titles = AwardTitles(record, progress.clear_count, world.conditions);
  progress.score = CalculateScore(record, progress.clear_count);
  progress.rank = DetermineRiskbreakerRank(record.titles, progress.score);
  return progress;
}
}  // namespace VS::Menu::Score
