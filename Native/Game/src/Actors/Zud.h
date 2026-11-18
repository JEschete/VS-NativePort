// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_ZUD_H_TYPES
#define VS_ACTORS_ZUD_H_TYPES
#include "Legacy/Psx.h"

struct Actors_ZudHeader;

// decomp: zudHeader (src/BATTLE/BATTLE.PRG/146C.c:275)
struct Actors_ZudHeader {
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
};
static_assert(sizeof(Actors_ZudHeader) == 0x30, "Actors_ZudHeader must keep its PS1 layout");

#endif // VS_ACTORS_ZUD_H_TYPES
