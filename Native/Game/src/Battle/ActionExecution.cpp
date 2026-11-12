// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Battle/TargetingLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/CutsceneLifecycleLive.hpp"
#include "Field/CameraLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Items/LootLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/TargetingState.h"
#include "Effects/Runtime.h"
#include "Event/RoomTriggerEntryLive.h"
#include "Field/CameraLiveState.h"
#include "Field/Interactions.h"
#include "Menu/TextBox.h"
#include "Render/Camera.h"

// decomp: D_800F19D8 (D_800F19D8@BATTLE/BATTLE.PRG)
int Battle_settledCameraDistance;

// decomp: D_800F19EC (D_800F19EC@BATTLE/BATTLE.PRG)
int Battle_cameraEaseMode;

// decomp: BATTLE.PRG func_80077F14 (src/BATTLE/BATTLE.PRG/146C.c:6821)
void Battle_ActionTargetLimbPosition(int actor_slot, int limb_tag, SVECTOR* position_out)
{
    int bone_index;

    if (limb_tag == -1) {
        bone_index = 0;
    } else {
        bone_index = VS::Actors::FindLiveActorBoneTag(actor_slot, limb_tag, 2);
    }
    VS::Actors::LiveActorBonePosition(actor_slot, bone_index, position_out, 0);
}

// decomp: BATTLE.PRG func_80077F70 (src/BATTLE/BATTLE.PRG/146C.c:6833)
int Battle_ActionTargetDistance(void)
{
    SVECTOR source_position;
    SVECTOR target_position;
    Battle_ActionRequest* queued_action;
    int delta_x;
    int delta_z;
    int horizontal_distance;
    int delta_y;

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex];
    VS::Actors::LiveActorBonePosition(queued_action->primaryTarget.target.targetActor, 0, &source_position, 0);
    if (queued_action->targetCount != 0) {
        if (queued_action->targets[0].targetsPosition == 0) {
            Battle_ActionTargetLimbPosition(queued_action->targets[0].target.targetActor,
                queued_action->targets[0].target.targetLimb, &target_position);
        } else {
            target_position = *(SVECTOR*)&queued_action->targets[0].target;
        }
    } else {
        VS::Actors::LiveActorBonePosition(queued_action->primaryTarget.target.targetActor, 0, &target_position, 0);
    }
    delta_x = target_position.vx - source_position.vx;
    delta_z = target_position.vz - source_position.vz;
    horizontal_distance = VS::Core::Math::SquareRootInteger((delta_x * delta_x) + (delta_z * delta_z));
    delta_y = target_position.vy - source_position.vy;

    return VS::Core::Math::SquareRootInteger((delta_y * delta_y) + (horizontal_distance * horizontal_distance));
}

// decomp: BATTLE.PRG func_800780A8 (src/BATTLE/BATTLE.PRG/146C.c:6863)
void Battle_ActionCameraMidpoint(SVECTOR* midpoint_out)
{
    SVECTOR source_position;
    SVECTOR target_position;
    Battle_ActionRequest* queued_action;

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex];
    VS::Actors::LiveActorBonePosition(queued_action->primaryTarget.target.targetActor, 0, &source_position, 0);
    if (queued_action->targetCount != 0) {
        if (queued_action->targets[0].targetsPosition == 0) {
            Battle_ActionTargetLimbPosition(queued_action->targets[0].target.targetActor,
                queued_action->targets[0].target.targetLimb, &target_position);
        } else {
            target_position = *(SVECTOR*)&queued_action->targets[0].target;
        }
    } else {
        target_position = queued_action->focusPosition;
    }
    midpoint_out->vx = source_position.vx + (target_position.vx - source_position.vx) / 2;
    midpoint_out->vz = source_position.vz + (target_position.vz - source_position.vz) / 2;
    midpoint_out->vy = source_position.vy + (target_position.vy - source_position.vy) / 2;
}

// decomp: BATTLE.PRG func_8007820C (src/BATTLE/BATTLE.PRG/146C.c:6886)
void Battle_StepCameraTowardActionMidpoint(int update_camera)
{
    SVECTOR action_midpoint;

    Battle_ActionCameraMidpoint(&action_midpoint);
    if (update_camera != 0) {
        VS::Field::LiveStepCameraLookAtTowardTarget(&action_midpoint, 1, 0x28);
    }
}

// decomp: BATTLE.PRG func_80078248 (src/BATTLE/BATTLE.PRG/146C.c:6896)
void Battle_UpdateInitialTargetSelection(void)
{
    if (Battle_actionState->isPlayerAction != 0) {
        if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
            if (++Battle_actionState->selectionFrames >= 0x1F) {
                VS::Battle::CommitLiveSelectedAction(1);
            }
        } else if (VS::Battle::LiveUpdateTargetSelectionMenu(&Battle_actionState->cameraPreview) != 0) {
            Battle_actionState->selectedPreview = Battle_actionState->cameraPreview;
            VS::Battle::CommitLiveSelectedAction(1);
        }
    } else {
        VS::Battle::CommitLiveSelectedAction(1);
    }
}

// decomp: BATTLE.PRG func_800782E4 (src/BATTLE/BATTLE.PRG/146C.c:6912)
void Battle_UpdateActionAreaSelection(void)
{
    if (Battle_actionState->isPlayerAction != 0) {
        if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
            u_int v = ++Battle_actionState->selectionFrames;
            if (v < 0x1F) {
                return;
            }
        } else if (VS::Battle::LiveUpdateSphereTargetPosition(&Battle_actionState->areaCenter) == 0) {
            return;
        }
    }
    VS::Battle::LiveFilterAreaActionTargets();
}

// decomp: BATTLE.PRG func_80078364 (src/BATTLE/BATTLE.PRG/146C.c:6927)
void Battle_UpdateFinalTargetSelection(void)
{
    if (Battle_actionState->isPlayerAction != 0) {
        if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
            if (++Battle_actionState->selectionFrames >= 0x1F) {
                Battle_actionState->selectedPreview = 0;
                VS::Battle::CommitLiveSelectedAction(2);
            }
        } else if (VS::Battle::LiveUpdateTargetSelectionMenu(&Battle_actionState->cameraPreview) != 0) {
            Battle_actionState->selectedPreview = Battle_actionState->cameraPreview;
            if (Battle_actionState->selectedPreview == 0xFF) {
                if (Battle_categoryNeedsTargetSelection[Battle_actionDefinitions[Battle_actionState->selection.actionId].category]
                    != 0) {
                    VS::Battle::LiveInitializeActionTargeting(Battle_actionState->selection.actionId);
                    return;
                }
                ((void (*)(...))Battle_ClearSphereTargets)();
                Battle_ClearPrimarySphereVolume();
                Battle_ClearSecondarySphereVolume();
                Battle_FreeSphere();
                VS::Battle::LiveReturnToBattleActionMenu();
                return;
            }
            VS::Battle::CommitLiveSelectedAction(2);
        }
    } else {
        Battle_actionState->selectedPreview = 0;
        VS::Battle::CommitLiveSelectedAction(2);
    }
}

// decomp: BATTLE.PRG _handleCombatCameraZoom (src/BATTLE/BATTLE.PRG/146C.c:6958)
void Battle_UpdateActionCameraZoom(void)
{
    SVECTOR action_midpoint;
    VECTOR camera_displacement;
    VECTOR camera_step;
    Core_Debugf("combat-flow", "camera mode=%d zoom=%d yaw=%d distance=%d action=%u targets=%u",
        Field_sphericalCamera.delta.mode, Battle_actionState->cameraZoomState,
        Field_sphericalCamera.delta.yaw, Field_sphericalCamera.values.distance,
        (unsigned)Battle_actionState->selection.actionId, (unsigned)Battle_actionState->selection.targetCount);

    if ((Field_sphericalCamera.delta.mode == 1)
        || (Field_sphericalCamera.delta.mode == 3)) {
        if (Battle_actionState->cameraZoomState == 0) {
            Battle_StepCameraTowardActionMidpoint(1);
            Battle_ActionCameraMidpoint(&action_midpoint);
            VS::Field::LiveComputeCameraTrackingStep(&action_midpoint, &camera_step, &camera_displacement, 1, 0x28);
                Core_Debugf("combat-flow", "tracking error=%ld,%ld,%ld", camera_displacement.vx, camera_displacement.vy, camera_displacement.vz);
            if ((((camera_displacement.vx | camera_displacement.vy | camera_displacement.vz) == 0)
                    && (Field_sphericalCamera.delta.yaw == 0))
                && ((Field_sphericalCamera.delta.mode == 1)
                    || (Field_sphericalCamera.delta.mode == 3))) {
                VS::Battle::StartLiveActionPresentation();
            }
        } else if (Battle_actionState->cameraZoomState == 1) {
            Battle_StepCameraTowardActionMidpoint(1);
            Render_projectionDistance += 0x40;
            if (Render_projectionDistance >= 0x300) {
                Render_projectionDistance = 0x300;
            }
            Core_Gte_SetGeomScreen(Render_projectionDistance);
            if (Field_sphericalCamera.values.distance > 0x600) {
                Field_sphericalCamera.values.distance -= 0xC0;
                if (Field_sphericalCamera.values.distance < 0x600) {
                    Field_sphericalCamera.values.distance = 0x600;
                }
            }
            if (Render_projectionDistance == 0x300) {
                Battle_ActionCameraMidpoint(&action_midpoint);
                VS::Field::LiveComputeCameraTrackingStep(&action_midpoint, &camera_step, &camera_displacement, 1, 0x28);
                Core_Debugf("combat-flow", "tracking error=%ld,%ld,%ld", camera_displacement.vx, camera_displacement.vy, camera_displacement.vz);
                if ((((camera_displacement.vx | camera_displacement.vy | camera_displacement.vz) == 0)
                        && (Field_sphericalCamera.delta.yaw == 0))
                    && ((Field_sphericalCamera.delta.mode == 1)
                        || (Field_sphericalCamera.delta.mode == 3))) {
                    VS::Battle::StartLiveActionPresentation();
                }
            }
        } else if (Battle_actionState->cameraZoomState == 2) {
            Battle_StepCameraTowardActionMidpoint(1);
            if (Field_sphericalCamera.values.distance < 0x900) {
                Field_sphericalCamera.values.distance += 0xC0;
                if (Field_sphericalCamera.values.distance > 0x900) {
                    Field_sphericalCamera.values.distance = 0x900;
                }
            }
            if (Field_sphericalCamera.values.distance == 0x900) {
                Battle_ActionCameraMidpoint(&action_midpoint);
                VS::Field::LiveComputeCameraTrackingStep(&action_midpoint, &camera_step, &camera_displacement, 1, 0x28);
                Core_Debugf("combat-flow", "tracking error=%ld,%ld,%ld", camera_displacement.vx, camera_displacement.vy, camera_displacement.vz);
                if ((((camera_displacement.vx | camera_displacement.vy | camera_displacement.vz) == 0)
                        && (Field_sphericalCamera.delta.yaw == 0))
                    && ((Field_sphericalCamera.delta.mode == 1)
                        || (Field_sphericalCamera.delta.mode == 3))) {
                    VS::Battle::StartLiveActionPresentation();
                }
            }
        } else if ((Field_sphericalCamera.delta.yaw == 0)
                   && ((Field_sphericalCamera.delta.mode == 1)
                       || (Field_sphericalCamera.delta.mode == 3))) {
            VS::Battle::StartLiveActionPresentation();
        }
    }
}

// decomp: BATTLE.PRG func_80078748 (src/BATTLE/BATTLE.PRG/146C.c:7025)
void Battle_WaitForActionEffectReady(void)
{
    Battle_ActionRequest* queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];
    if ((Menu_IsSceneNotificationPending() == 0)
        && ((VS::Actors::LiveActorStateFlags(queued_action->primaryTarget.target.targetActor) & 0x08000100) == 0x08000100)
        && (VS::Content::FindLiveFreeActorAssetRequest() == 0) && (Effects_CanStartEffectTrack() != 0)) {
        VS::Battle::AdvanceLiveActionEffectPhase();
    }
}

// decomp: BATTLE.PRG func_800787F0 (src/BATTLE/BATTLE.PRG/146C.c:7035)
void Battle_StartReadyActionAnimation(void)
{
    if (!(Effects_effectBusyFlags & 2)) {
        VS::Battle::StartLiveActionAnimation();
        Effects_StartEffectTrack(0);
    }
}

// decomp: BATTLE.PRG func_80079050 (src/BATTLE/BATTLE.PRG/146C.c:7356)
void Battle_WaitForActionResolution(void)
{
    if (VS::Battle::Ai::RebuildLiveActorActionPlans(&Battle_actionState->queue[Battle_actionState->queueIndex & 3]) == 0) {
        VS::Battle::ReleaseLiveQueuedActorAttacks();
        VS::Battle::BeginLiveActionTrailFade();
    }
}

// decomp: BATTLE.PRG func_800790BC (src/BATTLE/BATTLE.PRG/146C.c:7364)
void Battle_WaitForTargetRecovery(void)
{
    Battle_ActionRequest* queued_action;
    int j;
    int i;
    int var_s4;

    var_s4 = 1;

    for (i = 0; i < Battle_actionState->chainStep; ++i) {

        queued_action = &Battle_actionState->queue[i % 4];

        for (j = 0; j < queued_action->targetCount; ++j) {
            if ((queued_action->targets[j].targetsPosition == 0)
                && ((VS::Actors::IsLiveActorActionAnimationBusy(queued_action->targets[j].target.targetActor) + 1) > 1)) {
                var_s4 = 0;
            }
        }
    }

    if (var_s4 != 0) {

        var_s4 = 1;

        for (i = 0; i < Battle_actionState->chainStep; ++i) {

            queued_action = &Battle_actionState->queue[i % 4];

            if (queued_action->primaryTarget.targetsPosition == 0) {
                VS::Battle::BeginLiveActorDefeatAnimation(queued_action->primaryTarget.target.targetActor);
            }

            for (j = 0; j < queued_action->targetCount; ++j) {
                if ((queued_action->targets[j].targetsPosition == 0)
                    && (VS::Battle::BeginLiveActorDefeatAnimation(queued_action->targets[j].target.targetActor) != 0)) {
                    var_s4 = 2;
                }
            }
        }

        if (queued_action->primaryTarget.target.targetActor != 0) {
            var_s4 = 0;
        }

        VS::Battle::BeginLivePostActionRecovery(var_s4);
    }
}

// decomp: BATTLE.PRG func_800793C0 (src/BATTLE/BATTLE.PRG/146C.c:7435)
void Battle_FinishActionSequence(void)
{
    Battle_ActionRequest* action;
    int ready;
    int i;
    int j;
    u_char actor;

    if ((Field_sphericalCamera.delta.mode == 1) || (Field_sphericalCamera.delta.mode == 3)) {
        if (Render_projectionDistance > 512) {
            Render_projectionDistance -= 64;
            if (Render_projectionDistance <= 512) {
                Render_projectionDistance = 512;
            }
            Core_Gte_SetGeomScreen(Render_projectionDistance);
        } else if (Render_projectionDistance < 512) {
            Render_projectionDistance += 64;
            if (Render_projectionDistance >= 512) {
                Render_projectionDistance = 512;
            }
            Core_Gte_SetGeomScreen(Render_projectionDistance);
        }
        if (Field_sphericalCamera.values.distance < Battle_actionState->preferredCameraDistance) {
            Field_sphericalCamera.values.distance += 192;
            if (Field_sphericalCamera.values.distance > Battle_actionState->preferredCameraDistance) {
                Field_sphericalCamera.values.distance = Battle_actionState->preferredCameraDistance;
            }
        } else if (Field_sphericalCamera.values.distance
                   > Battle_actionState->preferredCameraDistance) {
            Field_sphericalCamera.values.distance -= 192;
            if (Field_sphericalCamera.values.distance < Battle_actionState->preferredCameraDistance) {
                Field_sphericalCamera.values.distance = Battle_actionState->preferredCameraDistance;
            }
        }
    }
    if ((Render_projectionDistance != 512) || (Field_sphericalCamera.values.distance != Battle_actionState->preferredCameraDistance)
        || !VS::Field::LiveIsCameraLookAtOnAshley()) {
        return;
    }
    ready = 1;
    for (i = 0; i < Battle_actionState->chainStep; ++i) {
        action = &Battle_actionState->queue[i % 4];
        for (j = 0; j < action->targetCount; ++j) {
            if ((action->targets[j].targetsPosition == 0)
                && ((VS::Actors::IsLiveActorActionAnimationBusy(action->targets[j].target.targetActor) + 1) >= 2)) {
                ready = 0;
            }
        }
    }
    if (!ready) {
        return;
    }
    VS::Field::LiveResumeFieldAfterMenu();
    Battle_SetHudHidden(0);
    VS::Actors::RestoreLiveActorIdlePose(Battle_actionState->selection.primaryTarget.target.targetActor);
    for (i = 0; i < Battle_actionState->chainStep; ++i) {
        action = &Battle_actionState->queue[i % 4];
        for (j = 0; j < action->targetCount; ++j) {
            if (action->targets[j].targetsPosition == 0) {
                VS::Battle::ResolveLiveActorDefeat(
                    action->primaryTarget.target.targetActor, action->targets[j].target.targetActor, 1);
            }
        }
    }
    for (i = 0; i < Battle_actionState->chainStep; ++i) {
        action = &Battle_actionState->queue[i % 4];
        if (action->primaryTarget.targetsPosition == 0) {
            actor = action->primaryTarget.target.targetActor;
            VS::Battle::ResolveLiveActorDefeat(actor, actor, 1);
        }
    }
    if (Field_controlMode == 3) {
        return;
    }
    if (Event_EvaluateRoomEventTriggersEntry(12, 0, 0, 0) == 1) {
        VS::Event::BeginLiveQueuedCutscene();
        return;
    }
    VS::Items::OpenLivePendingEnemyLoot();
}

}  // extern "C"
