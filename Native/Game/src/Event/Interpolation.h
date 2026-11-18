// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Event_InterpolationCurve;

// decomp: D_800F4B28_t (src/BATTLE/BATTLE.PRG/573B8.c:13)
struct Event_InterpolationCurve {
    u_char curveFamily;
    signed char power;
};
static_assert(sizeof(Event_InterpolationCurve) == 0x2, "Event_InterpolationCurve must keep its PS1 layout");
