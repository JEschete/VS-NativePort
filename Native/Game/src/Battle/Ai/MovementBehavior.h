#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_MOVEMENTBEHAVIOR_H
#define VS_OWNED_BATTLE_AI_MOVEMENTBEHAVIOR_H

typedef struct Battle_Ai_ActorAiState Battle_Ai_ActorAiState;
typedef struct Battle_Ai_TerrainMoveView Battle_Ai_TerrainMoveView;
typedef struct Battle_Ai_PerceptionUpdateView Battle_Ai_PerceptionUpdateView;

#ifdef __cplusplus
extern "C" {
#endif

int Battle_Ai_SetMovementBehavior(Battle_Ai_ActorAiState* state, int mode);
void Battle_Ai_RefreshAiMovementCooldown(Battle_Ai_TerrainMoveView* state);
int Battle_Ai_ApproachActor(Battle_Ai_ActorAiState* state, int target_id,
                            int distance, int option);
void Battle_Ai_RefreshAiTargetTile(void* state);
void Battle_Ai_RefreshAiTargetTileWhenTracking(
    Battle_Ai_PerceptionUpdateView* state);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_AI_MOVEMENTBEHAVIOR_H
