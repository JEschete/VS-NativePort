#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/DecisionPrimitives.hpp"

struct Battle_ActionDefinition;
struct Battle_Ai_ActionCandidate;
struct Battle_Ai_ActionPlanView;
struct Battle_Ai_ActorAiState;

// Bindings from the actor and room AI state, the battle actor list, the
// gimmick table, Ashley's model and the AI frame's random stream to the typed
// decision gates (DecisionPrimitives.cpp). Each aborts with a diagnostic when
// a state, ID or table entry it needs is invalid.
namespace VS::Battle::Ai {
[[nodiscard]] bool DoesLiveActionRequireCooldown(
    const Battle_Ai_ActorAiState* state,
    const Battle_Ai_ActionCandidate* candidate);
[[nodiscard]] bool IsLiveActionCooldownReady(
    const Battle_Ai_ActorAiState* state,
    const Battle_Ai_ActionCandidate* candidate);
// Whether a hostile actor stands next to gimmick `target`; the original
// returns zero in that case.
[[nodiscard]] bool HasLiveNearbyEnemy(const Battle_Ai_ActorAiState* state,
                                      int target);
[[nodiscard]] bool CanExecuteLiveActionCandidate(
    const Battle_Ai_ActorAiState* state,
    const Battle_Ai_ActionCandidate* candidate);
// The HP, MP and DP ratings are read as signed halfwords.
[[nodiscard]] int ScoreLiveActorCondition(int status, int hp, int mp, int dp);
// The flag for `action` against battle actor `actor_id`.
[[nodiscard]] int IsLiveActionHarmfulToActor(int action, int actor_id);
[[nodiscard]] int IsLiveActionBeneficialToActor(int action, int actor_id);
void CacheLiveFallbackActionCandidate(
    const Battle_Ai_ActionCandidate* candidate,
    const Battle_ActionDefinition* action);
// Promotes one of the first `count` ranked candidates.
void PromoteLiveRandomizedActionCandidate(Battle_Ai_ActionPlanView* state,
                                          int count);
int NextLiveActorRandomMasked(int range);
// `view` is an actor AI state; the original ignores `range`.
[[nodiscard]] bool IsLiveAiTargetBeyondNearRadius(const void* view, int target,
                                                  int range);
[[nodiscard]] bool CanApproachLiveEngagedTarget(
    const Battle_Ai_ActorAiState* state,
    const Battle_Ai_ActionCandidate* candidate);
}  // namespace VS::Battle::Ai
