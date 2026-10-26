#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
#define VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;
typedef struct Field_GimmickInstance Field_GimmickInstance;

void Battle_Ai_ReserveAiDestinationTile(int, int);
void Battle_Ai_AddAiTileOccupant(const Field_GimmickInstance* gimmick);
void Battle_Ai_RemoveAiTileOccupant(const Field_GimmickInstance* gimmick);

int Battle_Ai_IsAdjacentTileStepBlocked(unsigned int, int);
void Battle_Ai_MarkAiTileDynamicObstacle(int, int);
void Battle_Ai_BuildAiTerrainHeightsWithWorkspace(void);
void Battle_Ai_UpdateRouteGateAvailability(void);

int Battle_Ai_HasActorNavigationState(int);
int Battle_Ai_ActorNavigationOverrideStatus(int);
void Battle_Ai_CopyActorNavigationOverrideState(int, Battle_Ai_ActorAiState*);
void Battle_Ai_SetActorNavigationOverride(int actor_id, int x, int z);
void Battle_Ai_UpdateActorNavigationOverrides(void);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_NAVIGATIONPRIMITIVES_H
