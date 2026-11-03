#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/ActorCleanup.hpp"

struct Battle_Actor;
struct Battle_Ai_ActorAiState;
struct Battle_Ai_PerceptionUpdateView;
struct Battle_Ai_ReactionView;
struct Battle_Ai_SettingsView;

// Bindings from the AI state table, the room AI state, the tile flag grid, the
// battle actors, their actor records and the heap to the typed actor cleanup
// (ActorCleanup.cpp). Each binding aborts with a diagnostic when an actor ID,
// state, room, model or the grid is invalid.
namespace VS::Battle::Ai {
void ReleaseLiveAiDestinationTile(int x, int z);
void FreeLiveActorNavigationOverride(int actor_id);
void ForceReleaseLiveActorAiReservations(int actor_id);
void ReleaseLiveActorAiReservations(Battle_Ai_ActorAiState* state);
void FreeLiveActorAiState(Battle_Actor* actor);
void AllocateLiveOrbitingBossState(Battle_Ai_SettingsView* state);
void SetLiveActorAiReactionWithDelay(Battle_Ai_PerceptionUpdateView* state,
                                     int value, int range);
void MakeLiveTargetHostile(Battle_Ai_ReactionView* state, int target);
void RefreshLiveActorAiReservations(int actor_id);
// The battle module's pointer table stores this binding (0x800E6EB0).
void ReleaseLiveActorAiContactOccupancy(int actor_id);
void ClearLiveActorAiLinksAndPlan(int actor_id);
[[nodiscard]] std::int16_t PackLiveActorAiPersistenceFlags(int actor_id);
void ClearLiveActorAiNavigationReset(int actor_id);
void InitializeLiveActorAiExtensionHook();
}  // namespace VS::Battle::Ai
