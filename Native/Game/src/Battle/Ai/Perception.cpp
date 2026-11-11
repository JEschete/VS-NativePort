// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Ai/Perception.h"

// decomp: D_800F16F4 (D_800F16F4@BATTLE/BATTLE.PRG)
int Battle_Ai_frameTickBudget; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F58D4 (D_800F58D4@BATTLE/BATTLE.PRG)
int Battle_Ai_secondPerceptionTarget;

// decomp: D_800F58E0 (D_800F58E0@BATTLE/BATTLE.PRG)
int Battle_Ai_candidateEngagedThisUpdate;

}  // extern "C"
