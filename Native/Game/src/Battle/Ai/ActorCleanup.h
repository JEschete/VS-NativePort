#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_ACTORCLEANUP_H
#define VS_OWNED_BATTLE_AI_ACTORCLEANUP_H
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

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_ACTORCLEANUP_H
