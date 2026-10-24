#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_ROOMSTATE_H
#define VS_OWNED_BATTLE_AI_ROOMSTATE_H
#include "Actors/ActorAllocation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

void Battle_Ai_FreeRoomAiState(void);
void Battle_Ai_InvalidateSessionBindings(void);
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Shadow.h"
#include "Field/Gimmicks.h"

enum { kAiRoomGridSide = 32, kAiTileSampleCount = 16 };

typedef struct Battle_Ai_RoomAiMap {
  Battle_Ai_RoomAiState state;
  u_short flags[kAiRoomGridSide][kAiRoomGridSide];
  u_short heights[kAiRoomGridSide][kAiRoomGridSide];
} Battle_Ai_RoomAiMap;

u_short Battle_Ai_DecodeCollisionFlags(unsigned bits);
int Battle_Ai_SelectTileHeight(const int samples[kAiTileSampleCount],
                               int center);
int Battle_Ai_BuildRoomAiMap(Battle_Ai_RoomAiMap* map);
void Battle_Ai_InitializeRoomAiMap(void);

int Battle_Ai_SampleAiTileGroundHeight(int tile_x, int tile_z);

#endif  // VS_OWNED_BATTLE_AI_ROOMSTATE_H
