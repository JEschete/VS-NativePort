#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_BATTLE_AI_DECISIONPRIMITIVES_H
#define VS_OWNED_BATTLE_AI_DECISIONPRIMITIVES_H
#include "Battle/Action.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

int Battle_Ai_DoesActionRequireCooldown(Battle_Ai_ActorAiState*,
                                        Battle_Ai_ActionCandidate*);

int Battle_Ai_IsActionCooldownReady(Battle_Ai_ActorAiState*,
                                    Battle_Ai_ActionCandidate*);

int Battle_Ai_HasNearbyEnemy(Battle_Ai_ActorAiState*, int);

int Battle_Ai_CanExecuteActionCandidate(Battle_Ai_ActorAiState*,
                                        Battle_Ai_ActionCandidate*);

int Battle_Ai_ScoreActorCondition(int, int, int, int);

int Battle_Ai_IsActionHarmfulToActor(int, int);

int Battle_Ai_IsActionBeneficialToActor(int, int);

void Battle_Ai_CacheFallbackActionCandidate(Battle_Ai_ActionCandidate*,
                                            Battle_ActionDefinition*);

void Battle_Ai_PromoteRandomizedActionCandidate(Battle_Ai_ActionPlanView*, int);

int Battle_Ai_NextActorRandomMasked(int);

#ifdef __cplusplus
}
#endif
#endif  // VS_OWNED_BATTLE_AI_DECISIONPRIMITIVES_H
