// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONFLOW_H_TYPES
#define VS_BATTLE_ACTIONFLOW_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ACTIONFLOW_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ACTIONFLOW_H_DECLS)
#define VS_BATTLE_ACTIONFLOW_H_DECLS
#include "Battle/Action.h"
#include "Battle/Hit.h"
#include "Battle/TargetingState.h"
#include "Menu/Framework/ActionSelection.h"

extern Menu_Framework_ActionSelection* Battle_actionSelection;
extern u_int Battle_actionFlowMode;
extern int Battle_recoveryRewardMode;
extern Battle_ActionState* Battle_actionState;
extern char Battle_spellCommitFlag;

#endif // VS_BATTLE_ACTIONFLOW_H_DECLS
