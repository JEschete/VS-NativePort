#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_ENGAGEMENT_H
#define VS_OWNED_BATTLE_AI_ENGAGEMENT_H
#include "Battle/Ai/AiState.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Battle_Ai_SetActorAwarenessIndicator(int, int);

void Battle_Ai_ResetActorAwarenessIndicator(
    struct Battle_Ai_PlanEvaluationView*);

int Battle_Ai_IsEngagementReady(Battle_Ai_ActorAiState*);

void Battle_Ai_UpdateTemporaryAwarenessIndicator(
    Battle_Ai_PerceptionUpdateView*);

void Battle_Ai_DisengageActorById(int);

void Battle_Ai_DisengageActor(Battle_Ai_ActorAiState*);

int Battle_Ai_TryEngageActor(Battle_Ai_ActorAiState*);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_ENGAGEMENT_H
