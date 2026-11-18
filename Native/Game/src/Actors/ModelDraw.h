// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Actors_ModelRenderContext;
struct Actors_SkeletonPose;

// decomp: D_800F2458_t (src/BATTLE/BATTLE.PRG/30DB0.h:25)
struct Actors_ModelRenderContext {
    int projectedVertices[768];
    int vertexDepths[768];
    unsigned projectedVertexCount;
    u_char reserved1804[0x7CC];
    int modelScale[3];
    int reserved1FDC;
    u_short actorScale;
};
static_assert(sizeof(Actors_ModelRenderContext) == 0x1FE4, "Actors_ModelRenderContext must keep its PS1 layout");

// decomp: D_800F3C58_t (src/BATTLE/BATTLE.PRG/44F14.c:23)
struct Actors_SkeletonPose {
    MATRIX boneMatrices[63];
    VECTOR scale;
};
static_assert(sizeof(Actors_SkeletonPose) == 0x7F0, "Actors_SkeletonPose must keep its PS1 layout");
