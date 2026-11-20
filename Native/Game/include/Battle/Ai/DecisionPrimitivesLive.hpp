#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/DecisionPrimitives.hpp"

struct Battle_ActionDefinition;
namespace VS::Battle::Ai {
struct ActionCandidateRecord;
struct ActionPlanView;
struct ActorAiState;
}  // namespace VS::Battle::Ai

// Bindings from the actor and room AI state, the battle actor list, the
// gimmick table, Ashley's model and the AI frame's random stream to the typed
// decision gates (DecisionPrimitives.cpp). Each aborts with a diagnostic when
// a state, ID or table entry it needs is invalid.
namespace VS::Battle::Ai {
[[nodiscard]] bool DoesLiveActionRequireCooldown(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate);
[[nodiscard]] bool IsLiveActionCooldownReady(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate);
// Whether a hostile actor stands next to gimmick `target`; the original
// returns zero in that case.
[[nodiscard]] bool HasLiveNearbyEnemy(const VS::Battle::Ai::ActorAiState* state,
                                      int target);
[[nodiscard]] bool CanExecuteLiveActionCandidate(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate);
// The HP, MP and DP ratings are read as signed halfwords.
[[nodiscard]] int ScoreLiveActorCondition(int status, int hp, int mp, int dp);
// The flag for `action` against battle actor `actor_id`.
[[nodiscard]] int IsLiveActionHarmfulToActor(int action, int actor_id);
[[nodiscard]] int IsLiveActionBeneficialToActor(int action, int actor_id);
void CacheLiveFallbackActionCandidate(
    const VS::Battle::Ai::ActionCandidateRecord* candidate,
    const Battle_ActionDefinition* action);
// Promotes one of the first `count` ranked candidates.
void PromoteLiveRandomizedActionCandidate(VS::Battle::Ai::ActionPlanView* state,
                                          int count);
int NextLiveActorRandomMasked(int range);
// `view` is an actor AI state; the original ignores `range`.
[[nodiscard]] bool IsLiveAiTargetBeyondNearRadius(const void* view, int target,
                                                  int range);
[[nodiscard]] bool CanApproachLiveEngagedTarget(
    const VS::Battle::Ai::ActorAiState* state,
    const VS::Battle::Ai::ActionCandidateRecord* candidate);
}  // namespace VS::Battle::Ai
