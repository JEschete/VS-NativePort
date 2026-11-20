#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {

struct ActorStats;
struct StatusLifecycleState;
// Shared projection for vitals/results; callers retain the original stats
// owner and must reload after every external callback.
StatusLifecycleState ReadLiveStatusState(const VS::Battle::ActorStats& stats);
void WriteLiveStatusState(const StatusLifecycleState& state,
                          VS::Battle::ActorStats& stats);
// Missing stats and used invalid actor/mobility indices abort with a
// diagnostic. Movement retains the AI helper's empty-slot contract. Durations
// are the low bytes of resident status rules.
void PreserveLiveAshleyStatuses();
void RemoveLiveActorStatuses(int requested, VS::Battle::ActorStats* stats);
void InflictLiveActorStatuses(int requested, VS::Battle::ActorStats* stats);
void UpdateLiveActorStatusTimers(VS::Battle::ActorStats* stats);
}  // namespace VS::Battle
