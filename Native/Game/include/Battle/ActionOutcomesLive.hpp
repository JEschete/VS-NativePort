#pragma once
// Hand-maintained (tools/port/owned.csv).

struct SVECTOR;

namespace VS::Battle {
void ResolveLiveEnemyDefeat(int source_actor, int target_actor, int count_kill);
int PlayLiveWeaponHitSounds(int source_actor);
// The original module ABI retains its unread position argument.
void ApplyLiveActionSourceEffects(int source_actor, SVECTOR* unused);
int BeginLiveActorDefeatAnimation(int actor);
void ApplyLiveLearnedActionUnlocks();
}  // namespace VS::Battle
