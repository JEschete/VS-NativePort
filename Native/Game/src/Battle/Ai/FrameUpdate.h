#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_FRAMEUPDATE_H
#define VS_OWNED_BATTLE_AI_FRAMEUPDATE_H
#include "Legacy/Psx.h"

typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;
typedef struct Battle_Ai_RoomAiState Battle_Ai_RoomAiState;
typedef struct Battle_Ai_PerceptionUpdateView Battle_Ai_PerceptionUpdateView;
typedef struct Battle_Ai_AiCreationView Battle_Ai_AiCreationView;

enum {
  kAiFrameActorSlots = 16,
  kAiFrameGridSide = 32,
  kAiDirectionCacheWords = 3,
  kAiDirectionBytes = 8
};

typedef struct Battle_Ai_FrameContext {
  Battle_Ai_ActorAiState* actors[kAiFrameActorSlots];
  Battle_Ai_RoomAiState* room;
  u_short (*heightMap)[kAiFrameGridSide];
  u_short (*tileFlags)[kAiFrameGridSide];
  u_char* terrainRegions;
  size_t terrain_region_count;
  int movementResult;
  union {
    u_int directionCache[kAiDirectionCacheWords];
    struct {
      u_char walk_valid;
      u_char walk_passable;
      u_char blocker_valid;
      u_char reserved_cache;
      u_char blocker_actor[kAiDirectionBytes];
    };
  };
  u_int roomBounds;
  u_char directions[kAiDirectionBytes];
  void* roomData;
  u_int randomState;
  int lastBudgetTick;
} Battle_Ai_FrameContext;

#ifdef __cplusplus
extern "C" {
#endif

Battle_Ai_FrameContext* Battle_Ai_FrameState(void);
int Battle_Ai_TryTerrainRegionCell(int tile_x, int tile_z, u_char** cell);
u_char* Battle_Ai_TerrainRegionCell(int tile_x, int tile_z);
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
