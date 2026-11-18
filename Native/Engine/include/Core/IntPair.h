// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Core_IntPair;

// decomp: intPair (src/BATTLE/BATTLE.PRG/44F14.c:88)
struct Core_IntPair {
    int reserved0;
    int reserved4;
};
static_assert(sizeof(Core_IntPair) == 0x8, "Core_IntPair must keep its PS1 layout");
