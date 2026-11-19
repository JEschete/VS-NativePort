// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Battle/Action.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Legacy/Psx.h"

namespace VS::Menu::Framework {
struct ActionSelection;
}  // namespace VS::Menu::Framework

extern VS::Menu::Framework::ActionSelection* Battle_actionSelection;
extern u_int Battle_actionFlowMode;
extern int Battle_recoveryRewardMode;
extern Battle_ActionState* Battle_actionState;
