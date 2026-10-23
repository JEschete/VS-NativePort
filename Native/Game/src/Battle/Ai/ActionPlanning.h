// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_ACTIONPLANNING_H_TYPES
#define VS_BATTLE_AI_ACTIONPLANNING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_ACTIONPLANNING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_ACTIONPLANNING_H_DECLS)
#define VS_BATTLE_AI_ACTIONPLANNING_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

extern u_int Battle_Ai_kTimedStatusFlags[344];
extern Battle_ActorStats* Battle_Ai_planningStatsSnapshot;
extern Battle_Ai_CandidateEntryView (*Battle_Ai_planningPassCandidates)[16];
extern int Battle_Ai_fallbackCandidateCount;
extern int Battle_Ai_unusedPlanCounter;
extern Battle_Ai_CandidateEntryView* Battle_Ai_fallbackCandidates;
extern Battle_Ai_ReactionRecord* Battle_Ai_planningWorkspace;
extern u_char (*Battle_Ai_candidateExclusions)[256];
int Battle_Ai_RefreshActionCandidateReadiness(Battle_Ai_CandidateReadinessView* state);
void Battle_Ai_ApplySimulatedActionResults(Battle_Ai_ActionTiming* arg0);
void Battle_Ai_SimulateActionCandidate(
    Battle_Ai_ActorAiState* arg0, Battle_Ai_ActionTiming* arg1, Battle_Ai_ActionCandidate* arg2, int arg3);
void Battle_Ai_UpdateCandidateHitWeight(Battle_Ai_ActionCandidate* arg0);
int Battle_Ai_ActorStatusConditionScore(Battle_Ai_ActionTiming* stats);
int Battle_Ai_ActorHpRatio(Battle_Ai_ActionTiming* stats, int id);
int Battle_Ai_ActorMpRatio(Battle_Ai_ActionTiming* stats, int id);
int Battle_Ai_ActorEquipmentDpRatio(Battle_Ai_ActionTiming* stats, int id);
void Battle_Ai_EvaluateActionCandidate(
    Battle_Ai_ActorAiState* arg0, Battle_Ai_ActionCandidate* arg1, int arg2, Battle_ActionDefinition* arg3);
void Battle_Ai_SelectFallbackAction(Battle_Ai_ActorAiState* arg0);
void Battle_Ai_EnumerateActionCandidates(Battle_Ai_ActorAiState* arg0, int arg1);
void Battle_Ai_AdvancePlanningSimulation(int arg0);
int Battle_Ai_SnapshotPlanningActorStats(void);
void Battle_Ai_RestorePlanningActorStats(void);
void Battle_Ai_BuildActorActionPlan(Battle_Ai_ActionPlanView* state);
void Battle_Ai_ReconcileCompletedActionPlan(Battle_Ai_ReactionRecord* arg0);

#endif // VS_BATTLE_AI_ACTIONPLANNING_H_DECLS
