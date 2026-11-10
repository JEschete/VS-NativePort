// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_AI_ACTIONPLANNING_H_TYPES
#define VS_BATTLE_AI_ACTIONPLANNING_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_AI_ACTIONPLANNING_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_AI_ACTIONPLANNING_H_DECLS)
#define VS_BATTLE_AI_ACTIONPLANNING_H_DECLS
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.h"
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
void Battle_Ai_ReconcileCompletedActionPlan(Battle_Ai_ReactionRecord* completed_action);

#endif // VS_BATTLE_AI_ACTIONPLANNING_H_DECLS
