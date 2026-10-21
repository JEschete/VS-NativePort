#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_GROUNDNAVIGATION_H
#define VS_OWNED_BATTLE_AI_GROUNDNAVIGATION_H

typedef struct Battle_Ai_MoveTargetView Battle_Ai_MoveTargetView;
typedef struct Battle_Ai_MovementPathView Battle_Ai_MovementPathView;
typedef struct Battle_Ai_WanderView Battle_Ai_WanderView;
typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;
typedef struct Battle_Ai_TerrainMoveView Battle_Ai_TerrainMoveView;
typedef int (*Battle_Ai_PathTest)(Battle_Ai_MovementPathView*, int);

#ifdef __cplusplus
extern "C" {
#endif

int Battle_Ai_DestinationOccupancy(Battle_Ai_MoveTargetView* state,
                                   unsigned int tile);
int Battle_Ai_CanTraverseTileEdge(Battle_Ai_MoveTargetView* state,
                                  unsigned int from, unsigned int to,
                                  int direction);
int Battle_Ai_DirectionalBlockingActor(Battle_Ai_MovementPathView* state,
                                       int direction);
int Battle_Ai_CanWalkInDirection(Battle_Ai_WanderView* state, int direction);
unsigned int Battle_Ai_FindGapLandingTile(Battle_Ai_MoveTargetView* state,
                                          unsigned int tile, int direction,
                                          unsigned int from);
void Battle_Ai_SetDetourBoundary(Battle_Ai_ActorAiState* state);
unsigned int Battle_Ai_RefreshDetourProgress(Battle_Ai_ActorAiState* state,
                                             unsigned int flags);
int Battle_Ai_CardinalPassabilityMask(Battle_Ai_MovementPathView* state,
                                      Battle_Ai_PathTest test);
unsigned int Battle_Ai_InitializeObstacleDetour(Battle_Ai_ActorAiState* state,
                                                unsigned int flags,
                                                Battle_Ai_PathTest test);
int Battle_Ai_UpdateHesitationTurn(void* state, int chance, int kind, int slot);
int Battle_Ai_SelectActorVerticalMovementStep(Battle_Ai_ActorAiState* state,
                                              int* value, int* code);
int Battle_Ai_IsTileBehindActorTarget(Battle_Ai_ActorAiState* state,
                                      unsigned int tile);
unsigned int Battle_Ai_FindAiDescentTile(Battle_Ai_ActorAiState* state,
                                         unsigned int tile, int direction,
                                         int steps);
void Battle_Ai_ClearAiContactOccupancy(int bit);
void Battle_Ai_SetAiContactOccupancy(int bit);
void Battle_Ai_BeginAiTerrainMovement(void* state);
void Battle_Ai_BeginAiAlternateTerrainMovement(Battle_Ai_ActorAiState* state);
void Battle_Ai_MoveActorTowardDestinationTile(void* state, int angle);
void Battle_Ai_BeginAiGroundMovement(void* state, int angle);
void Battle_Ai_BeginAiMovementTowardPoint(Battle_Ai_ActorAiState* state,
                                          int kind);
void Battle_Ai_DispatchAiLocomotionMode(void* state, int kind, int angle);
void Battle_Ai_ResolveDirectionalMovement(Battle_Ai_ActorAiState* state,
                                          int angle, int mode);
extern unsigned int Content_battleAiHesitationLengths[1];

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_GROUNDNAVIGATION_H
