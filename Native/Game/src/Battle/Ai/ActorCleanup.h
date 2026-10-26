#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_ACTORCLEANUP_H
#define VS_OWNED_BATTLE_AI_ACTORCLEANUP_H
#include <stdint.h>

#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

void Battle_Ai_ReleaseAiDestinationTile(int, int);

void Battle_Ai_FreeActorNavigationOverride(int);

void Battle_Ai_ForceReleaseActorAiReservations(int);

void Battle_Ai_ReleaseActorAiReservations(Battle_Ai_ActorAiState*);

void Battle_Ai_FreeActorAiState(Battle_Actor*);
void Battle_Ai_SetActorAiReactionWithDelay(Battle_Ai_PerceptionUpdateView*,
                                           int value, int range);
void Battle_Ai_RefreshActorAiReservations(int actor_id);
void Battle_Ai_ClearActorAiLinksAndPlan(int actor_id);
int16_t Battle_Ai_PackActorAiPersistenceFlags(int actor_id);
void Battle_Ai_ClearActorAiNavigationReset(int actor_id);
void Battle_Ai_InitializeActorAiExtensionHook(void);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_ACTORCLEANUP_H
