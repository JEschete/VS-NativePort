// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Core/Random.h"

// decomp: _keystreamState (_keystreamState@BATTLE/BATTLE.PRG)
u_int Core_interfaceRandomState; /* contents: module image (ModuleData.cpp) */

// decomp: randArr (randArr@SLUS_010.40)
int Core_randomShuffleTable[97];

// decomp: randIndex (randIndex@SLUS_010.40)
int Core_randomShuffleIndex;

}  // extern "C"
