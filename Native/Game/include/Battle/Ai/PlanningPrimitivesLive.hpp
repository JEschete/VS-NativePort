#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <span>

#include "Battle/Ai/PlanningPrimitives.hpp"

struct Battle_ActorStats;
struct Battle_Ai_ActionCandidate;
struct Battle_Ai_ActionPlanView;
struct Battle_Ai_ActorAiState;
struct Battle_Ai_CandidateEntryView;
struct Battle_Ai_ActionTiming;

// Bindings from the actor AI state, its stats, the action table, the AI state
// table and the planner's exclusion table to the typed candidate list and
// planning checks (PlanningPrimitives.cpp). Each aborts with a diagnostic when
// a state, record or ID it needs is invalid.
namespace VS::Battle::Ai {
std::optional<bool> RefreshLiveActionCandidateReadiness(
    Battle_Ai_ActorAiState* state);
bool ApplyLiveSimulatedActionResults(
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows);

// The ranking of a plan view's first `count` candidates, whose handles index
// the view's order; Apply reorders the view's entries to match it.
class LiveCandidateRanking {
 public:
  LiveCandidateRanking(Battle_Ai_ActionPlanView* state, int count);

  [[nodiscard]] std::span<RankedCandidate> Ranking();
  void Apply();

 private:
  std::span<Battle_Ai_CandidateEntryView*> entries_;
  std::array<RankedCandidate, kCandidateCapacity> ranking_{};
};

void RememberLiveAiActionTarget(Battle_Ai_ActorAiState* state);
void CopyLiveActionCandidate(Battle_Ai_ActionCandidate* destination,
                             const Battle_Ai_ActionCandidate* source);
// Operation 2 restores the stat; any other operation spends it.
void ApplyLiveSimulatedStatDelta(std::int16_t* value,
                                 const std::int16_t* maximum, int delta,
                                 int operation);
void AppendLiveActionCandidate(Battle_Ai_ActorAiState* state,
                               const Battle_Ai_ActionCandidate* candidate);
void SortLiveActionCandidates(Battle_Ai_ActionPlanView* state);
// The source and destination may be the same record.
void CopyLivePlanningActorStats(const Battle_ActorStats* source,
                                Battle_ActorStats* destination);
[[nodiscard]] bool CanLiveAiActionAffectActor(
    const Battle_Ai_ActorAiState* state,
    const Battle_Ai_ActionCandidate* candidate, int actor_id);
bool RefreshLiveAiActionUsability(Battle_Ai_ActorAiState* state, int row,
                                  int column);
void RefreshLiveAiResourceRestrictions(Battle_Ai_ActorAiState* state);
}  // namespace VS::Battle::Ai
