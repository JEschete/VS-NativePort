#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Core::Gte {
struct SVECTOR;
}  // namespace VS::Core::Gte

namespace VS::Battle {
void ResolveLiveEnemyDefeat(int source_actor, int target_actor, int count_kill);
int PlayLiveWeaponHitSounds(int source_actor);
// The original module ABI retains its unread position argument.
void ApplyLiveActionSourceEffects(int source_actor,
                                  VS::Core::Gte::SVECTOR* unused);
int BeginLiveActorDefeatAnimation(int actor);
void ApplyLiveLearnedActionUnlocks();
}  // namespace VS::Battle
