// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/OrbitingBoss.h"

// decomp: D_800F17DC (D_800F17DC@BATTLE/BATTLE.PRG)
short Battle_Ai_radialRetreatPath[36][2]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F5918 (D_800F5918@BATTLE/BATTLE.PRG)
int Battle_Ai_orbitAngularVelocity;

// decomp: D_800F591C (D_800F591C@BATTLE/BATTLE.PRG)
int Battle_Ai_orbitHeightOffset;

// decomp: D_800F5920 (D_800F5920@BATTLE/BATTLE.PRG)
Battle_Ai_ArenaOrbitState* Battle_Ai_arenaOrbit;

}  // extern "C"
