#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

// The kill records and streak score the score screen and titles read (decomp:
// vs_main_scoredata, SLUS_010.40 main.h:202 vs_main_scoredata_t): kills per
// enemy class, a flag per defeated enemy sub-type, and the streaks that
// multiply each hostile kill's score.
namespace VS::State {
inline constexpr std::size_t kEnemyClassCount = 6;
inline constexpr std::size_t kEnemyKillFlagWords = 8;
inline constexpr unsigned kEnemyKillFlagCount = 256;
inline constexpr std::uint16_t kMaxEnemyClassKills = 0xFFFF;
inline constexpr std::uint8_t kMaxStreak = 100;
inline constexpr std::uint32_t kMaxStreakScore = 999'999'999;
// The action ids that extend the weapon kill streak, as the original checked.
inline constexpr std::uint16_t kWeaponStreakActionLimit = 40;

struct KillScore {
  std::array<std::uint16_t, kEnemyClassCount> enemy_kills{};
  std::array<std::uint32_t, kEnemyKillFlagWords> enemy_kill_flags{};
  std::uint32_t streak_score = 0;
  std::uint8_t kill_streak_enemy_class = 0;
  std::uint8_t kill_streak_enemy_count = 0;
  std::uint8_t weapon_kill_streak_blade_category = 0;
  std::uint8_t weapon_kill_streak = 0;
  std::uint16_t chain_streak = 0;

  friend bool operator==(const KillScore&, const KillScore&) = default;
};

// A defeated hostile enemy's scoring stats.
struct ScoredEnemy {
  std::uint8_t enemy_class = 0;
  std::int16_t max_hp = 0;
};

struct DefeatedEnemy {
  std::uint16_t sub_type = 0;
  // Present for a hostile enemy with stats; only those score.
  std::optional<ScoredEnemy> scored;
};

// decomp: BATTLE.PRG _updateEnemyKills (146C.c:1772). A scored enemy counts
// toward its class (saturating), extends or restarts the enemy kill streak
// and adds max HP times its streak bonuses to the streak score, capped at
// kMaxStreakScore. Any enemy sub-type below kEnemyKillFlagCount sets its
// kill flag. Returns false when the enemy class is outside the counted
// classes: its count is refused, where the original wrote past the counts
// into the record's reserved word.
bool RecordEnemyKill(KillScore& score, const DefeatedEnemy& enemy) noexcept;

// True for the actions that extend the weapon kill streak.
[[nodiscard]] constexpr bool ExtendsWeaponKillStreak(
    std::uint16_t action_id) noexcept {
  return action_id < kWeaponStreakActionLimit;
}

// decomp: BATTLE.PRG _updateWeaponKillStreak (146C.c:1806). An action that
// ExtendsWeaponKillStreak extends the weapon kill streak, restarting it when
// the blade category changed.
void UpdateWeaponKillStreak(KillScore& score, std::uint16_t action_id,
                            std::uint8_t blade_category) noexcept;
}  // namespace VS::State
