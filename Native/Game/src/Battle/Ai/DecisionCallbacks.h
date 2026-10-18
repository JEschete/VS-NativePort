#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_DECISIONCALLBACKS_H
#define VS_OWNED_BATTLE_AI_DECISIONCALLBACKS_H
#include "Legacy/Psx.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

int Battle_Ai_UpdateFleeOrWanderBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateAdaptivePursuitBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateSupportFollowBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateEngagementApproachBehavior(Battle_Ai_ActorAiState* state);

#endif // VS_OWNED_BATTLE_AI_DECISIONCALLBACKS_H
