// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

// decomp: D_800E8200 (D_800E8200@BATTLE/BATTLE.PRG)
u_char Battle_limbInjuryStatusIds[20]; /* contents: module image (ModuleData.cpp) */

// decomp: _actionEffectMediators (_actionEffectMediators@BATTLE/BATTLE.PRG)
void (*Battle_actionEffectMediators[70])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800F1A00 (D_800F1A00@BATTLE/BATTLE.PRG)
u_int Battle_hitPrerequisiteResult;

// decomp: D_800F1A04 (D_800F1A04@BATTLE/BATTLE.PRG)
u_int Battle_previousHitResult;

}  // extern "C"
