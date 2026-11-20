#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Battle/Ai/MovementBehavior.hpp"

struct SVECTOR;
namespace VS::Battle::Ai {
struct ActorAiState;
struct CandidateMovementView;
struct CandidateReadinessView;
struct EscortView;
struct PerceptionUpdateView;
struct ReactionRecord;
struct TargetApproachView;
struct TerrainMoveView;
union GuardTileWord;
}  // namespace VS::Battle::Ai

// Bindings from the actor AI state table, the actor models, the room's patrol
// routes and route gates and the battle actor list to the typed movement
// behaviours (MovementBehavior.cpp). Callers pass the AI state view they hold;
// each view aliases the actor AI state. Each binding aborts with a diagnostic
// when a state, target ID, model, patrol row or route gate is invalid.
namespace VS::Battle::Ai {
int SetLiveMovementBehavior(VS::Battle::Ai::ActorAiState* state, int mode);
void RefreshLiveAiMovementCooldown(VS::Battle::Ai::TerrainMoveView* state);
int ApproachLiveActor(VS::Battle::Ai::ActorAiState* state, int target,
                      int distance, int option);
void RefreshLiveAiTargetTile(VS::Battle::Ai::TargetApproachView* state);
void RefreshLiveAiTargetTileWhenTracking(
    VS::Battle::Ai::PerceptionUpdateView* state);
void WanderAroundLiveSavedCenter(VS::Battle::Ai::ActorAiState* state);
void WanderAroundLiveTile(VS::Battle::Ai::CandidateMovementView* state,
                          std::uint32_t tile, int radius);
void MoveLiveActorToTile(VS::Battle::Ai::CandidateMovementView* state,
                         std::uint32_t tile, int radius);
void PatrolLiveWaypoints(VS::Battle::Ai::ActorAiState* state);
int FleeLiveVisibleEnemies(VS::Battle::Ai::CandidateMovementView* state,
                           int kind);
int TrackLiveActorPosition(VS::Battle::Ai::ActorAiState* state, int target);
// The ID of the first other listed actor with relationship class `relation`,
// or -1; stores its tile when `tile` is set.
int FindLiveActorByRelationship(VS::Battle::Ai::EscortView* state,
                                VS::Battle::Ai::GuardTileWord* tile,
                                int relation);
int FindLiveActorByRelationship(VS::Battle::Ai::ActorAiState* state,
                                std::uint32_t* tile, int relation);
[[nodiscard]] int LiveTargetFacingCosine(const SVECTOR* from, const SVECTOR* to,
                                         int angle, std::uint32_t distance);
void SelectLiveActionMovementMode(
    VS::Battle::Ai::CandidateReadinessView* state);
// Actor creation passes no action record; the propagation pass still runs.
void ApplyLiveActionRelationshipChanges(VS::Battle::Ai::ReactionRecord* record);
}  // namespace VS::Battle::Ai
