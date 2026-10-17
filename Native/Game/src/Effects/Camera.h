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

#if !defined(VS_TYPES_ONLY) && !defined(VS_EFFECTS_CAMERA_H_DECLS)
#define VS_EFFECTS_CAMERA_H_DECLS
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"

extern Effects_CameraShakeChannel Effects_cameraShakeChannels[75];
extern short Effects_cameraMotionFrame;
extern Effects_EffectCamera Effects_effectCamera;
extern char Effects_enabledShakeChannels;
extern Effects_EffectCamera Effects_cameraMotionEnds[2];
extern short Effects_cameraMotionDuration;
extern Effects_EffectCamera Effects_savedActionCamera;
extern u_int Effects_cameraTransitionMode;
void Effects_ResetEffectCameraMotion(void);
void Effects_CaptureEffectCamera(Effects_EffectCamera* arg0);
void Effects_ApplyEffectCamera(Effects_EffectCamera* camera);
void Effects_EnableCameraShakeChannels(int arg0);
void Effects_DisableCameraShakeChannels(int arg0);
void Effects_SetEffectCameraTransitionMode(int arg0);
u_char Effects_CameraShakePhaseStep(Effects_CameraShakeChannel* arg0);
u_char Effects_RandomCameraShakePhaseStep(Effects_CameraShakeChannel* arg0);
void Effects_StartCameraShakeChannels(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void Effects_StopCameraShakeChannels(int arg0);
void Effects_UpdateCameraShakeChannels(void);
void Effects_InterpolateEffectCameraLinear(void);
void Effects_InterpolateEffectCameraCosine(void);
void Effects_HoldEffectCameraStartPose(void);
void Effects_UpdateEffectCameraMotion(void);
void Effects_FocusCameraOnEffectTransform(Effects_EffectInstance* arg0, u_char arg1, u_char arg2);
void Effects_RestoreActionEffectCamera(Effects_EffectInstance* arg0, u_char arg1);

#endif // VS_EFFECTS_CAMERA_H_DECLS
