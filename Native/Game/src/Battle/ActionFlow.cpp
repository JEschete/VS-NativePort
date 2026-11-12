// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Menu/Framework/ActionSelection.h"

// decomp: D_800F18EC (D_800F18EC@BATTLE/BATTLE.PRG)
Menu_Framework_ActionSelection* Battle_actionSelection;

// decomp: _cameraMode (_cameraMode@BATTLE/BATTLE.PRG)
u_int Battle_actionFlowMode;

// decomp: D_800F18FC (D_800F18FC@BATTLE/BATTLE.PRG)
int Battle_recoveryRewardMode;

// decomp: D_800F19CC (D_800F19CC@BATTLE/BATTLE.PRG)
Battle_ActionState* Battle_actionState;

// decomp: D_8005FFAF (D_8005FFAF@SLUS_010.40)
char Battle_spellCommitFlag;

}  // extern "C"
