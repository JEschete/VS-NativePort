#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <span>

#include "Battle/Ai/PlanningPrimitives.hpp"

namespace VS::Battle {
struct ActorStats;
struct ActionDefinition;
struct ActionRequestRecord;
}  // namespace VS::Battle
namespace VS::Battle::Ai {
struct ActionCandidateRecord;
struct ActionPlanView;
struct ActorAiState;
struct CandidateEntryView;
struct CandidateReadinessView;
struct ActionTiming;
struct ReactionRecord;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state, its stats, the action table, the AI state
// table and the planner's exclusion table to the typed candidate list and
// planning checks (PlanningPrimitives.cpp). Invalid input is diagnosed:
// required legacy contracts abort; fallible planning operations report failure.
namespace VS::Battle::Ai {
std::optional<bool> RefreshLiveActionCandidateReadiness(
    VS::Battle::Ai::ActorAiState* state);
bool ApplyLiveSimulatedActionResults(
    std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows);
bool SimulateLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    std::span<VS::Battle::Ai::ActionTiming, kBattleActorSlotCount> rows,
    const VS::Battle::Ai::ActionCandidateRecord* candidate,
    CandidateSimulationMode mode);
bool UpdateLiveCandidateHitWeight(
    VS::Battle::Ai::ActionCandidateRecord* candidate);
bool EvaluateLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate, int simulation_mode,
    const VS::Battle::ActionDefinition* action);

// The condition terms (ActorStatusConditionScore and the ratios,
// PlanningPrimitives.hpp) of a row of the planner's condition snapshot. The
// status score takes the row itself and reads its actor's AI state; the
// ratios take the table's first row and an actor ID below
// kBattleActorSlotCount.
[[nodiscard]] int LiveActorStatusConditionScore(
    const VS::Battle::Ai::ActionTiming* row);
[[nodiscard]] int LiveActorHpRatio(const VS::Battle::Ai::ActionTiming* rows,
                                   int id);
[[nodiscard]] int LiveActorMpRatio(const VS::Battle::Ai::ActionTiming* rows,
                                   int id);
[[nodiscard]] int LiveActorEquipmentDpRatio(
    const VS::Battle::Ai::ActionTiming* rows, int id);

// The ranking of a plan view's first `count` candidates, whose handles index
// the view's order; Apply reorders the view's entries to match it.
class LiveCandidateRanking {
 public:
  LiveCandidateRanking(VS::Battle::Ai::ActionPlanView* state, int count);

  [[nodiscard]] std::span<RankedCandidate> Ranking();
  void Apply();

 private:
  std::span<VS::Battle::Ai::CandidateEntryView*> entries_;
  std::array<RankedCandidate, kCandidateCapacity> ranking_{};
};

void RememberLiveAiActionTarget(VS::Battle::Ai::ActorAiState* state);
void ForgetLiveAiActionTarget(VS::Battle::Ai::ActorAiState* state);
void SelectLiveFallbackAction(VS::Battle::Ai::ActorAiState* state);
void AdvanceLivePlanningSimulation(std::int32_t time_step);
void EnumerateLiveActionCandidates(VS::Battle::Ai::ActorAiState* state,
                                   std::int32_t readiness_threshold);
void CopyLiveActionCandidate(
    VS::Battle::Ai::ActionCandidateRecord* destination,
    const VS::Battle::Ai::ActionCandidateRecord* source);
// Operation 2 restores the stat; any other operation spends it.
void ApplyLiveSimulatedStatDelta(std::int16_t* value,
                                 const std::int16_t* maximum, int delta,
                                 int operation);
void AppendLiveActionCandidate(
    VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate);
void SortLiveActionCandidates(VS::Battle::Ai::ActionPlanView* state);
void BuildLiveActorActionPlan(VS::Battle::Ai::ActorAiState* state);
// A null request is a no-op, as in the original.
void ReconcileLiveCompletedActionPlan(
    const VS::Battle::ActionRequestRecord* request);
// The source and destination may be the same record.
void CopyLivePlanningActorStats(const VS::Battle::ActorStats* source,
                                VS::Battle::ActorStats* destination);
bool SnapshotLivePlanningActorStats();
void RestoreLivePlanningActorStats();
// The game arena is already reset; forget bindings without dereferencing them.
void InvalidateLivePlanningStatsSnapshot();
[[nodiscard]] bool CanLiveAiActionAffectActor(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate, int actor_id);
bool RefreshLiveAiActionUsability(VS::Battle::Ai::ActorAiState* state, int row,
                                  int column);
void RefreshLiveAiResourceRestrictions(VS::Battle::Ai::ActorAiState* state);
// decomp: BATTLE.PRG func_800E5710 (src/BATTLE/BATTLE.PRG/7CD68.c:526); a
// slot action outside the action table aborts with an error.
void BuildLiveAiActionMetadata(VS::Battle::Ai::ActorAiState* state);

// The BATTLE.PRG module-image entries of the planning operations, in their
// original calling shape.
int RefreshActionCandidateReadinessEntry(
    VS::Battle::Ai::CandidateReadinessView* state);
void ApplySimulatedActionResultsEntry(VS::Battle::Ai::ActionTiming* rows);
void SimulateActionCandidateEntry(
    VS::Battle::Ai::ActorAiState* state, VS::Battle::Ai::ActionTiming* rows,
    VS::Battle::Ai::ActionCandidateRecord* candidate, int mode);
void UpdateCandidateHitWeightEntry(
    VS::Battle::Ai::ActionCandidateRecord* candidate);
void EvaluateActionCandidateEntry(
    VS::Battle::Ai::ActorAiState* state,
    VS::Battle::Ai::ActionCandidateRecord* candidate, int mode,
    VS::Battle::ActionDefinition* action);
void SelectFallbackActionEntry(VS::Battle::Ai::ActorAiState* state);
void BuildActorActionPlanEntry(VS::Battle::Ai::ActorAiState* state);
void ReconcileCompletedActionPlanEntry(
    VS::Battle::Ai::ReactionRecord* completed_action);
void AdvancePlanningSimulationEntry(int time_step);
void EnumerateActionCandidatesEntry(VS::Battle::Ai::ActorAiState* state,
                                    int readiness_threshold);
}  // namespace VS::Battle::Ai
