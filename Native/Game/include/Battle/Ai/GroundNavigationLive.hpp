#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "Battle/Ai/GroundNavigation.hpp"

namespace VS::Battle::Ai {
struct ActorAiState;
struct MoveTargetView;
struct MovementPathView;
struct WanderView;
}  // namespace VS::Battle::Ai

namespace VS::Content {
// The hesitation lengths (bytes 1..3) loaded from BATTLE.PRG.
extern unsigned int battleAiHesitationLengths[1];
}  // namespace VS::Content

namespace VS::Battle::Ai {
// decomp: movementPathTest (src/BATTLE/BATTLE.PRG/771A8.c:2383)
using PathTest = int (*)(VS::Battle::Ai::MovementPathView*, int);
// Bindings from the actor AI state table, the room's AI frame context, the
// battle actor list and the movement code to the typed ground navigation
// (GroundNavigation.cpp). The views are the actor's AI state. Each aborts
// with a diagnostic when the typed routine refuses its input; the original
// read past its tables or the room maps.
int LiveDestinationOccupancy(VS::Battle::Ai::MoveTargetView* state,
                             unsigned int tile);
int CanTraverseLiveTileEdge(VS::Battle::Ai::MoveTargetView* state,
                            unsigned int from, unsigned int to, int direction);
int LiveDirectionalBlockingActor(VS::Battle::Ai::MovementPathView* state,
                                 int direction);
int CanLiveActorWalkInDirection(VS::Battle::Ai::WanderView* state,
                                int direction);
unsigned int FindLiveGapLandingTile(VS::Battle::Ai::MoveTargetView* state,
                                    unsigned int tile, int direction,
                                    unsigned int from);
void SetLiveDetourBoundary(VS::Battle::Ai::ActorAiState* state);
unsigned int RefreshLiveDetourProgress(VS::Battle::Ai::ActorAiState* state,
                                       unsigned int flags);
int LiveCardinalPassabilityMask(VS::Battle::Ai::MovementPathView* state,
                                VS::Battle::Ai::PathTest test);
unsigned int InitializeLiveObstacleDetour(VS::Battle::Ai::ActorAiState* state,
                                          unsigned int flags,
                                          VS::Battle::Ai::PathTest test);
int FollowLiveObstacleDetour(VS::Battle::Ai::MovementPathView* state,
                             VS::Battle::Ai::PathTest test,
                             int blocked_flag_bit);
int UpdateLiveHesitationTurn(void* state, int chance, int kind, int slot);
int SelectLiveActorVerticalMovementStep(VS::Battle::Ai::ActorAiState* state,
                                        int* value, int* code);
int IsTileBehindLiveActorTarget(VS::Battle::Ai::ActorAiState* state,
                                unsigned int tile);
unsigned int FindLiveAiDescentTile(VS::Battle::Ai::ActorAiState* state,
                                   unsigned int tile, int direction, int steps);
void ClearLiveAiContactOccupancy(int bit);
void SetLiveAiContactOccupancy(int bit);
void BeginLiveAiTerrainMovement(void* state);
void BeginLiveAiAlternateTerrainMovement(VS::Battle::Ai::ActorAiState* state);
void MoveLiveActorTowardDestinationTile(void* state, int angle);
void BeginLiveAiGroundMovement(void* state, int angle);
void BeginLiveAiMovementTowardPoint(VS::Battle::Ai::ActorAiState* state,
                                    int kind);
void DispatchLiveAiLocomotionMode(void* state, int kind, int angle);
void ResolveLiveDirectionalMovement(VS::Battle::Ai::ActorAiState* state,
                                    int angle, int mode);
}  // namespace VS::Battle::Ai
