// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Ai/Movement.h"

// decomp: D_800F58C4 (D_800F58C4@BATTLE/BATTLE.PRG)
int Battle_Ai_savedMovementRandomState;

// decomp: D_800F58D8 (D_800F58D8@BATTLE/BATTLE.PRG)
unsigned short (*Battle_Ai_patrolRoutes)[8];

}  // extern "C"
