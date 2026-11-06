// Hand-maintained (tools/port/owned.csv).
// Live binding for the kill records (State/KillScore.hpp): the score record,
// the defeated actor and the player's weapon.
#include "State/KillScoreLive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "State/KillScore.hpp"

extern "C" {
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Core/Diagnostics.h"
#include "State/Progress.h"
}

namespace VS::State {
namespace {
// Actor kind bits (Battle_Actor::kind): 4 marks a hostile actor.
constexpr unsigned kHostileActorKind = 4;

[[nodiscard]] DefeatedEnemy DefeatedEnemyOf(const Battle_Actor& enemy) {
  DefeatedEnemy defeated{.sub_type = enemy.subType, .scored = std::nullopt};
  const Battle_ActorStats* stats = enemy.stats;
  if (stats != nullptr && enemy.kind == kHostileActorKind) {
    defeated.scored =
        ScoredEnemy{.enemy_class = static_cast<std::uint8_t>(stats->enemyClass),
                    .max_hp = stats->maxHp};
  }
  return defeated;
}
}  // namespace

KillScore LiveKillScore() noexcept {
  const State_ScoreData& record = Audio_Seq_unusedScoredata;
  KillScore score;
  for (std::size_t i = 0; i < kEnemyClassCount; ++i) {
    score.enemy_kills[i] = record.enemyKills[i];
  }
  for (std::size_t i = 0; i < kEnemyKillFlagWords; ++i) {
    score.enemy_kill_flags[i] =
        static_cast<std::uint32_t>(record.enemyKillFlags[i]);
  }
  score.streak_score = record.streakScore;
  score.kill_streak_enemy_class =
      static_cast<std::uint8_t>(record.killStreakEnemyClass);
  score.kill_streak_enemy_count =
      static_cast<std::uint8_t>(record.killStreakEnemyCount);
  score.weapon_kill_streak_blade_category =
      static_cast<std::uint8_t>(record.weaponKillStreakBladeCategory);
  score.weapon_kill_streak = static_cast<std::uint8_t>(record.weaponKillStreak);
  score.chain_streak = record.chainStreak;
  return score;
}

void StoreLiveKillScore(const KillScore& score) noexcept {
  State_ScoreData& record = Audio_Seq_unusedScoredata;
  for (std::size_t i = 0; i < kEnemyClassCount; ++i) {
    record.enemyKills[i] = score.enemy_kills[i];
  }
  for (std::size_t i = 0; i < kEnemyKillFlagWords; ++i) {
    record.enemyKillFlags[i] = static_cast<int>(score.enemy_kill_flags[i]);
  }
  record.streakScore = score.streak_score;
  record.killStreakEnemyClass =
      static_cast<char>(score.kill_streak_enemy_class);
  record.killStreakEnemyCount =
      static_cast<char>(score.kill_streak_enemy_count);
  record.weaponKillStreakBladeCategory =
      static_cast<char>(score.weapon_kill_streak_blade_category);
  record.weaponKillStreak = static_cast<char>(score.weapon_kill_streak);
  record.chainStreak = score.chain_streak;
}

void RecordLiveEnemyKillScore(Battle_Actor* enemy) {
  if (enemy == nullptr) {
    Core_Warnf("state", "RecordEnemyKillScore refused: no defeated enemy");
    return;
  }
  const DefeatedEnemy defeated = DefeatedEnemyOf(*enemy);
  KillScore score = LiveKillScore();
  if (!RecordEnemyKill(score, defeated) && defeated.scored.has_value()) {
    Core_Warnf("state",
               "RecordEnemyKillScore: enemy class %u of sub-type %u is "
               "outside the %u counted classes; its count was refused",
               static_cast<unsigned>(defeated.scored->enemy_class),
               static_cast<unsigned>(defeated.sub_type),
               static_cast<unsigned>(kEnemyClassCount));
  }
  StoreLiveKillScore(score);
}

void UpdateLiveWeaponKillStreak(int /*unused*/) {
  if (Battle_actionState == nullptr) {
    Core_Warnf("state", "UpdateWeaponKillStreak refused: no action state");
    return;
  }
  const std::uint16_t action_id = Battle_actionState->selection.actionId;
  if (!ExtendsWeaponKillStreak(action_id)) return;
  const Battle_Actor* player = Battle_characterState;
  if (player == nullptr || player->stats == nullptr) {
    Core_Warnf("state", "UpdateWeaponKillStreak refused: no player stats");
    return;
  }
  KillScore score = LiveKillScore();
  UpdateWeaponKillStreak(
      score, action_id,
      static_cast<std::uint8_t>(player->stats->weapon.blade.category));
  StoreLiveKillScore(score);
}
}  // namespace VS::State
