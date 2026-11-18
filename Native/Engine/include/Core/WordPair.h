// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Core_WordPair;

// decomp: func_800ACF54_t (src/BATTLE/BATTLE.PRG/40564.c:2546)
struct Core_WordPair {
    int low;
    int high;
};
static_assert(sizeof(Core_WordPair) == 0x8, "Core_WordPair must keep its PS1 layout");
