// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_ACTORS_MODELDRAW_H_TYPES
#define VS_ACTORS_MODELDRAW_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ModelRenderContext Actors_ModelRenderContext;
typedef struct Actors_SkeletonPose Actors_SkeletonPose;

// decomp: D_800F2458_t (src/BATTLE/BATTLE.PRG/30DB0.h:25)
typedef struct Actors_ModelRenderContext {
    u_char workArea[0x1FD0];
    int modelScale[3];
    int reserved1FDC;
    u_short actorScale;
} Actors_ModelRenderContext;
_Static_assert(sizeof(Actors_ModelRenderContext) == 0x1FE4, "Actors_ModelRenderContext must keep its PS1 layout");

// decomp: D_800F3C58_t (src/BATTLE/BATTLE.PRG/44F14.c:23)
typedef struct Actors_SkeletonPose {
    MATRIX boneMatrices[63];
    VECTOR scale;
} Actors_SkeletonPose;
_Static_assert(sizeof(Actors_SkeletonPose) == 0x7F0, "Actors_SkeletonPose must keep its PS1 layout");

#endif // VS_ACTORS_MODELDRAW_H_TYPES
