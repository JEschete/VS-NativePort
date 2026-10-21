#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_DECISIONCALLBACKS_H
#define VS_OWNED_BATTLE_AI_DECISIONCALLBACKS_H
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

int Battle_Ai_UpdatePatrolCombatBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateFleeOrWanderBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateAdaptivePursuitBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateSupportFollowBehavior(Battle_Ai_ActorAiState* state);

int Battle_Ai_UpdateEngagementApproachBehavior(Battle_Ai_ActorAiState* state);

void Battle_Ai_UpdateActorLocomotionMode(Battle_Ai_DecisionDispatchView* state);

void Battle_Ai_DispatchMovementBehavior(Battle_Ai_DecisionDispatchView* state);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_AI_DECISIONCALLBACKS_H
