// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/FrameUpdate.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/RoomState.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Timing.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Field/Platforms.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

static Battle_Ai_FrameContext frame_context;

Battle_Ai_FrameContext* Battle_Ai_FrameState(void) { return &frame_context; }

// decomp: BATTLE.PRG func_800E65DC (src/BATTLE/BATTLE.PRG/7DDDC.s:8).
int Battle_Ai_CacheActorAiWorkingState(void) {
  enum { kTerrainRegionOffset = 0x498 };
  for (size_t slot = 0;
       slot < sizeof frame_context.actors / sizeof frame_context.actors[0];
       ++slot) {
    frame_context.actors[slot] = Battle_Ai_actorAiStates[slot];
  }
  frame_context.room = Battle_Ai_roomAiState;
  frame_context.heightMap = Battle_Ai_specialTerrainHeights;
  frame_context.tileFlags = Battle_Ai_tileFlagMap;
  frame_context.roomData = Battle_Ai_roomHasWaypoints;
  frame_context.terrainRegions =
      frame_context.roomData != NULL
          ? (u_char*)frame_context.roomData + kTerrainRegionOffset
          : NULL;
  frame_context.randomState = (u_int)Battle_Ai_savedMovementRandomState;
  memcpy(frame_context.directions, Battle_Ai_directionDeltas,
         sizeof frame_context.directions);
  frame_context.roomBounds = 0;
  if (frame_context.room != NULL) {
    memcpy(&frame_context.roomBounds, frame_context.room->reserved4,
           sizeof frame_context.roomBounds);
  }
  return (int)frame_context.roomBounds;
}

// decomp: BATTLE.PRG func_800E511C; stages at 0x800F16BC (7C7E0.s:108).
void Battle_Ai_RunActorAiUpdateStages(void) {
  enum {
    kActorCount = sizeof frame_context.actors / sizeof frame_context.actors[0]
  };
  const unsigned actor_mask = (1u << kActorCount) - 1;
  if (Battle_Ai_tileFlagMap == NULL) return;
  if (Battle_Ai_roomAiState == NULL || Battle_actors[0] == NULL) {
    Core_Debugf("ai", "AI update requires a room and the linked actor list");
    abort();
  }
  Battle_Ai_CacheActorAiWorkingState();
  const unsigned mask = (unsigned)Battle_Ai_attributeActorMask & actor_mask;
  unsigned target =
      (unsigned)Battle_Ai_secondPerceptionTarget & (kActorCount - 1);
  if (mask == 0) {
    target = 0;
  } else {
    for (unsigned step = 0; step < kActorCount; ++step) {
      target = (target + 1) & (kActorCount - 1);
      if (mask & (1u << target)) break;
    }
  }
  Battle_Ai_secondPerceptionTarget = (int)target;
  Battle_Ai_UpdateActorPerception();
  Battle_Ai_UpdateActorDecisions();
  Battle_Ai_AnimateEngagementPhase();
  Battle_Ai_SaveMovementRandomState();
}

// decomp: BATTLE.PRG func_800E4358 (src/BATTLE/BATTLE.PRG/7B5DC.s:433).
void Battle_Ai_TickActorDecisionTimers(Battle_Ai_PerceptionUpdateView* view) {
  Battle_Ai_ActorAiState* state = (void*)view;
  if (state->decisionDelay != 0) --state->decisionDelay;
  if (state->minActionDelay != 0) --state->minActionDelay;
}

// decomp: BATTLE.PRG func_800E6828 (src/BATTLE/BATTLE.PRG/7E028.s:8).
void Battle_Ai_InitializeActorLocomotionMode(Battle_Ai_AiCreationView* view) {
  enum { kModeOneFlags = 0x30, kModeTwoFlag = 8 };
  Battle_Ai_ActorAiState* state = (void*)view;
  state->movementFlags =
      state->locomotionFlags | (state->locomotionFlags >> CHAR_BIT);
  state->locomotionMode = (state->movementFlags & kModeOneFlags) != 0  ? 1
                          : (state->movementFlags & kModeTwoFlag) != 0 ? 2
                                                                       : 0;
}

// decomp: BATTLE.PRG func_800E685C (src/BATTLE/BATTLE.PRG/7E05C.s:16).
void Battle_Ai_SetActorVerticalMovementSteps(int actor_id, int walk_step,
                                             int run_step) {
  if ((unsigned)actor_id >=
      sizeof Battle_Ai_actorAiStates / sizeof Battle_Ai_actorAiStates[0]) {
    abort();
  }
  Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[actor_id];
  if (state == NULL) return;
  const unsigned packed =
      (unsigned)walk_step | ((unsigned)run_step << CHAR_BIT);
  state->walkStep = (u_char)packed;
  state->runStep = (u_char)(packed >> CHAR_BIT);
  state->maxRange = (u_char)(walk_step < run_step ? walk_step : run_step);
}
