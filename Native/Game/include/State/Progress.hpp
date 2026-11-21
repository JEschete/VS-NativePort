#pragma once
// Campaign progress: learned actions and Break Arts, visited rooms and areas,
// and the score screen's records.
#include <cstddef>
#include <cstdint>

namespace VS::State {
inline constexpr std::size_t kLearnedActionFlagBytes = 32;
inline constexpr std::size_t kArtsLearnedBytes = 12;
inline constexpr std::size_t kKillWeaponCategories = 10;
inline constexpr std::size_t kVisitedRoomWords = 16;
inline constexpr std::size_t kVisitedAreaWords = 2;
inline constexpr std::size_t kEnemyClassCount = 6;
inline constexpr std::size_t kAttackWeaponCategories = 10;
inline constexpr std::size_t kTimeTrialBosses = 8;
inline constexpr std::size_t kTimeTrialRecords = 3;
// The record reserves 26 words; KillScore uses the first 8 (256 flags).
inline constexpr std::size_t kEnemyKillFlagRecordWords = 26;
inline constexpr std::size_t kArtsStatusBytes = 0x24;
inline constexpr std::size_t kRoomVisitFlagsBytes = 0x48;
inline constexpr std::size_t kScoreDataBytes = 0x114;

// decomp: anonymous kills member of vs_main_artsStatus_t
// (src/SLUS_010.40/main.h:358)
struct WeaponKillCounts {
  /* 0x00 */ std::uint16_t weapon_category_kills[kKillWeaponCategories];
  /* 0x14 */ std::uint16_t battle_abilities_unlocked;
  /* 0x16 */ std::uint16_t total_kills;
};

// decomp: vs_main_artsStatus_t (src/SLUS_010.40/main.h:356)
struct ArtsStatus {
  /* 0x00 */ std::uint8_t arts_learned[kArtsLearnedBytes];
  /* 0x0C */ WeaponKillCounts kills;
};
static_assert(sizeof(ArtsStatus) == kArtsStatusBytes);

// decomp: vs_main_mapStatus_t (src/SLUS_010.40/main.h:379)
struct RoomVisitFlags {
  /* 0x00 */ std::int32_t room_flags[kVisitedRoomWords];
  /* 0x40 */ std::int32_t area_flags[kVisitedAreaWords];
};
static_assert(sizeof(RoomVisitFlags) == kRoomVisitFlagsBytes);

// decomp: vs_trialScore_t (src/SLUS_010.40/main.h:163)
struct TrialScore {
  std::uint32_t time : 24;
  std::uint32_t round : 7;
  std::uint32_t difficulty : 1;
};
static_assert(sizeof(TrialScore) == sizeof(std::uint32_t));

// decomp: vs_main_scoredata_t (src/SLUS_010.40/main.h:169)
struct ScoreData {
  /* 0x000 */ std::int32_t titles;
  /* 0x004 */ std::uint16_t enemy_kills[kEnemyClassCount];
  /* 0x010 */ std::int32_t reserved_10;
  /* 0x014 */ std::uint16_t weapon_attacks[kAttackWeaponCategories];
  /* 0x028 */ TrialScore boss_time_trial_scores[kTimeTrialBosses]
                                               [kTimeTrialRecords];
  /* 0x088 */ std::uint16_t max_chain;
  /* 0x08A */ std::uint16_t reserved_8a;
  /* 0x08C */ std::uint32_t clear_count;
  /* 0x090 */ std::uint32_t completion_time_minutes;
  /* 0x094 */ std::uint32_t map_completion;
  /* 0x098 */ std::uint32_t opened_chest_count;
  /* 0x09C */ std::int32_t enemy_kill_flags[kEnemyKillFlagRecordWords];
  /* 0x104 */ std::uint32_t streak_score;
  /* 0x108 */ std::uint32_t misc_score;
  /* 0x10C */ std::uint8_t kill_streak_enemy_class;
  /* 0x10D */ std::uint8_t kill_streak_enemy_count;
  /* 0x10E */ std::uint8_t weapon_kill_streak_blade_category;
  /* 0x10F */ std::uint8_t weapon_kill_streak;
  /* 0x110 */ std::uint16_t chain_streak;
  /* 0x112 */ std::uint16_t heal_count;
};
static_assert(sizeof(ScoreData) == kScoreDataBytes);

// decomp: vs_main_actionsLearned (src/SLUS_010.40/main.c:1040): one bit per
// action.
extern std::uint8_t learnedActionFlags[kLearnedActionFlagBytes];
// decomp: vs_main_mapStatus (src/SLUS_010.40/main.c:1041)
extern RoomVisitFlags roomVisitProgress;
// decomp: vs_main_scoredata (src/SLUS_010.40/main.c:1038): the score screen's
// records.
extern ScoreData scoreData;
// decomp: vs_main_artsStatus (src/SLUS_010.40/main.c:1043): break-art progress.
extern ArtsStatus artsStatus;
}  // namespace VS::State
