// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/ActorSound.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionFlow.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#include "Core/Core/WordPair.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Runtime.h"
#include "Effects/ActionEffects.h"
#include "Field/Camera.h"
#include "Field/Collision.h"
#include "Field/Doors.h"
#include "Field/Gimmicks.h"
#include "Field/Interactions.h"
#include "Field/Mpd.h"
#include "Field/Platforms.h"
#include "Field/Transitions.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Input/Controllers.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: D_800E90AC (D_800E90AC@BATTLE/BATTLE.PRG)
u_char Actors_kJumpHangFrames[3204]; /* data: contents loaded in Step 5 */

// decomp: D_800E90B4 (D_800E90B4@BATTLE/BATTLE.PRG)
u_char Actors_kJumpHeightAdjustments[3196]; /* data: contents loaded in Step 5 */

// decomp: D_800E90BC (D_800E90BC@BATTLE/BATTLE.PRG)
int Actors_motionResultResetWord; /* data: contents loaded in Step 5 */

// decomp: D_800E90C3 (D_800E90C3@BATTLE/BATTLE.PRG)
u_char Actors_kMotionResultAnimations[3181]; /* data: contents loaded in Step 5 */

// decomp: D_800E90C4 (D_800E90C4@BATTLE/BATTLE.PRG)
u_char Actors_kMotionResultOneAnimationId; /* data: contents loaded in Step 5 */

// decomp: D_800E90C6 (D_800E90C6@BATTLE/BATTLE.PRG)
u_char Actors_kHangMotionAnimationId; /* data: contents loaded in Step 5 */

// decomp: D_800E90CC (D_800E90CC@BATTLE/BATTLE.PRG)
u_char Actors_kSlowQuadrantMoveAnimations[3172]; /* data: contents loaded in Step 5 */

// decomp: D_800E90D0 (D_800E90D0@BATTLE/BATTLE.PRG)
u_char Actors_kFastQuadrantMoveAnimations[3168]; /* data: contents loaded in Step 5 */

// decomp: D_800E92E8 (D_800E92E8@BATTLE/BATTLE.PRG)
u_char Actors_kAirTimerEndAnimations[2632]; /* data: contents loaded in Step 5 */

// decomp: D_800E92F0 (D_800E92F0@BATTLE/BATTLE.PRG)
u_char Actors_kSoftLandingAnimations[2624]; /* data: contents loaded in Step 5 */

// decomp: D_800E92F8 (D_800E92F8@BATTLE/BATTLE.PRG)
u_char Actors_kHardLandingAnimations[2616]; /* data: contents loaded in Step 5 */

// decomp: D_800E9300 (D_800E9300@BATTLE/BATTLE.PRG)
u_char Actors_kLandingRecoveryAnimations[2608]; /* data: contents loaded in Step 5 */

// decomp: D_800F1A50 (D_800F1A50@BATTLE/BATTLE.PRG)
int Actors_ashleyMoveInput[12];

// decomp: D_800F4538 (D_800F4538@BATTLE/BATTLE.PRG)
Actors_ActorRecord* Actors_actorRecords[17];

// decomp: D_800F49F4 (D_800F49F4@BATTLE/BATTLE.PRG)
u_short Actors_supportingPlatformLayer;

// decomp: D_800F49F8 (D_800F49F8@BATTLE/BATTLE.PRG)
u_char Actors_supportingGimmickIndex;

// decomp: D_800F4B18 (D_800F4B18@BATTLE/BATTLE.PRG)
char Actors_isActorUpdateComplete;

// decomp: D_800F4B19 (D_800F4B19@BATTLE/BATTLE.PRG)
u_char Actors_pushedGimmickIndex;

// decomp: BATTLE.PRG func_8006E7F0 (src/BATTLE/BATTLE.PRG/146C.c:2837)
int Actors_UpdateAshleyMovementInput(void)
{
    Actors_PlacementBuffer unksp10;
    int temp_a0;
    int temp_a1;
    int temp_s1;
    Battle_ActorStats* temp_s0;

    Actors_ActorPlacement(0, &unksp10);
    Field_UpdateCameraTileConstraint(0, (Actors_TileTarget*)&unksp10);
    temp_s0 = Battle_characterState->stats;
    temp_s1 = Actors_ActorStateFlags(0);

    if (((Field_screenTransitionStep - 1) < 2U)
        || (Field_screenTransitionStep == 4)) {
        if (Field_pendingDestinationDoor.reserved06Bit15) {
            Actors_SetDirectMovement(0, Field_pendingDestinationDoor.entryElevation << 10, temp_s0->walkSpeed * ONE / 2);
        } else {
            Actors_SetDirectMovement(0, 0, 0);
        }
    } else {

        Field_UpdateCameraOrbitInput();

        Actors_ashleyMoveInput[3] = 0;
        Actors_ashleyMoveInput[1] = 0;

        if (Input_controllerPorts->controllerType == 7) {

            temp_a1 = Input_controllerPorts->rightStickX - ONE / 32;
            temp_a0 = Input_controllerPorts->rightStickY - ONE / 32;

            if (((Input_controllerPorts->rightStickX - ONE / 64) > ONE / 32u) || (temp_a0 < -ONE / 64)
                || (temp_a0 > ONE / 64)) {

                if (temp_a1 > ONE / 64) {
                    temp_a1 -= ONE / 64;
                } else if (temp_a1 < -ONE / 64) {
                    temp_a1 += ONE / 64;
                } else {
                    temp_a1 = 0;
                }

                if (temp_a0 > ONE / 64) {
                    temp_a0 = temp_a0 - ONE / 64;
                } else if (temp_a0 < -ONE / 64) {
                    temp_a0 = temp_a0 + ONE / 64;
                } else {
                    temp_a0 = 0;
                }
                Actors_ashleyMoveInput[3] = 0;
                Actors_ashleyMoveInput[1] = -temp_a1;
                Actors_ashleyMoveInput[3] = temp_a0;
            }
        }

        if ((Actors_ashleyMoveInput[1] == 0) && (Actors_ashleyMoveInput[3] == 0)) {
            if (Input_previousButtons & PADLdown) {
                Actors_ashleyMoveInput[1] = 0;
                Actors_ashleyMoveInput[3] = ONE / 256;
            }
            if (Input_previousButtons & PADLup) {
                Actors_ashleyMoveInput[1] += 0;
                Actors_ashleyMoveInput[3] -= ONE / 256;
            }
            if (Input_previousButtons & PADLleft) {
                Actors_ashleyMoveInput[3] += 0;
                Actors_ashleyMoveInput[1] += ONE / 256;
            }
            if (Input_previousButtons & PADLright) {
                Actors_ashleyMoveInput[3] += 0;
                Actors_ashleyMoveInput[1] -= ONE / 256;
            }
        }

        if (Field_sphericalCamera.values.distance > ONE / 16 * 9) {
            Field_sphericalCamera.values.distance = ONE / 16 * 9;
        } else if (Field_sphericalCamera.values.distance < ONE / 16 * 6) {
            Field_sphericalCamera.values.distance = ONE / 16 * 6;
        }

        if ((Actors_ashleyMoveInput[1] != 0) || (Actors_ashleyMoveInput[3] != 0)) {
            if (temp_s1 & 0x80000) {
                Actors_SetDirectMovement(0,
                    (Core_Gte_Atan2(Actors_ashleyMoveInput[1], Actors_ashleyMoveInput[3])
                        + Field_sphericalCamera.values.yaw + ONE / 2)
                        & 0xFFF,
                    temp_s0->walkSpeed * ONE);
            } else if (Input_previousButtons & PADR2) {
                if (Input_buttonsPressed.allButtons & PADRleft) {
                    Actors_BeginActorJumpMovement(0,
                        (Core_Gte_Atan2(Actors_ashleyMoveInput[1], Actors_ashleyMoveInput[3])
                            + Field_sphericalCamera.values.yaw + ONE / 2)
                            & 0xFFF,
                        temp_s0->walkSpeed * ONE);
                } else {
                    Actors_SetDirectMovement(0,
                        (Core_Gte_Atan2(Actors_ashleyMoveInput[1], Actors_ashleyMoveInput[3])
                            + Field_sphericalCamera.values.yaw + ONE / 2)
                            & 0xFFF,
                        temp_s0->walkSpeed * ONE);
                }
            } else if (Input_buttonsPressed.allButtons & PADRleft) {
                Actors_BeginActorJumpMovement(0,
                    (Core_Gte_Atan2(Actors_ashleyMoveInput[1], Actors_ashleyMoveInput[3])
                        + Field_sphericalCamera.values.yaw + ONE / 2)
                        & 0xFFF,
                    temp_s0->runSpeed * ONE);
            } else {
                Actors_SetDirectMovement(0,
                    (Core_Gte_Atan2(Actors_ashleyMoveInput[1], Actors_ashleyMoveInput[3])
                        + Field_sphericalCamera.values.yaw + ONE / 2)
                        & 0xFFF,
                    temp_s0->runSpeed * ONE);
            }
        } else if (!(temp_s1 & 0x80000) && (Input_buttonsPressed.allButtons & PADRleft)) {
            Actors_BeginActorJumpMovement(0,
                (Core_Gte_Atan2(0, 0) + Field_sphericalCamera.values.yaw + ONE / 2)
                    & 0xFFF,
                0);
        } else {
            Actors_SetDirectMovement(0, Core_Gte_Atan2(0, 0) & 0xFFF, 0);
        }
    }

    return 0;
}

// decomp: BATTLE.PRG func_800753F8 (src/BATTLE/BATTLE.PRG/146C.c:5577)
void Actors_ConsumeActorMovementOffset(Battle_Actor* arg0, SVECTOR* arg1)
{
    if (arg0->commandArg0 < 0) {
        if (arg0->commandArg0 >= -0x7FFF) {
            arg1->vx = arg0->commandArg0 / ONE;
            arg0->commandArg0 = 0;
        } else {
            arg1->vx = -8;
            arg0->commandArg0 += 0x8000;
        }
    } else if (arg0->commandArg0 <= 0x7FFF) {
        arg1->vx = arg0->commandArg0 / ONE;
        arg0->commandArg0 = 0;
    } else {
        arg1->vx = 8;
        arg0->commandArg0 -= 0x8000;
    }

    if (arg0->commandArg1.word < 0) {
        if (arg0->commandArg1.word >= -0x7FFF) {
            arg1->vz = arg0->commandArg1.word / ONE;
            arg0->commandArg1.word = 0;
        } else {
            arg1->vz = -8;
            arg0->commandArg1.word += 0x8000;
        }
    } else if (arg0->commandArg1.word <= 0x7FFF) {
        arg1->vz = arg0->commandArg1.word / ONE;
        arg0->commandArg1.word = 0;
    } else {
        arg1->vz = 8;
        arg0->commandArg1.word -= 0x8000;
    }

    if (arg0->commandArg2 < 0) {
        if (arg0->commandArg2 >= -0x7FFF) {
            arg1->vy = arg0->commandArg2 / ONE;
            arg0->commandArg2 = 0;
        } else {
            arg1->vy = -8;
            arg0->commandArg2 += 0x8000;
        }
    } else if (arg0->commandArg2 <= 0x7FFF) {
        arg1->vy = arg0->commandArg2 / ONE;
        arg0->commandArg2 = 0;
    } else {
        arg1->vy = 8;
        arg0->commandArg2 -= 0x8000;
    }
}

// decomp: BATTLE.PRG func_80075554 (src/BATTLE/BATTLE.PRG/146C.c:5629)
int Actors_ExecuteQueuedActorCommands(void)
{
    SVECTOR step;
    Actors_ActorLocation position;
    Actors_ActorLocation target;
    Battle_Actor* actor;
    int actorId;
    int state;
    int movementSpeed;
    int angle;
    int result;
    u_short queuedFrame;
    u_int actorFlags;

    for (actorId = 0; actorId < 16; ++actorId) {
        actor = Battle_actors[actorId];
        if (actor == NULL || (Actors_ActorStateFlags(actorId) & 0x01000000)) {
            continue;
        }
        if (actor->kind != 4 && actor->kind != 2 && Field_controlMode != 3) {
            continue;
        }

        if (actor->commandState == 0x40) {
            state = actor->commandState;
            switch (actor->commandOpcode) {
            case 9:
            case 10:
            case 11:
            case 12:
            case 14:
            case 15:
            case 18:
                if (!(Actors_ActorStateFlags(actorId) & 0x4000)) {
                    state = 0;
                }
                break;
            case 13:
                Actors_ConsumeActorMovementOffset(actor, &step);
                Actors_QueueGroundOffset(actorId, &step);
                actorFlags = Actors_ActorStateFlags(actorId);
                state = ((actor->commandArg0 | actor->commandArg1.word | (actorFlags & 0x4000)) != 0) << 6;
                break;
            case 23:
                if (!(Actors_ActorStateFlags(actorId) & 0x12000000)) {
                    state = 0;
                }
                break;
            case 20:
            case 21:
            case 22:
                if (!(Actors_ActorStateFlags(actorId) & 0x04000000)) {
                    state = 0;
                }
                break;
            default:
                Core_IgnoreDiagnostic(0xC1, 0);
                break;
            }
            actor->commandState = state;
            if (Field_controlMode == 3) {
                Actors_UpdateActorMovement(actorId);
            }
            continue;
        }

        if (actor->commandState != 0x80) {
            continue;
        }

        Actors_ActorPlacement(actorId, &position);
        actorFlags = *(u_int*)&actor->commandState;
        if (actorFlags & 0x03000000) {
            Audio_PlayActorModelSound(actor->model, (actorFlags >> 24) & 3);
        }
        switch ((*(u_int*)&actor->commandState >> 26) & 3) {
        case 1:
            Effects_TintActionActorRed(actorId, 0);
            break;
        case 2:
            Effects_RestoreActionActorTint();
            break;
        }
        *(u_int*)&actor->commandState &= 0xF3FFFFFF;

        switch (actor->commandOpcode) {
        case 1:
        case 5:
            state = 0;
            break;
        case 2:
            if (actor->commandTarget >= 16) {
                Actors_RestoreActorNeutralPose(actorId);
            } else {
                Actors_RestoreActorIdlePose(actorId);
            }
            state = 0;
            break;
        case 3:
            Actors_SetActorHeadTracking(actorId, -4, NULL, 0);
            Actors_TurnNavigationActor(actorId, ((short*)&actor->commandArg0)[0]);
            state = 0;
            break;
        case 4:
            step.vx = 0;
            step.vy = ((short*)&actor->commandArg0)[0];
            Actors_SetActorHeadTracking(actorId, -3, &step, -1);
            Actors_TurnActorAtSpeed(actorId, ((short*)&actor->commandArg0)[0], ((short*)&actor->commandArg0)[1]);
            state = 0;
            break;
        case 15:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            if (actor->commandTarget >= 16) {
                Actors_QueueNavigationTileTarget(actorId, ((short*)&actor->commandArg0)[0],
                    ((short*)&actor->commandArg0)[1] * ONE, ((short*)&actor->commandArg0)[0]);
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
                Actors_QueueNavigationTileTarget(actorId, ((short*)&actor->commandArg0)[0],
                    ((short*)&actor->commandArg0)[1] * ONE, angle);
            }
            state = ((Actors_ActorStateFlags(actorId) & 0x4000) != 0) << 6;
            break;
        case 9:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            if (actor->stats->gaitClass == 2) {
                movementSpeed = actor->stats->runSpeed << 12;
            } else if (actor->stats->gaitClass == 0x40) {
                movementSpeed = actor->stats->runSpeed << 12;
            } else {
                movementSpeed = actor->stats->walkSpeed << 12;
            }
            if (actor->commandTarget >= 16) {
                Actors_QueueNavigationTileTarget(actorId, ((short*)&actor->commandArg0)[0], movementSpeed, ((short*)&actor->commandArg0)[0]);
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
                Actors_QueueNavigationTileTarget(actorId, ((short*)&actor->commandArg0)[0], movementSpeed, angle);
            }
            state = ((Actors_ActorStateFlags(actorId) & 0x4000) != 0) << 6;
            break;
        case 10:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            if (actor->stats->gaitClass == 4) {
                movementSpeed = actor->stats->runSpeed << 12;
            } else if (actor->stats->gaitClass == 0x80) {
                movementSpeed = actor->stats->runSpeed << 12;
            } else {
                movementSpeed = actor->stats->walkSpeed << 12;
            }
            Actors_QueueNavigationTileTarget(actorId, ((short*)&actor->commandArg0)[0], movementSpeed, ((short*)&actor->commandArg0)[0]);
            state = ((Actors_ActorStateFlags(actorId) & 0x4000) != 0) << 6;
            break;
        case 11:
        case 12:
        case 14:
        case 18:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            target.position.vx = ((u_char*)&actor->commandArg0)[0] * 128 + 64;
            target.position.vz = ((u_char*)&actor->commandArg0)[2] * 128 + 64;
            angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                target.position.vz - position.position.vz) + 0x800;
            switch (actor->commandOpcode) {
            case 11:
            case 12:
                Actors_BeginTileJump(actorId, &actor->commandArg0, actor->commandArg1.word, angle);
                break;
            case 18:
                Actors_BeginNavigationJump(actorId, &actor->commandArg0, actor->commandArg1.word, angle);
                break;
            case 14:
                Actors_BeginAirborneTileJump(actorId, &actor->commandArg0, actor->commandArg1.word, angle);
                break;
            }
            state = 0x40;
            break;
        case 13:
            Actors_ConsumeActorMovementOffset(actor, &step);
            Actors_QueueGroundOffset(actorId, &step);
            actorFlags = Actors_ActorStateFlags(actorId);
            state = ((actor->commandArg0 | actor->commandArg1.word | (actorFlags & 0x4000)) != 0) << 6;
            break;
        case 23:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            if (actor->commandTarget >= 16) {
                angle = position.position.pad;
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2((target.packedTile.target.tileX - ((u_char*)&actor->commandArg0)[0]) * ONE,
                    (target.packedTile.target.tileZ - ((u_char*)&actor->commandArg0)[2]) * ONE) + 0x800;
            }
            Actors_BeginTeleport(actorId, &actor->commandArg0, angle);
            state = 0x40;
            break;
        case 16:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            if (actor->stats->gaitClass == 0x10) {
                movementSpeed = actor->stats->runSpeed << 12;
            } else {
                movementSpeed = actor->stats->walkSpeed << 12;
            }
            Core_Gte_VectorNormalSS((SVECTOR*)&actor->commandArg0, &step);
            if (actor->commandTarget >= 16) {
                angle = Core_Gte_Atan2(step.vx, step.vz) + 0x800;
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
            }
            Actors_QueueFlightMovement(actorId, &step, movementSpeed, angle);
            state = 0;
            break;
        case 17:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            if (actor->stats->gaitClass == 0x20) {
                movementSpeed = actor->stats->runSpeed << 12;
            } else {
                movementSpeed = actor->stats->walkSpeed << 12;
            }
            Core_Gte_VectorNormalSS((SVECTOR*)&actor->commandArg0, &step);
            if (actor->commandTarget >= 16) {
                angle = Core_Gte_Atan2(step.vx, step.vz) + 0x800;
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
            }
            Actors_QueueFlightMovement(actorId, &step, movementSpeed, angle);
            state = 0;
            break;
        case 19:
            step.vx = actor->commandArg0 / ONE - (u_short)position.position.vx;
            step.vy = actor->commandArg2 / ONE - (u_short)position.position.vy;
            step.vz = actor->commandArg1.word / ONE - (u_short)position.position.vz;
            if (actor->commandTarget >= 16) {
                angle = Core_Gte_Atan2(step.vx, step.vz) + 0x800;
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
            }
            Actors_QueueMovementVector(actorId, &step, angle);
            state = 0;
            break;
        case 6:
            Actors_QueueNavigationTileTarget(actorId, position.position.pad, 0, position.position.pad);
            queuedFrame = ((u_short*)&actor->commandArg0)[0];
            step.vx = 0;
            step.vy = queuedFrame;
            Actors_SetActorHeadTracking(actorId, -4, &step, ((short*)&actor->commandArg0)[1]);
            state = 0;
            break;
        case 7:
            Actors_QueueNavigationTileTarget(actorId, position.position.pad, 0, position.position.pad);
            queuedFrame = ((u_short*)&actor->commandArg0)[0];
            step.vx = 0;
            step.vy = queuedFrame;
            Actors_SetActorHeadTracking(actorId, -4, &step, 0x1000);
            state = 0;
            break;
        case 22:
            if (actor->commandTarget >= 16) {
                step.vx = ((short*)&actor->commandArg0)[0] / ONE - (u_short)position.position.vx;
                step.vy = ((short*)&actor->commandArg0)[1] / ONE - (u_short)position.position.vy;
                step.vz = ((short*)&actor->commandArg0)[2] / ONE - (u_short)position.position.vz;
                angle = Core_Gte_Atan2(step.vx, step.vz) + 0x800;
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
            }
            Actors_BeginTimedTravel(actorId, &actor->commandArg0, angle);
            state = 0x40;
            break;
        case 21:
            if (actor->commandTarget >= 16) {
                step.vx = ((u_char*)&actor->commandArg0)[0] / ONE - position.packedTile.target.tileX;
                step.vy = ((u_char*)&actor->commandArg0)[1] / ONE - position.packedTile.target.tileLayer;
                step.vz = ((u_char*)&actor->commandArg0)[2] / ONE - position.packedTile.target.tileZ;
                angle = Core_Gte_Atan2(step.vx, step.vz) + 0x800;
            } else {
                Actors_ActorPlacement(actor->commandTarget, &target);
                angle = Core_Gte_Atan2(target.position.vx - position.position.vx,
                    target.position.vz - position.position.vz) + 0x800;
            }
            Actors_BeginTileTravel(actorId, &actor->commandArg0, angle);
            state = 0x40;
            break;
        case 20:
            Battle_FinishEnemyCombatPose(actorId, position.position.pad);
            state = 0x40;
            break;
        case 8:
            if (!(Actors_ActorStateFlags(actorId) & 0x200000)) {
                step.vx = 0;
                step.vy = 0;
                step.vz = 0;
                Actors_QueueGroundOffset(actorId, &step);
            }
            state = 0;
            break;
        case 24:
            Actors_SetActorHeadTracking(actorId, -2, NULL, -1);
            state = (u_short)actor->stats->limbActions[((u_short*)&actor->commandArg0)[0]][((u_short*)&actor->commandArg0)[1]].actionId;
            if (state == 0) {
                state = 1;
            }
            Battle_BeginActorActionSelection(actor->id, state);
            state = (Battle_actionState->selectedPreview == 0xFF) << 3;
            break;
        default:
            state = 0;
            break;
        }

        actor->commandState = state;
        if (Field_controlMode == 3) {
            Actors_UpdateActorMovement(actorId);
        } else {
            result = Field_CheckActorTrapAndFallTriggers(actorId, &position);
            if (result != 0) {
                return result;
            }
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_8007CFF8 (src/BATTLE/BATTLE.PRG/146C.c:9163)
void Actors_BeginScriptedActorPath(int arg0, SVECTOR* arg1)
{
    Field_SetActorCollisionBypass(arg0, 2);
    Battle_Ai_SetActorNavigationOverride(arg0, arg1->vx, arg1->vz);
}

// decomp: BATTLE.PRG func_8007D03C (src/BATTLE/BATTLE.PRG/146C.c:9169)
int Actors_IsScriptedActorPathFinished(int arg0)
{
    int temp_v0 = Battle_Ai_ActorNavigationOverrideStatus();
    if (temp_v0 == 0) {
        Field_SetActorCollisionBypass(arg0, Field_ActorCollisionBypass(arg0) & ~2);
        return 1;
    }
    return temp_v0 == 2;
}

// decomp: BATTLE.PRG func_8007D260 (src/BATTLE/BATTLE.PRG/146C.c:9232)
void Actors_ResetActorMovement(int arg0)
{
    Battle_Actor* temp_a1 = Battle_actors[arg0];
    if ((temp_a1 != NULL) && (temp_a1->commandState != 0x20)) {
        temp_a1->commandState = 0;
    }
    Actors_StopActorMotion(arg0);
}

// decomp: BATTLE.PRG func_8009DC38 (src/BATTLE/BATTLE.PRG/30DB0.c:2304)
void Actors_StopActorMotion(int arg0)
{
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];
    if (temp_s0 != NULL) {
        Actors_ClearActorMovementModes(arg0);
        temp_s0->instance.currentAnimation = 0;
        temp_s0->instance.turnFrames = 0;
        *(int*)&temp_s0->instance.rootMotion = 0;
    }
}

// decomp: BATTLE.PRG func_8009DC8C (src/BATTLE/BATTLE.PRG/30DB0.c:2315)
void Actors_ClearActorMovementModes(int arg0)
{
    Actors_ActorRecord* temp_a3 = Actors_actorRecords[arg0];

    if (temp_a3 == NULL) {
        return;
    }

    temp_a3->instance.jumpCountdown = 0;
    temp_a3->instance.movementMode = 0;
    temp_a3->weaponPoseActive = 0;
    temp_a3->instance.poseState = 0;
    temp_a3->instance.locomotionState = 0;
    temp_a3->instance.movementPending = 0;
}

// decomp: BATTLE.PRG func_8009E180 (src/BATTLE/BATTLE.PRG/30DB0.c:2491)
int Actors_ActorFacingDeltaToPoint(Actors_ActorRecord* arg0, SVECTOR* arg1)
{
    int temp_a0;
    int temp_a1;
    int var_v1;
    int var_v0;

    if (arg1 == NULL) {
        return 0;
    }

    temp_a1 = arg1->vx - arg0->instance.position.vx;
    temp_a0 = arg1->vz - arg0->instance.position.vz;

    if ((temp_a1 == 0) && (temp_a0 == 0)) {
        var_v1 = arg0->instance.facing;
    } else {
        var_v1 = (0xC00 - Core_Gte_Atan2(temp_a0, temp_a1)) & 0xFFF;
    }

    var_v1 -= arg0->instance.facing;

    if (var_v1 >= 0x801) {
        var_v0 = var_v1 - 0x1000;
    } else {
        var_v0 = var_v1;
        if (var_v1 < -0x800) {
            var_v0 = var_v1 + 0x1000;
        }
    }

    return var_v0;
}

// decomp: BATTLE.PRG func_800A0ABC (src/BATTLE/BATTLE.PRG/30DB0.c:4015)
void Actors_ResetActorRunMomentum(int arg0)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (temp_a0 != NULL) {
        temp_a0->momentumDistance = 0;
        temp_a0->runFrames = 0;
        temp_a0->isRunning = 0;
    }
}

// decomp: BATTLE.PRG func_800A0AFC (src/BATTLE/BATTLE.PRG/30DB0.c:4025)
void Actors_SetActorWalkRunSpeeds(int arg0, int arg1, int arg2)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (temp_a0 != NULL) {
        temp_a0->walkSpeed = arg1 / ONE;
        temp_a0->runSpeed = arg2 / ONE;
        if (arg1 >= arg2) {
            temp_a0->runSpeed = 0xFF;
        }
    }
}

// decomp: BATTLE.PRG func_800A0B5C (src/BATTLE/BATTLE.PRG/30DB0.c:4037)
void Actors_SetActorTravelStepCounts(int arg0, int arg1, int arg2)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];

    if (temp_a0 == NULL) {
        return;
    }

    if (arg1 < 0) {
        arg1 = 1;
    }
    temp_a0->moveRate = arg1;

    if (arg2 < 0) {
        arg2 = 1;
    }
    temp_a0->turnRate = arg2;
}

// decomp: BATTLE.PRG func_800A0BA0 (src/BATTLE/BATTLE.PRG/30DB0.c:4056)
void Actors_ActorTravelStepCounts(int arg0, int* arg1, int* arg2)
{
    Actors_ActorRecord* temp_a0 = Actors_actorRecords[arg0];
    if (temp_a0 != 0) {
        *arg1 = temp_a0->moveRate;
        *arg2 = temp_a0->turnRate;
    }
}

// decomp: BATTLE.PRG func_800A208C (src/BATTLE/BATTLE.PRG/38C1C.c:450)
void Actors_BeginActorJumpToTile(Actors_ActorRecord* actor, u_char* target, int speed, short facing)
{
    int x;
    int y;
    int z;
    int i;
    int dz;
    int minFrames;
    int accel;
    int animation;
    int hangFrames;
    int frames;
    int fall;
    int velocity;
    SVECTOR* anchor;
    int _[2] __attribute__((unused));

    if (*(u_int*)((char*)actor + 8) & 0x70000) {
        return;
    }

    actor->instance.jumpCountdown = 6;
    *(u_int*)((char*)actor + 0x5AC) &= ~0x4000000;
    if (speed >= actor->runSpeed) {
        actor->instance.jumpCountdown = 2;
    }
    if (speed < 18) {
        speed = 18;
        *(u_int*)((char*)actor + 0x5AC) |= 0x4000000;
    }

    if (target[1] >= 2) {
        anchor = ((SVECTOR* (*)(int))Field_FindMovingPlatformById)(target[1]);
        x = anchor->vx;
        y = anchor->vy;
        z = anchor->vz;
    } else {
        x = target[0] * 128 + 64;
        z = target[2] * 128 + 64;
        y = Field_ActorSupportHeight(x, z);
    }

    *(int*)&actor->routeTarget = *(int*)target;
    actor->destinationX = x;
    actor->destinationZ = z;
    x -= actor->instance.position.vx;
    y -= actor->instance.position.vy;
    z -= actor->instance.position.vz;

    i = x;
    if (x < 0) {
        i = -x;
    }
    dz = z;
    if (z < 0) {
        dz = -z;
    }
    if (i < dz) {
        i = dz;
    }
    minFrames = i / speed;

    if (*(u_int*)((char*)actor + 8) & 0x400000) {
        accel = 1;
        animation = 33;
    } else {
        accel = 6;
        animation = 47;
    }

    if (y <= 0) {
        if (actor->instance.slot != 0) {
            Audio_PlayActorModelSound(&actor->instance, 12);
        }
        i = y >= -64;
        if (*(u_int*)((char*)actor + 8) & 0x400000) {
            i += 3;
        }
        hangFrames = Actors_kJumpHangFrames[i];
        y -= Actors_kJumpHeightAdjustments[i];
    retry:
        for (frames = 0, fall = 0, velocity = 0; y < fall; ++frames) {
            velocity += accel;
            fall -= velocity / 2;
        }
        frames += hangFrames;
        actor->instance.rootMotion.vx = x / frames;
        actor->instance.rootMotion.vz = z / frames;
        actor->instance.rootMotion.vy = -velocity / 2;
        if (ABS(actor->instance.rootMotion.vx) > speed || ABS(actor->instance.rootMotion.vz) > speed) {
            ++hangFrames;
            y -= ((hangFrames + 1) * accel) / 2;
            goto retry;
        }
    } else {
        i = 2;
        if (*(u_int*)((char*)actor + 8) & 0x400000) {
            i = 5;
        }
        hangFrames = Actors_kJumpHangFrames[i];
        y += Actors_kJumpHeightAdjustments[i];
        if ((*(u_int*)((char*)actor + 8) & 0x400000) && (y > 128)) {
            y = 128;
        }
        for (frames = 0, fall = 0, velocity = 0; fall < y; ++frames) {
            velocity += accel;
            fall += velocity / 2;
        }
        frames += hangFrames;
        if (frames < minFrames) {
            hangFrames += (minFrames - frames) / 2 + 1;
            frames = minFrames;
        }
        actor->instance.rootMotion.vx = x / frames;
        actor->instance.rootMotion.vz = z / frames;
        actor->instance.rootMotion.vy = (-(hangFrames + 1) * accel) / 2;
        if (*(u_int*)((char*)actor + 8) & 0x400000) {
            animation = 35;
        }
    }

    actor->jumpFrames = frames;
    actor->destinationMode = 2;
    ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(actor->instance.slot, animation, 0, 4);
    *(u_int*)((char*)actor + 8) = (*(u_int*)((char*)actor + 8) & ~0x70000) | 0x20000;
    ((void (*)(int, int, int))Actors_TurnActorToAtRate)(actor->instance.slot, facing, 8);
}

// decomp: BATTLE.PRG func_800A25EC (src/BATTLE/BATTLE.PRG/38C1C.c:633)
void Actors_ApplyActorGroundOffset(Actors_ActorRecord* actor)
{
    SVECTOR next;

    actor->instance.motionLocked = 0;
    setVector(&next, actor->turnState.step.vx, 0, actor->turnState.step.vz);
    next.vy = Field_SampleActorSupportHeight(&actor->instance.position, next.vx, next.vz, 1);

    if (next.vy == -3000) {
        return;
    }

    next.vx += actor->instance.position.vx;
    next.vz += actor->instance.position.vz;

    if (Field_FindCollisionCellAtPosition(&next, 0) == NULL) {
        return;
    }

    actor->instance.position.vx = next.vx;
    actor->instance.position.vz = next.vz;

    if (actor->instance.movementMode) {
        ((void (*)(int, Field_GimmickInstance*))Actors_UpdateActorJumpAndFall)(actor->instance.slot, (Field_GimmickInstance*)actor);
    } else {
        short height = next.vy;

        if (height > actor->instance.position.vy && height - actor->instance.position.vy >= 64) {
            setVector(&actor->instance.rootMotion, 0, 0, 0);
            ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(actor->instance.slot, 47, 0, 4);
            actor->destinationMode = 0;
            actor->instance.locomotionState = 0;
            actor->instance.movementPending = 0;
            actor->instance.movementMode = 3;
        } else {
            actor->instance.position.vy = height;
        }
    }

    actor->instance.currentTileX = actor->instance.position.vx / 128;
    actor->instance.currentTileZ = actor->instance.position.vz / 128;
    actor->instance.currentTileLayer = 0;
}

// decomp: BATTLE.PRG func_800A2790 (src/BATTLE/BATTLE.PRG/38C1C.c:677)
void Actors_ApplyActorFlightMovement(Actors_ActorRecord* actor)
{
    VECTOR delta;
    int speed = actor->turnState.distance;
    int animation;

    setVector(&delta, actor->turnState.step.vx * speed / (ONE * ONE),
        actor->turnState.step.vy * speed / (ONE * ONE),
        actor->turnState.step.vz * speed / (ONE * ONE));
    addVector(&actor->instance.position, &delta);
    actor->instance.currentTileX = actor->instance.position.vx / 128;
    actor->instance.currentTileZ = actor->instance.position.vz / 128;
    actor->instance.motionLocked = 1;
    ((void (*)(int, int, int))Actors_TurnActorToOverTime)(actor->instance.slot, actor->turnState.facing, 12);
    animation = 1;

    if (speed > 0) {
        if (delta.vy < -2) {
            animation = 32;
        } else if (delta.vy >= 3) {
            animation = 34;
        } else {
            animation = 31;
        }

        if (speed >= 12) {
            animation += 6;
        }
    }

    if (actor->animationId != animation) {
        ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(actor->instance.slot, animation, 0, 4);
    }
}

// decomp: BATTLE.PRG func_800A291C (src/BATTLE/BATTLE.PRG/38C1C.c:712)
void Actors_ApplyActorMovementVector(Actors_ActorRecord* arg0)
{
    addVector(&arg0->instance.position, &arg0->turnState.step);

    arg0->instance.currentTileX = arg0->instance.position.vx / 128;
    arg0->instance.currentTileZ = arg0->instance.position.vz / 128;

    ((void (*)(int, int, int))Actors_TurnActorToOverTime)(arg0->instance.slot, arg0->turnState.facing, 12);
}

// decomp: BATTLE.PRG func_800A29A0 (src/BATTLE/BATTLE.PRG/3A1A0.c:86)
void Actors_UpdateActorTeleportTransition(Actors_ActorRecord* arg0)
{
    int busy;

    switch (arg0->instance.reactionState) {
    case 1:
        arg0->instance.motionLocked = 0;
        Actors_PlayActorAnimation(arg0->instance.slot, 0x13, 0, 8);
        Actors_QueueActorAnimation(arg0->instance.slot, 0x14, 8, 0);
        arg0->instance.baseColor.cd = 0;
        arg0->instance.reactionState = 2;
        arg0->instance.actionAnimationState = 6;
        return;

    case 2:
        if (arg0->instance.actionAnimationState < 8) {
            return;
        }

        if (arg0->instance.attachBone == 0xFC) {
            busy = Actors_actorRecords[arg0->instance.attachParent]->instance.actionAnimationState < 9;
        } else if (arg0->pose.reachedEnd) {
            busy = arg0->blendFrame;
        } else {
            return;
        }

        if (busy == 0) {
            arg0->instance.actionAnimationState = 9;
            ((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)(arg0->instance.slot, &arg0->routeTarget, 0);
            arg0->instance.facing = arg0->routeTarget.pad;
            Actors_PlayActorAnimation(arg0->instance.slot, 0x15, 0, 8);
            Actors_QueueActorAnimation(arg0->instance.slot, 0x16, 8, 0);
            arg0->instance.reactionState = 3;
        }

        return;

    case 3:
        if (arg0->instance.actionAnimationState) {
            return;
        }

        if (arg0->instance.attachBone == 0xFC ? Actors_actorRecords[arg0->instance.attachParent]->instance.actionAnimationState
                                     : arg0->blendFrame) {
            return;
        }

        Actors_PlayActorAnimation(arg0->instance.slot, 1, 0, 8);
        // fallthrough

    case 4:
        ((void (*)(Actors_ActorRecord*))Actors_SaveActorGroundedTransform)(arg0);
        Battle_Ai_ReleaseAiDestinationTile(arg0->instance.currentTileX, arg0->instance.currentTileZ);
        // fallthrough

    case 5:
        if (arg0->companionSlot >= 2 && Actors_actorRecords[arg0->companionSlot]->instance.reactionState) {
            arg0->instance.reactionState = 5;
            return;
        }

        arg0->instance.reactionState = 0;
        return;
    }
}

// decomp: BATTLE.PRG func_800A2C48 (src/BATTLE/BATTLE.PRG/3A1A0.c:153)
void Actors_CancelActorTeleportTransition(Actors_ActorRecord* arg0)
{
    ((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)(arg0->instance.slot, &arg0->sharedSequences[4], 0);
    arg0->queuedAnimation = 0;
    arg0->instance.reactionState = 4;
    Actors_UpdateActorTeleportTransition(arg0);
    if (arg0->companionSlot >= 2) {
        Actors_CancelActorTeleportTransition(Actors_actorRecords[arg0->companionSlot]);
    }
}

// decomp: BATTLE.PRG func_800A2CD4 (src/BATTLE/BATTLE.PRG/3A1A0.c:164)
void Actors_UpdateActorTravelTransition(Actors_ActorRecord* arg0)
{
    u_short steps;
    u_char animation;

    switch (arg0->instance.poseState) {
    case 1:
        Actors_PlayActorAnimation(arg0->instance.slot, 9, 0, 8);
        ((u_int (*)(u_char, void*, int))Actors_TranslateActorToTileAtRate)(arg0->instance.slot, &arg0->routeTarget, -1);
        ((void (*)())Actors_TurnActorToAtRate)(arg0->instance.slot, arg0->routeTarget.pad, 8);
        arg0->transitionFrames = arg0->instance.currentAnimation;
        arg0->instance.currentAnimation = 0;
        if (arg0->transitionFrames != 0) {
            arg0->instance.poseState = 2;
            return;
        }
        goto land;
    case 2:
        Actors_StepActorTransitionDisplacement(arg0);
        Field_MovingPlatformContactHeight(
            arg0->instance.position.vx, arg0->instance.position.vy, arg0->instance.position.vz, 2);
        if (*(int*)&Actors_supportingPlatformLayer != 0) {
            arg0->instance.motionLocked = 1;
            goto idle;
        }
        if (arg0->transitionFrames == 0) {
            Actors_PlayActorAnimation(arg0->instance.slot, 0xA, 0, 8);
            arg0->instance.motionLocked = 0;
            arg0->instance.poseState = 3;
        }
        return;
    case 3:
        if (arg0->instance.currentAnimation == 0 && arg0->pose.reachedEnd) {
        land:
            arg0->instance.motionLocked = 0;
            ((void (*)(Actors_ActorRecord*))Actors_SaveActorGroundedTransform)(arg0);
            Battle_Ai_ReleaseAiDestinationTile(arg0->instance.currentTileX, arg0->instance.currentTileZ);
            Actors_PlayActorAnimation(arg0->instance.slot, 1, 0, 8);
            arg0->instance.poseState = 0;
        }
        return;
    case 4:
        Actors_PlayActorAnimation(arg0->instance.slot, 7, 0, 8);
        Actors_QueueActorAnimation(arg0->instance.slot, 0x25, 8, 0);
        ((void (*)())Actors_TranslateActorToOverTime)(arg0->instance.slot, &arg0->routeTarget, -1);
        ((void (*)())Actors_TurnActorToAtRate)(arg0->instance.slot, arg0->routeTarget.pad, 8);
        steps = arg0->instance.currentAnimation;
        arg0->instance.currentAnimation = 0;
        arg0->transitionFrames = steps;
        arg0->instance.motionLocked = 1;
        if (arg0->transitionFrames == 0) {
            arg0->queuedAnimation = 0;
            goto idle;
        }
        arg0->instance.poseState = 5;
        return;
    case 5:
        Actors_StepActorTransitionDisplacement(arg0);
        Field_MovingPlatformContactHeight(
            arg0->instance.position.vx, arg0->instance.position.vy, arg0->instance.position.vz, 3);
        if (*(int*)&Actors_supportingPlatformLayer != 0 || arg0->transitionFrames == 0) {
        idle:
            Actors_PlayActorAnimation(arg0->instance.slot, 1, 0, 8);
            arg0->instance.poseState = 0;
        }
        return;
    case 6:
        animation = arg0->animationId;
        if (animation != 0xC7 && animation != 0xC9 && arg0->blendFrame == 0) {
            arg0->instance.poseState = 0;
        }
        return;
    }
}

// decomp: BATTLE.PRG func_800A2FBC (src/BATTLE/BATTLE.PRG/3A1A0.c:239)
void Actors_StepActorTransitionDisplacement(Actors_ActorRecord* arg0)
{
    int temp_a1 = arg0->instance.rootMotion.vx / arg0->transitionFrames;
    arg0->instance.rootMotion.vx -= temp_a1;
    arg0->instance.position.vx += temp_a1;

    temp_a1 = arg0->instance.rootMotion.vy / arg0->transitionFrames;
    arg0->instance.rootMotion.vy -= temp_a1;
    arg0->instance.position.vy += temp_a1;

    temp_a1 = arg0->instance.rootMotion.vz / arg0->transitionFrames;
    arg0->instance.rootMotion.vz -= temp_a1;
    arg0->instance.position.vz += temp_a1;

    --arg0->transitionFrames;
}

// decomp: BATTLE.PRG func_800A3054 (src/BATTLE/BATTLE.PRG/3A1A0.c:256)
void Actors_SetActorFallDestination(Actors_ActorRecord* arg0, Actors_FallDirection* arg1)
{
    int var_a2;
    int var_a3;

    var_a2 = 0x3F;
    var_a3 = 0x3F;
    if (arg1->directionX < 0) {
        var_a2 = -0x3F;
    }
    if (arg1->directionZ < 0) {
        var_a3 = -0x3F;
    }
    arg0->destinationMode = 1;
    arg0->destinationX = (u_short)arg0->instance.position.vx + var_a2;
    arg0->destinationZ = (u_short)arg0->instance.position.vz + var_a3;
}

// decomp: BATTLE.PRG func_800A48CC (src/BATTLE/BATTLE.PRG/3A1A0.c:1048)
void Actors_BeginActorJumpMovement(int index, int direction, int distance)
{
    Actors_ActorRecord* actor = Actors_actorRecords[index];

    if (actor->routePhase != 0) {
        return;
    }

    if (actor->instance.locomotionState != 0) {
        return;
    }

    if (actor->instance.movementMode != 0) {
        Actors_SetDirectMovement(index, direction, distance);
        return;
    }

    if (distance == 0) {
        actor->jumpStarted = 0;
        actor->jumpProfile = 0;
        Actors_PlayActorAnimation(index, 0x2F, 0, 4);
        actor->instance.jumpCountdown = 6;
        actor->instance.movementMode = 2;
    } else {
        actor->jumpStarted = 0;
        actor->jumpProfile = 1;

        if (distance / ONE >= actor->runSpeed) {
            actor->jumpProfile = 2;
            actor->jumpStarted = 1;
        }

        actor->instance.jumpCountdown = 0;
        actor->instance.movementMode = 1;
    }

    actor->turnState.mode = 7;
    actor->turnState.direction = direction;
    actor->turnState.distance = distance;
}

// decomp: BATTLE.PRG func_800A4A88 (src/BATTLE/BATTLE.PRG/3A1A0.c:1107)
void Actors_UpdateActorPlatformContact(Actors_ActorRecord* actor, int mode)
{
    Field_PlatformContactPoint* contact;
    int contactId;
    int current;
    int previous;

    switch (mode) {
    case 0:
        if (actor->instance.supportLayer == 0) {
            return;
        }

        contact = Field_FindMovingPlatformById(actor->instance.supportLayer);
        actor->platformOffsetX = contact->x - actor->platformAnchor.vx;
        actor->platformOffsetZ = contact->z - actor->platformAnchor.vz;
        actor->instance.position.vx += actor->platformOffsetX;
        actor->instance.position.vz += actor->platformOffsetZ;
        actor->instance.position.vy += contact->height - actor->platformAnchor.vy;
        break;

    case 1:
        if (actor->instance.movementMode == 2 || actor->instance.movementMode == 3) {
            return;
        }

        if (actor->instance.placementLayer != (contactId = actor->instance.supportLayer)) {
            if (actor->instance.slot == 0 && contactId != 0
                && contactId != actor->platformAnchor.pad) {
                Battle_Ai_SetAiContactOccupancy(contactId);
            }

            actor->platformOffsetX = 0;
            actor->platformOffsetZ = 0;
        }

        if (actor->instance.supportLayer == 0) {
            return;
        }

        contact = Field_FindMovingPlatformById(actor->instance.supportLayer);
        break;

    case 2:
        current = actor->instance.supportLayer;
        previous = actor->instance.placementLayer;

        if (current != previous) {
            if (actor->instance.supportLayer != 0) {
                actor->platformAnchor.pad = current;
                Field_SetMovingPlatformActor(actor->instance.supportLayer - 2, actor->instance.slot);
            } else {
                Field_SetMovingPlatformActor(previous - 2, -1);

                if (!actor->instance.movementMode) {
                    actor->platformOffsetX = 0;
                    actor->platformOffsetZ = 0;
                }
            }
        }

        if (actor->instance.slot == 0 && !actor->instance.movementMode
            && actor->instance.supportLayer != actor->platformAnchor.pad) {
            Battle_Ai_ClearAiContactOccupancy(actor->platformAnchor.pad);
            actor->platformAnchor.pad = 0;
        }

        actor->instance.currentTileLayer = actor->instance.supportLayer;
        actor->instance.placementLayer = actor->instance.supportLayer;
        return;
    }

    setVector(&actor->platformAnchor, contact->x, contact->height, contact->z);
}

// decomp: BATTLE.PRG func_800A4D8C (src/BATTLE/BATTLE.PRG/3A1A0.c:1182)
void Actors_UpdateRoomActorMovement(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        if ((Field_gimmickInstances[i] != NULL) && (Field_gimmickInstances[i]->boneCount != 0)) {
            ((void (*)(Field_GimmickInstance*, int))Field_UpdateGimmickMotion)(Field_gimmickInstances[i], State_tickSpeed / 2);
        }
    }
    for (i = 0; i < 17; ++i) {
        if ((Actors_actorRecords[i] != NULL) && (Actors_actorRecords[i]->instance.isSkipped == 0)) {
            Actors_UpdateActorMovement(i);
        }
    }
    Actors_isActorUpdateComplete = 1;
}

// decomp: BATTLE.PRG func_800A4E68 (src/BATTLE/BATTLE.PRG/3A1A0.c:1199)
void Actors_UpdateActorMovement(int actorId)
{
    SVECTOR movement;
    Actors_ActorRecord* actor;
    int x;
    int z;
    int mode;
    int contact;

    actor = Actors_actorRecords[actorId];
    if (actor->instance.actionAnimationState != 0 && actor->instance.actionAnimationState < 6) {
        return;
    }
    if (actorId == 0) {
        Actors_UpdateActorPlatformContact(actor, 0);
        if (actor->instance.movementMode && actor->destinationMode != 0) {
            ((void (*)())Actors_UpdateActorJumpAndFall)(0, actor);
        } else {
            mode = actor->turnState.mode;
            switch (mode) {
            case 7:
            case 0:
                movement.pad = 0;
                goto move;
            case 1:
                movement.pad = mode;
                movement.vx = actor->turnState.facing;
            move:
                Actors_MoveActorWithCollision(
                    0, (short)actor->turnState.direction, actor->turnState.distance, &movement);
                if (actor->instance.movementMode) {
                    ((void (*)())Actors_UpdateActorJumpAndFall)(0, actor);
                }
                break;
            case 2:
                ((void (*)())Actors_ApplyActorGroundOffset)(actor);
                break;
            }
        }
        Actors_UpdateActorPlatformContact(actor, 1);
        Actors_UpdateActorPlatformContact(actor, 2);
    } else {
        Actors_UpdateActorPlatformContact(actor, 0);
        if (actor->turnState.mode == 2) {
            ((void (*)())Actors_ApplyActorGroundOffset)(actor);
        } else if (actor->instance.movementMode) {
            ((void (*)())Actors_UpdateActorJumpAndFall)(actorId, actor);
        } else if (actor->instance.reactionState) {
            Actors_UpdateActorTeleportTransition(actor);
        } else if (actor->instance.poseState) {
            Actors_UpdateActorTravelTransition(actor);
        } else {
            mode = actor->turnState.mode;
            switch (mode) {
            case 1:
                if (actor->instance.wasDrawn) {
                    break;
                }
                if (actor->instance.motionLocked) {
                    break;
                }
                movement.pad = mode;
                movement.vx = actor->turnState.facing;
                Actors_MoveActorWithCollision(
                    actorId, (short)actor->turnState.direction, actor->turnState.distance, &movement);
                if (actor->instance.movementMode) {
                    ((void (*)())Actors_UpdateActorJumpAndFall)(actorId, actor);
                }
                break;

            case 3:
                ((void (*)())Actors_ApplyActorFlightMovement)(actor);
                break;
            case 4:
                ((void (*)())Actors_ApplyActorMovementVector)(actor);
                break;
            }
        }
        contact = actor->contactKind;
        if (contact != 0) {
            if (actor->instance.supportLayer == contact) {
                actor->instance.supportLayer = 0;
            }
            if (Field_IsPointOnMovingPlatform(actor->contactKind, &actor->instance.position) == 0) {
                actor->contactKind = 0;
                Battle_Ai_ClearActorAiNavigationReset(actor->instance.slot);
            }
            if (actor->destinationMode == 1) {
                actor->destinationX = actor->instance.position.vx;
                actor->destinationZ = actor->instance.position.vz;
            }
        }
        Actors_UpdateActorPlatformContact(actor, 1);
        Actors_UpdateActorPlatformContact(actor, 2);
        x = actor->instance.position.vx;
        if (x < 0) {
            x += 0x7F;
        }
        z = actor->instance.position.vz;
        actor->instance.currentTileX = x >> 7;
        if (z < 0) {
            z += 0x7F;
        }
        actor->instance.currentTileZ = z >> 7;
    }
    Actors_ClearActorMovementRequest(actor);
}

// decomp: BATTLE.PRG func_800A525C (src/BATTLE/BATTLE.PRG/3A1A0.c:1336)
void Actors_ClearActorMovementRequest(Actors_ActorRecord* arg0)
{
    arg0->turnState.mode = 1;
    if (arg0->instance.slot == 0) {
        arg0->turnState.mode = 0;
    }
    arg0->turnState.direction = 0;
    arg0->turnState.distance = 0;
    arg0->turnState.facing = 0;
}

// decomp: BATTLE.PRG func_800A5280 (src/BATTLE/BATTLE.PRG/3A1A0.c:1347)
int Actors_MoveActorWithCollision(int index, int direction, int distance, SVECTOR* input)
{
    SVECTOR motion;
    SVECTOR saved;
    int result;
    int previousArea;
    Actors_ActorRecord* actor;
    int turning;
    int speed;
    int facing;
    int status;
    int angle;
    int animation;
    int i;
    int previousX;
    int previousZ;
    int moveType;

    Field_GimmickInstance* object;

    turning = 0;
    if (index >= 17) {
        return 3;
    }
    actor = Actors_actorRecords[index];
    if (actor == NULL) {
        return 3;
    }
    if (actor->routePhase != 0) {
        return 3;
    }
    if (actor->instance.reactionState) {
        return 3;
    }
    if (actor->weaponPoseActive) {
        return 3;
    }
    if ((signed char)actor->instance.jumpCountdown > 0) {
        return 3;
    }
    if (actor->instance.locomotionState == 2) {
        if (actor->instance.supportLayer) {
            ((void (*)(Actors_ActorRecord*))Actors_SaveActorGroundedTransform)(actor);
            if ((*(u_char (*)[])&Field_areMovingPlatformsPaused)[actor->instance.supportLayer] == 7
                && Field_SampleActorSupportHeight(&actor->instance.position, 0, 0, 1)
                       <= actor->instance.position.vy) {
            fall:
                actor->instance.rootMotion.vy = 3;
                actor->animationSet = 0xFF;
                actor->instance.movementMode = 3;
                actor->instance.locomotionState = 0;
                actor->instance.movementPending = 0;
                actor->instance.supportLayer = 0;
                actor->supportGimmick = 0;
                return 3;
            }
        }
        if (distance == 0) {
            return 3;
        }
        i = (direction - (u_short)actor->moveFacing) & 0xFFF;
        if ((u_int)(i - ONE / 4) <= ONE / 2) {
            goto fall;
        }
        if (actor->instance.currentAnimation != 0) {
            return 3;
        }
        direction = (u_short)actor->moveFacing;
        if (distance >= actor->runSpeed << 12) {
            distance = actor->walkSpeed << 12;
        }
    } else if (actor->instance.locomotionState) {
        i = (direction - (u_short)actor->moveFacing) & 0xFFF;
        if (distance == 0) {
            return 3;
        }
        if (i <= ONE / 4) {
            return 3;
        }
        if (i >= ONE * 3 / 4) {
            return 3;
        }
        turning = 1;
        actor->instance.locomotionState = 0;
        actor->instance.movementPending = 0;
        actor->instance.currentAnimation = 0;
        actor->instance.rootMotion.vx = 0;
        actor->instance.rootMotion.vy = 0;
        actor->instance.rootMotion.vz = 0;
        actor->ledgeHeight = 0;
        Actors_PlayActorAnimation(index, 1, 0, 10);
    }

    if (actor->instance.movementMode) {
        if (index != 0) {
            return 3;
        }
        if (distance / ONE >= actor->runSpeed) {
            if (!actor->jumpStarted) {
                distance = actor->walkSpeed << 12;
            }
        } else {
            actor->jumpStarted = 0;
        }
        moveType = actor->instance.movementMode;

        if (moveType != 0) {
            if (moveType < 4) {
                if (distance == 0) {
                    goto stop;
                }
            } else if (moveType == 4) {
                return 3;
            }
        }
    }

    if (distance == -1) {
        if (input->vx == 0 && input->vz == 0) {
            animation = actor->animationId;
            if (animation >= 100) {
                animation -= 100;
            }
            if (animation == 1) {
                goto halt;
            }
            if ((u_int)(animation - 13) >= 14) {
                return 3;
            }
        }
        motion.vx = input->vx;
        motion.vz = input->vz;
        speed = actor->walkSpeed << 12;
        goto move;
    }
    if (distance == 0) {
        if (actor->isRunning && input->pad == 0) {
        coast:
            distance = actor->momentumDistance;
            direction = actor->momentumDirection;
            distance -= State_tickSpeed;
            actor->momentumDistance = distance;
            if (distance <= 0) {
                actor->isRunning = 0;
                distance = 0;
            }
            distance <<= 12;
            goto scale;
        }
        if (actor->momentumDistance >= actor->runSpeed) {
            if (actor->runFrames >= 20 && actor->runTurns < 9) {
                actor->runFrames = 0;
                actor->isRunning = 1;
                if (input->pad == 0) {
                    goto coast;
                }
                if (!actor->instance.movementMode) {
                    Actors_UpdateActorLocomotionAnimation(actor, distance, direction);
                }
                return status;
            }
            actor->runFrames = 0;
        }
        if (actor->contactKind == 0) {
            if (actor->momentumDistance != 0 || actor->instance.supportLayer) {
                actor->momentumDistance = 0;
            } else if (!actor->wallContact) {
                animation = actor->animationId;
                if (animation >= 100) {
                    animation -= 100;
                }
                if (animation != 0x54) {
                    if (animation == 1) {
                        return 3;
                    }
                    if ((u_int)(animation - 13) >= 14) {
                        return 3;
                    }
                }
            }
        }
    halt:
        speed = 0;
    stop:
        actor->instance.rootMotion.vx = 0;
        actor->instance.rootMotion.vz = 0;
        motion.vx = 0;
        motion.vz = 0;
        goto move;
    }

    actor->isRunning = 0;
    actor->momentumDistance = distance / ONE;
    if (distance >= actor->runSpeed << 12) {
        i = actor->runFrames;
        i += State_tickSpeed / 2;
        actor->momentumDirection = direction;
        if (i >= 0x100) {
            i = 0xFF;
        }
        actor->runFrames = i;
        actor->runTurns = 0;
    } else {
        i = actor->runTurns;
        i += State_tickSpeed / 2;
        if (i >= 0x100) {
            i = 0xFF;
        }
        actor->runTurns = i;
    }
    facing = direction;
    if (input->pad == 1) {
        facing = input->vx;
    }
    facing &= 0xFFF;
    if (distance != 0 && !actor->instance.locomotionState) {
        Actors_UpdateActorSteering(actor, facing, input);
    }

scale:
    speed = distance;
    if (actor->collisionState != 4) {
        speed = distance * actor->collisionState / 4;
        if (input->pad == 0 && speed > 0x1EFFF) {
            speed = 0x1F000;
        }
    }
    if (speed <= 0) {
        if (!actor->instance.movementMode) {
            Actors_UpdateActorLocomotionAnimation(actor, distance, direction);
        }
        return status;
    }
    angle = ONE * 3 / 4 - direction;
    motion.vx = Core_Gte_Cos(angle) * speed / 0x1000000;
    motion.vz = Core_Gte_Sin(angle) * speed / 0x1000000;

move:
    if (actor->instance.movementMode == 3) {
        if (actor->instance.position.vy > 0) {
            actor->platformOffsetX = 0;
            actor->platformOffsetZ = 0;
        }
        motion.vx += (signed char)actor->platformOffsetX;
        motion.vz += (signed char)actor->platformOffsetZ;
    }
    if (actor->wallContact) {
        motion.vx /= 4;
        motion.vz /= 4;
    }
    Actors_motionResultResetWord = 0;
    status = Field_ResolveActorTerrainMotion(actor, &motion, &result, input->pad);
    switch (status) {
    case 0:
        distance = speed / ONE;
        break;
    case 1:
        distance = motion.vx;
        if (distance < 0) {
            distance = -distance;
        }
        break;
    case 2:
        distance = motion.vz;
        if (distance < 0) {
            distance = -distance;
        }
        break;
    case 3:
        distance = 0;
        if (actor->instance.locomotionState == 2) {
            return 3;
        }
        if (turning) {
            actor->instance.movementMode = 3;
        }
        if (actor->instance.movementMode) {
            actor->instance.rootMotion.vx = 0;
            actor->instance.rootMotion.vz = 0;
            if (actor->instance.movementMode == 1) {
                Actors_PlayActorAnimation(index, 0x30, 0, 2);
                actor->instance.movementMode = 2;
            }
            return 3;
        }
        motion.vx = 0;
        motion.vz = 0;
        break;
    }

    motion.vy = Field_SampleActorSupportHeight(&actor->instance.position, motion.vx, motion.vz, 1);
    motion.vy = motion.vy - actor->instance.position.vy;
    if (!actor->instance.movementMode && !actor->instance.locomotionState) {

        previousArea = actor->supportGimmick;
        actor->supportGimmick = Actors_supportingGimmickIndex;
        actor->instance.supportLayer = Actors_supportingPlatformLayer;
    }
    if (input->pad == 0) {
        if (motion.vy < -0x20 || status == 3) {
            i = -(motion.vy / 64);
            if (motion.vy & 0x3F) {
                i++;
            }
            if (result < i) {
                result = i;
            }
        } else if (result == 4) {
            motion.vy = motion.vy + actor->instance.position.vy;
            motion.vy = Field_collisionProbeHeight - motion.vy;
            if (motion.vy >= -0xEC) {
                result = 3;
            }
        } else if (Field_collisionProbeHeight - actor->instance.position.vy < -0x5F
                   && actor->instance.position.vy < 0) {
            result = 2;
        }
        if (status == 3 && result >= 0) {
            result = 5;
        }
    }

    if (actor->wallContact && speed == 0) {
        if (actor->previousTargetX == actor->instance.position.vx + motion.vx
            && actor->previousTargetZ == actor->instance.position.vz + motion.vz) {
            distance = 0;
            goto done;
        }
        actor->previousTargetX = actor->pendingTargetX;
        actor->previousTargetZ = actor->pendingTargetZ;
        actor->pendingTargetX = actor->instance.position.vx + motion.vx;
        actor->pendingTargetZ = actor->instance.position.vz + motion.vz;
    } else {
        previousX = actor->pendingTargetX;
        previousZ = actor->pendingTargetZ;
        actor->pendingTargetX = 0;
        actor->pendingTargetZ = 0;
        actor->previousTargetX = previousX;
        actor->previousTargetZ = previousZ;
    }

    actor->collisionState = 4;
    if (actor->instance.movementMode) {
        if (actor->instance.movementMode == 1) {
            actor->instance.rootMotion.vx = motion.vx;
            actor->instance.rootMotion.vz = motion.vz;
            Actors_PlayActorAnimation(index, 0x30, 0, 4);
            actor->instance.movementMode = 2;
            return 3;
        }
        if (result < 2) {
            if (motion.vy > 0) {
                actor->instance.rootMotion.vx = motion.vx;
                actor->instance.rootMotion.vz = motion.vz;
                return 3;
            }
            actor->instance.rootMotion.vx = 0;
            actor->instance.rootMotion.vz = 0;
            return 3;
        }
        if (result != 2) {
            if (result != 3) {
                if (result != 4) {
                    actor->instance.rootMotion.vx = 0;
                    actor->instance.rootMotion.vz = 0;
                    return 3;
                }
            }
        }
    }

    if (result > 0) {
        if (((*Field_collisionProbeTile >> 5) & 1) && result == 1) {
            actor->collisionState = 2;
            ((void (*)())Actors_SetActorTranslation)(index, &motion, 0);
            goto done;
        }
        if (result >= 5) {
            actor->opaque17FF = 0;
        blocked:
            if (actor->instance.locomotionState) {
                return 3;
            }
        reject:
            actor->instance.rootMotion.vx = 0;
            actor->instance.rootMotion.vz = 0;
            if (turning) {
                actor->instance.rootMotion.vy = 0;
                actor->instance.movementMode = 3;
                return 3;
            }
            actor->instance.supportLayer = actor->instance.placementLayer;
            actor->supportGimmick = previousArea;
            distance = 0;
            status = 3;
            goto done;
        }
        if (result >= 2 && (actor->instance.weaponDrawn || actor->actionPoseActive)) {
            goto blocked;
        }
        if (!actor->instance.movementMode && input->pad == 0 && result != 1 && actor->opaque17FF < 8) {
            actor->opaque17FF++;
            if (actor->instance.locomotionState != 2) {
                goto blocked;
            }
            return 3;
        }
        *(Field_SavedPositionWords*)&saved = *(Field_SavedPositionWords*)&motion;
        motion.vx = Field_collisionProbePosition.vx - actor->instance.position.vx;
        motion.vy = Field_collisionProbePosition.vy - actor->instance.position.vy;
        motion.vz = Field_collisionProbePosition.vz - actor->instance.position.vz;
        actor->instance.supportLayer = Field_collisionProbePosition.pad;
        if (result == 4) {
            if (actor->instance.locomotionState == 2) {
                goto fallStep;
            }
            if (Field_CanActorHangFromLedge(actor, &motion, &saved) == 0) {
                goto reject;
            }
            actor->supportGimmick = (u_short)Field_collisionProbePosition.pad >> 8;
            actor->ledgeHeight = Field_collisionProbePosition.vy;
            if (actor->supportGimmick) {
                object = Field_gimmickInstances[actor->supportGimmick];

                if (object->mode == 0xFD) {
                    for (i = object->objectInfo.supportIndex; i < 15; i++) {
                        motion.vy += (signed char)Field_kGimmickSpinHeightSteps[i];
                        actor->ledgeHeight += (signed char)Field_kGimmickSpinHeightSteps[i];
                    }
                }
            }
            if (((int (*)())Field_CalcActorLedgeOffset)(actor, &motion) == 0) {
                goto reject;
            }
            ((void (*)())Actors_SetActorMotion)(index, &motion, 4);
            ((void (*)(int, short, int))Actors_TurnActorToOverTime)(index, Field_collisionProbeDirection << 9, 4);
            actor->opaque17FF = 0;
            actor->instance.currentTileX = actor->instance.position.vx / 128;
            actor->instance.currentTileZ = actor->instance.position.vz / 128;
            if (actor->instance.previousTileX != actor->instance.currentTileX
                || actor->instance.previousTileZ != actor->instance.currentTileZ

                || actor->instance.previousTileLayer != actor->instance.supportLayer) {
                actor->instance.previousTileX = Field_collisionProbePosition.vx / 128;
                actor->instance.previousTileZ = Field_collisionProbePosition.vz / 128;
                actor->instance.previousTileLayer = actor->instance.supportLayer;
            }
        } else if (actor->instance.locomotionState == 2) {
        fallStep:
            if (actor->wallContact) {
                motion.vx = 0;
                motion.vz = 0;
            }
            ((void (*)())Actors_SetActorMotion)(index, &motion, 0x18);
            actor->ledgeHeight = 0;
        } else {
            if (input->pad == 0) {
                if ((u_short)(motion.vy + 0xFF) >= 0x100) {
                    goto blocked;
                }
                if (distance == 0) {
                    goto blocked;
                }
            }
            if (result >= 2) {
                if (actor->instance.movementMode && actor->wallContact) {
                    goto reject;
                }
                if (Field_HasActorLedgeSideSupport(actor) == 0) {
                    goto reject;
                }
            }
            ((void (*)())Actors_SetActorMotion)(index, &motion, result * 8);
        }
        if (result == 1) {
            animation = actor->animationId;
            if (animation >= 100) {
                animation -= 100;
            }
            if ((u_int)(animation - 13) >= 2 && animation != 0x13 && animation != 0x14
                && animation != 0x54) {
                Actors_PlayActorAnimation(index, Actors_kMotionResultOneAnimationId, 0, 4);
            }
            actor->instance.locomotionState = 1;
            actor->moveFacing = (actor->instance.facing + actor->instance.facingDelta) & 0xFFF;
            actor->instance.movementPending = 1;
        } else if (actor->instance.locomotionState == 2) {
            Actors_PlayActorAnimation(index, Actors_kHangMotionAnimationId, 0, 4);
            if (index == 0) {
                ((void (*)())Audio_PlayActorSoundPair)(actor, 0x2A, 0x2B);
            } else {
                Audio_PlayActorModelSound(&actor->instance, 0xC);
            }
            Actors_UpdateActorSteering(actor, facing, input);
        } else {
            if (result == 4) {
                actor->instance.locomotionState = 2;
            }
            Actors_PlayActorAnimation(index, Actors_kMotionResultAnimations[result], 0, 4);
            actor->moveFacing = (actor->instance.facing + actor->instance.facingDelta) & 0xFFF;
            if (index == 0) {
                ((void (*)())Audio_PlayActorSoundPair)(actor, 0x29, 0);
            } else {
                Audio_PlayActorModelSound(&actor->instance, 0xC);
            }
        }
        actor->instance.jumpCountdown = 0;
        actor->platformOffsetX = 0;
        actor->platformOffsetZ = 0;
        actor->instance.movementMode = 0;
        return 3;
    }

    if (motion.vy > 0) {
        if (actor->instance.locomotionState == 2) {
            motion.vx = 0;
            motion.vz = 0;
            goto fall;
        }
        if (actor->instance.supportLayer) {
            Field_MovingPlatformContactHeight(actor->instance.position.vx, actor->instance.position.vy,
                actor->instance.position.vz, 0);
            actor->instance.supportLayer = Actors_supportingPlatformLayer;
        }
        if (motion.vy >= 0x40) {
            if (input->pad == 0) {
                if (motion.vy >= 0x100) {
                    if (actor->instance.locomotionState != 2 && actor->opaque17FF < 8) {
                        actor->opaque17FF++;
                        goto blocked;
                    }
                    actor->instance.jumpCountdown = 0;
                }
            } else {
                Actors_SetActorFallDestination(actor, &motion);
            }
            actor->instance.rootMotion.vx = motion.vx;
            actor->instance.rootMotion.vz = motion.vz;
            actor->instance.rootMotion.vy = 0;
            Actors_PlayActorAnimation(index, 0x2F, 0, 4);
            actor->instance.movementMode = 3;
            actor->jumpStarted = 0;
            actor->jumpProfile = 1;
            if (distance >= actor->runSpeed) {
                actor->jumpProfile = 2;
                actor->jumpStarted = 1;
            }
            actor->instance.supportLayer = 0;
            return 3;
        }
        actor->opaque17FF = 0;
        if (!((*Field_collisionProbeTile >> 5) & 1)) {
            actor->collisionState = 5;
        }
        goto land;
    }
    actor->ledgeHeight = 0;
    actor->instance.locomotionState = 0;
    actor->instance.movementPending = 0;
    if (motion.vy < -2) {
        actor->collisionState = 2;
    }
    actor->opaque17FF = 0;
land:
    ((void (*)())Actors_SetActorTranslation)(index, &motion, 0);

done:
    if (!actor->instance.movementMode) {
        Actors_UpdateActorLocomotionAnimation(actor, distance, direction);
    }
    return status;
}

// decomp: BATTLE.PRG func_800A6660 (src/BATTLE/BATTLE.PRG/3A1A0.c:1920)
void Actors_UpdateActorSteering(Actors_ActorRecord* arg0, int arg1, Actors_TurnRequest* arg2)
{
    int delta;

    if (arg1 == arg0->instance.facing) {
        return;
    }
    if (arg0->instance.turnFrames != 0) {
        if (arg2->restartTurn == 0) {
            delta = arg1 - ((arg0->instance.facing + arg0->instance.facingDelta) & 0xFFF);
            if (delta >= ONE / 2) {
                delta -= ONE;
            } else if (delta < -ONE / 2) {
                delta += ONE;
            }
            arg0->instance.facingDelta += delta;
        } else {
            delta = arg1 - arg0->instance.facing;
            if (delta >= ONE / 2) {
                delta -= ONE;
            } else if (delta < -ONE / 2) {
                delta += ONE;
            }
            arg0->instance.facingDelta = delta;
            arg0->instance.turnFrames = 12;
        }
    } else {
        ((void (*)(int, short, int))Actors_TurnActorToOverTime)(arg0->instance.slot, arg1, 12);
    }
    if (arg0->collisionRadius > 0x80) {
        delta = arg0->instance.facingDelta / 32;
        if (delta < 0) {
            delta = -delta;
        }
        if (arg0->instance.turnFrames < delta) {
            arg0->instance.turnFrames = delta;
        }
    }
}

// decomp: BATTLE.PRG func_800A97EC (src/BATTLE/BATTLE.PRG/40564.c:522)
void Actors_MoveActorToTileAtRate(int arg0, Actors_TileTarget* arg1, int arg2, int arg3)
{
    int angle;

    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    if (arg2 == 0 && arg3 == 0) {

        Actors_SetActorTileTranslation(arg0, arg1, -1, 0);

        angle = arg1->facing;
        angle <<= 4;
        angle %= ONE;
        angle -= actor->instance.facing;

        Actors_SetActorYawTurn(arg0, (short)angle, 0);

        actor->routePhase = 0;
        actor->instance.currentTileX = arg1->tileX;
        actor->instance.currentTileLayer = arg1->tileLayer;
        actor->instance.currentTileZ = arg1->tileZ;
    } else {
        if (arg3 == -1) {
            actor->routeTurnRate = actor->turnRate;
        } else {
            actor->routeTurnRate = arg3;
        }

        if (arg2 == -1) {
            actor->routeMoveRate = actor->moveRate;
        } else {
            actor->routeMoveRate = arg2;
        }

        Actors_SetActorTileTurn(arg0, arg1, -1, actor->routeTurnRate);

        actor->routeTarget.vx = (arg1->tileX * 128) + 64;
        actor->routeTarget.vz = (arg1->tileZ * 128) + 64;
        actor->routeTarget.vy = Field_SampleActorSupportHeight(&actor->routeTarget, 0, 0, 1);
        angle = 0x8000;

        if (arg1->facing == 0xFF) {
            actor->routeTarget.pad = angle;
        } else {
            actor->routeTarget.pad = arg1->facing * 16;
        }

        actor->routePhase = 1;
        actor->routeUsesDuration = 0;
    }
}

// decomp: BATTLE.PRG func_800A9988 (src/BATTLE/BATTLE.PRG/40564.c:574)
void Actors_MoveActorToPositionAtRate(int id, SVECTOR* target, int arg2, int arg3)
{
    Actors_ActorRecord* actor = Actors_actorRecords[id];
    int angle;
    int dz;

    if (arg2 == 0 && arg3 == 0) {
        u_short facing = target->pad;

        if (facing == 0x8000) {
            angle = target->vx - actor->instance.position.vx;
            dz = target->vz - actor->instance.position.vz;

            if (angle == 0 && dz == 0) {
                angle = (short)facing;
            } else {
                angle = ONE * 3 / 4 - Core_Gte_Atan2(dz, angle);
                angle &= 0xFFF;
            }

            angle = (short)angle;
            actor->instance.facing = angle;
        } else {
            actor->instance.facing = facing;
        }

        actor->instance.turnFrames = 0;
        Actors_TranslateActorToAtRate(id, target, 0);
        return;
    }

    angle = target->vx - actor->instance.position.vx;
    dz = target->vz - actor->instance.position.vz;

    if (angle == 0 && dz == 0) {
        u_short facing = target->pad;

        if (facing == 0x8000) {
            return;
        }

        angle = (short)facing;
    } else {
        angle = ONE * 3 / 4 - Core_Gte_Atan2(dz, angle);
        angle &= 0xFFF;
    }

    angle = (short)angle;
    Actors_TurnActorToAtRate(id, angle, arg3);

    if (arg2 == -1) {
        actor->routeMoveRate = actor->moveRate;
    } else {
        actor->routeMoveRate = arg2;
    }

    if (arg3 == -1) {
        actor->routeTurnRate = actor->turnRate;
    } else {
        actor->routeTurnRate = arg3;
    }

    actor->routeTarget.vx = target->vx;
    actor->routeTarget.vz = target->vz;
    angle = (u_short)target->vy;

    if (angle == 0x8000) {
        angle = Field_InterpolatedFloorHeight(target->vx, target->vz);
        angle <<= 17;
        angle >>= 17;
        // BUG: passes the x tile twice
        dz = Field_GimmickSupportHeight(target->vx / 128, target->vx / 128);

        if (dz != 0) {
            angle = dz;
        }

        actor->routeTarget.vy = angle;
    } else {
        actor->routeTarget.vy = angle;
    }

    actor->routeTarget.pad = target->pad;
    actor->routePhase = 1;
    actor->routeUsesDuration = 0;
}

// decomp: BATTLE.PRG func_800A9C54 (src/BATTLE/BATTLE.PRG/40564.c:661)
void Actors_TranslateActorToTileAtRate(int arg0, Actors_TileTarget* arg1, int arg2)
{
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (arg2 == -1) {
        Actors_SetActorTileTranslation(arg0, arg1, -1, temp_s0->moveRate);
    } else {
        Actors_SetActorTileTranslation(arg0, arg1, -1, arg2);
    }

    temp_s0->instance.currentTileX = arg1->tileX;
    temp_s0->instance.currentTileLayer = arg1->tileLayer;
    temp_s0->instance.currentTileZ = arg1->tileZ;
}

// decomp: BATTLE.PRG func_800A9CDC (src/BATTLE/BATTLE.PRG/40564.c:676)
void Actors_TurnActorTowardTileAtRate(int arg0, Actors_TileTarget* arg1, int arg2)
{
    Actors_ActorRecord* new_var = Actors_actorRecords[arg0];

    if (arg2 == -1) {
        Actors_SetActorTileTurn(arg0, arg1, -1, new_var->turnRate);
    } else {
        Actors_SetActorTileTurn(arg0, arg1, -1, arg2);
    }
}

// decomp: BATTLE.PRG func_800A9D24 (src/BATTLE/BATTLE.PRG/40564.c:687)
void Actors_TranslateActorByAtRate(int arg0, SVECTOR* arg1, int arg2)
{
    u_short temp_s0;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    if (arg2 == -1) {
        Actors_SetActorTranslation(arg0, arg1, -1);
        return;
    }

    temp_s0 = temp_s1->moveRate;
    temp_s1->moveRate = arg2;

    Actors_SetActorTranslation(arg0, arg1, -1);

    temp_s1->moveRate = temp_s0;
}

// decomp: BATTLE.PRG func_800A9D90 (src/BATTLE/BATTLE.PRG/40564.c:705)
void Actors_TranslateActorToAtRate(int arg0, SVECTOR* arg1, int arg2)
{
    SVECTOR sp10;
    u_short temp_s0;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    sp10.vx = arg1->vx - temp_s1->instance.position.vx;
    sp10.vy = arg1->vy - temp_s1->instance.position.vy;
    sp10.vz = arg1->vz - temp_s1->instance.position.vz;

    if (arg2 == -1) {
        Actors_SetActorTranslation(arg0, &sp10, -1);
        return;
    }

    temp_s0 = temp_s1->moveRate;
    temp_s1->moveRate = arg2;

    Actors_SetActorTranslation(arg0, &sp10, -1);

    temp_s1->moveRate = temp_s0;
}

// decomp: BATTLE.PRG func_800A9E38 (src/BATTLE/BATTLE.PRG/40564.c:728)
void Actors_TurnActorByAtRate(int arg0, short arg1, int arg2)
{
    u_short temp_s0;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    if (arg2 == -1) {
        Actors_SetActorYawTurn(arg0, arg1, -1);
        return;
    }

    temp_s0 = temp_s1->turnRate;
    temp_s1->turnRate = arg2;

    Actors_SetActorYawTurn(arg0, arg1, -1);

    temp_s1->turnRate = temp_s0;
}

// decomp: BATTLE.PRG func_800A9EB4 (src/BATTLE/BATTLE.PRG/40564.c:746)
void Actors_TurnActorToAtRate(int arg0, short arg1, int arg2)
{
    u_short temp_s0;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    arg1 -= temp_s1->instance.facing;

    if (arg1 > ONE / 2) {
        arg1 -= ONE;
    }

    if (arg1 < -ONE / 2) {
        arg1 += ONE;
    }

    if (arg2 == -1) {
        Actors_SetActorYawTurn(arg0, arg1, -1);
        return;
    }

    temp_s0 = temp_s1->turnRate;
    temp_s1->turnRate = arg2;

    Actors_SetActorYawTurn(arg0, arg1, -1);

    temp_s1->turnRate = temp_s0;
}

// decomp: BATTLE.PRG func_800A9F64 (src/BATTLE/BATTLE.PRG/40564.c:774)
void Actors_RotateActorByAtRate(int arg0, SVECTOR* arg1, int arg2)
{
    u_short temp_s0;

    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    if (arg2 == -1) {
        Actors_SetActorRotation(arg0, arg1, -1);
        return;
    }

    temp_s0 = temp_s1->turnRate;
    temp_s1->turnRate = arg2;

    Actors_SetActorRotation(arg0, arg1, -1);

    temp_s1->turnRate = temp_s0;
}

// decomp: BATTLE.PRG func_800A9FD0 (src/BATTLE/BATTLE.PRG/40564.c:793)
void Actors_RotateActorToAtRate(int arg0, SVECTOR* arg1, int arg2)
{
    SVECTOR sp10;
    int temp_s0;
    Actors_ActorRecord* temp_s1 = Actors_actorRecords[arg0];

    sp10.vx = arg1->vx - temp_s1->instance.rotationX;

    if (sp10.vx > ONE / 2) {
        sp10.vx -= ONE;
    }

    if (sp10.vx < -ONE / 2) {
        sp10.vx += ONE;
    }

    sp10.vy = arg1->vy - temp_s1->instance.facing;

    if (sp10.vy > ONE / 2) {
        sp10.vy -= ONE;
    }

    if (sp10.vy < -ONE / 2) {
        sp10.vy += ONE;
    }

    sp10.vz = arg1->vz - temp_s1->instance.rotationZ;

    if (sp10.vz > ONE / 2) {
        sp10.vz -= ONE;
    }

    if (sp10.vz < -ONE / 2) {
        sp10.vz += ONE;
    }

    if (arg2 == -1) {
        Actors_SetActorRotation(arg0, &sp10, -1);
        return;
    }

    temp_s0 = temp_s1->turnRate;
    temp_s1->turnRate = arg2;

    Actors_SetActorRotation(arg0, &sp10, -1);

    temp_s1->turnRate = temp_s0;
}

// decomp: BATTLE.PRG func_800AA108 (src/BATTLE/BATTLE.PRG/40564.c:842)
void Actors_MoveActorToTileOverTime(int arg0, Actors_TileTarget* arg1, int arg2, int arg3)
{
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0 != NULL) {

        temp_s0->routeMoveRate = arg2;

        if ((arg2 == 0) && (arg3 == 0)) {
            int temp_a1;

            Actors_SetActorTileTranslation(arg0, arg1, -1, 0);

            temp_a1 = arg1->facing;
            temp_a1 *= 0x10;
            temp_a1 %= 0x1000;
            temp_a1 -= temp_s0->instance.facing;

            Actors_SetActorYawTurn(arg0, temp_a1, 0);
            temp_s0->routePhase = 0;
        } else {
            Actors_SetActorTileTurn(arg0, arg1, arg3, temp_s0->turnRate);
            temp_s0->routePhase = 1;
        }

        temp_s0->instance.currentTileX = arg1->tileX;
        temp_s0->instance.currentTileLayer = arg1->tileLayer;
        temp_s0->instance.currentTileZ = arg1->tileZ;
        temp_s0->routeUsesDuration = 1;
    }
}

// decomp: BATTLE.PRG func_800AA218 (src/BATTLE/BATTLE.PRG/40564.c:874)
void Actors_TranslateActorToTileOverTime(int arg0, Actors_TileTarget* arg1, int arg2)
{
    Actors_ActorRecord* temp_s0 = Actors_actorRecords[arg0];

    if (temp_s0 != NULL) {
        Actors_SetActorTileTranslation(arg0, arg1, arg2, temp_s0->moveRate);
        temp_s0->instance.currentTileX = arg1->tileX;
        temp_s0->instance.currentTileLayer = arg1->tileLayer;
        temp_s0->instance.currentTileZ = arg1->tileZ;
    }
}

// decomp: BATTLE.PRG func_800AA290 (src/BATTLE/BATTLE.PRG/40564.c:886)
void Actors_SetActorTileTranslation(int arg0, Actors_TileTarget* arg1, int arg2, int arg3)
{
    SVECTOR target;
    int dist;
    int temp;
    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    if (actor == NULL) {
        return;
    }

    target.vx = arg1->tileX * 128 + 64;
    target.vz = arg1->tileZ * 128 + 64;
    actor->instance.rootMotion.vx = target.vx - actor->instance.position.vx;
    actor->instance.rootMotion.vz = target.vz - actor->instance.position.vz;
    actor->instance.rootMotion.vy = Field_SampleActorSupportHeight(&target, 0, 0, 4) - actor->instance.position.vy;

    if ((actor->instance.rootMotion.vx == 0) && (actor->instance.rootMotion.vz == 0)
        && (actor->instance.rootMotion.vy == 0)) {
        return;
    }

    if (arg2 == 0) {
        actor->instance.currentAnimation = 0;
    move:
        addVector(&actor->instance.position, &actor->instance.rootMotion);
        return;
    }

    if (arg2 == -1) {
        temp = actor->instance.rootMotion.vx;
        temp *= temp;
        if (temp < 0) {
            temp = -temp;
        }
        dist = temp;
        temp = actor->instance.rootMotion.vz;
        temp *= temp;
        if (temp < 0) {
            temp = -temp;
        }
        dist += temp;
        temp = actor->instance.rootMotion.vy;
        temp *= temp;
        if (temp < 0) {
            temp = -temp;
        }
        dist = Core_Math_SquareRootInteger(dist + temp) * arg3;
        temp = dist / 128;
        actor->instance.currentAnimation = temp;
        if (dist & 0x7F) {
            actor->instance.currentAnimation = temp + 1;
        }
        if (actor->instance.currentAnimation == 0) {
            goto move;
        }
        return;
    }

    actor->instance.currentAnimation = arg2;
}

// decomp: BATTLE.PRG func_800AA454 (src/BATTLE/BATTLE.PRG/40564.c:948)
void Actors_TurnActorTowardTileOverTime(int arg0, Actors_TileTarget* arg1, int arg2)
{
    Actors_SetActorTileTurn(arg0, arg1, arg2, Actors_actorRecords[arg0]->turnRate);
}

// decomp: BATTLE.PRG func_800AA490 (src/BATTLE/BATTLE.PRG/40564.c:953)
void Actors_SetActorTileTurn(int arg0, Actors_TileTarget* arg1, int arg2, int arg3)
{
    int angle;
    int dz;
    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    if (actor == NULL) {
        return;
    }

    angle = arg1->tileX - actor->instance.currentTileX;
    dz = arg1->tileZ - actor->instance.currentTileZ;

    if (angle == 0 && dz == 0) {
        angle = arg1->facing * 16;
    } else {
        angle = 0xC00 - Core_Gte_Atan2(dz, angle);
        angle %= ONE;
    }

    angle -= actor->instance.facing;

    if (angle >= ONE / 2) {
        angle -= ONE;
    }

    if (angle < -ONE / 2) {
        angle += ONE;
    }

    actor->instance.facingDelta = angle;
    actor->instance.rotationStepX = 0;
    actor->instance.rotationStepZ = 0;

    if (angle == 0) {
        return;
    }

    if (arg2 == 0) {
        actor->instance.turnFrames = 0;
    turn:
        actor->instance.facing = (actor->instance.facing + actor->instance.facingDelta) & 0xFFF;
        return;
    }

    if (arg2 == -1) {
        if (angle < 0) {
            angle = -angle;
        }
        angle *= arg3;
        actor->instance.turnFrames = angle / ONE;
        if (angle & 0xFFF) {
            actor->instance.turnFrames = angle / ONE + 1;
        }
        if (actor->instance.turnFrames == 0) {
            goto turn;
        }
        return;
    }

    actor->instance.turnFrames = arg2;
}

// decomp: BATTLE.PRG func_800AA600 (src/BATTLE/BATTLE.PRG/40564.c:1016)
void Actors_SetActorMotion(int arg0, SVECTOR* arg1, int arg2) { Actors_SetActorTranslation(arg0, arg1, arg2); }

// decomp: BATTLE.PRG func_800AA620 (src/BATTLE/BATTLE.PRG/40564.c:1018)
void Actors_TranslateActorToOverTime(int arg0, SVECTOR* arg1, int arg2)
{
    SVECTOR sp10;
    Actors_ActorRecord* temp_a3 = Actors_actorRecords[arg0];

    if (temp_a3 != NULL) {
        sp10.vx = arg1->vx - temp_a3->instance.position.vx;
        sp10.vy = arg1->vy - temp_a3->instance.position.vy;
        sp10.vz = arg1->vz - temp_a3->instance.position.vz;

        Actors_SetActorTranslation(arg0, &sp10, arg2);
    }
}

// decomp: BATTLE.PRG func_800AA698 (src/BATTLE/BATTLE.PRG/40564.c:1032)
void Actors_SetActorTranslation(int arg0, SVECTOR* arg1, int arg2)
{
    int dist;
    int temp;
    Actors_ActorRecord* actor = Actors_actorRecords[arg0];

    if (actor == NULL) {
        return;
    }

    actor->instance.rootMotion.vx = arg1->vx;
    actor->instance.rootMotion.vy = arg1->vy;
    actor->instance.rootMotion.vz = arg1->vz;

    if (arg2 == 0) {
        actor->instance.currentAnimation = 0;
    move:
        actor->instance.position.vx += arg1->vx;
        actor->instance.position.vy += arg1->vy;
        actor->instance.position.vz += arg1->vz;
        actor->instance.currentTileX = actor->instance.position.vx / 128;
        actor->instance.currentTileZ = actor->instance.position.vz / 128;
        return;
    }

    if (arg2 == -1) {
        temp = actor->instance.rootMotion.vx;
        temp *= temp;
        if (temp < 0) {
            temp = -temp;
        }
        dist = temp;
        temp = actor->instance.rootMotion.vz;
        temp *= temp;
        if (temp < 0) {
            temp = -temp;
        }
        dist += temp;
        temp = actor->instance.rootMotion.vy;
        temp *= temp;
        if (temp < 0) {
            temp = -temp;
        }
        dist = Core_Math_SquareRootInteger(dist + temp) * actor->moveRate;
        actor->instance.currentAnimation = dist / 128;
        if (dist & 0x7F) {
            actor->instance.currentAnimation = dist / 128 + 1;
        }
        if (actor->instance.currentAnimation == 0) {
            goto move;
        }
        return;
    }

    actor->instance.currentAnimation = arg2;
}

// decomp: BATTLE.PRG func_800AA82C (src/BATTLE/BATTLE.PRG/40564.c:1089)
void Actors_SetActorYawMotion(int arg0, short arg1, int arg2) { Actors_SetActorYawTurn(arg0, arg1, arg2); }

// decomp: BATTLE.PRG func_800AA850 (src/BATTLE/BATTLE.PRG/40564.c:1091)
void Actors_TurnActorToOverTime(int arg0, u_short arg1, int arg2)
{
    short temp_a1;
    short var_v1;
    Actors_ActorRecord* temp_v0 = Actors_actorRecords[arg0];

    if (temp_v0 == NULL) {
        return;
    }

    temp_a1 = arg1 - temp_v0->instance.facing;
    var_v1 = temp_a1;

    if (temp_a1 > ONE / 2) {
        var_v1 = temp_a1 - ONE;
    }

    if (var_v1 < -ONE / 2) {
        var_v1 += ONE;
    }

    Actors_SetActorYawTurn(arg0, var_v1, arg2);
}

// decomp: BATTLE.PRG func_800AA8D0 (src/BATTLE/BATTLE.PRG/40564.c:1115)
void Actors_TurnActorToAtAngularSpeed(int arg0, short arg1, int arg2)
{
    int arg2_2;
    int var_v0;
    Actors_ActorRecord* temp_v0 = Actors_actorRecords[arg0];

    if (temp_v0 == NULL) {
        return;
    }

    arg1 -= temp_v0->instance.facing;

    if (arg1 > ONE / 2) {
        arg1 -= ONE;
    }

    if (arg1 < -ONE / 2) {
        arg1 += ONE;
    }

    var_v0 = arg1;

    var_v0 = ABS(var_v0);
    arg2 = ABS(arg2);

    arg2_2 = var_v0 / arg2;

    if ((var_v0 % arg2) != 0) {
        ++arg2_2;
    }

    Actors_SetActorYawTurn(arg0, arg1, arg2_2);
}

// decomp: BATTLE.PRG func_800AA984 (src/BATTLE/BATTLE.PRG/40564.c:1149)
void Actors_SetActorYawTurn(int arg0, short arg1, int arg2)
{
    int m;
    Actors_ActorRecord* a = Actors_actorRecords[arg0];

    if (a == NULL) {
        return;
    }

    m = arg1;
    a->instance.facingDelta = m;
    a->instance.rotationStepX = 0;
    a->instance.rotationStepZ = 0;

    if (arg2 == 0) {
        short sum;
        a->instance.turnFrames = 0;
    recenter:
        sum = (u_short)a->instance.facing + a->instance.facingDelta;
        a->instance.facing = sum;
        a->instance.facing = sum % ONE;
    } else if (arg2 == -1) {
        int speed;

        if (m < 0) {
            m = -m;
        }

        speed = m * a->turnRate;
        a->instance.turnFrames = speed / ONE;

        if (speed & 0xFFF) {
            a->instance.turnFrames = (speed / ONE) + 1;
        }

        if (a->instance.turnFrames != 0) {
            return;
        }

        goto recenter;

    } else {
        a->instance.turnFrames = arg2;
    }
}

// decomp: BATTLE.PRG func_800AAA68 (src/BATTLE/BATTLE.PRG/40564.c:1195)
void Actors_SetActorRotationMotion(int arg0, SVECTOR* arg1, int arg2) { Actors_SetActorRotation(arg0, arg1, arg2); }

// decomp: BATTLE.PRG func_800AAA88 (src/BATTLE/BATTLE.PRG/40564.c:1197)
void Actors_SetActorRotation(int arg0, SVECTOR* arg1, int arg2)
{
    Actors_ActorRecord* a = Actors_actorRecords[arg0];
    int m;

    a->instance.rotationStepX = arg1->vx;
    a->instance.facingDelta = arg1->vy;
    a->instance.rotationStepZ = arg1->vz;

    if (arg2 == 0) {
        a->instance.turnFrames = 0;
    recenter:
        a->instance.rotationX += a->instance.rotationStepX;
        a->instance.facing += a->instance.facingDelta;
        a->instance.rotationZ += a->instance.rotationStepZ;
        a->instance.rotationX &= 0xFFF;
        a->instance.facing &= 0xFFF;
        a->instance.rotationZ &= 0xFFF;
    } else if (arg2 == -1) {
        m = ABS(a->instance.rotationStepX);

        if (m < ABS(a->instance.facingDelta)) {
            m = ABS(a->instance.facingDelta);
        }

        if (m < ABS(a->instance.rotationStepZ)) {
            m = ABS(a->instance.rotationStepZ);
        }

        m *= a->turnRate;
        a->instance.turnFrames = m / ONE;

        if (m & 0xFFF) {
            a->instance.turnFrames = (m / ONE) + 1;
        }

        if (a->instance.turnFrames != 0) {
            return;
        }

        goto recenter;

    } else {
        a->instance.turnFrames = arg2;
    }
}

// decomp: BATTLE.PRG func_800AABD0 (src/BATTLE/BATTLE.PRG/40564.c:1244)
void Actors_ScaleActorToOverTime(int arg0, SVECTOR* arg1, int arg2)
{
    Actors_ActorRecord* temp_a3 = Actors_actorRecords[arg0];

    if (temp_a3 != NULL) {

        if (arg2 == 0) {
            temp_a3->scaleFrames = 0;
            temp_a3->instance.scaleX = arg1->vx;
            temp_a3->instance.scaleY = arg1->vy;
            temp_a3->instance.scaleZ = arg1->vz;
            return;
        }

        temp_a3->instance.scaleDelta.vx = arg1->vx - temp_a3->instance.scaleX;
        temp_a3->instance.scaleDelta.vy = arg1->vy - temp_a3->instance.scaleY;
        temp_a3->instance.scaleDelta.vz = arg1->vz - temp_a3->instance.scaleZ;

        if ((*(int*)&temp_a3->instance.scaleDelta == 0)
            && ((temp_a3->instance.scaleDelta.vz << 0x10) == 0)) {
            temp_a3->scaleFrames = 0;
            return;
        }

        temp_a3->scaleFrames = arg2;
    }
}

// decomp: BATTLE.PRG func_800AC540 (src/BATTLE/BATTLE.PRG/40564.c:2233)
void Actors_UpdateActorRoute(int arg0, Actors_ActorRecord* arg1)
{

    switch (arg1->routePhase) {
    case 1:
        if (arg1->instance.turnFrames == 0) {

            if (arg1->instance.motionLocked) {
                ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, 0x1F, 0, 8);
            } else {
                ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, arg1->travelAnimation, 0, 8);
            }

            if (!(arg1->routeUsesDuration << 2)) {
                Actors_TranslateActorToAtRate(arg0, &arg1->routeTarget, arg1->routeMoveRate);
            } else {
                Actors_TranslateActorToOverTime(arg0, &arg1->routeTarget, arg1->routeMoveRate);
            }

            arg1->routePhase = 2;
        }
        break;

    case 2:
        if (arg1->instance.currentAnimation == 0) {
            int temp_a1;

            ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, 1, 0, 8);

            temp_a1 = (u_short)arg1->routeTarget.pad;

            if (temp_a1 != 0x8000) {
                temp_a1 = temp_a1 & 0xFFF;
                if ((arg1->routeUsesDuration << 2) == 0) {
                    Actors_TurnActorToAtRate(arg0, temp_a1, arg1->routeTurnRate);
                } else {
                    Actors_TurnActorToOverTime(arg0, temp_a1, arg1->routeTurnRate);
                }
            }
            arg1->routePhase = 0;
        }
        break;
    }
}

// decomp: BATTLE.PRG func_800AC690_getProfile (src/BATTLE/BATTLE.PRG/40564.c:2278)
int Actors_ActorJumpAnimationProfile(Actors_ActorRecord* actor)
{
    u_int flags = *(u_int*)((char*)actor + 8);

    if (flags & 0xF00) {
        return 3;
    }
    if (flags & 0x400000) {
        return 4;
    }
    if (flags & 0x200000) {
        return 5;
    }
    if (actor->actionPoseActive) {
        return 6;
    }
    return actor->jumpProfile & 3;
}

// decomp: BATTLE.PRG func_800AC690 (src/BATTLE/BATTLE.PRG/40564.c:2297)
void Actors_UpdateActorJumpAndFall(int arg0, Actors_ActorRecord* actor)
{
    Field_GimmickInstance* platform;
    SVECTOR* position;
    int profile;
    int state;
    int limit;
    int floor;
    int y;
    int ground;
    int top;
    int vy;
    int nextX;
    int nextZ;
    int hit;
    short speed;
    u_short velocity;
    signed char timer;
    u_int flags;

    profile = Actors_ActorJumpAnimationProfile(actor);
    flags = *(u_int*)((char*)actor + 8);
    state = (flags >> 16) & 7;

    switch (state) {
    case 2:
        if ((signed char)actor->instance.jumpCountdown > 0) {
            timer = actor->instance.jumpCountdown - State_tickSpeed / 2;
            actor->instance.jumpCountdown = timer;
            if (timer < 2) {
                ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, Actors_kAirTimerEndAnimations[profile], 0, 8);
                actor->instance.jumpCountdown = 0;
            }
        } else {
            if (actor->destinationMode == 0) {
                actor->instance.rootMotion.vy = -30;
            }
            actor->jumpLanding = 0;
            actor->instance.movementMode = 3;
            if (actor->instance.supportLayer) {
                ((void (*)(int, int))Field_SetMovingPlatformActor)(actor->instance.supportLayer - 2, -1);
                actor->instance.supportLayer = 0;
            }
        }
        break;
    case 3:
        if ((signed char)actor->instance.jumpCountdown > 0) {
            timer = actor->instance.jumpCountdown - State_tickSpeed / 2;
            actor->instance.jumpCountdown = timer;
            if (timer > 0) {
                break;
            }
            ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(actor->instance.slot, 0x2F, 0, 8);
            actor->instance.jumpCountdown = 0;
        }
        if (actor->instance.isJumping) {
            y = State_tickSpeed / 2;
            ((Actors_AirborneTickAccumulator*)&actor->sequenceFlags)->tickRemainder += y;
            limit = 7;
            y = ((Actors_AirborneTickAccumulator*)&actor->sequenceFlags)->tickRemainder >> 1;
            ((Actors_AirborneTickAccumulator*)&actor->sequenceFlags)->tickRemainder -= y * 2;
        } else {
            limit = 9;
            y = (State_tickSpeed / 2) * state;
        }
        velocity = actor->instance.rootMotion.vy + y;
        actor->instance.rootMotion.vy = velocity;
        if ((short)velocity < 0) {
            y = 1;
            floor = Field_CeilingHeightAt(actor->instance.position.vx, actor->instance.position.vz);
            floor <<= 17;
            floor >>= 17;
            if (arg0 != 0) {
                y = 3;
                Field_platformContactSearchDepth = actor->collisionHeight + 0x40;
            }
            ground = Field_MovingPlatformContactHeight(actor->instance.position.vx, actor->instance.position.vy,
                actor->instance.position.vz, y);
            if (ground != 0 && floor < ground) {
                floor = ground;
            }
            top = actor->instance.position.vy + actor->instance.rootMotion.vy - 0xC0;
            if (top < floor) {
                actor->instance.rootMotion.vy = -actor->instance.rootMotion.vy;
                if (arg0 != 0) {
                    actor->instance.rootMotion.vx = 0;
                    actor->instance.rootMotion.vz = 0;
                    actor->destinationMode = 0;
                    ((void (*)())Battle_Ai_ReleaseAiDestinationTile)(
                        ((u_char*)&actor->routeTarget)[0], ((u_char*)&actor->routeTarget)[2]);
                }
            }
        }
        y = actor->instance.rootMotion.vy;
        if (y <= limit) {
            if (y >= -limit) {
                ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, Actors_kSoftLandingAnimations[profile], 0, 16);
            }
        } else {
            ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, Actors_kHardLandingAnimations[profile], 0, 8);
        }
        switch (actor->destinationMode) {
        case 0:
            if (actor->instance.rootMotion.vy < 0) {
                goto fall;
            }
            break;
        case 1:
        slide:
            speed = actor->instance.rootMotion.vx;
            if (speed != 0) {
                nextX = actor->instance.position.vx + speed;
                if (speed > 0) {
                    hit = actor->destinationX < nextX;
                } else {
                    hit = nextX < actor->destinationX;
                }
                if (hit) {
                    actor->instance.rootMotion.vx = 0;
                }
            }
            speed = actor->instance.rootMotion.vz;
            if (speed != 0) {
                nextZ = actor->instance.position.vz + speed;
                if (speed > 0) {
                    hit = actor->destinationZ < nextZ;
                } else {
                    hit = nextZ < actor->destinationZ;
                }
                if (hit) {
                    actor->instance.rootMotion.vz = 0;
                }
            }
            break;
        case 2:
            vy = actor->instance.rootMotion.vy;
            --actor->jumpFrames;
            if (vy < 0) {
                goto fall;
            }
            y = Field_MovingPlatformContactHeight(actor->instance.position.vx + actor->instance.rootMotion.vx,
                actor->instance.position.vy + vy,
                actor->instance.position.vz + actor->instance.rootMotion.vz, 2);
            if ((*(int (*))&Actors_supportingPlatformLayer) != 0 && (*(int (*))&Actors_supportingPlatformLayer) != ((u_char*)&actor->routeTarget)[1]) {
                actor->contactKind = (*(int (*))&Actors_supportingPlatformLayer);
                actor->instance.currentTileX = actor->instance.position.vx / 128;
                actor->instance.currentTileZ = actor->instance.position.vz / 128;
                actor->instance.currentTileLayer = 0;
                ((void (*)(int, int))Field_SetMovingPlatformActor)((*(int (*))&Actors_supportingPlatformLayer) - 2, arg0);
                ((void (*)(int, int))Field_SetMovingPlatformActor)((*(int (*))&Actors_supportingPlatformLayer) - 2, -1);
                (*(int (*))&Actors_supportingPlatformLayer) = 0;
                Battle_Ai_ForceReleaseActorAiReservations(arg0);
                goto land;
            }
            if (actor->jumpFrames > 0) {
                goto fall;
            }
            goto slide;
        default:
            break;
        }
        position = &actor->instance.position;
        y = Field_SampleActorSupportHeight(position, actor->instance.rootMotion.vx, actor->instance.rootMotion.vz, 1);
        if (y == 3000 && Field_kTileSurfaceIds[*Field_collisionProbeTile & 0x1F] == 20) {
            if (actor->instance.position.vy <= 0
                && actor->instance.position.vy + actor->instance.rootMotion.vy >= 0) {
                ((void (*)(SVECTOR*))Effects_SpawnGroundPlaneImpactEffect)(position);
            }
        }
        top = actor->instance.position.vy + actor->instance.rootMotion.vy;
        if (top < y) {
            goto fall;
        }
        if ((*(int (*))&Actors_supportingPlatformLayer) != 0) {
            if (y - top < -0x80) {
                actor->instance.rootMotion.vx = 0;
                actor->instance.rootMotion.vz = 0;
                goto fall;
            }
        }
    land:
        if (Actors_supportingGimmickIndex != 0) {
            platform = Field_gimmickInstances[Actors_supportingGimmickIndex];
            if (platform->mode != 0) {
                actor->instance.position.vy = platform->positionY - 0x80;
                break;
            }
        }
        *(short*)&actor->platformOffsetX = 0;
        actor->instance.supportLayer = (*(int (*))&Actors_supportingPlatformLayer);
        actor->instance.currentTileLayer = (*(int (*))&Actors_supportingPlatformLayer);
        actor->supportGimmick = Actors_supportingGimmickIndex;
        actor->instance.rootMotion.vy = y - actor->instance.position.vy;
        actor->instance.movementMode = 4;
        Actors_SetActorTranslation(arg0, &actor->instance.rootMotion, 0);
        floor = actor->instance.position.vy - ((SVECTOR*)&actor->instance.supportOffset)->vy;
        floor /= 64;
        actor->instance.jumpCountdown = 0;
        if (y < 3000) {
            if (actor->instance.slot != 0 || !actor->instance.actionAnimationState || actor->instance.actionAnimationState >= 6) {
                if (floor >= 4) {
                    Audio_PlayActorSurfaceContactSound(arg0, actor, 0x42);
                    if (floor >= 6 && arg0 == 0) {
                        actor->instance.jumpCountdown = 8;
                    }
                } else {
                    Audio_PlayActorSurfaceContactSound(arg0, actor, 0x41);
                }
            }
        }
        actor->destinationMode = 0;
        actor->landingPending = 0;
        ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, Actors_kLandingRecoveryAnimations[profile], 0, 8);
        break;
    fall:
        Actors_SetActorTranslation(arg0, &actor->instance.rootMotion, 0);
        break;
    case 4:
        if (flags & 0x200000) {
            actor->instance.motionLocked = 0;
            actor->instance.isJumping = 0;
        }
        Actors_SaveActorGroundedTransform((Field_GimmickInstance*)actor);
        actor->instance.jumpCountdown -= State_tickSpeed / 2;
        if ((signed char)actor->instance.jumpCountdown < 0) {
            actor->instance.jumpCountdown = 0;
        }
        if ((signed char)actor->instance.jumpCountdown == 0) {
            actor->ledgeHeight = 0;
            if (actor->instance.slot != 0) {
                if (!actor->instance.isJumping && actor->blendFrame != 0) {
                    break;
                }
                if (profile != 3) {
                    ((void (*)(int, int, int, int))Actors_PlayActorAnimation)(arg0, 1, 0, 8);
                }
                ((void (*)())Battle_Ai_ReleaseAiDestinationTile)(actor->instance.currentTileX, actor->instance.currentTileZ);
                Battle_Ai_ReleaseActorAiContactOccupancy(actor->instance.slot);
            }
            actor->runFrames = 0;
            actor->instance.movementMode = 0;
        }
        break;
    }
}

// decomp: BATTLE.PRG func_800ACF54 (src/BATTLE/BATTLE.PRG/40564.c:2548)
void Actors_SaveActorGroundedTransform(Field_GimmickInstance* arg0)
{

    arg0->savedTile = *(int*)&arg0->tileX;
    *(Core_WordPair*)&arg0->savedPosition = *(Core_WordPair*)&arg0->positionX;

    if (!(arg0->supportLayer)) {
        arg0->savedPlacement = *(int*)&arg0->tileX;
    }

    arg0->savedRotationY = arg0->rotationY;
}

// decomp: BATTLE.PRG func_800AF960 (src/BATTLE/BATTLE.PRG/44F14.c:1225)
void Actors_UpdateActorAuxiliaryMotion(Field_GimmickInstance* arg0)
{
    int temp_lo;

    Actors_ShadowBlend* a2 = &arg0->shadowBlend;

    if (a2->levelFrames != 0) {
        temp_lo = a2->levelDelta / a2->levelFrames;
        a2->levelDelta -= temp_lo;
        a2->level += temp_lo;
        --a2->levelFrames;
    }

    a2->level &= 0xFFF;

    if (a2->radiusFrames != 0) {
        temp_lo = a2->radiusExpandDelta / a2->radiusFrames;
        a2->radiusExpandDelta -= temp_lo;
        a2->radiusExpandRate += temp_lo;
        temp_lo = a2->radiusShrinkDelta / a2->radiusFrames;
        a2->radiusShrinkDelta -= temp_lo;
        a2->radiusShrinkRate += temp_lo;
        --a2->radiusFrames;
    }
}

// decomp: BATTLE.PRG func_800B63AC (src/BATTLE/BATTLE.PRG/4A0A8.c:340) [inline-asm: not yet ported]
void Actors_BeginTileJump(int, int*, int, int)
{
    VS_UNPORTED("Actors::BeginTileJump");
}

// decomp: BATTLE.PRG func_800B6404 (src/BATTLE/BATTLE.PRG/4A0A8.c:368) [inline-asm: not yet ported]
void Actors_BeginAirborneTileJump(int, int*, int, int)
{
    VS_UNPORTED("Actors::BeginAirborneTileJump");
}

// decomp: BATTLE.PRG func_800B6438 (src/BATTLE/BATTLE.PRG/4A0A8.c:383) [C: not yet ported]
void Actors_BeginNavigationJump(int, int*, int, int)
{
    VS_UNPORTED("Actors::BeginNavigationJump");
}

// decomp: BATTLE.PRG func_800B64A8 (src/BATTLE/BATTLE.PRG/4A0A8.c:415) [inline-asm: not yet ported]
void Actors_SetDirectMovement(int, int, int)
{
    VS_UNPORTED("Actors::SetDirectMovement");
}

// decomp: BATTLE.PRG func_800B64CC (src/BATTLE/BATTLE.PRG/4A0A8.c:428) [C: not yet ported]
void Actors_QueueNavigationTileTarget(int, int, int, int)
{
    VS_UNPORTED("Actors::QueueNavigationTileTarget");
}

// decomp: BATTLE.PRG func_800B6500 (src/BATTLE/BATTLE.PRG/4A0A8.c:445) [inline-asm: not yet ported]
void Actors_QueueGroundOffset(int, SVECTOR*)
{
    VS_UNPORTED("Actors::QueueGroundOffset");
}

// decomp: BATTLE.PRG func_800B6530 (src/BATTLE/BATTLE.PRG/4A0A8.c:459) [inline-asm: not yet ported]
void Actors_QueueMovementVector(int, SVECTOR*, int)
{
    VS_UNPORTED("Actors::QueueMovementVector");
}

// decomp: BATTLE.PRG func_800B653C (src/BATTLE/BATTLE.PRG/4A0A8.c:467) [inline-asm: not yet ported]
void Actors_QueueFlightMovement(int, SVECTOR*, int, int)
{
    VS_UNPORTED("Actors::QueueFlightMovement");
}

// decomp: BATTLE.PRG func_800B6580 (src/BATTLE/BATTLE.PRG/4A0A8.c:490) [inline-asm: not yet ported]
void Actors_BeginTeleport(int, int*, int)
{
    VS_UNPORTED("Actors::BeginTeleport");
}

// decomp: BATTLE.PRG func_800B65E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:522) [inline-asm: not yet ported]
void Actors_BeginTileTravel(int, int*, int)
{
    VS_UNPORTED("Actors::BeginTileTravel");
}

// decomp: BATTLE.PRG func_800B65F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:531) [inline-asm: not yet ported]
void Actors_BeginTimedTravel(int, int*, int)
{
    VS_UNPORTED("Actors::BeginTimedTravel");
}

// decomp: BATTLE.PRG func_800B6634 (src/BATTLE/BATTLE.PRG/4A0A8.c:551) [C: not yet ported]
void Actors_TurnNavigationActor(int, int)
{
    VS_UNPORTED("Actors::TurnNavigationActor");
}

// decomp: BATTLE.PRG func_800B668C (src/BATTLE/BATTLE.PRG/4A0A8.c:578) [inline-asm: not yet ported]
void Actors_TurnActorAtSpeed(int arg0, short arg1, int arg2)
{
    VS_UNPORTED("Actors::TurnActorAtSpeed");
}

