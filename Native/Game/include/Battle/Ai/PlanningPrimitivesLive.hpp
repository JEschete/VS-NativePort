#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <span>

#include "Battle/Ai/PlanningPrimitives.hpp"

struct Battle_ActorStats;
struct Battle_ActionDefinition;
struct Battle_Ai_ActionCandidate;
struct Battle_Ai_ActionPlanView;
struct Battle_Ai_ActorAiState;
struct Battle_Ai_CandidateEntryView;
struct Battle_Ai_ActionTiming;

// Bindings from the actor AI state, its stats, the action table, the AI state
// table and the planner's exclusion table to the typed candidate list and
// planning checks (PlanningPrimitives.cpp). Invalid input is diagnosed:
// required legacy contracts abort; fallible planning operations report failure.
namespace VS::Battle::Ai {
std::optional<bool> RefreshLiveActionCandidateReadiness(
    Battle_Ai_ActorAiState* state);
bool ApplyLiveSimulatedActionResults(
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows);
bool SimulateLiveActionCandidate(
    Battle_Ai_ActorAiState* state,
    std::span<Battle_Ai_ActionTiming, kBattleActorSlotCount> rows,
    const Battle_Ai_ActionCandidate* candidate, CandidateSimulationMode mode);
bool UpdateLiveCandidateHitWeight(Battle_Ai_ActionCandidate* candidate);
bool EvaluateLiveActionCandidate(Battle_Ai_ActorAiState* state,
                                 Battle_Ai_ActionCandidate* candidate,
                                 int simulation_mode,
                                 const Battle_ActionDefinition* action);

// The condition terms (ActorStatusConditionScore and the ratios,
// PlanningPrimitives.hpp) of a row of the planner's condition snapshot. The
// status score takes the row itself and reads its actor's AI state; the
// ratios take the table's first row and an actor ID below
// kBattleActorSlotCount.
[[nodiscard]] int LiveActorStatusConditionScore(
    const Battle_Ai_ActionTiming* row);
[[nodiscard]] int LiveActorHpRatio(const Battle_Ai_ActionTiming* rows, int id);
[[nodiscard]] int LiveActorMpRatio(const Battle_Ai_ActionTiming* rows, int id);
[[nodiscard]] int LiveActorEquipmentDpRatio(const Battle_Ai_ActionTiming* rows,
                                            int id);

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
void SelectLiveFallbackAction(Battle_Ai_ActorAiState* state);
void AdvanceLivePlanningSimulation(std::int32_t time_step);
void EnumerateLiveActionCandidates(Battle_Ai_ActorAiState* state,
                                   std::int32_t readiness_threshold);
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
bool SnapshotLivePlanningActorStats();
void RestoreLivePlanningActorStats();
// The game arena is already reset; forget bindings without dereferencing them.
void InvalidateLivePlanningStatsSnapshot();
[[nodiscard]] bool CanLiveAiActionAffectActor(
    const Battle_Ai_ActorAiState* state,
    const Battle_Ai_ActionCandidate* candidate, int actor_id);
bool RefreshLiveAiActionUsability(Battle_Ai_ActorAiState* state, int row,
                                  int column);
void RefreshLiveAiResourceRestrictions(Battle_Ai_ActorAiState* state);
}  // namespace VS::Battle::Ai
