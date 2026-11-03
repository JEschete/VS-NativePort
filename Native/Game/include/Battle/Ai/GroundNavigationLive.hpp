#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/GroundNavigation.hpp"

struct Battle_Ai_ActorAiState;
struct Battle_Ai_MoveTargetView;
struct Battle_Ai_MovementPathView;
struct Battle_Ai_WanderView;
// NOLINTNEXTLINE(modernize-use-using): shared with the C movement headers.
typedef int (*Battle_Ai_PathTest)(Battle_Ai_MovementPathView*, int);

extern "C" {
// The hesitation lengths (bytes 1..3) loaded from BATTLE.PRG.
extern unsigned int Content_battleAiHesitationLengths[1];
}

// Bindings from the actor AI state table, the room's AI frame context, the
// battle actor list and the movement code to the typed ground navigation
// (GroundNavigation.cpp). The views are the actor's AI state. Each aborts
// with a diagnostic when the typed routine refuses its input; the original
// read past its tables or the room maps.
namespace VS::Battle::Ai {
int LiveDestinationOccupancy(Battle_Ai_MoveTargetView* state,
                             unsigned int tile);
int CanTraverseLiveTileEdge(Battle_Ai_MoveTargetView* state, unsigned int from,
                            unsigned int to, int direction);
int LiveDirectionalBlockingActor(Battle_Ai_MovementPathView* state,
                                 int direction);
int CanLiveActorWalkInDirection(Battle_Ai_WanderView* state, int direction);
unsigned int FindLiveGapLandingTile(Battle_Ai_MoveTargetView* state,
                                    unsigned int tile, int direction,
                                    unsigned int from);
void SetLiveDetourBoundary(Battle_Ai_ActorAiState* state);
unsigned int RefreshLiveDetourProgress(Battle_Ai_ActorAiState* state,
                                       unsigned int flags);
int LiveCardinalPassabilityMask(Battle_Ai_MovementPathView* state,
                                Battle_Ai_PathTest test);
unsigned int InitializeLiveObstacleDetour(Battle_Ai_ActorAiState* state,
                                          unsigned int flags,
                                          Battle_Ai_PathTest test);
int UpdateLiveHesitationTurn(void* state, int chance, int kind, int slot);
int SelectLiveActorVerticalMovementStep(Battle_Ai_ActorAiState* state,
                                        int* value, int* code);
int IsTileBehindLiveActorTarget(Battle_Ai_ActorAiState* state,
                                unsigned int tile);
unsigned int FindLiveAiDescentTile(Battle_Ai_ActorAiState* state,
                                   unsigned int tile, int direction, int steps);
void ClearLiveAiContactOccupancy(int bit);
void SetLiveAiContactOccupancy(int bit);
void BeginLiveAiTerrainMovement(void* state);
void BeginLiveAiAlternateTerrainMovement(Battle_Ai_ActorAiState* state);
void MoveLiveActorTowardDestinationTile(void* state, int angle);
void BeginLiveAiGroundMovement(void* state, int angle);
void BeginLiveAiMovementTowardPoint(Battle_Ai_ActorAiState* state, int kind);
void DispatchLiveAiLocomotionMode(void* state, int kind, int angle);
void ResolveLiveDirectionalMovement(Battle_Ai_ActorAiState* state, int angle,
                                    int mode);
}  // namespace VS::Battle::Ai
