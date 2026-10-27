// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Actors/Location.h"
#include "Actors/Skeleton.h"
#include "Core/Diagnostics.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Trig.h"
#include "Core/Math/Vector.h"
#include "Core/Memory/Copy.h"
#include "Core/Memory/Heap.h"
#include "Effects/ScreenDistortion/ScreenDistortion.h"
#include "Event/Camera.h"
#include "Event/Screen.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptVm.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Render/Camera.h"
#include "Render/FramebufferEffects.h"
#include "State/Flags.h"
#include "State/Timers.h"

// decomp: D_800E9C08 (D_800E9C08@BATTLE/BATTLE.PRG)
CVECTOR Event_kDefaultScreenTint; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C0C (D_800E9C0C@BATTLE/BATTLE.PRG)
Render_FramebufferEffectTranslation Event_kDefaultScreenTransform; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C1C (D_800E9C1C@BATTLE/BATTLE.PRG)
u_char Event_fieldCameraReturnCommands[276]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C24 (D_800E9C24@BATTLE/BATTLE.PRG)
short Event_kOrbitReturnHeightOffsets[134]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C28 (D_800E9C28@BATTLE/BATTLE.PRG)
u_char Event_kOrbitReturnFirstCommand[4]; /* contents: module image (ModuleData.c) */

// decomp: D_800E9C2C (D_800E9C2C@BATTLE/BATTLE.PRG)
u_char Event_kOrbitReturnSecondCommand[3]; /* contents: module image (ModuleData.c) */

// decomp: D_800F4BA0 (D_800F4BA0@BATTLE/BATTLE.PRG)
char Event_skipNextFieldCameraRestore;

// decomp: D_800F4BA4 (D_800F4BA4@BATTLE/BATTLE.PRG)
Event_CameraState* Event_camera;

// decomp: BATTLE.PRG func_800BB33C (src/BATTLE/BATTLE.PRG/4A0A8.c:2761)
intptr_t Event_CommandStopCameraMotion(u_char* instruction, short thread_index)
{
    Event_StopEventCameraMotion();
    return 0;
}

// decomp: BATTLE.PRG func_800BB35C (src/BATTLE/BATTLE.PRG/4A0A8.c:2767)
intptr_t Event_CommandSetCameraFollowing(u_char* instruction, short thread_index)
{
    int eye_follow_bit = instruction[1] & 2;
    Event_SetEventCameraFollowing(instruction[1] & 1, eye_follow_bit != 0);
    return 0;
}

// decomp: BATTLE.PRG func_800BB38C (src/BATTLE/BATTLE.PRG/4A0A8.c:2774)
intptr_t Event_CommandSetCameraOriginActor(u_char* instruction, short thread_index)
{
    Event_SetEventCameraOriginActor(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(instruction + 1)));
    return 0;
}

// decomp: BATTLE.PRG func_800BB3BC (src/BATTLE/BATTLE.PRG/4A0A8.c:2780)
intptr_t Event_CommandRestoreFieldCamera(u_char* instruction, short thread_index)
{
    Event_RestoreEventFieldCamera(
        instruction[1] != 0xFF ? ((instruction[1] + 4) & 7) : -1, instruction[2] != 0 ? instruction[2] : -1);
    Event_isFieldCameraRestoreScripted = 1;
    return 0;
}

// decomp: BATTLE.PRG func_800BB41C (src/BATTLE/BATTLE.PRG/4A0A8.c:2788)
intptr_t Event_CommandWaitCameraTracks(u_char* instruction, short thread_index)
{
    return instruction[1] != (instruction[1] & Event_EventCameraCompletionMask());
}

// decomp: BATTLE.PRG func_800BB450 (src/BATTLE/BATTLE.PRG/4A0A8.c:2793)
intptr_t Event_CommandSetScreenCapture(u_char* instruction, short thread_index)
{
    Render_SetFramebufferEffectEnabled(instruction[1]);
    return 0;
}

// decomp: BATTLE.PRG func_800BBBE8 (src/BATTLE/BATTLE.PRG/4A0A8.c:3014)
void Event_InitializeEventCamera(void)
{
    VECTOR _ __attribute__((unused));
    VECTOR field_eye_position;
    VECTOR field_look_at;

    Event_camera = Core_Memory_AllocateFromHighEnd(sizeof *Event_camera);
    Event_skipNextFieldCameraRestore = 0;

    Field_CameraPosition(&field_eye_position);
    Field_CameraLookAt(&field_look_at);
    Core_Math_CopyVector(&Event_camera->eye.currentPosition, &field_eye_position);
    Core_Math_CopyVector(&Event_camera->lookAt.partnerPosition, &field_eye_position);
    Core_Math_CopyVector(&Event_camera->lookAt.currentPosition, &field_look_at);
    Core_Math_CopyVector(&Event_camera->eye.partnerPosition, &field_look_at);

    Event_camera->lookAt.motionMode = 0xF;
    Event_camera->lookAt.followRequest = Event_camera->lookAt.isFollowing = 0;
    Event_camera->eye.motionMode = 0xF;
    Event_camera->eye.followRequest = Event_camera->eye.isFollowing = 0;
    Event_camera->cameraRoll.durationFrames = 0;
    Event_camera->cameraRoll.angle = 0;
    Event_camera->cameraRoll.hasChanged = 1;

    Render_SetFramebufferEffectEnabled(0);

    Event_camera->capturedScreenRoll.durationFrames = 0;
    Event_camera->capturedScreenRoll.angle = 0;

    Render_SetFramebufferEffectRotation(0);

    Event_camera->capturedScreenRoll.hasChanged = 1;
    Event_camera->screenScale.currentValue1 = ONE;
    Event_camera->screenScale.currentValue0 = ONE;

    Render_SetFramebufferEffectScale(&Event_kDefaultScreenTransform);

    Event_camera->screenTint.currentValue0 = Event_camera->screenTint.currentValue1 = Event_camera->screenTint.currentValue2 =
        Event_camera->screenScale.durationFrames = 0;

    Render_SetFramebufferEffectTint(&Event_kDefaultScreenTint);

    Event_camera->screenTint.durationFrames = 0;
    Event_camera->screenTranslation.currentValue1 = ONE;
    Event_camera->screenTranslation.currentValue0 = ONE;

    Render_SetFramebufferEffectOffset(&Event_kDefaultScreenTransform);

    Event_camera->screenTranslation.durationFrames = 0;

    Render_SetFramebufferEffectBlendMode(1);

    Event_camera->screenEffectTween.currentValue0 = ONE;
    Event_camera->screenEffectTween.durationFrames = Event_camera->screenEffectTween.currentValue1 = 0;
    Event_camera->projectionDistance.value = 0x200;
    Event_camera->projectionDistance.durationFrames = 0;
    Event_camera->projectionDistance.hasChanged = 1;
    Event_camera->nearClip.value = 0x40;
    Event_camera->nearClip.durationFrames = 0;
    Event_camera->nearClip.hasChanged = 1;
    Event_camera->farClip.value = 0x1000;
    Event_camera->farClip.durationFrames = 0;
    Event_camera->farClip.hasChanged = 1;
    Event_camera->shakeOscillatorA.envelopeShape.count = 0;
    Event_camera->shakeOscillatorA.reserved0B = Event_camera->shakeOscillatorB.reserved0B = 0;
    Event_camera->originActorId = 0x2000;
}

// decomp: BATTLE.PRG func_800BBDDC (src/BATTLE/BATTLE.PRG/4A0A8.c:3082)
void Event_FreeEventCamera(void) { Core_Memory_FreeFromHighEnd(Event_camera); }

// decomp: BATTLE.PRG func_800BBE04 (src/BATTLE/BATTLE.PRG/4A0A8.c:3084)
void Event_SetEventFieldCameraOverride(int field_camera_override) { Event_skipNextFieldCameraRestore = field_camera_override; }

// decomp: BATTLE.PRG func_800BBE10 (src/BATTLE/BATTLE.PRG/4A0A8.c:3086)
void Event_SetEventCameraOriginActor(u_short origin_actor)
{
    int _[4];
    if (origin_actor == 0x2000) {
        if (Event_camera->originActorId != origin_actor) {
            Core_Math_AddVectors(
                &Event_camera->eye.currentPosition, &Event_camera->eye.currentPosition, &Event_camera->originOffset);
            Core_Math_AddVectors(&Event_camera->lookAt.currentPosition, &Event_camera->lookAt.currentPosition,
                &Event_camera->originOffset);
        }
    }
    Event_camera->originActorId = origin_actor;
    Event_UpdateEventCameraOrigin();
}

// decomp: BATTLE.PRG func_800BBE94 (src/BATTLE/BATTLE.PRG/4A0A8.c:3101)
void Event_UpdateEventCameraOrigin(void)
{
    Actors_ActorLocationQuery origin_actor_location;

    if (Event_camera->originActorId == 0x2000) {
        Event_camera->originOffset.vz = 0;
        Event_camera->originOffset.vy = 0;
        Event_camera->originOffset.vx = 0;
        return;
    }
    Actors_ActorPlacement(Event_camera->originActorId, &origin_actor_location);
    Event_camera->originOffset.vx = origin_actor_location.location.position.vx * ONE;
    Event_camera->originOffset.vy = origin_actor_location.location.position.vy * ONE;
    Event_camera->originOffset.vz = origin_actor_location.location.position.vz * ONE;
}

// decomp: BATTLE.PRG vs_battle_applyCameraState (src/BATTLE/BATTLE.PRG/4A0A8.c:3117)
void Event_ApplyEventCameraState(void)
{
    VECTOR cameraLookAt;
    VECTOR cameraPos;
    SVECTOR* vec;

    if (Event_skipNextFieldCameraRestore != 0) {
        Field_InitializeSphericalCamera(&cameraPos, &cameraLookAt, -1, -1);
        Core_Math_CopyVector(&Event_camera->lookAt.currentPosition, &cameraLookAt);
        Core_Math_CopyVector(&Event_camera->eye.currentPosition, &cameraPos);
    } else {
        Core_Math_CopyVector(&cameraLookAt, &Event_camera->lookAt.currentPosition);
        Core_Math_CopyVector(&cameraPos, &Event_camera->eye.currentPosition);
        if (Event_camera->originActorId != 0x2000) {
            Core_Math_AddVectors(&cameraLookAt, &cameraLookAt, &Event_camera->originOffset);
            Core_Math_AddVectors(&cameraPos, &cameraPos, &Event_camera->originOffset);
        }
    }
    if (*(short*)&Event_camera->shakeOscillatorA.envelopeShape != 0) {
        Core_Math_ShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (SVECTOR*)&Event_camera->shakeEyeOffsetX);
        Core_Math_ShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (SVECTOR*)&Event_camera->shakeLookAtOffsetX);
        vec = (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x088);
        vec[10].vz = 0;
        vec[10].vx = 0;
        vec[10].vy = Core_Math_FixedAtan2(
            cameraLookAt.vx - cameraPos.vx, cameraLookAt.vz - cameraPos.vz);
        Core_Gte_RotMatrix((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
        Core_Math_AddVectors(&cameraLookAt, &cameraLookAt, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        Core_Math_AddVectors(&cameraPos, &cameraPos, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
    }
    Field_SetCameraLookAt(&cameraLookAt);
    Field_SetCameraPosition(&cameraPos);
    if (Event_camera->cameraRoll.hasChanged != 0) {
        Field_SetCameraRoll(Event_camera->cameraRoll.angle);
        Event_camera->cameraRoll.hasChanged = 0;
    }
    if (Event_camera->capturedScreenRoll.hasChanged != 0) {
        Render_SetFramebufferEffectRotation(Event_camera->capturedScreenRoll.angle);
        Event_camera->capturedScreenRoll.hasChanged = 0;
    }
    if (Event_camera->projectionDistance.hasChanged != 0) {
        Render_SetCameraProjectionDistance(Event_camera->projectionDistance.value);
        Event_camera->projectionDistance.hasChanged = 0;
    }
    if (Event_camera->nearClip.hasChanged != 0) {
        Render_SetCameraNearClip(Event_camera->nearClip.value);
        Event_camera->nearClip.hasChanged = 0;
    }
    if (Event_camera->farClip.hasChanged != 0) {
        Render_SetCameraFarClip(Event_camera->farClip.value, Event_isScriptBackdropEnabled);
        Event_camera->farClip.hasChanged = 0;
    }
}

// decomp: BATTLE.PRG func_800BC1CC (src/BATTLE/BATTLE.PRG/4A0A8.c:3173)
void Event_RestoreEventFieldCamera(short field_facing, int field_distance_mode)
{
    VECTOR field_eye_position;
    VECTOR field_look_at;
    int eye_travel_distance;
    short return_duration_frames;

    if (Event_skipNextFieldCameraRestore != 0) {
        Event_skipNextFieldCameraRestore = 0;
        return;
    }
    Field_InitializeSphericalCamera(&field_eye_position, &field_look_at, field_facing, (short)field_distance_mode);
    Core_Math_CopyVector(&Event_camera->eye.destination, &field_eye_position);
    Core_Math_CopyVector(&Event_camera->lookAt.destination, &field_look_at);
    eye_travel_distance = Core_Math_FixedVectorMagnitude(Core_Math_SubtractVectors(
        (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &Event_camera->eye.currentPosition, &Event_camera->eye.destination));
    return_duration_frames = (eye_travel_distance
                  + Core_Math_FixedVectorMagnitude(Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098),
                      &Event_camera->lookAt.currentPosition, &Event_camera->lookAt.destination)))
            / 0x28000;
    if ((return_duration_frames << 0x10) != 0) {
        Event_fieldCameraReturnCommands[2] = Event_fieldCameraReturnCommands[6] = return_duration_frames;
        Event_CommandCameraMotion(Event_fieldCameraReturnCommands, 0);
        Event_CommandCameraMotion(Event_fieldCameraReturnCommands + 3, 0);
    }
}

// decomp: BATTLE.PRG func_800BC2E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:3200)
intptr_t Event_CommandCameraMotion(u_char* script, short unused)
{
    Event_CameraTrack* c;
    Event_CameraTrack* other;
    if (!(script[0] & 16)) {
        c = &Event_camera->lookAt;
        other = &Event_camera->eye;
    } else {
        c = &Event_camera->eye;
        other = &Event_camera->lookAt;
    }
    switch (script[0] & 15) {
    case 0:
        Event_ReadScriptFixedVector(&c->destination, script + 1);
        return 0;
    case 1:
        c->motionMode = 15;
        Core_Math_CopyVector(&c->currentPosition, &c->destination);
        Core_Math_CopyVector(&other->partnerPosition, &c->destination);
        return 0;
    case 2:
        c->motionMode = 2;
        c->elapsedFrames = 0;
        c->easingPrimary = script[1];
        c->durationFrames = script[2];
        Core_Debugf("event.camera", "start track=%s mode=%u easing=%u duration=%u",
            c == &Event_camera->eye ? "eye" : "look_at",
            (unsigned)c->motionMode, (unsigned)c->easingPrimary,
            (unsigned)c->durationFrames);
        Core_Math_CopyVector(&c->startPosition, &c->currentPosition);
        Core_Math_SubtractVectors(&c->displacement, &c->destination, &c->currentPosition);
        return 0;
    case 3:
        c->motionMode = 3;
        c->elapsedFrames = 0;
        c->easingPrimary = script[1];
        c->durationFrames = script[2];
        Core_Debugf("event.camera", "start track=%s mode=%u easing=%u duration=%u",
            c == &Event_camera->eye ? "eye" : "look_at",
            (unsigned)c->motionMode, (unsigned)c->easingPrimary,
            (unsigned)c->durationFrames);
        Core_Math_CopyVector(&c->startPosition, &c->currentPosition);
        Core_Math_SubtractVectors(&c->displacement, &c->destination, &c->currentPosition);
        c->motionScalar = Core_Math_FixedVectorMagnitude(
            Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->partnerPosition, &c->currentPosition));
        return 0;
    case 4: {
        Event_CameraTrackScratch* s;
        c->motionMode = 4;
        c->elapsedFrames = 0;
        c->durationFrames = script[3];
        c->easingPrimary = script[1] >> 4;
        c->easingSecondary = script[2] & 15;
        c->easingTertiary = script[2] >> 4;
        Core_Math_CopyVector(&c->startPosition, &c->partnerPosition);
        s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        setVector(&s->currentOffset, c->currentPosition.vx - c->partnerPosition.vx, 0, c->currentPosition.vz - c->partnerPosition.vz);
        c->motionScalar = Core_Math_FixedVectorMagnitude((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        c->orbitAngle = Core_Math_FixedAtan2(s->currentOffset.vx, s->currentOffset.vz) & 4095;
        setVector(
            &s->destinationOffset, c->destination.vx - c->partnerPosition.vx, 0, c->destination.vz - c->partnerPosition.vz);
        c->motionScalarDelta = Core_Math_FixedVectorMagnitude((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8)) - c->motionScalar;
        s->value = (Core_Math_FixedAtan2(s->destinationOffset.vx, s->destinationOffset.vz) & 4095) - c->orbitAngle;
        if (s->value < -ONE / 2) {
            s->value += ONE;
        }
        if (s->value > ONE / 2) {
            s->value -= ONE;
        }
        switch (script[1] & 15) {
        case 15:
            if (s->value == 0) {
                s->value = -ONE;
            } else if (s->value > 0) {
                s->value -= ONE;
            }
            break;
        case 1:
            if (s->value == 0) {
                s->value = ONE;
            } else if (s->value < 0) {
                s->value += ONE;
            }
            break;
        }
        c->orbitAngleDelta = ((Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088))->value;
        c->heightOffset.vy = c->currentPosition.vy - c->partnerPosition.vy;
        c->displacement.vy = c->destination.vy - c->partnerPosition.vy - c->heightOffset.vy;
        return 0;
    }
    case 5: {
        Event_CameraTrackScratch* s;
        int height;
        c->motionMode = 5;
        c->elapsedFrames = 0;
        c->durationFrames = script[8];
        c->easingPrimary = script[6] & 15;
        c->easingSecondary = script[6] >> 4;
        c->easingTertiary = script[7] & 15;
        Core_Math_CopyVector(&c->destination, &c->partnerPosition);
        s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        setVector(&s->currentOffset, c->currentPosition.vx - c->partnerPosition.vx, 0, c->currentPosition.vz - c->partnerPosition.vz);
        c->motionScalar = Core_Math_FixedVectorMagnitude((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        c->motionScalarDelta = ((c->motionScalar * script[5]) >> 6) - c->motionScalar;
        c->orbitAngle = Core_Math_FixedAtan2(s->currentOffset.vx, s->currentOffset.vz);
        c->orbitAngleDelta = Core_Memory_ReadUnalignedShort(script + 1);
        height = Core_Memory_ReadUnalignedShort(script + 3) * ONE;
        c->startPosition.vy = c->currentPosition.vy;
        c->displacement.vy = height - c->startPosition.vy;
        return 0;
    }
    case 7:
        ((void (*)(int, int, SVECTOR*, int))Actors_ActorTaggedBonePosition)(Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(script + 1)), script[3],
            (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8), 2);
        Core_Math_ShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8));
        Event_ReadScriptFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), script + 4);
        Core_Math_AddVectors(&c->destination, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
        if (Event_camera->originActorId != 0x2000) {
            Core_Math_SubtractVectors(&c->destination, &c->destination, &Event_camera->originOffset);
        }
        return 0;
    case 8:
        c->motionMode = 8;
        c->motionScalar = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(script + 1));
        c->motionScalarDelta = script[3];
        c->elapsedFrames = 0;
        c->durationFrames = script[10];
        Event_ReadScriptFixedVector(&c->displacement, script + 4);
        return 0;
    case 9:
        c->motionMode = 9;
        c->elapsedFrames = 0;
        c->durationFrames = script[1];
        Core_Math_SubtractVectors(&c->displacement, &c->currentPosition, &c->partnerPosition);
        return 0;
    case 10: {
        int id = Event_ResolveScriptActorReference(Core_Memory_ReadUnalignedShort(script + 1));
        Event_CameraTrackScratch* s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        s->value = id;
        Actors_ActorPlacement(id, &Event_camera->actorQueryScratch[1]);
        s->actorRotation.vz = 0;
        s->actorRotation.vx = 0;
        s->actorRotation.vy = *(short*)((char*)Event_camera + 0x1DA);
        Core_Gte_RotMatrix((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0E0), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        Event_ReadScriptFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), script + 4);
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        ((void (*)(int, int, SVECTOR*, int))Actors_ActorTaggedBonePosition)(s->value, script[3], (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0E0), 2);
        Core_Math_AddVectors(&c->destination,
            Core_Math_ShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0E0)),
            (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        if (Event_camera->originActorId != 0x2000) {
            Core_Math_SubtractVectors(&c->destination, &c->destination, &Event_camera->originOffset);
        }
        return 0;
    }
    case 11: {
        Event_CameraTrackScratch* s;
        c->motionMode = 11;
        c->elapsedFrames = 0;
        c->easingPrimary = 0;
        c->durationFrames = script[2];
        Core_Debugf("event.camera", "start track=%s mode=%u easing=%u duration=%u",
            c == &Event_camera->eye ? "eye" : "look_at",
            (unsigned)c->motionMode, (unsigned)c->easingPrimary,
            (unsigned)c->durationFrames);
        Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->partnerPosition, &c->currentPosition);
        Core_Math_DirectionRotationMatrix((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        s->matrix.t[0] = c->currentPosition.vx >> 12;
        s->matrix.t[1] = c->currentPosition.vy >> 12;
        s->matrix.t[2] = c->currentPosition.vz >> 12;
        c->currentPosition.vy = Event_EventCameraGroundClearance((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        Core_Math_CopyVector(&c->computedPosition, &c->currentPosition);
        c->motionScalarDelta = Core_Math_FixedVectorMagnitude(
            Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->destination, &c->computedPosition));
        c->motionScalar = c->motionScalarDelta / script[2];
        Event_camera->lastTerrainPosition.vx = Event_camera->lastTerrainPosition.vy = Event_camera->lastTerrainPosition.vz =
            0x80000000;
        return 0;
    }
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
    Event_UpdateEventCameraTrack(&Event_camera->lookAt, &Event_camera->eye);
    Event_UpdateEventCameraTrack(&Event_camera->eye, &Event_camera->lookAt);
    Event_UpdateEventAngleTween(&Event_camera->cameraRoll);
    Event_UpdateEventAngleTween(&Event_camera->capturedScreenRoll);
    Event_UpdateEventLensTween(&Event_camera->projectionDistance);
    Event_UpdateEventLensTween(&Event_camera->nearClip);
    Event_UpdateEventLensTween(&Event_camera->farClip);
    Event_UpdateEventCameraShake();
}

// decomp: BATTLE.PRG func_800BCA8C (src/BATTLE/BATTLE.PRG/4A0A8.c:3389)
void Event_UpdateEventCameraTrack(Event_CameraTrack* c, Event_CameraTrack* other)
{
    if (c->motionMode == 15) {
        return;
    }
    if (c->durationFrames == 0 && c->motionMode != 11) {
        Core_Warnf("event.camera", "zero duration track=%s mode=%u easing=%u elapsed=%u tick_speed=%u",
            c == &Event_camera->eye ? "eye" : "look_at",
            (unsigned)c->motionMode, (unsigned)c->easingPrimary,
            (unsigned)c->elapsedFrames, (unsigned)State_tickSpeed);
    }
    if (c->motionMode != 11) {
        c->elapsedFrames += State_tickSpeed >> 1;
        if (c->elapsedFrames > c->durationFrames) {
            c->elapsedFrames = c->durationFrames;
        }
    }
    switch (c->motionMode) {
    case 2:
        ((Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088))->value =
            ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingPrimary, c->elapsedFrames, c->durationFrames);
        Core_Math_ScaleFixedVector(
            (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->displacement, ((Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088))->value);
        Core_Math_AddVectors(&c->computedPosition, &c->startPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        break;
    case 3: {
        Event_CameraTrackScratch* s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        s->value = ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingPrimary, c->elapsedFrames, c->durationFrames);
        Core_Math_ScaleFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->displacement, s->value);
        Core_Math_AddVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), &c->startPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), &c->partnerPosition);
        Core_Math_NormalizeFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        s->distance = Core_Math_FixedVectorMagnitude(
            Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), &c->partnerPosition, &c->destination));
        s->value = c->motionScalar
                 + Core_Math_MultiplyFixed(s->distance - c->motionScalar,
                     ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingPrimary, c->elapsedFrames, c->durationFrames));
        Core_Math_ScaleFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), s->value);
        Core_Math_AddVectors(&c->computedPosition, &c->partnerPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        break;
    }
    case 4: {
        Event_CameraTrackScratch* s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        s->rotation.vz = 0;
        s->rotation.vx = 0;
        s->rotation.vy =
            c->orbitAngle
            + ((c->orbitAngleDelta * ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingPrimary, c->elapsedFrames, c->durationFrames)) >> 12);
        Core_Gte_RotMatrix((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        s->currentOffset.vz =
            c->motionScalar
            + Core_Math_MultiplyFixed(c->motionScalarDelta, ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingSecondary, c->elapsedFrames, c->durationFrames));
        s->currentOffset.vx = 0;
        s->currentOffset.vy = 0;
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        s->currentOffset.vy = c->heightOffset.vy
                 + Core_Math_MultiplyFixed(
                     c->displacement.vy, ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingTertiary, c->elapsedFrames, c->durationFrames));
        Core_Math_AddVectors(&c->computedPosition, &c->startPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        break;
    }
    case 5: {
        Event_CameraTrackScratch* s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        s->rotation.vz = 0;
        s->rotation.vx = 0;
        s->rotation.vy =
            c->orbitAngle
            + ((c->orbitAngleDelta * ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingPrimary, c->elapsedFrames, c->durationFrames)) >> 12);
        Core_Gte_RotMatrix((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        s->currentOffset.vz =
            c->motionScalar
            + Core_Math_MultiplyFixed(c->motionScalarDelta, ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingSecondary, c->elapsedFrames, c->durationFrames));
        s->currentOffset.vx = 0;
        s->currentOffset.vy = 0;
        Core_Gte_ApplyMatrixLV((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        c->computedPosition.vx = c->destination.vx + s->currentOffset.vx;
        c->computedPosition.vz = c->destination.vz + s->currentOffset.vz;
        c->computedPosition.vy = c->startPosition.vy
                       + Core_Math_MultiplyFixed(
                           c->displacement.vy, ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(c->easingTertiary, c->elapsedFrames, c->durationFrames));
        break;
    }
    case 8:
        ((void (*)(int, int, SVECTOR*, int))Actors_ActorTaggedBonePosition)(c->motionScalar, c->motionScalarDelta, (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8), 2);
        Core_Math_AddVectors(&c->computedPosition,
            Core_Math_ShortVectorToFixed((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8)),
            &c->displacement);
        break;
    case 9:
        Core_Math_AddVectors(&c->computedPosition, &c->displacement, &c->partnerPosition);
        break;
    case 11:
        Event_AdvanceTerrainCameraTrack(c);
        break;
    }
    Core_Math_CopyVector(&c->previousCameraPosition, &c->currentPosition);
    if (c->followRequest & c->isFollowing) {
        int fraction;
        Event_CameraTrackScratch* s;
        Core_Math_AddVectors(&c->followPosition, &c->followPosition, &c->followDelta);
        fraction = ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(1, c->elapsedFrames, c->durationFrames);
        s = (Event_CameraTrackScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        s->value = fraction;
        Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->computedPosition, &c->followPosition);
        Core_Math_ScaleFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), s->value);
        Core_Math_AddVectors(&c->currentPosition, &c->followPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
    } else {
        Core_Math_CopyVector(&c->currentPosition, &c->computedPosition);
    }
    if (c->elapsedFrames == c->durationFrames) {
        c->motionMode = 15;
        if (c->followRequest) {
            c->isFollowing = 1;
            Core_Math_CopyVector(&c->followPosition, &c->currentPosition);
            Core_Math_SubtractVectors(&c->followDelta, &c->currentPosition, &c->previousCameraPosition);
        }
    }
}

// decomp: BATTLE.PRG func_800BCFB4 (src/BATTLE/BATTLE.PRG/4A0A8.c:3502)
void Event_AdvanceTerrainCameraTrack(Event_CameraTrack* c)
{
    Event_CameraMotionScratch* scratch;
    Event_CameraMotionScratch* finalScratch;
    short i;
    int height;
    Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->partnerPosition, &c->destination);
    Core_Math_DirectionRotationMatrix((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
    scratch = (Event_CameraMotionScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
    scratch->matrix.t[0] = c->destination.vx >> 12;
    scratch->matrix.t[1] = c->destination.vy >> 12;
    scratch->matrix.t[2] = c->destination.vz >> 12;
    setVector(&scratch->delta, c->destination.vx - c->computedPosition.vx,
        Event_EventCameraGroundClearance((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8)) - c->computedPosition.vy,
        c->destination.vz - c->computedPosition.vz);
    scratch->distance = Core_Math_FixedVectorMagnitude((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
    scratch->steps = scratch->distance / c->motionScalar;
    if (scratch->steps < 2) {
        c->elapsedFrames = c->durationFrames;
    } else if (scratch->steps > 10) {
        scratch->steps = 10;
    }
    Core_Math_NormalizeFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
    Core_Math_ScaleFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), c->motionScalar);
    Core_Math_CopyVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), &c->computedPosition);
    for (i = 1; i <= ((Event_CameraMotionScratch*)((uintptr_t)Core_ScratchBase() + 0x088))->steps; ++i) {
        scratch = (Event_CameraMotionScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
        Core_Math_AddVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
        Core_Math_SubtractVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0B8), &c->partnerPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
        Core_Math_DirectionRotationMatrix((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0B8), (MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        scratch->matrix.t[0] = scratch->candidate.vx >> 12;
        scratch->matrix.t[1] = scratch->candidate.vy >> 12;
        scratch->matrix.t[2] = scratch->candidate.vz >> 12;
        height = Event_EventCameraGroundClearance((MATRIX*)((uintptr_t)Core_ScratchBase() + 0x0F8));
        if (height < scratch->candidate.vy) {
            setVector(&scratch->delta, scratch->candidate.vx - c->computedPosition.vx,
                height - c->computedPosition.vy, scratch->candidate.vz - c->computedPosition.vz);
            Core_Math_NormalizeFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098));
            Core_Math_ScaleFixedVector((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), c->motionScalar);
            break;
        }
    }
    Core_Math_AddVectors((VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8), (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x098), &c->computedPosition);
    finalScratch = (Event_CameraMotionScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
    if (finalScratch->candidate.vx == Event_camera->lastTerrainPosition.vx
        && finalScratch->candidate.vy == Event_camera->lastTerrainPosition.vy
        && finalScratch->candidate.vz == Event_camera->lastTerrainPosition.vz) {
        c->elapsedFrames = c->durationFrames;
    } else {
        Core_Math_CopyVector(&Event_camera->lastTerrainPosition, &c->computedPosition);
        Core_Math_CopyVector(&c->computedPosition, (VECTOR*)((uintptr_t)Core_ScratchBase() + 0x0A8));
    }
}

// decomp: BATTLE.PRG func_800BD2B8 (src/BATTLE/BATTLE.PRG/4A0A8.c:3556)
int Event_EventCameraGroundClearance(MATRIX* camera_transform)
{
    VECTOR pos;
    SVECTOR offset;
    long flag;
    int first;
    int second;
    int h;
    short lowest;

    Core_Gte_SetRotMatrix(camera_transform);
    Core_Gte_SetTransMatrix(camera_transform);

    setVector(&offset, 80, 80, 150);
    Core_Gte_RotTrans(&offset, &pos, &flag);
    first = ((Field_FloorHeightAt(pos.vx, pos.vz) << 0x11) >> 0x11) - 200;
    lowest = first;

    setVector(&offset, -80, 80, 150);
    Core_Gte_RotTrans(&offset, &pos, &flag);
    second = ((Field_FloorHeightAt(pos.vx, pos.vz) << 0x11) >> 0x11) - 200;

    if (second < first) {
        lowest = second;
    }

    setVector(&offset, 80, -80, 150);
    Core_Gte_RotTrans(&offset, &pos, &flag);
    h = ((Field_FloorHeightAt(pos.vx, pos.vz) << 0x11) >> 0x11) - 200;

    if (h < lowest) {
        lowest = h;
    }

    setVector(&offset, -80, -80, 150);
    Core_Gte_RotTrans(&offset, &pos, &flag);
    h = ((Field_FloorHeightAt(pos.vx, pos.vz) << 0x11) >> 0x11) - 200;

    if (h < lowest) {
        lowest = h;
    }

    return lowest * ONE;
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
        roll_tween->angle = Core_Memory_ReadUnalignedShort(instruction + 1);
        roll_tween->hasChanged = 1;
    } else {
        roll_tween->startAngle = roll_tween->angle;
        roll_tween->deltaAngle = Core_Memory_ReadUnalignedShort(instruction + 1) - roll_tween->angle;
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
                      + ((angle_tween->deltaAngle * ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(angle_tween->easingMode, angle_tween->elapsedFrames, angle_tween->durationFrames))
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
        Render_SetFramebufferEffectDepth(Core_Memory_ReadUnalignedShort(script + 1));
        return 0;

    case 186:
        Render_SetFramebufferPreviousFrame(script[1]);
        return 0;

    case 228:
        Event_camera->screenScale.durationFrames = script[4];
        if (Event_camera->screenScale.durationFrames == 0) {
            values.x = (script[1] << 7);
            values.y = (script[2] << 7);
            Render_SetFramebufferEffectScale(&values);
        } else {
            Event_camera->screenScale.elapsedFrames = 0;
            Render_FramebufferEffectScale(&values.x);
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
            Render_SetFramebufferEffectTint(&color);
        } else {
            Event_camera->screenTint.elapsedFrames = 0;
            Render_FramebufferEffectTint(&color);
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
            Render_SetFramebufferEffectOffset(&values);
        } else {
            Event_camera->screenTranslation.elapsedFrames = 0;
            Render_FramebufferEffectOffset(&values);
            Event_camera->screenTranslation.deltaValue0 =
                (signed char)script[1] - (Event_camera->screenTranslation.startValue0 = values.x);
            Event_camera->screenTranslation.deltaValue1 =
                (signed char)script[2] - (Event_camera->screenTranslation.startValue1 = values.y);
            Event_camera->screenTranslation.easingMode = script[3];
        }
        return 0;

    case 231:
        Render_SetFramebufferEffectBlendMode(script[1]);
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
        mode = Core_Memory_ReadUnalignedShort(script + 1);
        if (!mode) {
            Render_SetFramebufferEffectBlendMode(0);
            values.x = values.y = 0;
            Render_SetFramebufferEffectOffset(&values);
            values.x = values.y = ONE;
            Render_SetFramebufferEffectScale(&values);
            color.r = color.g = color.b = 128;
            Render_SetFramebufferEffectTint(&color);
            Render_SetFramebufferEffectDepth(0);
            Render_SetFramebufferPreviousFrame(0);
            Render_SetFramebufferEffectEnabled(0);
        } else {
            Render_SetFramebufferEffectEnabled(1);
            Render_SetFramebufferEffectBlendMode(0);
            values.x = 0;
            values.y = 2;
            Render_SetFramebufferEffectOffset(&values);
            values.x = values.y = ONE;
            Render_SetFramebufferEffectScale(&values);
            color.r = color.g = color.b = 144;
            Render_SetFramebufferPreviousFrame(1);
            Render_SetFramebufferEffectTint(&color);
            Render_SetFramebufferEffectDepth(mode);
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
        Render_SetFramebufferEffectScale(&screen_transform);
    }
    if (Event_UpdateEventTripleTween(&Event_camera->screenTint) != 0) {
        screen_tint.r = Event_camera->screenTint.currentValue0;
        screen_tint.g = Event_camera->screenTint.currentValue1;
        screen_tint.b = Event_camera->screenTint.currentValue2;
        Render_SetFramebufferEffectTint(&screen_tint);
    }
    if (Event_UpdateEventTripleTween(&Event_camera->screenTranslation) != 0) {
        screen_transform.x = Event_camera->screenTranslation.currentValue0;
        screen_transform.y = Event_camera->screenTranslation.currentValue1;
        Render_SetFramebufferEffectOffset(&screen_transform);
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

    eased_fraction = ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(triple_tween->easingMode, triple_tween->elapsedFrames, triple_tween->durationFrames);
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

// decomp: BATTLE.PRG func_800BDC9C (src/BATTLE/BATTLE.PRG/4A0A8.c:3862)
intptr_t Event_CommandCameraLensOrShake(u_char* script, short thread_index)
{
    Event_ShakeOscillator* entry;
    short i;
    switch (script[0]) {
    case 234:
        if ((Event_camera->projectionDistance.durationFrames = script[3]) == 0) {
            Event_camera->projectionDistance.value = script[1] ? (script[1] << 2) : ONE / 8;
            Event_camera->projectionDistance.hasChanged = 1;
        } else {
            Event_camera->projectionDistance.deltaValue =
                script[1] ? (script[1] << 2)
                                - (Event_camera->projectionDistance.startValue =
                                        Event_camera->projectionDistance.value)
                          : ONE / 8;
            Event_camera->projectionDistance.easingMode = script[2];
            Event_camera->projectionDistance.elapsedFrames = 0;
        }
        return 0;
    case 235:
        if ((Event_camera->nearClip.durationFrames = script[3]) == 0) {
            Event_camera->nearClip.value = script[1] ? script[1] : 64;
            Event_camera->nearClip.hasChanged = 1;
        } else {
            Event_camera->nearClip.deltaValue =
                (script[1] ? script[1] : 64)
                - (Event_camera->nearClip.startValue = Event_camera->nearClip.value);
            Event_camera->nearClip.easingMode = script[2];
            Event_camera->nearClip.elapsedFrames = 0;
        }
        return 0;
    case 236:
        if ((Event_camera->farClip.durationFrames = script[3]) == 0) {
            Event_camera->farClip.value = script[1] ? (script[1] << 7) : ONE;
            Event_camera->farClip.hasChanged = 1;
        } else {
            Event_camera->farClip.deltaValue =
                script[1] ? (script[1] << 7)
                                - (Event_camera->farClip.startValue = Event_camera->farClip.value)
                          : ONE;
            Event_camera->farClip.easingMode = script[2];
            Event_camera->farClip.elapsedFrames = 0;
        }
        return 0;
    case 239:
        entry = (Event_ShakeOscillator*)&Event_camera->shakeOscillatorA.displacement;
        if (script[2] == 0) {
            entry[0].durationFrames = entry[1].durationFrames = 0;
            return 0;
        }
        for (i = 0; i < 2; ++i, ++entry) {
            if (entry->durationFrames == 0) {
                entry->cycles = script[1];
                entry->amplitude = script[2];
                entry->axisMask = script[3];
                entry->envelopeShape.bytes[1] = script[4];
                entry->durationFrames = script[5];
                entry->elapsedFrames = 0;
                break;
            }
        }
        return 0;
    }
}

// decomp: BATTLE.PRG func_800BDF6C (src/BATTLE/BATTLE.PRG/4A0A8.c:3927)
void Event_UpdateEventLensTween(Event_LensTween* lens_tween)
{
    if (lens_tween->durationFrames != 0) {
        short* delta_value_ptr;
        lens_tween->elapsedFrames += State_tickSpeed >> 1;
        if (lens_tween->durationFrames < lens_tween->elapsedFrames) {
            lens_tween->elapsedFrames = lens_tween->durationFrames;
        }
        *(int*)((uintptr_t)Core_ScratchBase() + 0x088) = ((int (*)(int, int, int))Event_EvaluateInterpolationCurve)(lens_tween->easingMode, lens_tween->elapsedFrames, lens_tween->durationFrames);
        delta_value_ptr = &lens_tween->deltaValue;
        lens_tween->value = lens_tween->startValue + ((*delta_value_ptr * *(int*)((uintptr_t)Core_ScratchBase() + 0x088)) >> 0xC);
        if (lens_tween->elapsedFrames == lens_tween->durationFrames) {
            lens_tween->durationFrames = 0;
        }
        lens_tween->hasChanged = 1;
    }
}

// decomp: BATTLE.PRG func_800BE01C (src/BATTLE/BATTLE.PRG/4A0A8.c:3945)
int Event_UpdateEventShakeOscillator(Event_ShakeOscillator* shake_oscillator)
{
    int total_phase_angle = shake_oscillator->cycles << 0xC;

    if (shake_oscillator->durationFrames == 0) {
        return 0;
    }

    shake_oscillator->elapsedFrames += State_tickSpeed >> 1;

    if (shake_oscillator->durationFrames < shake_oscillator->elapsedFrames) {
        shake_oscillator->elapsedFrames = shake_oscillator->durationFrames;
    }

    switch (shake_oscillator->envelopeShape.bytes[1]) {
    case 0:
        *(int*)((uintptr_t)Core_ScratchBase() + 0x088) = 0x1000;
        break;
    case 1:
        *(int*)((uintptr_t)Core_ScratchBase() + 0x088) = Core_Gte_Sin(((shake_oscillator->elapsedFrames << 0xA) / shake_oscillator->durationFrames) + 0x400);
        break;
    case 2:
        *(int*)((uintptr_t)Core_ScratchBase() + 0x088) = Core_Gte_Sin((shake_oscillator->elapsedFrames << 0xB) / shake_oscillator->durationFrames);
        break;
    case 3:
        *(int*)((uintptr_t)Core_ScratchBase() + 0x088) = Core_Gte_Sin((shake_oscillator->elapsedFrames << 0xA) / shake_oscillator->durationFrames);
        break;
    }
    shake_oscillator->displacement = (((Core_Gte_Sin((total_phase_angle * shake_oscillator->elapsedFrames) / shake_oscillator->durationFrames) * shake_oscillator->amplitude) >> 0xC)
                     * *(int*)((uintptr_t)Core_ScratchBase() + 0x088))
              >> 0xC;
    if (shake_oscillator->elapsedFrames == shake_oscillator->durationFrames) {
        shake_oscillator->durationFrames = 0;
    }
    return 1;
}

// decomp: BATTLE.PRG func_800BE180 (src/BATTLE/BATTLE.PRG/4A0A8.c:3983)
void Event_UpdateEventCameraShake(void)
{
    short count = 0;
    Event_CameraShakeScratch* clear = (Event_CameraShakeScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
    short i = 0;
    int x, y, z;
    Event_CameraShakeScratch* scratch;
    Event_ShakeOscillator* entry = (Event_ShakeOscillator*)&Event_camera->shakeOscillatorA.displacement;
    clear->eyeShake.vx = clear->eyeShake.vy = clear->eyeShake.vz = clear->lookAtShake.vx = clear->lookAtShake.vy = clear->lookAtShake.vz = 0;
    scratch = (Event_CameraShakeScratch*)((uintptr_t)Core_ScratchBase() + 0x088);
    for (; i < 2; ++entry, ++i) {
        if (Event_UpdateEventShakeOscillator(entry)) {
            setVector(&scratch->axisShake, x = entry->displacement * (((entry->axisMask >> 2) & 3) - 1),
                y = entry->displacement * (((entry->axisMask >> 4) & 3) - 1),
                z = entry->displacement * ((entry->axisMask >> 6) - 1));
            if (entry->axisMask & 1) {
                applyVector(&scratch->eyeShake, x, y, z, +=);
            }
            if (entry->axisMask & 2) {
                addVector(&scratch->lookAtShake, &scratch->axisShake);
            }
            ++count;
        }
    }
    Event_camera->shakeOscillatorA.envelopeShape.count = count;
    if (count) {
        Event_camera->shakeEyeOffsetX = ((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8))->vx;
        Event_camera->shakeEyeOffsetY = ((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8))->vy;
        Event_camera->shakeEyeOffsetZ = ((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0D8))->vz;
        Event_camera->shakeLookAtOffsetX = ((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0E0))->vx;
        Event_camera->shakeLookAtOffsetY = ((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0E0))->vy;
        Event_camera->shakeLookAtOffsetZ = ((SVECTOR*)((uintptr_t)Core_ScratchBase() + 0x0E0))->vz;
    }
}

// decomp: BATTLE.PRG func_800BE36C (src/BATTLE/BATTLE.PRG/4A0A8.c:4018)
void Event_SetEventCameraFollowing(int look_at_follow, int eye_follow)
{
    Event_camera->lookAt.followRequest = look_at_follow;
    Event_camera->lookAt.isFollowing = 0;
    Event_camera->eye.followRequest = eye_follow;
    Event_camera->eye.isFollowing = 0;
}

// decomp: BATTLE.PRG func_800BE3A0 (src/BATTLE/BATTLE.PRG/4A0A8.c:4026)
void Event_StopEventCameraMotion(void)
{
    Event_CameraState* c1 = Event_camera;
    Event_CameraState* c2;
    c1->eye.motionMode = 0xF;
    c1->lookAt.motionMode = 0xF;
    c2 = Event_camera;
    c2->eye.isFollowing = 0;
    c2->eye.followRequest = 0;
    c2->lookAt.isFollowing = 0;
    c2->lookAt.followRequest = 0;
}

// decomp: BATTLE.PRG func_800BE3D0 (src/BATTLE/BATTLE.PRG/4A0A8.c:4039)
void Event_StartEventCameraOrbitReturn(char orbit_side_selector)
{
    VECTOR start_look_at;
    VECTOR destination_look_at;
    VECTOR start_eye_position;
    VECTOR destination_eye_position;
    u_char distance_mode_index;
    u_char saved_facing;
    int preceding_facing;

    saved_facing = State_ScriptFlag(0xAF);
    distance_mode_index = State_ScriptFlag(0xB0);
    preceding_facing = saved_facing - 1;
    Field_InitializeSphericalCamera(
        &start_eye_position, &start_look_at, (preceding_facing + (orbit_side_selector * 2)) & 7, distance_mode_index + 1);

    start_eye_position.vy += Event_kOrbitReturnHeightOffsets[distance_mode_index] << 0xC;
    start_look_at.vy += Event_kOrbitReturnHeightOffsets[distance_mode_index] << 0xC;

    Core_Math_CopyVector(&Event_camera->lookAt.currentPosition, &start_look_at);
    Core_Math_CopyVector(&Event_camera->lookAt.partnerPosition, &start_eye_position);
    Core_Math_CopyVector(&Event_camera->eye.partnerPosition, &start_look_at);
    Core_Math_CopyVector(&Event_camera->eye.currentPosition, &start_eye_position);

    Event_camera->lookAt.motionMode = 0xF;

    Field_InitializeSphericalCamera(&destination_eye_position, &destination_look_at, saved_facing, distance_mode_index + 1);

    Core_Math_CopyVector(&Event_camera->eye.destination, &destination_eye_position);
    Core_Math_CopyVector(&Event_camera->lookAt.destination, &destination_look_at);

    Event_CommandCameraMotion(Event_kOrbitReturnFirstCommand, 0);
    Event_CommandCameraMotion(Event_kOrbitReturnSecondCommand, 0);
}

// decomp: BATTLE.PRG func_800BE53C (src/BATTLE/BATTLE.PRG/4A0A8.c:4074)
void Event_ScaleEventCameraTrackDuration(Event_CameraTrack* camera_track)
{
    VECTOR remaining_displacement;

    Core_Math_SubtractVectors(&remaining_displacement, &camera_track->currentPosition, &camera_track->destination);
    camera_track->durationFrames = ((Core_Math_FixedVectorMagnitude(&remaining_displacement) >> 0xC) * camera_track->durationFrames) / 1000;
}

// decomp: BATTLE.PRG func_800BE5A4 (src/BATTLE/BATTLE.PRG/4A0A8.c:4082)
void Event_ScaleEventCameraDuration(short track_selector)
{
    Event_CameraTrack* look_at_track;
    Event_CameraTrack* selected_track;
    Event_CameraTrack* eye_track;

    look_at_track = &Event_camera->lookAt;
    eye_track = &Event_camera->eye;

    if (track_selector == 2) {
        Event_ScaleEventCameraTrackDuration(look_at_track);
        Event_ScaleEventCameraTrackDuration(eye_track);
        look_at_track->durationFrames = eye_track->durationFrames =
            (look_at_track->durationFrames + eye_track->durationFrames) >> 1;

        return;
    }
    selected_track = look_at_track;
    if (track_selector != 0) {
        selected_track = eye_track;
    }
    Event_ScaleEventCameraTrackDuration(selected_track);
}

// decomp: BATTLE.PRG _setVectorImm (src/BATTLE/BATTLE.PRG/4A0A8.c:4148)
VECTOR* Event_ReadScriptFixedVector(VECTOR* destination, u_char* operand_bytes)
{
    setVector(destination, Core_Memory_ReadUnalignedShort(operand_bytes) * ONE, Core_Memory_ReadUnalignedShort(operand_bytes + 2) * ONE,
        Core_Memory_ReadUnalignedShort(operand_bytes + 4) * ONE);
    return destination;
}

