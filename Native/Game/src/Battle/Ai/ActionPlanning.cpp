// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/AiViews.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

// decomp: D_80069BBC (D_80069BBC@BATTLE/BATTLE.PRG)
u_int Battle_Ai_kTimedStatusFlags[344]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F58E8 (D_800F58E8@BATTLE/BATTLE.PRG)
Battle_ActorStats* Battle_Ai_planningStatsSnapshot;

// decomp: D_800F58EC (D_800F58EC@BATTLE/BATTLE.PRG)
Battle_Ai_CandidateEntryView (*Battle_Ai_planningPassCandidates)[16];

// decomp: D_800F58F0 (D_800F58F0@BATTLE/BATTLE.PRG)
int Battle_Ai_fallbackCandidateCount;

// decomp: D_800F58F4 (D_800F58F4@BATTLE/BATTLE.PRG)
int Battle_Ai_unusedPlanCounter;

// decomp: D_800F58F8 (D_800F58F8@BATTLE/BATTLE.PRG)
Battle_Ai_CandidateEntryView* Battle_Ai_fallbackCandidates;

// decomp: D_800F5900 (D_800F5900@BATTLE/BATTLE.PRG)
Battle_Ai_ReactionRecord* Battle_Ai_planningWorkspace;

// decomp: D_800F5904 (D_800F5904@BATTLE/BATTLE.PRG)
u_char (*Battle_Ai_candidateExclusions)[256];

}  // extern "C"
