// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_BEHAVIOR_H_TYPES
#define VS_BATTLE_AI_BEHAVIOR_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_BEHAVIOR_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_BEHAVIOR_H_DECLS)
#define VS_BATTLE_AI_BEHAVIOR_H_DECLS
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"

void Battle_Ai_SelectActionMovementMode(Battle_Ai_CandidateReadinessView*);
int Battle_Ai_MoveForActionCandidate(Battle_Ai_CandidateMovementView* state, Battle_Ai_CandidateEntryView* entry);
int Battle_Ai_UpdateCombatPursuit(Battle_Ai_ActorAiState* arg0);
int Battle_Ai_TryApproachPlannedTarget(Battle_Ai_ActorAiState* arg0, Battle_Ai_ActionCandidate* arg1);
int Battle_Ai_UpdateCloseFollowBehavior(Battle_Ai_EscortView* state);
int Battle_Ai_UpdateEnemyPursuitBehavior(Battle_Ai_EscortView* state);
int Battle_Ai_UpdateGuardMovementBehavior(Battle_Ai_EscortView* state);
int Battle_Ai_UpdatePatrolCombatBehavior(Battle_Ai_EscortView* state);
int Battle_Ai_UpdateEscortCombatBehavior(Battle_Ai_EscortView* state);
int Battle_Ai_TryExecuteActionCandidate(Battle_Ai_ActorAiState* arg0, Battle_Ai_ActionCandidate* arg1);
int Battle_Ai_TryExecutePlannedActions(Battle_Ai_ActorAiState* arg0);
void Battle_Ai_UpdateActorDecisions(void);
int Battle_Ai_CanApproachEngagedTarget(Battle_Ai_ActorAiState*, Battle_Ai_ActionCandidate*);
void Battle_Ai_IssueActorCommand(Battle_Ai_ActorAiState*, int);
int Battle_Ai_UpdateHesitationTurn(void*, int, int, int);

#endif // VS_BATTLE_AI_BEHAVIOR_H_DECLS
