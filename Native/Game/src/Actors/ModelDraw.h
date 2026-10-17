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

// decomp: D_800F3C58_t (src/BATTLE/BATTLE.PRG/44F14.c:23)
typedef struct Actors_SkeletonPose {
    MATRIX boneMatrices[63];
    VECTOR scale;
} Actors_SkeletonPose;

#endif // VS_ACTORS_MODELDRAW_H_TYPES
