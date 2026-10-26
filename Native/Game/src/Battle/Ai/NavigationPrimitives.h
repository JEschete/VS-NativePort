#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
#define VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;

void Battle_Ai_ReserveAiDestinationTile(int, int);

int Battle_Ai_IsAdjacentTileStepBlocked(unsigned int, int);

int Battle_Ai_HasActorNavigationState(int);
int Battle_Ai_ActorNavigationOverrideStatus(int);
void Battle_Ai_CopyActorNavigationOverrideState(int, Battle_Ai_ActorAiState*);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
