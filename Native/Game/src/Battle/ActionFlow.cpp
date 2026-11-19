// Copied from the decomp by tools/port/copy_to_native.py; see each item's
// decomp line.
#include "Battle/ActionFlow.h"

#include "Battle/Action.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Legacy/Psx.h"

// decomp: D_800F18EC (D_800F18EC@BATTLE/BATTLE.PRG)
VS::Menu::Framework::ActionSelection* Battle_actionSelection;

// decomp: _cameraMode (_cameraMode@BATTLE/BATTLE.PRG)
u_int Battle_actionFlowMode;

// decomp: D_800F18FC (D_800F18FC@BATTLE/BATTLE.PRG)
int Battle_recoveryRewardMode;

// decomp: D_800F19CC (D_800F19CC@BATTLE/BATTLE.PRG)
Battle_ActionState* Battle_actionState;

// D_8005FFAF was vs_main_scoredata.weaponKillStreak (+0x10F), not a separate
// global (#448; State/KillScoreLive.cpp ResetLiveWeaponKillStreak).

