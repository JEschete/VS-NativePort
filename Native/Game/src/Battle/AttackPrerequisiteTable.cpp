// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/AttackPrerequisiteTable.h"

// decomp: _attackPrerequisiteFunctions (_attackPrerequisiteFunctions@BATTLE/BATTLE.PRG)
int (*Battle_attackPrerequisiteFunctions[40])(Battle_ActionDefinition*, u_char*); /* contents: module image, relocated (ModulePointers.cpp) */

}  // extern "C"
