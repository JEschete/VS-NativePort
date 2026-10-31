// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Event/Script.h"

// decomp: D_800E9B5C (D_800E9B5C@BATTLE/BATTLE.PRG)
u_char Event_specialSubroutineScript[468]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4C28 (D_800F4C28@BATTLE/BATTLE.PRG)
intptr_t (**Event_legacySkipHandlerCopy)(u_char*, short);

}  // extern "C"
