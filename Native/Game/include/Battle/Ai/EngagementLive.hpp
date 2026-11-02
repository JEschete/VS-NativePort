#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/Engagement.hpp"

struct Battle_Ai_ActorAiState;
struct Battle_Ai_PerceptionUpdateView;
struct Battle_Ai_PlanEvaluationView;
struct Battle_Ai_RoomAiState;

// Bindings from the room AI state, the AI state table and the battle actor
// list to the typed engagement roster and awareness markers (Engagement.cpp).
// Each aborts with a diagnostic when a state, actor ID or the room is invalid.
namespace VS::Battle::Ai {
void SetLiveActorAwarenessIndicator(int actor_id, int marker);
void ResetLiveActorAwarenessIndicator(Battle_Ai_PlanEvaluationView* state);
[[nodiscard]] bool IsLiveEngagementReady(const Battle_Ai_ActorAiState* state);
void UpdateLiveTemporaryAwarenessIndicator(
    Battle_Ai_PerceptionUpdateView* state);
// The battle action flow passes the player's target here, so an ID outside
// the AI state table is ignored with a diagnostic rather than aborting.
void DisengageLiveActorById(int actor_id);
void DisengageLiveActor(Battle_Ai_ActorAiState* state);
bool TryEngageLiveActor(Battle_Ai_ActorAiState* state);

// The room's engaged actor mask, stored as two unaligned bytes.
[[nodiscard]] std::uint16_t EngagedMask(const Battle_Ai_RoomAiState& room);
void StoreEngagedMask(Battle_Ai_RoomAiState& room, std::uint16_t mask);
}  // namespace VS::Battle::Ai
