// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Field_ManualDisplayState;

// decomp: vs_battle_manualDisplayState_t (src/BATTLE/BATTLE.PRG/146C.h:655)
struct Field_ManualDisplayState {
    u_short currentManual;
    u_short reserved02;
    u_short scrollPositions[14];
    u_short reserved20;
    char reserved22;
    char reserved23;
};
static_assert(sizeof(Field_ManualDisplayState) == 0x24, "Field_ManualDisplayState must keep its PS1 layout");

extern Field_ManualDisplayState Field_manualDisplayState;
