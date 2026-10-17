// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ZUD_H_TYPES
#define VS_ACTORS_ZUD_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ZudHeader Actors_ZudHeader;

// decomp: zudHeader (src/BATTLE/BATTLE.PRG/146C.c:275)
typedef struct Actors_ZudHeader {
    int characterIds;
    int shieldIds;
    int characterShapeOffset;
    int characterShapeSize;
    int weaponOffset;
    int weaponSize;
    int shieldOffset;
    int shieldSize;
    int commonAnimationOffset;
    int commonAnimationSize;
    int battleAnimationOffset;
    int battleAnimationSize;
} Actors_ZudHeader;
_Static_assert(sizeof(Actors_ZudHeader) == 0x30, "Actors_ZudHeader must keep its PS1 layout");

#endif // VS_ACTORS_ZUD_H_TYPES
