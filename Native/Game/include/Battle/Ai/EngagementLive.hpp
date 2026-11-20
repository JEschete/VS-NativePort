#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/Engagement.hpp"

namespace VS::Battle::Ai {
struct ActorAiState;
struct PerceptionUpdateView;
struct PlanEvaluationView;
struct RoomAiState;
}  // namespace VS::Battle::Ai

// Bindings from the room AI state, the AI state table and the battle actor
// list to the typed engagement roster and awareness markers (Engagement.cpp).
// Each aborts with a diagnostic when a state, actor ID or the room is invalid.
namespace VS::Battle::Ai {
void SetLiveActorAwarenessIndicator(int actor_id, int marker);
void ResetLiveActorAwarenessIndicator(
    VS::Battle::Ai::PlanEvaluationView* state);
void SetLiveTemporaryAwarenessIndicator(VS::Battle::Ai::ActorAiState* state,
                                        int marker, int frames);
[[nodiscard]] bool IsLiveEngagementReady(
    const VS::Battle::Ai::ActorAiState* state);
void UpdateLiveTemporaryAwarenessIndicator(
    VS::Battle::Ai::PerceptionUpdateView* state);
// The battle action flow passes the player's target here, so an ID outside
// the AI state table is ignored with a diagnostic rather than aborting.
void DisengageLiveActorById(int actor_id);
void DisengageLiveActor(VS::Battle::Ai::ActorAiState* state);
bool TryEngageLiveActor(VS::Battle::Ai::ActorAiState* state);
// `state` must be the AI state table's entry for its actor ID; the engaged
// list's pointers are matched against that table.
void UnlinkLiveEngagedActor(VS::Battle::Ai::ActorAiState* state);
void UpdateLiveEngagementBuildup(VS::Battle::Ai::ActorAiState* state);
void AnimateLiveEngagementPhase();

// The room's engaged actor mask, stored as two unaligned bytes.
[[nodiscard]] std::uint16_t EngagedMask(
    const VS::Battle::Ai::RoomAiState& room);
void StoreEngagedMask(VS::Battle::Ai::RoomAiState& room, std::uint16_t mask);
}  // namespace VS::Battle::Ai
