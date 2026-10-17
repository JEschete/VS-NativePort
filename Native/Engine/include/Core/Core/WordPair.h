// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_CORE_CORE_WORDPAIR_H_TYPES
#define VS_CORE_CORE_WORDPAIR_H_TYPES
#include "Legacy/Psx.h"

typedef struct Core_WordPair Core_WordPair;

// decomp: func_800ACF54_t (src/BATTLE/BATTLE.PRG/40564.c:2546)
typedef struct Core_WordPair {
    int low;
    int high;
} Core_WordPair;
_Static_assert(sizeof(Core_WordPair) == 0x8, "Core_WordPair must keep its PS1 layout");

#endif // VS_CORE_CORE_WORDPAIR_H_TYPES
