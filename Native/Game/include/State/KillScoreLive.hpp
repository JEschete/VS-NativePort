#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "State/KillScore.hpp"

struct Battle_Actor;

// Bindings from the live score record (Audio_Seq_unusedScoredata, the
// original vs_main_scoredata) and the battle actors to the typed kill
// records (State/KillScore.hpp), in the original argument order for the
// generated callers (tools/port/copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::State {
[[nodiscard]] KillScore LiveKillScore() noexcept;
void StoreLiveKillScore(const KillScore& score) noexcept;

// A null enemy is refused with a "state" warning; the original read it.
void RecordLiveEnemyKillScore(Battle_Actor* enemy);
// Reads the selected action and the player's blade category. A missing
// action state, player or player stats is refused with a "state" warning.
void UpdateLiveWeaponKillStreak(int unused);
}  // namespace VS::State
