#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_FLIGHTTERRAIN_H
#define VS_OWNED_BATTLE_AI_FLIGHTTERRAIN_H
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

int Battle_Ai_LowestReachableGroundHeight(Battle_Ai_ActorAiState* state,
                                          u_int movement_angle);
#include "Actors/Skeleton.h"
#include "Field/Gimmicks.h"

int Battle_Ai_MinimumAiGroundHeightAlongSegment(int start_x, int start_z,
                                                int end_x, int end_z);

int Battle_Ai_RoomNavigationCeilingHeight(void);

#endif  // VS_OWNED_BATTLE_AI_FLIGHTTERRAIN_H
