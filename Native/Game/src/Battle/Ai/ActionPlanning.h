// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.h"
#include "Items/ItemRecords.hpp"
#include "Legacy/Psx.h"

extern u_int Battle_Ai_kTimedStatusFlags[344];
extern Battle_ActorStats* Battle_Ai_planningStatsSnapshot;
extern Battle_Ai_CandidateEntryView (*Battle_Ai_planningPassCandidates)[16];
extern int Battle_Ai_fallbackCandidateCount;
extern int Battle_Ai_unusedPlanCounter;
extern Battle_Ai_CandidateEntryView* Battle_Ai_fallbackCandidates;
extern Battle_Ai_ReactionRecord* Battle_Ai_planningWorkspace;
extern u_char (*Battle_Ai_candidateExclusions)[256];
