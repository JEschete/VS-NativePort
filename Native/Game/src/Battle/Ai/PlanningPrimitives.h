#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_PLANNINGPRIMITIVES_H
#define VS_OWNED_BATTLE_AI_PLANNINGPRIMITIVES_H
#include <stdint.h>

#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif
void Battle_Ai_CopyActionCandidate(Battle_Ai_ActionCandidate*,
                                   Battle_Ai_ActionCandidate*);

void Battle_Ai_ApplySimulatedStatDelta(int16_t*, int16_t*, int, int);

void Battle_Ai_AppendActionCandidate(Battle_Ai_ActorAiState*,
                                     Battle_Ai_ActionCandidate*);

void Battle_Ai_SortActionCandidates(Battle_Ai_ActionPlanView*);

void Battle_Ai_CopyPlanningActorStats(Battle_ActorStats* src,
                                      Battle_ActorStats* dst);

int Battle_Ai_CanAiActionAffectActor(Battle_Ai_ActorAiState*,
                                     Battle_Ai_ActionCandidate*, int);

int Battle_Ai_RefreshAiActionUsability(Battle_Ai_ActorAiState*, int, int);

void Battle_Ai_RefreshAiResourceRestrictions(Battle_Ai_ActorAiState*);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_PLANNINGPRIMITIVES_H
