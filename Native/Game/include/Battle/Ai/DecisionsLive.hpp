#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/Decisions.hpp"

struct Battle_Ai_ActorAiState;
struct Battle_Ai_DecisionDispatchView;

// Bindings from the battle actor AI state to the typed decision behaviours
// (Decisions.cpp). The behaviour entries keep the original callback signature
// because the BATTLE.PRG behaviour table and actor AI state store them. Each
// aborts with a diagnostic when given no state.
namespace VS::Battle::Ai {
using LiveDecisionBehavior = int (*)(Battle_Ai_ActorAiState*);

int UpdateLivePatrolCombatBehavior(Battle_Ai_ActorAiState* state);
int UpdateLiveFleeOrWanderBehavior(Battle_Ai_ActorAiState* state);
int UpdateLiveAdaptivePursuitBehavior(Battle_Ai_ActorAiState* state);
int UpdateLiveSupportFollowBehavior(Battle_Ai_ActorAiState* state);
int UpdateLiveEngagementApproachBehavior(Battle_Ai_ActorAiState* state);
void UpdateLiveActorLocomotionMode(Battle_Ai_DecisionDispatchView* view);
void DispatchLiveMovementBehavior(Battle_Ai_DecisionDispatchView* view);

// The relocated behaviour table entry the spawn settings word selects, or
// null with a warning when its index is past the table (#272); a null
// behaviour makes the dispatcher wander.
[[nodiscard]] LiveDecisionBehavior SelectLiveDecisionBehavior(
    std::uint32_t spawn_settings);
}  // namespace VS::Battle::Ai
