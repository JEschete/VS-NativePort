// Hand-maintained (tools/port/owned.csv).
// The kill records and streak score (State/KillScore.hpp).
#include "State/KillScore.hpp"

#include <cstdint>

namespace VS::State {
namespace {
constexpr unsigned kFlagWordBits = 32;
constexpr int kStreakBonusDivisor = 2;

// A matching key extends the streak up to kMaxStreak; another restarts it.
void ExtendStreak(std::uint8_t& streak_key, std::uint8_t& streak,
                  std::uint8_t key) noexcept {
  if (streak_key != key) {
    streak_key = key;
    streak = 0;
  } else if (streak < kMaxStreak) {
    ++streak;
  }
}

[[nodiscard]] int StreakBonus(int max_hp, int streak) noexcept {
  return (max_hp * streak) / kStreakBonusDivisor;
}

void ScoreEnemy(KillScore& score, const ScoredEnemy& enemy) noexcept {
  ExtendStreak(score.kill_streak_enemy_class, score.kill_streak_enemy_count,
               enemy.enemy_class);
  const int max_hp = enemy.max_hp;
  const int bonus = max_hp +
                    StreakBonus(max_hp, score.kill_streak_enemy_count) +
                    StreakBonus(max_hp, score.weapon_kill_streak) +
                    StreakBonus(max_hp, score.chain_streak);
  score.streak_score += static_cast<std::uint32_t>(bonus);
  if (score.streak_score > kMaxStreakScore) {
    score.streak_score = kMaxStreakScore;
  }
}
}  // namespace

bool RecordEnemyKill(KillScore& score, const DefeatedEnemy& enemy) noexcept {
  bool counted = true;
  if (enemy.scored.has_value()) {
    const ScoredEnemy& scored = *enemy.scored;
    if (scored.enemy_class < kEnemyClassCount) {
      std::uint16_t& kills = score.enemy_kills[scored.enemy_class];
      if (kills < kMaxEnemyClassKills) ++kills;
    } else {
      counted = false;
    }
    ScoreEnemy(score, scored);
  }
  if (enemy.sub_type < kEnemyKillFlagCount) {
    score.enemy_kill_flags[enemy.sub_type / kFlagWordBits] |=
        1U << (enemy.sub_type % kFlagWordBits);
  }
  return counted;
}

void UpdateWeaponKillStreak(KillScore& score, std::uint16_t action_id,
                            std::uint8_t blade_category) noexcept {
  if (!ExtendsWeaponKillStreak(action_id)) return;
  ExtendStreak(score.weapon_kill_streak_blade_category,
               score.weapon_kill_streak, blade_category);
}
}  // namespace VS::State
