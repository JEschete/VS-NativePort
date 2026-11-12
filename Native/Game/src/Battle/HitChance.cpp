// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"

// decomp: _hitFunctions (_hitFunctions@BATTLE/BATTLE.PRG)
int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.cpp) */

}  // extern "C"
