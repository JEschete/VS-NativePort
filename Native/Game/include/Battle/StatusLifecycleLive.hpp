#pragma once
// Hand-maintained (tools/port/owned.csv).

struct Battle_ActorStats;

namespace VS::Battle {
// Missing stats and used invalid actor/mobility indices abort with a
// diagnostic. Movement retains the AI helper's empty-slot contract. Durations
// are the low bytes of resident status rules.
void PreserveLiveAshleyStatuses();
void RemoveLiveActorStatuses(int requested, Battle_ActorStats* stats);
void InflictLiveActorStatuses(int requested, Battle_ActorStats* stats);
void UpdateLiveActorStatusTimers(Battle_ActorStats* stats);
}  // namespace VS::Battle
