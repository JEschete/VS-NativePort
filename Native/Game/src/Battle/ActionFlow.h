// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"


#include "Battle/Action.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Menu/Framework/ActionSelection.h"

extern Menu_Framework_ActionSelection* Battle_actionSelection;
extern u_int Battle_actionFlowMode;
extern int Battle_recoveryRewardMode;
extern Battle_ActionState* Battle_actionState;
