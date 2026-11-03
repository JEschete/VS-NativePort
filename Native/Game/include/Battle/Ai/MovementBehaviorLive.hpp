#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/MovementBehavior.hpp"

struct SVECTOR;
struct Battle_Ai_ActorAiState;
struct Battle_Ai_CandidateMovementView;
struct Battle_Ai_CandidateReadinessView;
struct Battle_Ai_EscortView;
struct Battle_Ai_PerceptionUpdateView;
struct Battle_Ai_ReactionRecord;
struct Battle_Ai_TargetApproachView;
struct Battle_Ai_TerrainMoveView;
union Battle_Ai_GuardTileWord;

// Bindings from the actor AI state table, the actor models, the room's patrol
// routes and route gates and the battle actor list to the typed movement
// behaviours (MovementBehavior.cpp). Callers pass the AI state view they hold;
// each view aliases the actor AI state. Each binding aborts with a diagnostic
// when a state, target ID, model, patrol row or route gate is invalid.
namespace VS::Battle::Ai {
int SetLiveMovementBehavior(Battle_Ai_ActorAiState* state, int mode);
void RefreshLiveAiMovementCooldown(Battle_Ai_TerrainMoveView* state);
int ApproachLiveActor(Battle_Ai_ActorAiState* state, int target, int distance,
                      int option);
void RefreshLiveAiTargetTile(Battle_Ai_TargetApproachView* state);
void RefreshLiveAiTargetTileWhenTracking(Battle_Ai_PerceptionUpdateView* state);
void WanderAroundLiveSavedCenter(Battle_Ai_ActorAiState* state);
void WanderAroundLiveTile(Battle_Ai_CandidateMovementView* state,
                          std::uint32_t tile, int radius);
void MoveLiveActorToTile(Battle_Ai_CandidateMovementView* state,
                         std::uint32_t tile, int radius);
void PatrolLiveWaypoints(Battle_Ai_ActorAiState* state);
int FleeLiveVisibleEnemies(Battle_Ai_CandidateMovementView* state, int kind);
int TrackLiveActorPosition(Battle_Ai_ActorAiState* state, int target);
// The ID of the first other listed actor with relationship class `relation`,
// or -1; stores its tile when `tile` is set.
int FindLiveActorByRelationship(Battle_Ai_EscortView* state,
                                Battle_Ai_GuardTileWord* tile, int relation);
int FindLiveActorByRelationship(Battle_Ai_ActorAiState* state,
                                std::uint32_t* tile, int relation);
[[nodiscard]] int LiveTargetFacingCosine(const SVECTOR* from, const SVECTOR* to,
                                         int angle, std::uint32_t distance);
void SelectLiveActionMovementMode(Battle_Ai_CandidateReadinessView* state);
// Actor creation passes no action record; the propagation pass still runs.
void ApplyLiveActionRelationshipChanges(Battle_Ai_ReactionRecord* record);
}  // namespace VS::Battle::Ai
