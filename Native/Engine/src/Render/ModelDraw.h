// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_MODELDRAW_H_TYPES
#define VS_RENDER_MODELDRAW_H_TYPES
#include "Legacy/Psx.h"

struct Actors_ShpModel;
struct Render_DrawDepthScratch;
struct Render_ModelScratchpad;
struct Render_ModelScratchpadAnglesView;
struct Render_ModelScratchpadBoneView;
struct Render_ModelScratchpadCameraView;

// decomp: D_1F80035C_t (src/BATTLE/BATTLE.PRG/30DB0.h:43)
struct Render_ModelScratchpad {
    u_char reserved0[0x34];
    u_short texturePage;
    u_short palette;
    u_short reserved38;
    u_short angleX;
    u_short angleY;
    u_char reserved3E[0xE];
    SVECTOR position;
    SVECTOR rootOffset;
    MATRIX* parentBoneMatrix;
    int reserved60;
    MATRIX matrix;
    SVECTOR rotation;
    VECTOR scale;
    Actors_ShpModel* model;
};

// decomp: actorTransformScratch (src/BATTLE/BATTLE.PRG/44F14.c:75)
struct Render_ModelScratchpadAnglesView {
    char reserved0[0x3A];
    u_short angleX, angleY;
    char reserved3E[0x16];
    SVECTOR rootOffset;
    char reserved5C[8];
    MATRIX matrix;
    SVECTOR rotation;
};
static_assert(sizeof(Render_ModelScratchpadAnglesView) == 0x8C, "Render_ModelScratchpadAnglesView must keep its PS1 layout");

// decomp: cameraTransformScratch (src/BATTLE/BATTLE.PRG/44F14.c:82)
struct Render_ModelScratchpadCameraView {
    char reserved0[0x64];
    MATRIX reserved64;
    char reserved84[8];
    VECTOR reserved8C;
};
static_assert(sizeof(Render_ModelScratchpadCameraView) == 0x9C, "Render_ModelScratchpadCameraView must keep its PS1 layout");

// decomp: frameScratch (src/BATTLE/BATTLE.PRG/44F14.c:137)
struct Render_DrawDepthScratch {
    int reserved0;
    int depth;
};
struct Render_DrawDepthScratch;
static_assert(sizeof(Render_DrawDepthScratch) == 0x8, "Render_DrawDepthScratch must keep its PS1 layout");

// decomp: func_800A1DE8_t (src/BATTLE/BATTLE.PRG/38C1C.c:16)
struct Render_ModelScratchpadBoneView {
    u_char reserved0[0x4C];
    SVECTOR position;
    SVECTOR rootOffset;
    u_char reserved5C[8];
    MATRIX matrix;
};
static_assert(sizeof(Render_ModelScratchpadBoneView) == 0x84, "Render_ModelScratchpadBoneView must keep its PS1 layout");

#endif // VS_RENDER_MODELDRAW_H_TYPES
