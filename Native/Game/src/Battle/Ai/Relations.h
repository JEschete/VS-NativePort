#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_RELATIONS_H
#define VS_OWNED_BATTLE_AI_RELATIONS_H

typedef struct Battle_Ai_AiCreationView Battle_Ai_AiCreationView;

#ifdef __cplusplus
extern "C" {
#endif

void Battle_Ai_InitializeActorAiRelations(Battle_Ai_AiCreationView* state);
void Battle_Ai_ApplySavedActorAiRelations(void);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_AI_RELATIONS_H
