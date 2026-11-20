// Hand-maintained (tools/port/owned.csv).
// Live binding for the kill records (State/KillScore.hpp): the score record,
// the defeated actor and the player's weapon.
#include "State/KillScoreLive.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.hpp"
#include "Battle/Actor.h"
#include "Core/Diagnostics.hpp"
#include "State/KillScore.hpp"
#include "State/Progress.hpp"

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
  const VS::State::ScoreData& record = scoreData;
  KillScore score;
  for (std::size_t i = 0; i < kEnemyClassCount; ++i) {
    score.enemy_kills[i] = record.enemy_kills[i];
  }
  for (std::size_t i = 0; i < kEnemyKillFlagWords; ++i) {
    score.enemy_kill_flags[i] =
        static_cast<std::uint32_t>(record.enemy_kill_flags[i]);
  }
  score.streak_score = record.streak_score;
  score.kill_streak_enemy_class =
      static_cast<std::uint8_t>(record.kill_streak_enemy_class);
  score.kill_streak_enemy_count =
      static_cast<std::uint8_t>(record.kill_streak_enemy_count);
  score.weapon_kill_streak_blade_category =
      static_cast<std::uint8_t>(record.weapon_kill_streak_blade_category);
  score.weapon_kill_streak =
      static_cast<std::uint8_t>(record.weapon_kill_streak);
  score.chain_streak = record.chain_streak;
  return score;
}

void StoreLiveKillScore(const KillScore& score) noexcept {
  VS::State::ScoreData& record = scoreData;
  for (std::size_t i = 0; i < kEnemyClassCount; ++i) {
    record.enemy_kills[i] = score.enemy_kills[i];
  }
  for (std::size_t i = 0; i < kEnemyKillFlagWords; ++i) {
    record.enemy_kill_flags[i] = static_cast<int>(score.enemy_kill_flags[i]);
  }
  record.streak_score = score.streak_score;
  record.kill_streak_enemy_class =
      static_cast<char>(score.kill_streak_enemy_class);
  record.kill_streak_enemy_count =
      static_cast<char>(score.kill_streak_enemy_count);
  record.weapon_kill_streak_blade_category =
      static_cast<char>(score.weapon_kill_streak_blade_category);
  record.weapon_kill_streak = static_cast<char>(score.weapon_kill_streak);
  record.chain_streak = score.chain_streak;
}

void RecordLiveEnemyKillScore(Battle_Actor* enemy) {
  if (enemy == nullptr) {
    VS::Core::Warnf("state", "RecordEnemyKillScore refused: no defeated enemy");
    return;
  }
  const DefeatedEnemy defeated = DefeatedEnemyOf(*enemy);
  KillScore score = LiveKillScore();
  if (!RecordEnemyKill(score, defeated) && defeated.scored.has_value()) {
    VS::Core::Warnf("state",
                    "RecordEnemyKillScore: enemy class %u of sub-type %u is "
                    "outside the %u counted classes; its count was refused",
                    static_cast<unsigned>(defeated.scored->enemy_class),
                    static_cast<unsigned>(defeated.sub_type),
                    static_cast<unsigned>(kEnemyClassCount));
  }
  StoreLiveKillScore(score);
}

void UpdateLiveWeaponKillStreak(int /*unused*/) {
  if (VS::Battle::actionState == nullptr) {
    VS::Core::Warnf("state", "UpdateWeaponKillStreak refused: no action state");
    return;
  }
  const std::uint16_t action_id = VS::Battle::actionState->selection.actionId;
  if (!ExtendsWeaponKillStreak(action_id)) return;
  const Battle_Actor* player = Battle_characterState;
  if (player == nullptr || player->stats == nullptr) {
    VS::Core::Warnf("state", "UpdateWeaponKillStreak refused: no player stats");
    return;
  }
  KillScore score = LiveKillScore();
  UpdateWeaponKillStreak(
      score, action_id,
      static_cast<std::uint8_t>(player->stats->weapon.blade.category));
  StoreLiveKillScore(score);
}

// decomp: BATTLE.PRG func_80070F28 at 0x80071284 (`sb zero` to
// vs_main_scoredata+0x10F, 0x8005FFAF).
void ResetLiveWeaponKillStreak() noexcept { scoreData.weapon_kill_streak = 0; }
}  // namespace VS::State
