#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_RELATIONS_H
#define VS_OWNED_BATTLE_AI_RELATIONS_H

typedef struct Battle_Ai_AiCreationView Battle_Ai_AiCreationView;
typedef struct Battle_Ai_ReactionView Battle_Ai_ReactionView;

#ifdef __cplusplus
extern "C" {
#endif

void Battle_Ai_InitializeActorAiRelations(Battle_Ai_AiCreationView* state);
void Battle_Ai_ApplySavedActorAiRelations(void);
void Battle_Ai_EndAshleyAlliance(Battle_Ai_ReactionView* state);
void Battle_Ai_TryBeginAshleyAlliance(Battle_Ai_ReactionView* state,
                                      int forced);

#ifdef __cplusplus
}
#endif

#endif  // VS_OWNED_BATTLE_AI_RELATIONS_H
