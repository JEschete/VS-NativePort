// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_RENDER_MODELDRAW_H_TYPES
#define VS_RENDER_MODELDRAW_H_TYPES
#include "Legacy/Psx.h"

typedef struct Actors_ShpModel Actors_ShpModel;
typedef struct Render_DrawDepthScratch Render_DrawDepthScratch;
typedef struct Render_ModelScratchpad Render_ModelScratchpad;
typedef struct Render_ModelScratchpadAnglesView Render_ModelScratchpadAnglesView;
typedef struct Render_ModelScratchpadBoneView Render_ModelScratchpadBoneView;
typedef struct Render_ModelScratchpadCameraView Render_ModelScratchpadCameraView;

// decomp: D_1F80035C_t (src/BATTLE/BATTLE.PRG/30DB0.h:43)
typedef struct Render_ModelScratchpad {
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
} Render_ModelScratchpad;

// decomp: actorTransformScratch (src/BATTLE/BATTLE.PRG/44F14.c:75)
typedef struct Render_ModelScratchpadAnglesView {
    char reserved0[0x3A];
    u_short angleX, angleY;
    char reserved3E[0x16];
    SVECTOR rootOffset;
    char reserved5C[8];
    MATRIX matrix;
    SVECTOR rotation;
} Render_ModelScratchpadAnglesView;

// decomp: cameraTransformScratch (src/BATTLE/BATTLE.PRG/44F14.c:82)
typedef struct Render_ModelScratchpadCameraView {
    char reserved0[0x64];
    MATRIX reserved64;
    char reserved84[8];
    VECTOR reserved8C;
} Render_ModelScratchpadCameraView;

// decomp: frameScratch (src/BATTLE/BATTLE.PRG/44F14.c:137)
struct Render_DrawDepthScratch {
    int reserved0;
    int depth;
};
typedef struct Render_DrawDepthScratch Render_DrawDepthScratch;

// decomp: func_800A1DE8_t (src/BATTLE/BATTLE.PRG/38C1C.c:16)
typedef struct Render_ModelScratchpadBoneView {
    u_char reserved0[0x4C];
    SVECTOR position;
    SVECTOR rootOffset;
    u_char reserved5C[8];
    MATRIX matrix;
} Render_ModelScratchpadBoneView;

#endif // VS_RENDER_MODELDRAW_H_TYPES
