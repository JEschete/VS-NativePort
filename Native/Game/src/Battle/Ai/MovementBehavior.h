#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_MOVEMENTBEHAVIOR_H
#define VS_OWNED_BATTLE_AI_MOVEMENTBEHAVIOR_H
#include "Legacy/Psx.h"

typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;
typedef struct Battle_Ai_TerrainMoveView Battle_Ai_TerrainMoveView;
typedef struct Battle_Ai_PerceptionUpdateView Battle_Ai_PerceptionUpdateView;
typedef struct Battle_Ai_CandidateMovementView Battle_Ai_CandidateMovementView;
typedef struct Battle_Ai_CandidateReadinessView
    Battle_Ai_CandidateReadinessView;
typedef struct Battle_Ai_ReactionRecord Battle_Ai_ReactionRecord;

#ifdef __cplusplus
extern "C" {
#endif

int Battle_Ai_SetMovementBehavior(Battle_Ai_ActorAiState* state, int mode);
void Battle_Ai_RefreshAiMovementCooldown(Battle_Ai_TerrainMoveView* state);
int Battle_Ai_ApproachActor(Battle_Ai_ActorAiState* state, int target_id,
                            int distance, int option);
void Battle_Ai_RefreshAiTargetTile(void* state);
void Battle_Ai_RefreshAiTargetTileWhenTracking(
    Battle_Ai_PerceptionUpdateView* state);
void Battle_Ai_WanderAroundSavedCenter(void* state);
void Battle_Ai_WanderAroundTile(Battle_Ai_CandidateMovementView* state,
                                unsigned int tile, int radius);
void Battle_Ai_MoveToTile(Battle_Ai_CandidateMovementView* state,
                          unsigned int tile, int radius);
void Battle_Ai_PatrolWaypoints(void* state);
int Battle_Ai_FleeVisibleEnemies(Battle_Ai_CandidateMovementView* state,
                                 int kind);
int Battle_Ai_TrackActorPosition(Battle_Ai_ActorAiState* state, int target);
int Battle_Ai_FindActorByRelationship(void* state, void* tile, int relation);
int Battle_Ai_TargetFacingCosine(SVECTOR* from, SVECTOR* to, int angle,
                                 u_int distance);
void Battle_Ai_SelectActionMovementMode(Battle_Ai_CandidateReadinessView*);
void Battle_Ai_ApplyActionRelationshipChanges(Battle_Ai_ReactionRecord*);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_AI_MOVEMENTBEHAVIOR_H
