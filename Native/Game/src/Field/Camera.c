// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/Movement.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/Targeting.h"
#include "Core/Memory/Heap.h"
#include "DiscQueue.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Map.h"
#include "Field/Mpd.h"
#include "Field/RoomLoading.h"
#include "Field/RoomRendering.h"
#include "Field/Znd.h"
#include "Field/Zone.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Input/Controllers.h"
#include "Render/Camera.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: _firstPersonViewEnabled (_firstPersonViewEnabled@BATTLE/BATTLE.PRG)
int Field_isFirstPersonViewEnabled;

// decomp: vs_battle_cameraTransition (vs_battle_cameraTransition@BATTLE/BATTLE.PRG)
Field_CameraTransition* Field_cameraTransition;

// decomp: D_800F19C8 (D_800F19C8@BATTLE/BATTLE.PRG)
int Field_roomGeometryDrawMode;

// decomp: vs_battle_cameraCurrentSpherical (vs_battle_cameraCurrentSpherical@BATTLE/BATTLE.PRG)
Field_SphericalCamera Field_sphericalCamera;

// decomp: BATTLE.PRG func_80069D78 (src/BATTLE/BATTLE.PRG/146C.c:776)
void Field_UpdateCameraFacingState(void)
{
    Field_SyncCameraAnglesFromPosition(NULL);
    Render_frameScratch.firstPersonViewFacing =
        ((Render_frameScratch.firstPersonViewFacing & 0x7FFF) | (Field_isFirstPersonViewEnabled << 0x10));
}

// decomp: BATTLE.PRG _addMinDelta (src/BATTLE/BATTLE.PRG/146C.c:2389)
int Field_AddCameraStepWithFallback(int base, int a, int b)
{
    if (b < 0) {
        if (a >= b) {
            if (a != 0) {
                base += a;
            } else {
                base += b;
            }
        } else {
            base += b;
        }
    } else if ((b >= a) && (a != 0)) {
        base += a;
    } else {
        base += b;
    }
    return base;
}

// decomp: BATTLE.PRG _add_min (src/BATTLE/BATTLE.PRG/146C.c:2409)
int Field_AddClampedCameraStep(int base, int a, int b)
{

    if (b < 0) {
        if (a < b) {
            base += b;
        } else {
            base += a;
        }
    } else if (b < a) {
        base += b;
    } else {
        base += a;
    }
    return base;
}

// decomp: BATTLE.PRG _clampPositionToZoneBounds (src/BATTLE/BATTLE.PRG/146C.c:2426)
void Field_ClampCameraLookAtToZone(VECTOR* inPos, VECTOR* outPos)
{
    int _[2] __attribute__((unused));
    int bound;
    int entityRadius;
    int maxBound;
    int v;

    entityRadius = (Field_sphericalCamera.values.distance * ONE) / 2304;
    bound = Field_zoneContext.bounds.minX * ONE * 128 + entityRadius * 360;
    maxBound = Field_zoneContext.bounds.maxX * ONE * 128 - entityRadius * 360;

    if (maxBound < bound) {
        outPos->vx = (bound + maxBound) >> 1;
    } else {
        if (inPos->vx < bound) {
            outPos->vx = bound;
        } else if (maxBound < inPos->vx) {
            outPos->vx = maxBound;
        } else {
            outPos->vx = inPos->vx;
        }
    }

    bound = Field_zoneContext.bounds.minZ * ONE * 128 + entityRadius * 360;
    maxBound = Field_zoneContext.bounds.maxZ * ONE * 128 - entityRadius * 360;

    if (maxBound < bound) {
        outPos->vz = (bound + maxBound) >> 1;
    } else {
        if (inPos->vz < bound) {
            outPos->vz = bound;
        } else if (maxBound < inPos->vz) {
            outPos->vz = maxBound;
        } else {
            outPos->vz = inPos->vz;
        }
    }

    outPos->vx &= ~(ONE - 1);
    outPos->vz &= ~(ONE - 1);

    bound = inPos->vy - Render_frameScratch.camera.lookAt.vy;

    v = -(ONE * 96);
    if ((bound > (ONE * 96)) || (bound < v)) {
        outPos->vy = inPos->vy;
    } else {
        outPos->vy = Render_frameScratch.camera.lookAt.vy;
    }
    if (outPos->vy > 0) {
        outPos->vy = 0;
    }
}

// decomp: BATTLE.PRG _computeStepAndDisplacement (src/BATTLE/BATTLE.PRG/146C.c:2481)
int Field_ComputeCameraTrackingStep(SVECTOR* targetPos, VECTOR* outStep,
    VECTOR* outDisplacement, int clampToZone, int maxSpeed)
{
    SVECTOR targetPosScaled;
    VECTOR targetPosFixed;
    VECTOR direction;
    long sxy;
    long p;
    long notCentered;

    targetPosFixed.vx = targetPos->vx * ONE;
    targetPosFixed.vz = targetPos->vz * ONE;
    targetPosFixed.vy = targetPos->vy * ONE;

    if (clampToZone != 0) {
        Field_ClampCameraLookAtToZone(&targetPosFixed, &targetPosFixed);
    }

    targetPosScaled.vx = targetPosFixed.vx / ONE;
    targetPosScaled.vz = targetPosFixed.vz / ONE;
    targetPosScaled.vy = targetPosFixed.vy / ONE;

    Core_Gte_SetRotMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_SetTransMatrix(&Render_frameScratch.viewMatrix);
    Core_Gte_RotTransPers(&targetPosScaled, &sxy, &p, &notCentered);

    if (clampToZone == 0) {
        int projected_y = sxy / 0x10000;
        if (((sxy - (projected_y * 0x10000)) - 61) < 199U) {
            int dead_zone_bottom_y = 180;
            if ((projected_y > 60) && (projected_y < dead_zone_bottom_y)) {
                notCentered = 0;
            } else {
                notCentered = 1;
            }
        } else {
            notCentered = 1;
        }
    } else {
        notCentered = 1;
    }

    outDisplacement->vx = (targetPosScaled.vx * ONE) - Render_frameScratch.camera.lookAt.vx;
    targetPosFixed.vx = outDisplacement->vx / ONE;
    outDisplacement->vz = (targetPosScaled.vz * ONE) - Render_frameScratch.camera.lookAt.vz;
    targetPosFixed.vz = outDisplacement->vz / ONE;
    outDisplacement->vy = (targetPosScaled.vy * ONE) - Render_frameScratch.camera.lookAt.vy;
    targetPosFixed.vy = outDisplacement->vy / ONE;

    Core_Gte_VectorNormal(&targetPosFixed, &direction);

    if (notCentered) {
        int distanceToTarget;
        if (direction.vx != 0) {
            distanceToTarget = outDisplacement->vx / direction.vx;
        } else if (direction.vz != 0) {
            distanceToTarget = outDisplacement->vz / direction.vz;
        } else if (direction.vy != 0) {
            distanceToTarget = outDisplacement->vy / direction.vy;
        } else {
            distanceToTarget = 1;
        }
        if (distanceToTarget > 40) {
            Field_sphericalCamera.speed += 8;
            if (maxSpeed < Field_sphericalCamera.speed) {
                Field_sphericalCamera.speed = maxSpeed;
            }
        } else {
            Field_sphericalCamera.speed -= 8;
            if (Field_sphericalCamera.speed < 8) {
                Field_sphericalCamera.speed = 8;
            }
        }
        outStep->vx = direction.vx * Field_sphericalCamera.speed;
        outStep->vz = direction.vz * Field_sphericalCamera.speed;
        outStep->vy = direction.vy * Field_sphericalCamera.speed;
    } else {
        Field_sphericalCamera.speed -= 8;
        if (Field_sphericalCamera.speed < 0) {
            Field_sphericalCamera.speed = 0;
        }
        outStep->vx = direction.vx * Field_sphericalCamera.speed;
        outStep->vz = direction.vz * Field_sphericalCamera.speed;
        outStep->vy = direction.vy * Field_sphericalCamera.speed;
    }
    return notCentered;
}

// decomp: BATTLE.PRG _stepTowardTarget (src/BATTLE/BATTLE.PRG/146C.c:2569)
int Field_StepCameraLookAtTowardTarget(SVECTOR* targetPos, int transformFlag, int maxSpeed)
{
    VECTOR displacement;
    VECTOR step;

    if (Field_ComputeCameraTrackingStep(
            targetPos, &step, &displacement, transformFlag, maxSpeed)
        != 0) {
        Render_frameScratch.camera.lookAt.vx =
            Field_AddCameraStepWithFallback(Render_frameScratch.camera.lookAt.vx, step.vx, displacement.vx);
        Render_frameScratch.camera.lookAt.vz =
            Field_AddCameraStepWithFallback(Render_frameScratch.camera.lookAt.vz, step.vz, displacement.vz);
        Render_frameScratch.camera.lookAt.vy =
            Field_AddCameraStepWithFallback(Render_frameScratch.camera.lookAt.vy, step.vy, displacement.vy);
    } else {
        Render_frameScratch.camera.lookAt.vx =
            Field_AddClampedCameraStep(Render_frameScratch.camera.lookAt.vx, step.vx, displacement.vx);
        Render_frameScratch.camera.lookAt.vz =
            Field_AddClampedCameraStep(Render_frameScratch.camera.lookAt.vz, step.vz, displacement.vz);
        Render_frameScratch.camera.lookAt.vy =
            Field_AddClampedCameraStep(Render_frameScratch.camera.lookAt.vy, step.vy, displacement.vy);
    }
    if (Render_frameScratch.camera.lookAt.vy > 0) {
        Render_frameScratch.camera.lookAt.vy = 0;
    }
    return Field_sphericalCamera.speed;
}

// decomp: BATTLE.PRG _snapLookAtToPosition (src/BATTLE/BATTLE.PRG/146C.c:2597)
void Field_TrackCameraLookAtWithinDeadZone(VECTOR* target_position)
{
    Field_CameraPositionPair target_and_displacement;
    int negative_axis_threshold;

    Field_ClampCameraLookAtToZone(target_position, &target_and_displacement.position);
    target_and_displacement.lookAt.vx = target_and_displacement.position.vx - Render_frameScratch.camera.lookAt.vx;
    target_and_displacement.lookAt.vz = target_and_displacement.position.vz - Render_frameScratch.camera.lookAt.vz;
    target_and_displacement.lookAt.vy = target_and_displacement.position.vy - Render_frameScratch.camera.lookAt.vy;

    if (target_and_displacement.lookAt.vx > 0x12000) {
        Render_frameScratch.camera.lookAt.vx += 0x12000;
    } else {
        negative_axis_threshold = 0xFFFEE000;
        if (target_and_displacement.lookAt.vx < negative_axis_threshold) {
            Render_frameScratch.camera.lookAt.vx -= 0x12000;
        } else {
            Render_frameScratch.camera.lookAt.vx = target_and_displacement.position.vx;
        }
    }
    if (target_and_displacement.lookAt.vz > 0x12000) {
        Render_frameScratch.camera.lookAt.vz += 0x12000;
    } else {
        negative_axis_threshold = 0xFFFEE000;
        if (target_and_displacement.lookAt.vz < negative_axis_threshold) {
            Render_frameScratch.camera.lookAt.vz -= 0x12000;
        } else {
            Render_frameScratch.camera.lookAt.vz = target_and_displacement.position.vz;
        }
    }
    if (target_and_displacement.lookAt.vy < 0) {
        negative_axis_threshold = 0xFFFA0000;
        if (target_and_displacement.lookAt.vy < negative_axis_threshold) {
            Render_frameScratch.camera.lookAt.vy += 0x60000 + target_and_displacement.lookAt.vy;
        }
    } else if (target_and_displacement.lookAt.vy > 0x60000) {
        Render_frameScratch.camera.lookAt.vy -= 0x60000 - target_and_displacement.lookAt.vy;
    }
    if (Render_frameScratch.camera.lookAt.vy > 0) {
        Render_frameScratch.camera.lookAt.vy = 0;
    }
}

// decomp: src/BATTLE/BATTLE.PRG/146C.c:27
#define CAMERA_INPUT_LOCKED()                                                           \
    ((Field_controlMode == 3) || ((Field_controlMode == 2) && ((Battle_actionFlowMode == 4) || (Battle_actionFlowMode == 11))))
// decomp: BATTLE.PRG func_8006E158 (src/BATTLE/BATTLE.PRG/146C.c:2641)
#include "Battle/TargetPages.h"

void Field_UpdateCameraOrbitInput(void)
{
    if (Field_sphericalCamera.delta.yaw == 0) {
        if (!CAMERA_INPUT_LOCKED() && !Battle_TargetPagesUseShoulders()) {
            if ((Input_previousButtons & (PADL1 | PADR1)) == PADR1) {
                Field_sphericalCamera.delta.yaw = 1;
                Field_sphericalCamera.delta.pitch =
                    (Field_sphericalCamera.values.yaw - 0x200) & ~0x1FF;
            } else if ((Input_previousButtons & (PADL1 | PADR1)) == PADL1) {
                Field_sphericalCamera.delta.yaw = 2;
                Field_sphericalCamera.delta.pitch =
                    (Field_sphericalCamera.values.yaw + 0x200) & ~0x1FF;
            }
        }
    } else {
        switch (Field_sphericalCamera.delta.yaw) {
        case 1:
            Field_sphericalCamera.values.yaw -=
                Field_sphericalCamera.delta.distance;
            if (Field_sphericalCamera.values.yaw
                <= Field_sphericalCamera.delta.pitch + 0x78) {
                if (((Input_previousButtons & (PADL1 | PADR1)) == PADR1)
                    && !CAMERA_INPUT_LOCKED() && !Battle_TargetPagesUseShoulders()) {
                    Field_sphericalCamera.delta.pitch =
                        (Field_sphericalCamera.delta.pitch - 0x200) & ~0x1FF;
                }
                if (Field_sphericalCamera.delta.distance > 8) {
                    Field_sphericalCamera.delta.distance -= 8;
                }
            } else if (Field_sphericalCamera.delta.distance < 40) {
                Field_sphericalCamera.delta.distance += 8;
                if (Field_sphericalCamera.delta.distance > 40) {
                    Field_sphericalCamera.delta.distance = 40;
                }
            }
            if (Field_sphericalCamera.values.yaw
                <= Field_sphericalCamera.delta.pitch) {
                Field_sphericalCamera.values.yaw &= 0xE00;
                Field_sphericalCamera.delta.yaw = 0;
            }
            break;

        case 2:
            Field_sphericalCamera.values.yaw +=
                Field_sphericalCamera.delta.distance;
            if (Field_sphericalCamera.values.yaw
                >= Field_sphericalCamera.delta.pitch - 0x78) {
                if (((Input_previousButtons & (PADL1 | PADR1)) == PADL1)
                    && !CAMERA_INPUT_LOCKED() && !Battle_TargetPagesUseShoulders()) {
                    Field_sphericalCamera.delta.pitch =
                        (Field_sphericalCamera.delta.pitch + 0x200) & ~0x1FF;
                }
                if (Field_sphericalCamera.delta.distance > 8) {
                    Field_sphericalCamera.delta.distance -= 8;
                }
            } else if (Field_sphericalCamera.delta.distance < 40) {
                Field_sphericalCamera.delta.distance += 8;
                if (Field_sphericalCamera.delta.distance > 40) {
                    Field_sphericalCamera.delta.distance = 40;
                }
            }
            if (Field_sphericalCamera.values.yaw
                >= Field_sphericalCamera.delta.pitch) {
                Field_sphericalCamera.values.yaw &= 0xE00;
                Field_sphericalCamera.delta.yaw = 0;
            }
            break;
        }
    }

    if (Input_buttonsPressed.allButtons & PADselect) {
        if (!CAMERA_INPUT_LOCKED()) {
            switch (Field_sphericalCamera.delta.mode) {
            case 0:
            case 1:
                Field_sphericalCamera.delta.mode = 2;
                break;
            case 2:
            case 3:
                Field_sphericalCamera.delta.mode = 0;
                break;
            }
        }
        switch (Field_sphericalCamera.delta.mode) {
        case 0:
        case 1:
            Field_sphericalCamera.initialDistance = 0x600;
            break;
        case 2:
        case 3:
            Field_sphericalCamera.initialDistance = 0x900;
            break;
        }
    }

    switch (Field_sphericalCamera.delta.mode) {
    case 0:
        if (Field_sphericalCamera.values.distance <= 0x600) {
            Field_sphericalCamera.values.distance = 0x600;
            Field_sphericalCamera.delta.mode = 1;
            return;
        }
        if (Field_sphericalCamera.values.distance
            <= Field_sphericalCamera.initialDistance + 0xA0) {
            if (Field_sphericalCamera.zoomVelocity < -16) {
                Field_sphericalCamera.zoomVelocity += 16;
            } else {
                Field_sphericalCamera.zoomVelocity = -16;
            }
        } else if (Field_sphericalCamera.zoomVelocity >= -63) {
            Field_sphericalCamera.zoomVelocity -= 16;
            if (Field_sphericalCamera.zoomVelocity < -64) {
                Field_sphericalCamera.zoomVelocity = -64;
            }
        }
        if (Field_sphericalCamera.values.distance
            > Field_sphericalCamera.initialDistance) {
            Field_sphericalCamera.values.distance +=
                Field_sphericalCamera.zoomVelocity;
        }
        break;

    case 1:
        break;

    case 2:
        if (Field_sphericalCamera.values.distance >= 0x900) {
            Field_sphericalCamera.values.distance = 0x900;
            Field_sphericalCamera.delta.mode = 3;
            return;
        }
        if (Field_sphericalCamera.values.distance
            >= Field_sphericalCamera.initialDistance - 0xA0) {
            if (Field_sphericalCamera.zoomVelocity > 16) {
                Field_sphericalCamera.zoomVelocity -= 16;
            } else {
                Field_sphericalCamera.zoomVelocity = 16;
            }
        } else if (Field_sphericalCamera.zoomVelocity < 64) {
            Field_sphericalCamera.zoomVelocity += 16;
            if (Field_sphericalCamera.zoomVelocity > 64) {
                Field_sphericalCamera.zoomVelocity = 64;
            }
        }
        if (Field_sphericalCamera.values.distance
            < Field_sphericalCamera.initialDistance) {
            Field_sphericalCamera.values.distance +=
                Field_sphericalCamera.zoomVelocity;
        }
        break;
    }
}
#undef CAMERA_INPUT_LOCKED

// decomp: BATTLE.PRG func_8006E640 (src/BATTLE/BATTLE.PRG/146C.c:2794)
void Field_TrackActionCameraTarget(int tracked_actor_index)
{
    Actors_ActorLocationQuery tracked_actor_placement;
    SVECTOR targetPos;
    int _[2] __attribute__((unused));

    Actors_ActorPlacement(tracked_actor_index, &tracked_actor_placement);
    targetPos.vx = tracked_actor_placement.location.position.vx;
    targetPos.vz = tracked_actor_placement.location.position.vz;
    targetPos.vy = tracked_actor_placement.location.position.vy - 0x5A;
    Field_UpdateCameraTileConstraint(tracked_actor_index, &tracked_actor_placement.location.packedTile.target);
    switch (Battle_actionFlowMode) {
    case 1:
    case 3:
        Field_UpdateCameraOrbitInput();
        if ((Battle_actionState->cameraPreview != 0xFF) && (Battle_actionState->previewCount != 0)) {
            targetPos.vx = Battle_actionState->previews[Battle_actionState->cameraPreview].position.vx;
            targetPos.vy = Battle_actionState->previews[Battle_actionState->cameraPreview].position.vy;
            targetPos.vz = Battle_actionState->previews[Battle_actionState->cameraPreview].position.vz;
        }
        Field_StepCameraLookAtTowardTarget(&targetPos, 0, 0x28);
        break;
    case 2:
        Field_UpdateCameraOrbitInput();
        targetPos.vx = Battle_actionState->areaCenter.vx;
        targetPos.vy = Battle_actionState->areaCenter.vy;
        targetPos.vz = Battle_actionState->areaCenter.vz;
        if (Battle_SphereInteractionMode() == 1) {
            Field_StepCameraLookAtTowardTarget(&targetPos, 0, 8);
            return;
        }
        Field_StepCameraLookAtTowardTarget(&targetPos, 0, 0x28);
        break;
    case 11:
        Field_UpdateCameraOrbitInput();
        Field_StepCameraLookAtTowardTarget(&targetPos, 1, 0x28);
        break;
    case 4:
        Field_UpdateCameraOrbitInput();
        break;
    }
}

// decomp: BATTLE.PRG func_8006EBF8 (src/BATTLE/BATTLE.PRG/146C.c:2965)
int Field_UpdateAshleyFollowCamera(void)
{
    int v;
    Actors_ActorLocationQuery ashley_placement;
    VECTOR position;

    if (Field_isInteractionPaused == 0) {
        Actors_UpdateRoomActorMovement();
    }
    Actors_ActorPlacement(0, &ashley_placement);

    position.vx = ashley_placement.location.position.vx << 0xC;
    position.vy = (ashley_placement.location.position.vy << 0xC) + 0xFFFA6000;
    position.vz = ashley_placement.location.position.vz << 0xC;
    Field_TrackCameraLookAtWithinDeadZone(&position);
    Field_SetCameraPositionFromAngles(&Field_sphericalCamera.values);
    return v;
}

// decomp: BATTLE.PRG func_8006EC7C (src/BATTLE/BATTLE.PRG/146C.c:2984)
void Field_UpdateFirstPersonCamera(void)
{
    Actors_ActorLocationQuery ashley_placement;
    int pitch_stick_offset;
    int yaw_stick_offset;

    if (Input_previousButtons & PADLup) {
        Field_sphericalCamera.values.pitch =
            (Field_sphericalCamera.values.pitch - 32) % ONE;
    }
    if (Input_previousButtons & PADLdown) {
        Field_sphericalCamera.values.pitch =
            (Field_sphericalCamera.values.pitch + 32) % ONE;
    }
    if (Input_previousButtons & PADLleft) {
        Field_sphericalCamera.values.yaw =
            (Field_sphericalCamera.values.yaw - 32) % ONE;
    }
    if (Input_previousButtons & PADLright) {
        Field_sphericalCamera.values.yaw =
            (Field_sphericalCamera.values.yaw + 32) % ONE;
    }
    if (Input_controllerPorts->controllerType == 7) {
        yaw_stick_offset = Input_controllerPorts->rightStickX - 128;
        pitch_stick_offset = Input_controllerPorts->rightStickY - 128;

        if ((Input_controllerPorts->rightStickX - 64u) > 128) {
            if (yaw_stick_offset <= 0) {
                yaw_stick_offset = Input_controllerPorts->rightStickX - 64;
            } else {
                yaw_stick_offset = Input_controllerPorts->rightStickX - 192;
            }
            Field_sphericalCamera.values.yaw =
                (Field_sphericalCamera.values.yaw + (yaw_stick_offset / 3)) % ONE;
        }
        if ((pitch_stick_offset + 64) > 128u) {
            if (pitch_stick_offset <= 0) {
                pitch_stick_offset += 64;
            } else {
                pitch_stick_offset -= 64;
            }
            Field_sphericalCamera.values.pitch =
                (Field_sphericalCamera.values.pitch + (pitch_stick_offset / 3)) % ONE;
        }
    }

    if (Field_sphericalCamera.values.pitch > 896) {
        Field_sphericalCamera.values.pitch = 896;
    } else if (Field_sphericalCamera.values.pitch < -896) {
        Field_sphericalCamera.values.pitch = -896;
    }

    Actors_TurnActorToOverTime(0, Field_sphericalCamera.values.yaw & 0xFFF, 0);
    Actors_ActorPlacement(0, &ashley_placement);
    Render_frameScratch.camera.position.vx = ashley_placement.location.position.vx * ONE;
    Render_frameScratch.camera.position.vz = ashley_placement.location.position.vz * ONE;
    Render_frameScratch.camera.position.vy = (ashley_placement.location.position.vy - 180) * ONE;
    Field_SetCameraLookAtFromAngles(&Field_sphericalCamera.values);
}

// decomp: BATTLE.PRG func_8006EF10 (src/BATTLE/BATTLE.PRG/146C.c:3044)
void Field_StorePreferredActionCameraDistance(void)
{
    switch (Field_sphericalCamera.delta.mode) {
    case 0:
    case 1:
        Battle_actionState->preferredCameraDistance = 0x600;
        break;
    case 2:
    case 3:
        Battle_actionState->preferredCameraDistance = 0x900;
        break;
    }
}

// decomp: BATTLE.PRG _computeCameraTransition (src/BATTLE/BATTLE.PRG/146C.c:3058)
void Field_ComputeFirstPersonCameraTransition(VECTOR* targetPosition)
{
    VECTOR cameraToTarget;

    cameraToTarget.vx = (targetPosition->vx - Render_frameScratch.camera.position.vx) / ONE;
    cameraToTarget.vz = (targetPosition->vz - Render_frameScratch.camera.position.vz) / ONE;
    cameraToTarget.vy = (targetPosition->vy - Render_frameScratch.camera.position.vy) / ONE;

    Core_Gte_VectorNormal(&cameraToTarget, &Field_cameraTransition->direction);

    Field_cameraTransition->transitionSpeed = 0x80;

    copyVector(&Field_cameraTransition->targetPosition, targetPosition);

    cameraToTarget.vy = (targetPosition->vy - Render_frameScratch.camera.position.vy);

    Field_cameraTransition->transitionFrames =
        cameraToTarget.vy
        / (Field_cameraTransition->direction.vy
            * Field_cameraTransition->transitionSpeed);

    Field_cameraTransition->targetLookAt.vx =
        -Core_Gte_Sin(Field_cameraTransition->toSpherical.values.yaw);
    Field_cameraTransition->targetLookAt.vz =
        -Core_Gte_Cos(Field_cameraTransition->toSpherical.values.yaw);
    Field_cameraTransition->targetLookAt.vy =
        (-Core_Gte_Sin(Field_cameraTransition->toSpherical.values.pitch) * ONE)
        / Core_Gte_Cos(Field_cameraTransition->toSpherical.values.pitch);

    Core_Gte_VectorNormal(&Field_cameraTransition->targetLookAt,
        &Field_cameraTransition->targetLookAt);

    Field_cameraTransition->targetLookAt.vx =
        (Field_cameraTransition->targetLookAt.vx * 0x900)
        + Field_cameraTransition->targetPosition.vx;
    Field_cameraTransition->targetLookAt.vz =
        (Field_cameraTransition->targetLookAt.vz * 0x900)
        + Field_cameraTransition->targetPosition.vz;
    Field_cameraTransition->targetLookAt.vy =
        (Field_cameraTransition->targetLookAt.vy * 0x900)
        + Field_cameraTransition->targetPosition.vy;

    Field_cameraTransition->lookAtStep.vx =
        (Field_cameraTransition->targetLookAt.vx - Render_frameScratch.camera.lookAt.vx)
        / Field_cameraTransition->transitionFrames;
    Field_cameraTransition->lookAtStep.vz =
        (Field_cameraTransition->targetLookAt.vz - Render_frameScratch.camera.lookAt.vz)
        / Field_cameraTransition->transitionFrames;
    Field_cameraTransition->lookAtStep.vy =
        (Field_cameraTransition->targetLookAt.vy - Render_frameScratch.camera.lookAt.vy)
        / Field_cameraTransition->transitionFrames;

    Field_cameraTransition->farClipStep =
        (Field_cameraTransition->toFarClip - Render_frameScratch.camera.farClip)
        / Field_cameraTransition->transitionFrames;
    Field_cameraTransition->nearClipStep =
        (Field_cameraTransition->toNearClip - Render_nearClip)
        / Field_cameraTransition->transitionFrames;
    Field_cameraTransition->projectionDistanceStep =
        (Field_cameraTransition->toProjectionDistance - Render_projectionDistance)
        / Field_cameraTransition->transitionFrames;
}

// decomp: BATTLE.PRG _stepCameraTransition (src/BATTLE/BATTLE.PRG/146C.c:3121)
int Field_StepFirstPersonCameraTransition(void)
{
    int targetY;
    int nextY;
    int currentY;
    int _[4] __attribute__((unused));

    currentY = Render_frameScratch.camera.position.vy & ~0xFFF;
    targetY = Field_cameraTransition->targetPosition.vy & ~0xFFF;
    nextY = (Render_frameScratch.camera.position.vy
                + (Field_cameraTransition->direction.vy
                    * Field_cameraTransition->transitionSpeed))
          & ~0xFFF;

    if (((currentY >= targetY) && (targetY >= nextY))
        || ((targetY >= currentY) && (nextY >= targetY))) {

        Render_frameScratch.camera.position.vx = Field_cameraTransition->targetPosition.vx;
        Render_frameScratch.camera.position.vz = Field_cameraTransition->targetPosition.vz;
        Render_frameScratch.camera.position.vy = Field_cameraTransition->targetPosition.vy;
        Render_frameScratch.camera.lookAt.vx = Field_cameraTransition->targetLookAt.vx;
        Render_frameScratch.camera.lookAt.vz = Field_cameraTransition->targetLookAt.vz;
        Render_frameScratch.camera.lookAt.vy = Field_cameraTransition->targetLookAt.vy;

        Field_sphericalCamera = Field_cameraTransition->toSpherical;

        Render_frameScratch.camera.farClip = Field_cameraTransition->toFarClip;

        Render_SetCameraNearClip(Field_cameraTransition->toNearClip);
        Render_SetCameraProjectionDistance(Field_cameraTransition->toProjectionDistance);
        return 1;
    }
    Render_frameScratch.camera.position.vx += Field_cameraTransition->direction.vx
                                   * Field_cameraTransition->transitionSpeed;
    Render_frameScratch.camera.position.vz += Field_cameraTransition->direction.vz
                                   * Field_cameraTransition->transitionSpeed;
    Render_frameScratch.camera.position.vy += Field_cameraTransition->direction.vy
                                   * Field_cameraTransition->transitionSpeed;
    Render_frameScratch.camera.lookAt.vx += Field_cameraTransition->lookAtStep.vx;
    Render_frameScratch.camera.lookAt.vz += Field_cameraTransition->lookAtStep.vz;
    Render_frameScratch.camera.lookAt.vy += Field_cameraTransition->lookAtStep.vy;
    Render_frameScratch.camera.farClip += Field_cameraTransition->farClipStep;

    Render_SetCameraNearClip(Render_nearClip + Field_cameraTransition->nearClipStep);
    Render_SetCameraProjectionDistance(
        Render_projectionDistance + Field_cameraTransition->projectionDistanceStep);

    return 0;
}

// decomp: BATTLE.PRG _initBattleCameraTransition (src/BATTLE/BATTLE.PRG/146C.c:4544)
void Field_BeginFirstPersonView(void)
{
    VECTOR eye_position_fixed12;
    Actors_ActorLocationQuery ashley_placement;
    int i;

    if (Field_cameraTransition == NULL) {
        Field_cameraTransition =
            Core_Memory_AllocateFromHighEnd(sizeof *Field_cameraTransition);
    }
    if (Field_controlMode != 7) {
        Field_cameraTransition->fromCameraState = Render_frameScratch.camera;
        Field_cameraTransition->fromSpherical = Field_sphericalCamera;
        Field_cameraTransition->fromNearClip = Render_nearClip;
        Field_cameraTransition->fromProjectionDistance = Render_projectionDistance;
        Field_cameraTransition->fromFarClip = Render_frameScratch.camera.farClip;
    }
    Field_cameraTransition->toNearClip = 4;
    Field_cameraTransition->toProjectionDistance = 224;
    Field_cameraTransition->toFarClip = 0x2000;
    Actors_ActorPlacement(0, &ashley_placement);

    Field_cameraTransition->toSpherical.values.yaw = ashley_placement.location.position.pad;
    Field_cameraTransition->toSpherical.values.pitch = 0;
    Field_cameraTransition->toSpherical.values.distance =
        Field_sphericalCamera.values.distance;

    setVector(&eye_position_fixed12, ashley_placement.location.position.vx * ONE, (ashley_placement.location.position.vy - 0xB4) * ONE,
        ashley_placement.location.position.vz * ONE);

    Field_ComputeFirstPersonCameraTransition(&eye_position_fixed12);

    Field_controlMode = 7;
    Battle_actionFlowMode = 0;

    Battle_SetHudHidden(1);

    for (i = 0; i < 16; ++i) {
        Actors_RestoreActorIdlePose(i);
    }

    Field_SetMinimapVisible(0);
    Field_SetMovingPlatformsPaused(1);
    Battle_SetActorMarkersHidden(1);
    Actors_SetActorTransitionMode(1);
}

// decomp: BATTLE.PRG _endBattleCameraTransition (src/BATTLE/BATTLE.PRG/146C.c:4591)
void Field_EndFirstPersonView(void)
{
    Render_frameScratch.camera = Field_cameraTransition->fromCameraState;

    Field_sphericalCamera = Field_cameraTransition->fromSpherical;

    Field_sphericalCamera.delta.pitch =
        Field_sphericalCamera.values.yaw;
    Render_SetCameraNearClip(Field_cameraTransition->fromNearClip);
    Render_SetCameraProjectionDistance(Field_cameraTransition->fromProjectionDistance);
    if (Field_cameraTransition != NULL) {
        Core_Memory_FreeFromHighEnd(Field_cameraTransition);
        Field_cameraTransition = NULL;
    }
    Battle_SetHudHidden(0);
    Field_SetMinimapVisible(1);
    Field_SetMovingPlatformsPaused(0);
    Battle_SetActorMarkersHidden(0);
    Actors_SetActorTransitionMode(0);
    Field_EnterFieldExplorationMode();
}

// decomp: BATTLE.PRG func_80074580 (src/BATTLE/BATTLE.PRG/146C.c:5165)
void Field_UpdateFirstPersonViewEntry(void)
{
    if (Render_projectionDistance < 272) {
        Actors_SetActorDisplayFlags(0, Actors_ActorStateFlags(0) & ~2);
    }

    if (Field_StepFirstPersonCameraTransition() != 0) {
        Battle_actionFlowMode = 1;
        Field_isFirstPersonViewEnabled = 1;
        Field_roomGeometryDrawMode = 1;
        Field_ClearGeometryOcclusion();
    }
}

// decomp: BATTLE.PRG _checkFirstPersonViewExit (src/BATTLE/BATTLE.PRG/146C.c:5179)
void Field_CheckFirstPersonViewExit(void)
{
    Actors_ActorLocationQuery ashley_placement;

    if (!(Input_buttonsPressed.allButtons & (PADRdown | PADj | PADstart))) {
        return;
    }

    Battle_actionFlowMode = 2;
    Field_isFirstPersonViewEnabled = 0;
    Field_roomGeometryDrawMode = 0;

    Actors_ActorPlacement(0, &ashley_placement);
    Field_UpdateCameraTileConstraint(0, &ashley_placement.location.packedTile.target);
    Field_SyncCameraAnglesFromPosition(&Field_cameraTransition->toSpherical);

    if ((Field_cameraTransition->toSpherical.values.yaw
            - (Field_cameraTransition->toSpherical.values.yaw
                - (Field_cameraTransition->toSpherical.values.yaw % 512)))
        > 0x100) {
        Field_cameraTransition->toSpherical.values.yaw =
            (Field_cameraTransition->toSpherical.values.yaw & ~0x1FF) + 0x200;
    } else {
        Field_cameraTransition->toSpherical.values.yaw =
            Field_cameraTransition->toSpherical.values.yaw & ~0x1FF;
    }

    Field_cameraTransition->fromSpherical.delta.yaw = 0;
    Field_cameraTransition->fromSpherical.values.yaw =
        Field_cameraTransition->toSpherical.values.yaw;

    Field_cameraTransition->toSpherical = Field_cameraTransition->fromSpherical;

    Field_cameraTransition->toNearClip = Field_cameraTransition->fromNearClip;
    Field_cameraTransition->toProjectionDistance =
        Field_cameraTransition->fromProjectionDistance;
    Field_cameraTransition->toFarClip = Field_cameraTransition->fromFarClip;
    Field_ComputeSphericalCameraPosition(&Field_cameraTransition->fromCameraState.position,
        &Field_cameraTransition->fromCameraState.lookAt,
        &Field_cameraTransition->toSpherical.values);
    Field_ComputeFirstPersonCameraTransition(&Field_cameraTransition->fromCameraState.position);
}

// decomp: BATTLE.PRG func_80074744 (src/BATTLE/BATTLE.PRG/146C.c:5222)
void Field_UpdateFirstPersonViewExit(void)
{
    if (Render_projectionDistance > 272) {
        Actors_SetActorDisplayFlags(0, Actors_ActorStateFlags(0) | 2);
    }
    if (Field_StepFirstPersonCameraTransition() != 0) {
        Field_EndFirstPersonView();
    }
}

// decomp: BATTLE.PRG func_80074B14 (src/BATTLE/BATTLE.PRG/146C.c:5339)
void Field_UpdateCameraTileConstraint(int actor_index, Actors_TileTarget* actor_tile)
{
    u_int platform_layer_flags;
    int collision_layer;
    char collision_tile_x;
    char collision_tile_z;

    platform_layer_flags = Actors_ActorStateFlags(actor_index) & 0x70000;
    collision_tile_x = 0;
    if (platform_layer_flags != 0) {
        collision_tile_z = 0;
        collision_layer = platform_layer_flags >> 0x10;
    } else {
        collision_tile_x = actor_tile->tileX;
        collision_tile_z = actor_tile->tileZ;
        collision_layer = actor_tile->tileLayer;
    }
    if (Field_CollisionCellAt(collision_tile_x, collision_tile_z, collision_layer)->occlusionAndFloorFlags & 1) {
        Field_ApplyCameraGeometryOcclusion(actor_tile->tileX, actor_tile->tileZ, actor_tile->tileLayer,
            Field_sphericalCamera.initialDistance == 0x900);
        return;
    }
    Field_ClearGeometryOcclusion();
}

// decomp: BATTLE.PRG _isLookAtAtDestination (src/BATTLE/BATTLE.PRG/146C.c:6788)
int Field_IsCameraLookAtOnAshley(void)
{
    Field_CameraPositionPair target_and_current_look_at;
    Actors_ActorLocationQuery ashley_placement;

    Actors_ActorPlacement(0, &ashley_placement);
    target_and_current_look_at.position.vx = ashley_placement.location.position.vx * ONE;
    target_and_current_look_at.position.vy = ashley_placement.location.position.vy * ONE + 0xFFFA6000;
    target_and_current_look_at.position.vz = ashley_placement.location.position.vz * ONE;
    if (target_and_current_look_at.position.vy > 0) {
        target_and_current_look_at.position.vy = 0;
    }
    Field_ClampCameraLookAtToZone(&target_and_current_look_at.position, &target_and_current_look_at.position);
    target_and_current_look_at.lookAt = Render_frameScratch.camera.lookAt;

    if ((target_and_current_look_at.position.vx == (target_and_current_look_at.lookAt.vx & ~0xFFF))
        && (target_and_current_look_at.position.vz == (target_and_current_look_at.lookAt.vz & ~0xFFF))
        && (target_and_current_look_at.position.vy == (target_and_current_look_at.lookAt.vy & ~0xFFF))) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG func_80077EC4 (src/BATTLE/BATTLE.PRG/146C.c:6811)
void Field_ResetCameraToAshley(void)
{
    Field_InitializeSphericalCamera(
        &Render_frameScratch.camera.position, &Render_frameScratch.camera.lookAt, -1, -1);
    Render_frameScratch.camera.farClip = 0x1000;
    Render_frameScratch.camera.angles.vz = 0;
    Render_frameScratch.camera.angles.vy = 0;
    Render_frameScratch.camera.angles.vx = 0;
}

// decomp: BATTLE.PRG vs_battle_syncCameraAnglesFromPosition (src/BATTLE/BATTLE.PRG/146C.c:8057)
int Field_SyncCameraAnglesFromPosition(Field_SphericalCamera* output_spherical_camera)
{
    VECTOR toCamera;
    VECTOR toCameraNorm;
    int yaw;
    int pitch;

    toCamera.vx = (Render_frameScratch.camera.position.vx - Render_frameScratch.camera.lookAt.vx) / ONE;
    toCamera.vz = (Render_frameScratch.camera.position.vz - Render_frameScratch.camera.lookAt.vz) / ONE;
    toCamera.vy = (Render_frameScratch.camera.position.vy - Render_frameScratch.camera.lookAt.vy) / ONE;

    Core_Gte_VectorNormal(&toCamera, &toCameraNorm);

    yaw = Core_Gte_Atan2(toCameraNorm.vx, toCameraNorm.vz);
    yaw = (yaw + ONE) % ONE;
    Render_frameScratch.camera.angles.vy = yaw;
    pitch = Core_Gte_Atan2(
        toCameraNorm.vy, Core_Gte_SquareRoot12((toCameraNorm.vx * toCameraNorm.vx) / ONE
                                      + (toCameraNorm.vz * toCameraNorm.vz) / ONE));
    Render_frameScratch.camera.angles.vx = pitch;

    if (output_spherical_camera != NULL) {
        output_spherical_camera->values.yaw = yaw;
        output_spherical_camera->values.pitch = pitch;
        if (toCamera.vx != 0) {
            output_spherical_camera->values.distance = (toCamera.vx * ONE) / toCameraNorm.vx;
        } else if (toCamera.vz != 0) {
            output_spherical_camera->values.distance = (toCamera.vz * ONE) / toCameraNorm.vz;
        } else {
            output_spherical_camera->values.distance = ONE / 4;
        }
    }
    return yaw;
}

// decomp: BATTLE.PRG _computeSphericalOffset (src/BATTLE/BATTLE.PRG/146C.c:8092)
void Field_ComputeSphericalCameraPosition(
    VECTOR* outPosition, VECTOR* basePosition, Field_SphericalCameraValues* angles)
{
    outPosition->vx = Core_Gte_Sin(angles->yaw);
    outPosition->vz = Core_Gte_Cos(angles->yaw);
    outPosition->vy = (Core_Gte_Sin(angles->pitch) * ONE) / Core_Gte_Cos(angles->pitch);
    Core_Gte_VectorNormal(outPosition, outPosition);
    outPosition->vx = (outPosition->vx * angles->distance) + basePosition->vx;
    outPosition->vz = (outPosition->vz * angles->distance) + basePosition->vz;
    outPosition->vy = (outPosition->vy * angles->distance) + basePosition->vy;
}

// decomp: BATTLE.PRG _setCameraPositionFromAngles (src/BATTLE/BATTLE.PRG/146C.c:8104)
void Field_SetCameraPositionFromAngles(Field_SphericalCameraValues* spherical)
{
    Field_ComputeSphericalCameraPosition(
        &Render_frameScratch.camera.position, &Render_frameScratch.camera.lookAt, spherical);
}

// decomp: BATTLE.PRG _setCameraLookAtFromAngles (src/BATTLE/BATTLE.PRG/146C.c:8110)
void Field_SetCameraLookAtFromAngles(Field_SphericalCameraValues* spherical)
{
    Render_frameScratch.camera.lookAt.vx = -Core_Gte_Sin(spherical->yaw);
    Render_frameScratch.camera.lookAt.vz = -Core_Gte_Cos(spherical->yaw);
    Render_frameScratch.camera.lookAt.vy =
        (-Core_Gte_Sin(spherical->pitch) * 0x1000) / Core_Gte_Cos(spherical->pitch);
    Core_Gte_VectorNormal(&Render_frameScratch.camera.lookAt, &Render_frameScratch.camera.lookAt);
    Render_frameScratch.camera.lookAt.vx = (Render_frameScratch.camera.lookAt.vx * spherical->distance)
                                + Render_frameScratch.camera.position.vx;
    Render_frameScratch.camera.lookAt.vz = (Render_frameScratch.camera.lookAt.vz * spherical->distance)
                                + Render_frameScratch.camera.position.vz;
    Render_frameScratch.camera.lookAt.vy = (Render_frameScratch.camera.lookAt.vy * spherical->distance)
                                + Render_frameScratch.camera.position.vy;
}

// decomp: BATTLE.PRG vs_battle_getCameraPosition (src/BATTLE/BATTLE.PRG/146C.c:8125)
void Field_CameraPosition(VECTOR* outPosition)
{
    outPosition->vx = Render_frameScratch.camera.position.vx;
    outPosition->vy = Render_frameScratch.camera.position.vy;
    outPosition->vz = Render_frameScratch.camera.position.vz;
}

// decomp: BATTLE.PRG vs_battle_setCameraPosition (src/BATTLE/BATTLE.PRG/146C.c:8132)
void Field_SetCameraPosition(VECTOR* inPosition)
{
    Render_frameScratch.camera.position.vx = inPosition->vx;
    Render_frameScratch.camera.position.vy = inPosition->vy;
    Render_frameScratch.camera.position.vz = inPosition->vz;
}

// decomp: BATTLE.PRG vs_battle_getCameraLookAt (src/BATTLE/BATTLE.PRG/146C.c:8139)
void Field_CameraLookAt(VECTOR* outLookAt)
{
    outLookAt->vx = Render_frameScratch.camera.lookAt.vx;
    outLookAt->vy = Render_frameScratch.camera.lookAt.vy;
    outLookAt->vz = Render_frameScratch.camera.lookAt.vz;
}

// decomp: BATTLE.PRG vs_battle_setCameraLookAt (src/BATTLE/BATTLE.PRG/146C.c:8146)
void Field_SetCameraLookAt(VECTOR* inLookAt)
{
    Render_frameScratch.camera.lookAt.vx = inLookAt->vx;
    Render_frameScratch.camera.lookAt.vy = inLookAt->vy;
    Render_frameScratch.camera.lookAt.vz = inLookAt->vz;
}

// decomp: BATTLE.PRG vs_battle_setCameraRoll (src/BATTLE/BATTLE.PRG/146C.c:8153)
void Field_SetCameraRoll(int roll_angle) { Render_frameScratch.camera.angles.vz = roll_angle; }

// decomp: BATTLE.PRG vs_battle_getCameraRoll (src/BATTLE/BATTLE.PRG/146C.c:8155)
int Field_CameraRoll(void) { return Render_frameScratch.camera.angles.vz & 0xFFF; }

// decomp: BATTLE.PRG func_8007BDD8 (src/BATTLE/BATTLE.PRG/146C.c:8573)
int Field_SetFirstPersonViewDisabled(int disabled)
{
    Field_ClearGeometryOcclusion();
    return Field_isFirstPersonViewEnabled = ~disabled & 1;
}

// decomp: BATTLE.PRG vs_battle_initialiseCameraFromSpherical (src/BATTLE/BATTLE.PRG/146C.c:9071)
void Field_InitializeSphericalCamera(
    VECTOR* outputPos, VECTOR* outputLookAt, int facing, int mode)
{
    Actors_ActorLocationQuery ashley_placement;
    int temp_v0;
    int facing_division_numerator;

    if (facing != -1) {
        facing_division_numerator = facing;
        temp_v0 = Field_sphericalCamera.values.yaw;
        if (facing < 0) {
            facing_division_numerator = facing + 7;
        }
        Field_sphericalCamera.delta.pitch = (facing - ((facing_division_numerator >> 3) * 8))
                                                    << 9;
        Field_sphericalCamera.values.yaw = (facing - ((facing_division_numerator >> 3) * 8)) << 9;
    }
    if (mode == 1) {
        Field_sphericalCamera.initialDistance = 0x600;
        Field_sphericalCamera.values.distance = 0x600;
        Field_sphericalCamera.delta.mode = 0;
    } else if (mode == 2) {
        Field_sphericalCamera.initialDistance = 0x900;
        Field_sphericalCamera.values.distance = 0x900;
        Field_sphericalCamera.delta.mode = mode;
    }
    Actors_ActorPlacement(0, &ashley_placement);
    outputLookAt->vx = ashley_placement.location.position.vx << 0xC;
    outputLookAt->vy = (ashley_placement.location.position.vy << 0xC) + 0xFFFA6000;
    outputLookAt->vz = ashley_placement.location.position.vz << 0xC;
    Field_ClampCameraLookAtToZone(outputLookAt, outputLookAt);
    Field_ComputeSphericalCameraPosition(
        outputPos, outputLookAt, &Field_sphericalCamera.values);
}

// decomp: BATTLE.PRG func_8007CE74 (src/BATTLE/BATTLE.PRG/146C.c:9106)
void Field_SetCameraPitchPreset(int requested_pitch)
{
    if ((requested_pitch == -0x300) || (requested_pitch == -0x80)) {
        Field_sphericalCamera.values.pitch = requested_pitch;
        return;
    }
    Field_sphericalCamera.values.pitch = -0x180;
}

