#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/Decisions.hpp"

namespace VS::Battle::Ai {
struct ActorAiState;
struct DecisionDispatchView;
}  // namespace VS::Battle::Ai

// Bindings from the battle actor AI state to the typed decision behaviours
// (Decisions.cpp). The behaviour entries keep the original callback signature
// because the BATTLE.PRG behaviour table and actor AI state store them. Each
// aborts with a diagnostic when given no state.
namespace VS::Battle::Ai {
using LiveDecisionBehavior = int (*)(VS::Battle::Ai::ActorAiState*);

int UpdateLivePatrolCombatBehavior(VS::Battle::Ai::ActorAiState* state);
int UpdateLiveFleeOrWanderBehavior(VS::Battle::Ai::ActorAiState* state);
int UpdateLiveAdaptivePursuitBehavior(VS::Battle::Ai::ActorAiState* state);
int UpdateLiveSupportFollowBehavior(VS::Battle::Ai::ActorAiState* state);
int UpdateLiveEngagementApproachBehavior(VS::Battle::Ai::ActorAiState* state);
void UpdateLiveActorLocomotionMode(VS::Battle::Ai::DecisionDispatchView* view);
void DispatchLiveMovementBehavior(VS::Battle::Ai::DecisionDispatchView* view);

// The relocated behaviour table entry the spawn settings word selects, or
// null with a warning when its index is past the table (#272); a null
// behaviour makes the dispatcher wander.
[[nodiscard]] LiveDecisionBehavior SelectLiveDecisionBehavior(
    std::uint32_t spawn_settings);
}  // namespace VS::Battle::Ai
