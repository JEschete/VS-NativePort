// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/VectorLive.hpp"
#include "Core/Memory/CopyLive.hpp"
#include "Event/EventCameraLensLive.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Event/Camera.h"
#include "Event/InterpolationCurveLive.h"
#include "Event/Screen.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptVm.h"
#include "Render/FramebufferEffects.h"
#include "State/Timers.h"

// decomp: D_800E9C08 (D_800E9C08@BATTLE/BATTLE.PRG)
CVECTOR Event_kDefaultScreenTint; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C0C (D_800E9C0C@BATTLE/BATTLE.PRG)
Render_FramebufferEffectTranslation Event_kDefaultScreenTransform; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C1C (D_800E9C1C@BATTLE/BATTLE.PRG)
u_char Event_fieldCameraReturnCommands[276]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C24 (D_800E9C24@BATTLE/BATTLE.PRG)
short Event_kOrbitReturnHeightOffsets[134]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C28 (D_800E9C28@BATTLE/BATTLE.PRG)
u_char Event_kOrbitReturnFirstCommand[4]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800E9C2C (D_800E9C2C@BATTLE/BATTLE.PRG)
u_char Event_kOrbitReturnSecondCommand[3]; /* contents: module image (ModuleData.cpp) */

// decomp: D_800F4BA0 (D_800F4BA0@BATTLE/BATTLE.PRG)
char Event_skipNextFieldCameraRestore;

// decomp: D_800F4BA4 (D_800F4BA4@BATTLE/BATTLE.PRG)
Event_CameraState* Event_camera;

// decomp: BATTLE.PRG func_800BB41C (src/BATTLE/BATTLE.PRG/4A0A8.c:2788)
intptr_t Event_CommandWaitCameraTracks(u_char* instruction, short thread_index)
{
    return instruction[1] != (instruction[1] & Event_EventCameraCompletionMask());
}

// decomp: BATTLE.PRG func_800BB450 (src/BATTLE/BATTLE.PRG/4A0A8.c:2793)
intptr_t Event_CommandSetScreenCapture(u_char* instruction, short thread_index)
{
    VS::Render::SetLiveFramebufferEffectEnabled(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG vs_battle_applyCameraState (src/BATTLE/BATTLE.PRG/4A0A8.c:3117)
void Event_ApplyEventCameraState(void)
{
    VECTOR cameraLookAt;
    VECTOR cameraPos;
    SVECTOR* vec;

    if (Event_skipNextFieldCameraRestore != 0) {
        VS::Field::LiveInitializeSphericalCamera(&cameraPos, &cameraLookAt, -1, -1);
        VS::Core::Math::CopyLiveVector(&Event_camera->lookAt.currentPosition, &cameraLookAt);
        VS::Core::Math::CopyLiveVector(&Event_camera->eye.currentPosition, &cameraPos);
    } else {
        VS::Core::Math::CopyLiveVector(&cameraLookAt, &Event_camera->lookAt.currentPosition);
        VS::Core::Math::CopyLiveVector(&cameraPos, &Event_camera->eye.currentPosition);
        if (Event_camera->originActorId != 0x2000) {
            VS::Core::Math::AddLiveVectors(&cameraLookAt, &cameraLookAt, &Event_camera->originOffset);
            VS::Core::Math::AddLiveVectors(&cameraPos, &cameraPos, &Event_camera->originOffset);
        }
    }
    if (*(short*)&Event_camera->shakeOscillatorA.envelopeShape != 0) {
        VS::Core::Math::WidenLiveShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (SVECTOR*)&Event_camera->shakeEyeOffsetX);
        VS::Core::Math::WidenLiveShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (SVECTOR*)&Event_camera->shakeLookAtOffsetX);
        vec = (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x088);
        vec[10].vz = 0;
        vec[10].vx = 0;
        vec[10].vy = VS::Core::Math::FixedAtan2(
            cameraLookAt.vx - cameraPos.vx, cameraLookAt.vz - cameraPos.vz);
        VS::Core::Math::BuildLiveRotationXyz((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
        VS::Core::Math::AddLiveVectors(&cameraLookAt, &cameraLookAt, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        VS::Core::Math::AddLiveVectors(&cameraPos, &cameraPos, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
    }
    VS::Field::LiveSetCameraLookAt(&cameraLookAt);
    VS::Field::LiveSetCameraPosition(&cameraPos);
    if (Event_camera->cameraRoll.hasChanged != 0) {
        VS::Field::LiveSetCameraRoll(Event_camera->cameraRoll.angle);
        Event_camera->cameraRoll.hasChanged = 0;
    }
    if (Event_camera->capturedScreenRoll.hasChanged != 0) {
        VS::Render::SetLiveFramebufferEffectRotation(Event_camera->capturedScreenRoll.angle);
        Event_camera->capturedScreenRoll.hasChanged = 0;
    }
    if (Event_camera->projectionDistance.hasChanged != 0) {
        VS::Render::SetLiveCameraProjectionDistance(Event_camera->projectionDistance.value);
        Event_camera->projectionDistance.hasChanged = 0;
    }
    if (Event_camera->nearClip.hasChanged != 0) {
        VS::Render::SetLiveCameraNearClip(Event_camera->nearClip.value);
        Event_camera->nearClip.hasChanged = 0;
    }
    if (Event_camera->farClip.hasChanged != 0) {
        VS::Render::SetLiveCameraFarClip(Event_camera->farClip.value, Event_isScriptBackdropEnabled);
        Event_camera->farClip.hasChanged = 0;
    }
}

// decomp: BATTLE.PRG func_800BC9E0 (src/BATTLE/BATTLE.PRG/4A0A8.c:3371)
void Event_UpdateEventCameraTracks(void)
{
    Event_camera->eye.partnerPosition.vx = Event_camera->lookAt.currentPosition.vx;
    Event_camera->eye.partnerPosition.vy = Event_camera->lookAt.currentPosition.vy;
    Event_camera->eye.partnerPosition.vz = Event_camera->lookAt.currentPosition.vz;
    Event_camera->lookAt.partnerPosition.vx = Event_camera->eye.currentPosition.vx;
    Event_camera->lookAt.partnerPosition.vy = Event_camera->eye.currentPosition.vy;
    Event_camera->lookAt.partnerPosition.vz = Event_camera->eye.currentPosition.vz;
    VS::Event::UpdateEventCameraTrackEntry(&Event_camera->lookAt, &Event_camera->eye);
    VS::Event::UpdateEventCameraTrackEntry(&Event_camera->eye, &Event_camera->lookAt);
    Event_UpdateEventAngleTween(&Event_camera->cameraRoll);
    Event_UpdateEventAngleTween(&Event_camera->capturedScreenRoll);
    VS::Event::UpdateEventLensTweenEntry(&Event_camera->projectionDistance);
    VS::Event::UpdateEventLensTweenEntry(&Event_camera->nearClip);
    VS::Event::UpdateEventLensTweenEntry(&Event_camera->farClip);
    VS::Event::UpdateEventCameraShakeEntry();
}

// decomp: BATTLE.PRG func_800BD444 (src/BATTLE/BATTLE.PRG/4A0A8.c:3601)
intptr_t Event_CommandTweenCameraRoll(u_char* instruction, short thread_index)
{
    Event_AngleTween* roll_tween;

    switch (instruction[0]) {
    case 0xE2:
        roll_tween = &Event_camera->cameraRoll;
        break;
    case 0xE3:
        roll_tween = &Event_camera->capturedScreenRoll;
        break;
    }

    roll_tween->durationFrames = instruction[4];

    if (roll_tween->durationFrames == 0) {
        roll_tween->angle = VS::Core::Memory::ReadLiveUnalignedShort(instruction + 1);
        roll_tween->hasChanged = 1;
    } else {
        roll_tween->startAngle = roll_tween->angle;
        roll_tween->deltaAngle = VS::Core::Memory::ReadLiveUnalignedShort(instruction + 1) - roll_tween->angle;
        if (roll_tween->deltaAngle < -(ONE / 2)) {
            roll_tween->deltaAngle += ONE;
        } else if (roll_tween->deltaAngle > (ONE / 2)) {
            roll_tween->deltaAngle -= ONE;
        }

        switch (instruction[3] >> 4) {
        case 0:
            if (roll_tween->deltaAngle > 0) {
                roll_tween->deltaAngle -= ONE;
            }
            break;
        case 2:
            if (roll_tween->deltaAngle < 0) {
                roll_tween->deltaAngle += ONE;
            }
            break;
        }

        roll_tween->easingMode = instruction[3] & 0xF;
        roll_tween->elapsedFrames = 0;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800BD57C (src/BATTLE/BATTLE.PRG/4A0A8.c:3647)
void Event_UpdateEventAngleTween(Event_AngleTween* angle_tween)
{
    if (angle_tween->durationFrames > 0) {
        angle_tween->elapsedFrames += State_tickSpeed >> 1;

        angle_tween->angle = (angle_tween->startAngle
                      + ((angle_tween->deltaAngle * ((int (*)(int, int, int))Event_EvaluateInterpolationCurveEntry)(angle_tween->easingMode, angle_tween->elapsedFrames, angle_tween->durationFrames))
                          >> 0xC));

        if (angle_tween->elapsedFrames >= angle_tween->durationFrames) {
            angle_tween->durationFrames = 0;
        }
        angle_tween->hasChanged = 1;
    }
}

// decomp: BATTLE.PRG func_800BD610 (src/BATTLE/BATTLE.PRG/4A0A8.c:3663)
int Event_EventCameraCompletionMask(void)
{
    char completion_mask = Event_camera->lookAt.motionMode == 0xF;

    if (Event_camera->eye.motionMode == 0xF) {
        completion_mask = completion_mask | 2;
    }
    if (Event_camera->cameraRoll.durationFrames == 0) {
        completion_mask |= 4;
    }
    if (Event_camera->projectionDistance.durationFrames == 0) {
        completion_mask |= 8;
    }
    if (Event_camera->nearClip.durationFrames == 0) {
        completion_mask |= 0x10;
    }
    if (Event_camera->farClip.durationFrames == 0) {
        completion_mask |= 0x20;
    }
    if (Event_camera->screenEffectTween.durationFrames == 0) {
        completion_mask |= 0x40;
    }
    if ((Event_camera->shakeOscillatorA.reserved0B + Event_camera->shakeOscillatorB.reserved0B) == 0) {
        completion_mask |= 0x80;
    }
    return completion_mask;
}

// decomp: BATTLE.PRG func_800BD6C4 (src/BATTLE/BATTLE.PRG/4A0A8.c:3691)
intptr_t Event_CommandScreenTransform(u_char* script, short thread_index)
{
    Render_FramebufferEffectTranslation values;
    CVECTOR color;
    short mode;
    short effectValue;
    switch (script[0]) {
    case 185:
        VS::Render::SetLiveFramebufferEffectDepth(VS::Core::Memory::ReadLiveUnalignedShort(script + 1));
        return 0;

    case 186:
        VS::Render::SetLiveFramebufferPreviousFrame(script[1]);
        return 0;

    case 228:
        Event_camera->screenScale.durationFrames = script[4];
        if (Event_camera->screenScale.durationFrames == 0) {
            values.x = (script[1] << 7);
            values.y = (script[2] << 7);
            VS::Render::SetLiveFramebufferEffectScale(&values);
        } else {
            Event_camera->screenScale.elapsedFrames = 0;
            VS::Render::ReadLiveFramebufferEffectScale(&values);
            Event_camera->screenScale.deltaValue0 =
                (script[1] << 7) - (Event_camera->screenScale.startValue0 = values.x);
            Event_camera->screenScale.deltaValue1 =
                (script[2] << 7) - (Event_camera->screenScale.startValue1 = values.y);
            Event_camera->screenScale.easingMode = script[3];
        }
        return 0;

    case 229:
        Event_camera->screenTint.durationFrames = script[5];
        if (Event_camera->screenTint.durationFrames == 0) {
            color.r = script[1];
            color.g = script[2];
            color.b = script[3];
            VS::Render::SetLiveFramebufferEffectTint(&color);
        } else {
            Event_camera->screenTint.elapsedFrames = 0;
            VS::Render::ReadLiveFramebufferEffectTint(&color);
            Event_camera->screenTint.deltaValue0 = script[1] - (Event_camera->screenTint.startValue0 = color.r);
            Event_camera->screenTint.deltaValue1 = script[2] - (Event_camera->screenTint.startValue1 = color.g);
            Event_camera->screenTint.deltaValue2 = script[3] - (Event_camera->screenTint.startValue2 = color.b);
            Event_camera->screenTint.easingMode = script[4];
        }
        return 0;

    case 230:
        Event_camera->screenTranslation.durationFrames = script[4];
        if (Event_camera->screenTranslation.durationFrames == 0) {
            values.x = (signed char)script[1];
            values.y = (signed char)script[2];
            VS::Render::SetLiveFramebufferEffectOffset(&values);
        } else {
            Event_camera->screenTranslation.elapsedFrames = 0;
            VS::Render::ReadLiveFramebufferEffectOffset(&values);
            Event_camera->screenTranslation.deltaValue0 =
                (signed char)script[1] - (Event_camera->screenTranslation.startValue0 = values.x);
            Event_camera->screenTranslation.deltaValue1 =
                (signed char)script[2] - (Event_camera->screenTranslation.startValue1 = values.y);
            Event_camera->screenTranslation.easingMode = script[3];
        }
        return 0;

    case 231:
        VS::Render::SetLiveFramebufferEffectBlendMode(script[1]);
        return 0;

    case 237:
        if (Event_camera->screenEffectTween.durationFrames > 0) {
            return 1;
        }

        Event_camera->screenEffectTween.durationFrames = script[4];
        if (Event_camera->screenEffectTween.durationFrames == 0) {
            Event_camera->screenEffectTween.currentValue0 = effectValue = script[1] << 6;
            Event_camera->screenEffectTween.currentValue1 = (signed char)script[2];
            ((void (*)(int, int))Effects_ScreenDistortion_SetRadialWarp)(effectValue, Event_camera->screenEffectTween.currentValue1);
        } else {
            Event_camera->screenEffectTween.elapsedFrames = 0;
            Event_camera->screenEffectTween.deltaValue0 =
                (script[1] << 6) - (Event_camera->screenEffectTween.startValue0 = Event_camera->screenEffectTween.currentValue0);
            Event_camera->screenEffectTween.deltaValue1 =
                (signed char)script[2]
                - (Event_camera->screenEffectTween.startValue1 = Event_camera->screenEffectTween.currentValue1);
            Event_camera->screenEffectTween.easingMode = script[3];
        }
        return 0;

    case 191:
        mode = VS::Core::Memory::ReadLiveUnalignedShort(script + 1);
        if (!mode) {
            VS::Render::SetLiveFramebufferEffectBlendMode(0);
            values.x = values.y = 0;
            VS::Render::SetLiveFramebufferEffectOffset(&values);
            values.x = values.y = ONE;
            VS::Render::SetLiveFramebufferEffectScale(&values);
            color.r = color.g = color.b = 128;
            VS::Render::SetLiveFramebufferEffectTint(&color);
            VS::Render::SetLiveFramebufferEffectDepth(0);
            VS::Render::SetLiveFramebufferPreviousFrame(0);
            VS::Render::SetLiveFramebufferEffectEnabled(0);
        } else {
            VS::Render::SetLiveFramebufferEffectEnabled(1);
            VS::Render::SetLiveFramebufferEffectBlendMode(0);
            values.x = 0;
            values.y = 2;
            VS::Render::SetLiveFramebufferEffectOffset(&values);
            values.x = values.y = ONE;
            VS::Render::SetLiveFramebufferEffectScale(&values);
            color.r = color.g = color.b = 144;
            VS::Render::SetLiveFramebufferPreviousFrame(1);
            VS::Render::SetLiveFramebufferEffectTint(&color);
            VS::Render::SetLiveFramebufferEffectDepth(mode);
        }
        return 0;
    }
}

// decomp: BATTLE.PRG func_800BDAB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:3812)
void Event_UpdateEventScreenTransforms(void)
{
    Render_FramebufferEffectTranslation screen_transform;
    CVECTOR screen_tint;

    if (Event_UpdateEventTripleTween(&Event_camera->screenScale) != 0) {
        screen_transform.x = Event_camera->screenScale.currentValue0;
        screen_transform.y = Event_camera->screenScale.currentValue1;
        VS::Render::SetLiveFramebufferEffectScale(&screen_transform);
    }
    if (Event_UpdateEventTripleTween(&Event_camera->screenTint) != 0) {
        screen_tint.r = Event_camera->screenTint.currentValue0;
        screen_tint.g = Event_camera->screenTint.currentValue1;
        screen_tint.b = Event_camera->screenTint.currentValue2;
        VS::Render::SetLiveFramebufferEffectTint(&screen_tint);
    }
    if (Event_UpdateEventTripleTween(&Event_camera->screenTranslation) != 0) {
        screen_transform.x = Event_camera->screenTranslation.currentValue0;
        screen_transform.y = Event_camera->screenTranslation.currentValue1;
        VS::Render::SetLiveFramebufferEffectOffset(&screen_transform);
    }
    if (Event_UpdateEventTripleTween(&Event_camera->screenEffectTween) != 0) {
        ((void (*)(int, int))Effects_ScreenDistortion_SetRadialWarp)(Event_camera->screenEffectTween.currentValue0, Event_camera->screenEffectTween.currentValue1);
    }
}

// decomp: BATTLE.PRG func_800BDBB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:3838)
int Event_UpdateEventTripleTween(Event_TripleTween* triple_tween)
{
    Event_EasedFractionScratch* fraction_scratch;
    int eased_fraction;

    if (triple_tween->durationFrames == 0) {
        return 0;
    }

    triple_tween->elapsedFrames = triple_tween->elapsedFrames + (State_tickSpeed >> 1);

    eased_fraction = ((int (*)(int, int, int))Event_EvaluateInterpolationCurveEntry)(triple_tween->easingMode, triple_tween->elapsedFrames, triple_tween->durationFrames);
    fraction_scratch = (Event_EasedFractionScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
    fraction_scratch->easedFraction = eased_fraction;
    triple_tween->currentValue0 = (triple_tween->startValue0 + ((triple_tween->deltaValue0 * fraction_scratch->easedFraction) >> 0xC));
    triple_tween->currentValue1 = (triple_tween->startValue1 + ((triple_tween->deltaValue1 * fraction_scratch->easedFraction) >> 0xC));
    triple_tween->currentValue2 = (triple_tween->startValue2 + ((triple_tween->deltaValue2 * fraction_scratch->easedFraction) >> 0xC));

    if (triple_tween->elapsedFrames == triple_tween->durationFrames) {
        triple_tween->durationFrames = 0;
    }
    return 1;
}

}  // extern "C"
