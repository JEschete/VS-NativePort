// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Interpolation.h"
#include "Effects/Camera.h"
#include "Effects/Curves.h"
#include "Effects/Particles.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Field/Camera.h"
#include "Render/Camera.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: D_800F54B8 (D_800F54B8@BATTLE/BATTLE.PRG)
Effects_CameraShakeChannel Effects_cameraShakeChannels[75];

// decomp: D_800F54D0 (D_800F54D0@BATTLE/BATTLE.PRG)
short Effects_cameraMotionFrame;

// decomp: D_800F54D8 (D_800F54D8@BATTLE/BATTLE.PRG)
Effects_EffectCamera Effects_effectCamera;

// decomp: D_800F5518 (D_800F5518@BATTLE/BATTLE.PRG)
char Effects_enabledShakeChannels;

// decomp: D_800F5520 (D_800F5520@BATTLE/BATTLE.PRG)
Effects_EffectCamera Effects_cameraMotionEnds[2];

// decomp: D_800F55A0 (D_800F55A0@BATTLE/BATTLE.PRG)
short Effects_cameraMotionDuration;

// decomp: D_800F55A8 (D_800F55A8@BATTLE/BATTLE.PRG)
Effects_EffectCamera Effects_savedActionCamera;

// decomp: D_800F55E8 (D_800F55E8@BATTLE/BATTLE.PRG)
u_int Effects_cameraTransitionMode;

// decomp: BATTLE.PRG func_800D1930 (src/BATTLE/BATTLE.PRG/5BF94.c:6474)
void Effects_ResetEffectCameraMotion(void)
{
    int i;
    Effects_enabledShakeChannels = 0;
    Effects_cameraTransitionMode = 0;
    Effects_cameraMotionFrame = 0;
    Effects_cameraMotionDuration = 0;

    for (i = 0; i < 4; ++i) {
        Effects_cameraShakeChannels[i].phase = 0xFF;
    }
}

// decomp: BATTLE.PRG func_800D1B18 (src/BATTLE/BATTLE.PRG/5BF94.c:6516)
void Effects_CaptureEffectCamera(Effects_EffectCamera* arg0)
{
    Field_CameraPosition(&arg0->position);
    Field_CameraLookAt(&arg0->lookAt);
    arg0->roll = Field_CameraRoll() + ONE * 8;
    arg0->nearClip = Render_nearClip;
    arg0->projectionDistance = Render_projectionDistance;
    arg0->farClip = Render_frameScratch.camera.farClip;
}

// decomp: BATTLE.PRG func_800D1B80 (src/BATTLE/BATTLE.PRG/5BF94.c:6526)
void Effects_ApplyEffectCamera(Effects_EffectCamera* camera)
{
    VECTOR offset, position;
    MATRIX unusedTransform;
    SVECTOR unusedRotation;
    int dx, dz, yaw, pitch, horizontal;
    dx = (camera->position.vx - camera->lookAt.vx) >> 12;
    dz = (camera->position.vz - camera->lookAt.vz) >> 12;
    yaw = Core_Gte_Atan2(dx, dz);
    offset.vx = Core_Gte_Cos(yaw) * (camera->shakeLateral >> 12);
    offset.vz = -Core_Gte_Sin(yaw) * (camera->shakeLateral >> 12);
    pitch = Core_Gte_Atan2(
        (camera->position.vy - camera->lookAt.vy) >> 12, Core_Math_SquareRootInteger(dx * dx + dz * dz));
    offset.vy = -Core_Gte_Sin(pitch) * (camera->shakeDepth >> 12);
    horizontal = -Core_Gte_Cos(pitch) * (camera->shakeDepth >> 12);
    offset.vy += Core_Gte_Cos(pitch) * (camera->shakeVertical >> 12);
    horizontal += -Core_Gte_Sin(pitch) * (camera->shakeVertical >> 12);
    offset.vx += Core_Gte_Sin(yaw) * (horizontal >>= 12);
    offset.vz += Core_Gte_Cos(yaw) * horizontal;
    position.vx = camera->position.vx + offset.vx;
    position.vy = camera->position.vy + offset.vy;
    position.vz = camera->position.vz + offset.vz;
    Field_SetCameraPosition(&position);
    position.vx = camera->lookAt.vx + offset.vx;
    position.vy = camera->lookAt.vy + offset.vy;
    position.vz = camera->lookAt.vz + offset.vz;
    Field_SetCameraLookAt(&position);
    {
        int rollOffset = camera->shakeRoll - 0x8000;
        Field_SetCameraRoll(camera->roll + rollOffset);
    }
    Render_SetCameraNearClip(camera->nearClip);
    Render_SetCameraProjectionDistance(camera->projectionDistance);
    /* This original call site does not initialize the setter's second argument. */
    ((void (*)())Render_SetCameraFarClip)(camera->farClip);
    Render_BuildCameraViewMatrix();
}

// decomp: BATTLE.PRG func_800D1DE4 (src/BATTLE/BATTLE.PRG/5BF94.c:6564)
void Effects_EnableCameraShakeChannels(int arg0) { Effects_enabledShakeChannels |= arg0; }

// decomp: BATTLE.PRG func_800D1DFC (src/BATTLE/BATTLE.PRG/5BF94.c:6566)
void Effects_DisableCameraShakeChannels(int arg0)
{
    if (arg0 != 0) {
        Effects_enabledShakeChannels &= ~arg0;
        return;
    }
    Effects_enabledShakeChannels = 0;
}

// decomp: BATTLE.PRG func_800D1E20 (src/BATTLE/BATTLE.PRG/5BF94.c:6575)
void Effects_SetEffectCameraTransitionMode(int arg0) { Effects_cameraTransitionMode = arg0; }

// decomp: BATTLE.PRG func_800D1E2C (src/BATTLE/BATTLE.PRG/5BF94.c:6577)
u_char Effects_CameraShakePhaseStep(Effects_CameraShakeChannel* arg0)
{
    int var_v0;

    if (arg0->curveStep) {
        var_v0 = Effects_SampleEffectCurve(arg0->stepCurve, arg0->phase);
    } else {
        var_v0 = arg0->stepCurve;
    }
    return var_v0;
}

// decomp: BATTLE.PRG func_800D1E78 (src/BATTLE/BATTLE.PRG/5BF94.c:6589)
u_char Effects_RandomCameraShakePhaseStep(Effects_CameraShakeChannel* arg0)
{
    int var_s0;

    if (arg0->randomStep) {
        var_s0 = Effects_SampleEffectCurve(arg0->randomStepCurve, arg0->phase);
    } else {
        var_s0 = arg0->randomStepCurve;
    }

    if (var_s0 == 0) {
        return 0;
    }

    return (Core_Random_Rand() % var_s0);
}

// decomp: BATTLE.PRG func_800D1EF0 (src/BATTLE/BATTLE.PRG/5BF94.c:6606)
void Effects_StartCameraShakeChannels(int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if ((arg0 >> i) & 1) {
            Effects_CameraShakeChannel* p = &Effects_cameraShakeChannels[i];
            p->shapeCurve = arg1;
            p->amplitude = arg2;
            p->curveStep = arg3;
            p->stepCurve = arg4;
            p->randomStep = arg5;
            p->randomStepCurve = arg6;
            p->phase = 0;
            p->angle = Effects_RandomCameraShakePhaseStep(p);
            Effects_EnableCameraShakeChannels(1 << i);
        }
    }
}

// decomp: BATTLE.PRG func_800D1FEC (src/BATTLE/BATTLE.PRG/5BF94.c:6626)
void Effects_StopCameraShakeChannels(int arg0)
{
    int i;

    for (i = 0; i < 4; ++i) {
        if ((arg0 >> i) & 1) {
            Effects_cameraShakeChannels[i].phase = 0xFF;
            Effects_DisableCameraShakeChannels(1 << i);
        }
    }
}

// decomp: BATTLE.PRG func_800D206C (src/BATTLE/BATTLE.PRG/5BF94.c:6638)
void Effects_UpdateCameraShakeChannels(void)
{
    int i;

    for (i = 0; i < 4; ++i) {
        int v;

        if (!((Effects_enabledShakeChannels >> i) & 1)) {
            continue;
        }

        v = Effects_SampleSegmentedEffectCurve(Effects_cameraShakeChannels[i].shapeCurve, Effects_cameraShakeChannels[i].phase);

        if (v >= 0) {
            v = ((Core_Gte_Sin(Effects_cameraShakeChannels[i].angle * 32) * v) * Effects_cameraShakeChannels[i].amplitude) >> 8;

            switch (i) {
            case 0:
                Effects_effectCamera.shakeLateral = v;
                break;

            case 1:
                Effects_effectCamera.shakeVertical = v;
                break;

            case 2:
                Effects_effectCamera.shakeDepth = v;
                break;

            case 3:
                Effects_effectCamera.shakeRoll = v >> 12;
                break;
            }

            Effects_cameraShakeChannels[i].angle +=
                Effects_CameraShakePhaseStep(&Effects_cameraShakeChannels[i]) + Effects_RandomCameraShakePhaseStep(&Effects_cameraShakeChannels[i]);
            ++Effects_cameraShakeChannels[i].phase;
        } else {
            Effects_StopCameraShakeChannels(1 << i);
        }
    }
}

// decomp: BATTLE.PRG func_800D21C0 (src/BATTLE/BATTLE.PRG/5BF94.c:6681)
void Effects_InterpolateEffectCameraLinear(void)
{
    VECTOR start;
    VECTOR end;
    VECTOR result;
    setVector(&start, Effects_cameraMotionEnds[0].position.vx >> 12, Effects_cameraMotionEnds[0].position.vy >> 12,
        Effects_cameraMotionEnds[0].position.vz >> 12);
    setVector(&end, Effects_cameraMotionEnds[1].position.vx >> 12, Effects_cameraMotionEnds[1].position.vy >> 12,
        Effects_cameraMotionEnds[1].position.vz >> 12);
    Core_Math_InterpolateTimedWideVector(&start, &end, Effects_cameraMotionDuration, Effects_cameraMotionFrame, &result);
    setVector(&Effects_effectCamera.position, result.vx << 12, result.vy << 12, result.vz << 12);
    setVector(&start, Effects_cameraMotionEnds[0].lookAt.vx >> 12, Effects_cameraMotionEnds[0].lookAt.vy >> 12,
        Effects_cameraMotionEnds[0].lookAt.vz >> 12);
    setVector(&end, Effects_cameraMotionEnds[1].lookAt.vx >> 12, Effects_cameraMotionEnds[1].lookAt.vy >> 12,
        Effects_cameraMotionEnds[1].lookAt.vz >> 12);
    Core_Math_InterpolateTimedWideVector(&start, &end, Effects_cameraMotionDuration, Effects_cameraMotionFrame, &result);
    Effects_effectCamera.lookAt.vx = result.vx << 12;
    Effects_effectCamera.lookAt.vy = result.vy << 12;
    Effects_effectCamera.lookAt.vz = result.vz << 12;
    Effects_effectCamera.roll =
        Core_Math_InterpolateTimedScalar(Effects_cameraMotionEnds[0].roll, Effects_cameraMotionEnds[1].roll, Effects_cameraMotionDuration, Effects_cameraMotionFrame);
    Effects_effectCamera.farClip = Core_Math_InterpolateTimedScalar(
        Effects_cameraMotionEnds[0].farClip, Effects_cameraMotionEnds[1].farClip, Effects_cameraMotionDuration, Effects_cameraMotionFrame);
    Effects_effectCamera.nearClip = Core_Math_InterpolateTimedScalar(
        Effects_cameraMotionEnds[0].nearClip, Effects_cameraMotionEnds[1].nearClip, Effects_cameraMotionDuration, Effects_cameraMotionFrame);
}

// decomp: BATTLE.PRG func_800D236C (src/BATTLE/BATTLE.PRG/5BF94.c:6708)
void Effects_InterpolateEffectCameraCosine(void)
{
    VECTOR start;
    VECTOR end;
    VECTOR result;
    setVector(&start, Effects_cameraMotionEnds[0].position.vx >> 12, Effects_cameraMotionEnds[0].position.vy >> 12,
        Effects_cameraMotionEnds[0].position.vz >> 12);
    setVector(&end, Effects_cameraMotionEnds[1].position.vx >> 12, Effects_cameraMotionEnds[1].position.vy >> 12,
        Effects_cameraMotionEnds[1].position.vz >> 12);
    Core_Math_InterpolateCosineWideVector(&start, &end, Effects_cameraMotionDuration, Effects_cameraMotionFrame, &result);
    setVector(&Effects_effectCamera.position, result.vx << 12, result.vy << 12, result.vz << 12);
    setVector(&start, Effects_cameraMotionEnds[0].lookAt.vx >> 12, Effects_cameraMotionEnds[0].lookAt.vy >> 12,
        Effects_cameraMotionEnds[0].lookAt.vz >> 12);
    setVector(&end, Effects_cameraMotionEnds[1].lookAt.vx >> 12, Effects_cameraMotionEnds[1].lookAt.vy >> 12,
        Effects_cameraMotionEnds[1].lookAt.vz >> 12);
    Core_Math_InterpolateCosineWideVector(&start, &end, Effects_cameraMotionDuration, Effects_cameraMotionFrame, &result);
    Effects_effectCamera.lookAt.vx = result.vx << 12;
    Effects_effectCamera.lookAt.vy = result.vy << 12;
    Effects_effectCamera.lookAt.vz = result.vz << 12;
    Effects_effectCamera.roll =
        Core_Math_InterpolateCosineTimedScalar(Effects_cameraMotionEnds[0].roll, Effects_cameraMotionEnds[1].roll, Effects_cameraMotionDuration, Effects_cameraMotionFrame);
    Effects_effectCamera.farClip = Core_Math_InterpolateCosineTimedScalar(
        Effects_cameraMotionEnds[0].farClip, Effects_cameraMotionEnds[1].farClip, Effects_cameraMotionDuration, Effects_cameraMotionFrame);
    Effects_effectCamera.nearClip = Core_Math_InterpolateCosineTimedScalar(
        Effects_cameraMotionEnds[0].nearClip, Effects_cameraMotionEnds[1].nearClip, Effects_cameraMotionDuration, Effects_cameraMotionFrame);
}

// decomp: BATTLE.PRG func_800D2518 (src/BATTLE/BATTLE.PRG/5BF94.c:6735)
void Effects_HoldEffectCameraStartPose(void) { Effects_effectCamera = Effects_cameraMotionEnds[0]; }

// decomp: BATTLE.PRG func_800D2560 (src/BATTLE/BATTLE.PRG/5BF94.c:6737)
void Effects_UpdateEffectCameraMotion(void)
{
    int temp_s0 = Effects_cameraTransitionMode != 0;

    if (Effects_cameraMotionDuration != 0) {
        switch (Effects_cameraTransitionMode) {
        case 0:
            break;
        case 1:
            Effects_InterpolateEffectCameraLinear();
            break;
        case 2:
            Effects_InterpolateEffectCameraCosine();
            break;
        case 3:
            Effects_HoldEffectCameraStartPose();
            break;
        }
        if (Effects_cameraMotionFrame <= Effects_cameraMotionDuration) {
            ++Effects_cameraMotionFrame;
            if (Effects_cameraMotionFrame > Effects_cameraMotionDuration) {
                Effects_cameraMotionFrame = 0;
                Effects_cameraMotionDuration = 0;
                Effects_SetEffectCameraTransitionMode(0);
            }
        }
    }
    if (Effects_enabledShakeChannels != 0) {
        Effects_UpdateCameraShakeChannels();
    } else {
        Effects_effectCamera.shakeRoll = 0;
        Effects_effectCamera.shakeDepth = 0;
        Effects_effectCamera.shakeVertical = 0;
        Effects_effectCamera.shakeLateral = 0;
    }
    if (Effects_enabledShakeChannels != 0 || temp_s0) {
        Effects_ApplyEffectCamera(&Effects_effectCamera);
    }
}

// decomp: BATTLE.PRG func_800D66FC (src/BATTLE/BATTLE.PRG/5BF94.c:8308)
void Effects_FocusCameraOnEffectTransform(Effects_EffectInstance* arg0, u_char arg1, u_char arg2)
{
    Effects_CaptureEffectCamera(Effects_cameraMotionEnds);
    Effects_CaptureEffectCamera(&Effects_cameraMotionEnds[1]);
    Effects_cameraMotionEnds[1].lookAt.vx = arg0->transforms[arg1].world.t[0] * ONE;
    Effects_cameraMotionEnds[1].lookAt.vy = arg0->transforms[arg1].world.t[1] * ONE;
    Effects_cameraMotionEnds[1].lookAt.vz = arg0->transforms[arg1].world.t[2] * ONE;
    Effects_cameraMotionFrame = 0;
    if (arg2 == 0) {
        arg2 = 1;
    }
    Effects_cameraMotionDuration = arg2;
    Effects_SetEffectCameraTransitionMode(2);
}

// decomp: BATTLE.PRG func_800D67C4 (src/BATTLE/BATTLE.PRG/5BF94.c:8323)
void Effects_RestoreActionEffectCamera(Effects_EffectInstance* arg0, u_char arg1)
{
    Effects_CaptureEffectCamera(Effects_cameraMotionEnds);
    Effects_cameraMotionEnds[1] = Effects_savedActionCamera;
    Effects_cameraMotionFrame = 0;
    if (arg1 == 0) {
        arg1 = 1;
    }
    Effects_cameraMotionDuration = arg1;
    Effects_SetEffectCameraTransitionMode(2);
}

