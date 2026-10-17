// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_SHADOW_H_TYPES
#define VS_ACTORS_SHADOW_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ShadowBlend Actors_ShadowBlend;

// decomp: D_800F4538_unk1864 (src/BATTLE/BATTLE.PRG/3A1A0.h:52)
typedef struct Actors_ShadowBlend {
    int reserved0;
    u_char levelFrames;
    u_char radiusFrames;
    u_char reserved6;
    u_char reserved7;
    u_short level;
    short levelDelta;
    u_short radiusExpandRate;
    u_short radiusShrinkRate;
    short radiusExpandDelta;
    short radiusShrinkDelta;
} Actors_ShadowBlend;

#endif // VS_ACTORS_SHADOW_H_TYPES
