// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/VectorLive.hpp"
#include "Event/EventCameraLensLive.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/EventScreenTweensLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ScreenOverlayLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Event/Camera.h"
#include "Event/Screen.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptVm.h"
#include "Render/FramebufferEffects.h"

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
    VS::Event::UpdateEventAngleTweenEntry(&Event_camera->cameraRoll);
    VS::Event::UpdateEventAngleTweenEntry(&Event_camera->capturedScreenRoll);
    VS::Event::UpdateEventLensTweenEntry(&Event_camera->projectionDistance);
    VS::Event::UpdateEventLensTweenEntry(&Event_camera->nearClip);
    VS::Event::UpdateEventLensTweenEntry(&Event_camera->farClip);
    VS::Event::UpdateEventCameraShakeEntry();
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

}  // extern "C"
