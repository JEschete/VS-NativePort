#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_FRAMEUPDATE_H
#define VS_OWNED_BATTLE_AI_FRAMEUPDATE_H
#include "Legacy/Psx.h"

typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;
typedef struct Battle_Ai_RoomAiState Battle_Ai_RoomAiState;
typedef struct Battle_Ai_PerceptionUpdateView Battle_Ai_PerceptionUpdateView;
typedef struct Battle_Ai_AiCreationView Battle_Ai_AiCreationView;

typedef struct Battle_Ai_FrameContext {
  Battle_Ai_ActorAiState* actors[16];
  Battle_Ai_RoomAiState* room;
  u_short (*heightMap)[32];
  u_short (*tileFlags)[32];
  u_char* terrainRegions;
  int movementResult;
  u_int directionCache[3];
  u_int roomBounds;
  u_char directions[8];
  void* roomData;
  u_int randomState;
  int lastBudgetTick;
} Battle_Ai_FrameContext;

#ifdef __cplusplus
extern "C" {
#endif

Battle_Ai_FrameContext* Battle_Ai_FrameState(void);
int Battle_Ai_CacheActorAiWorkingState(void);
void Battle_Ai_RunActorAiUpdateStages(void);
void Battle_Ai_TickActorDecisionTimers(Battle_Ai_PerceptionUpdateView* state);
void Battle_Ai_InitializeActorLocomotionMode(Battle_Ai_AiCreationView* state);
void Battle_Ai_SetActorVerticalMovementSteps(int actor_id, int walk_step,
                                             int run_step);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_AI_FRAMEUPDATE_H
