#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_ACTORCOMMANDS_H
#define VS_OWNED_BATTLE_AI_ACTORCOMMANDS_H
#include "Battle/Ai/AiState.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif
void Battle_Ai_IssueActorCommand(Battle_Ai_ActorAiState*, int);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_ACTORCOMMANDS_H
