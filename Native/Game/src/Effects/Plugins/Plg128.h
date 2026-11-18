// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#pragma once
#include "Legacy/Psx.h"

struct Effects_Plg128_CameraEffectState;

#include "Effects/Camera.h"

// decomp: func_800FD9A4_t (src/EFFECT/PLG128.BIN/F8.c:24)
struct Effects_Plg128_CameraEffectState {
    u_char lifetime, effect;
    short mode;
    int age;
    Effects_EffectCamera startCamera;
    Effects_EffectCamera endCamera;
    VECTOR lookAt;
    VECTOR eye;
    MATRIX view;
    int nearClip;
    int reservedCC;
};
static_assert(sizeof(Effects_Plg128_CameraEffectState) == 0xD0, "Effects_Plg128_CameraEffectState must keep its PS1 layout");

extern SVECTOR Effects_Plg128_surfaceVertices[5][16];
extern CVECTOR Effects_Plg128_closedRingColorFactors[8];
extern SVECTOR Effects_Plg128_surfaceVertices2[5][16];
extern int Effects_Plg128_trailEmissionCounter;
