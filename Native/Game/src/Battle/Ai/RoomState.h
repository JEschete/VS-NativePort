#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_ROOMSTATE_H
#define VS_OWNED_BATTLE_AI_ROOMSTATE_H
#include "Legacy/Psx.h"
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
#include "Render/Palette.h"

void Battle_Ai_FreeRoomAiState(void);
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Shadow.h"
#include "Field/Gimmicks.h"

typedef struct Battle_Ai_RoomAiMap {
	Battle_Ai_RoomAiState state;
	u_short flags[32][32];
	u_short heights[32][32];
} Battle_Ai_RoomAiMap;

u_short Battle_Ai_DecodeCollisionFlags(unsigned bits);
int Battle_Ai_SelectTileHeight(const int samples[16], int center);
int Battle_Ai_BuildRoomAiMap(Battle_Ai_RoomAiMap* map);
void Battle_Ai_InitializeRoomAiMap(void);

int Battle_Ai_SampleAiTileGroundHeight(int tileX, int tileZ);

#endif // VS_OWNED_BATTLE_AI_ROOMSTATE_H
