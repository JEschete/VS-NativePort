// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_EFFECTS_CAMERA_H_TYPES
#define VS_EFFECTS_CAMERA_H_TYPES
#include "Legacy/Psx.h"

typedef struct Effects_CameraShakeChannel Effects_CameraShakeChannel;
typedef struct Effects_EffectCamera Effects_EffectCamera;

// decomp: D_800F54B8_t (src/BATTLE/BATTLE.PRG/5BF94.c:243)
typedef struct Effects_CameraShakeChannel {
    u_char phase;
    u_char shapeCurve : 5;
    u_char curveStep : 1;
    u_char randomStep : 1;
    u_char reserved1Bit7 : 1;
    u_char stepCurve;
    u_char randomStepCurve;
    u_char amplitude;
    u_char angle;
} Effects_CameraShakeChannel;
_Static_assert(sizeof(Effects_CameraShakeChannel) == 0x6, "Effects_CameraShakeChannel must keep its PS1 layout");

// decomp: D_800F54D8_t (src/BATTLE/BATTLE.PRG/5BF94.h:386)
typedef struct Effects_EffectCamera {
    VECTOR position;
    VECTOR lookAt;
    int shakeLateral;
    int shakeVertical;
    int shakeDepth;
    int shakeRoll;
    int roll;
    int nearClip;
    int projectionDistance;
    int farClip;
} Effects_EffectCamera;
_Static_assert(sizeof(Effects_EffectCamera) == 0x40, "Effects_EffectCamera must keep its PS1 layout");

#endif // VS_EFFECTS_CAMERA_H_TYPES
