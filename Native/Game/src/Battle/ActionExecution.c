// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/Targeting.h"
#include "Core/Math/FixedPoint.h"
#include "Effects/Runtime.h"
#include "Event/Cutscene.h"
#include "Event/Triggers.h"
#include "Field/Camera.h"
#include "Field/GameMode.h"
#include "Field/Interactions.h"
#include "Items/Loot.h"
#include "Menu/TextBox.h"
#include "Render/Camera.h"

// decomp: D_800F19D8 (D_800F19D8@BATTLE/BATTLE.PRG)
int Battle_settledCameraDistance;

// decomp: D_800F19EC (D_800F19EC@BATTLE/BATTLE.PRG)
int Battle_cameraEaseMode;

// decomp: BATTLE.PRG func_80077F14 (src/BATTLE/BATTLE.PRG/146C.c:6821)
void Battle_ActionTargetLimbPosition(int arg0, int arg1, SVECTOR* arg2)
{
    int var_v0;

    if (arg1 == -1) {
        var_v0 = 0;
    } else {
        var_v0 = Actors_FindActorBoneTag(arg0, arg1, 2);
    }
    Actors_ActorBonePosition(arg0, var_v0, arg2, 0);
}

// decomp: BATTLE.PRG func_80077F70 (src/BATTLE/BATTLE.PRG/146C.c:6833)
int Battle_ActionTargetDistance(void)
{
    SVECTOR sp10;
    SVECTOR sp18;
    Battle_ActionRequest* temp_s0;
    int temp_v0;
    int temp_v0_2;
    int temp_v0_3;
    int temp_v1;

    temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex];
    Actors_ActorBonePosition(temp_s0->primaryTarget.target.targetActor, 0, &sp10, 0);
    if (temp_s0->targetCount != 0) {
        if (temp_s0->targets[0].targetsPosition == 0) {
            Battle_ActionTargetLimbPosition(temp_s0->targets[0].target.targetActor,
                temp_s0->targets[0].target.targetLimb, &sp18);
        } else {
            sp18 = *(SVECTOR*)&temp_s0->targets[0].target;
        }
    } else {
        Actors_ActorBonePosition(temp_s0->primaryTarget.target.targetActor, 0, &sp18, 0);
    }
    temp_v0 = sp18.vx - sp10.vx;
    temp_v0_2 = sp18.vz - sp10.vz;
    temp_v0_3 = Core_Math_SquareRootInteger((temp_v0 * temp_v0) + (temp_v0_2 * temp_v0_2));
    temp_v1 = sp18.vy - sp10.vy;

    return Core_Math_SquareRootInteger((temp_v1 * temp_v1) + (temp_v0_3 * temp_v0_3));
}

// decomp: BATTLE.PRG func_800780A8 (src/BATTLE/BATTLE.PRG/146C.c:6863)
void Battle_ActionCameraMidpoint(SVECTOR* arg0)
{
    SVECTOR sp10;
    SVECTOR sp18;
    Battle_ActionRequest* temp_s0;

    temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex];
    Actors_ActorBonePosition(temp_s0->primaryTarget.target.targetActor, 0, &sp10, 0);
    if (temp_s0->targetCount != 0) {
        if (temp_s0->targets[0].targetsPosition == 0) {
            Battle_ActionTargetLimbPosition(temp_s0->targets[0].target.targetActor,
                temp_s0->targets[0].target.targetLimb, &sp18);
        } else {
            sp18 = *(SVECTOR*)&temp_s0->targets[0].target;
        }
    } else {
        sp18 = temp_s0->focusPosition;
    }
    arg0->vx = sp10.vx + (sp18.vx - sp10.vx) / 2;
    arg0->vz = sp10.vz + (sp18.vz - sp10.vz) / 2;
    arg0->vy = sp10.vy + (sp18.vy - sp10.vy) / 2;
}

// decomp: BATTLE.PRG func_8007820C (src/BATTLE/BATTLE.PRG/146C.c:6886)
void Battle_StepCameraTowardActionMidpoint(int arg0)
{
    SVECTOR sp10;

    Battle_ActionCameraMidpoint(&sp10);
    if (arg0 != 0) {
        Field_StepCameraLookAtTowardTarget(&sp10, 1, 0x28);
    }
}

// decomp: BATTLE.PRG func_80078248 (src/BATTLE/BATTLE.PRG/146C.c:6896)
void Battle_UpdateInitialTargetSelection(void)
{
    if (Battle_actionState->isPlayerAction != 0) {
        if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
            if (++Battle_actionState->selectionFrames >= 0x1F) {
                Battle_CommitSelectedAction(1);
            }
        } else if (Battle_UpdateTargetSelectionMenu(&Battle_actionState->cameraPreview) != 0) {
            Battle_actionState->selectedPreview = Battle_actionState->cameraPreview;
            Battle_CommitSelectedAction(1);
        }
    } else {
        Battle_CommitSelectedAction(1);
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
        } else if (Battle_UpdateSphereTargetPosition(&Battle_actionState->areaCenter) == 0) {
            return;
        }
    }
    Battle_FilterAreaActionTargets();
}

// decomp: BATTLE.PRG func_80078364 (src/BATTLE/BATTLE.PRG/146C.c:6927)
void Battle_UpdateFinalTargetSelection(void)
{
    if (Battle_actionState->isPlayerAction != 0) {
        if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
            if (++Battle_actionState->selectionFrames >= 0x1F) {
                Battle_actionState->selectedPreview = 0;
                Battle_CommitSelectedAction(2);
            }
        } else if (Battle_UpdateTargetSelectionMenu(&Battle_actionState->cameraPreview) != 0) {
            Battle_actionState->selectedPreview = Battle_actionState->cameraPreview;
            if (Battle_actionState->selectedPreview == 0xFF) {
                if (Battle_categoryNeedsTargetSelection[Battle_actionDefinitions[Battle_actionState->selection.actionId].category]
                    != 0) {
                    Battle_InitializeActionTargeting(Battle_actionState->selection.actionId);
                    return;
                }
                ((void (*)())Battle_ClearSphereTargets)();
                Battle_ClearPrimarySphereVolume();
                Battle_ClearSecondarySphereVolume();
                Battle_FreeSphere();
                Battle_ReturnToBattleActionMenu();
                return;
            }
            Battle_CommitSelectedAction(2);
        }
    } else {
        Battle_actionState->selectedPreview = 0;
        Battle_CommitSelectedAction(2);
    }
}

// decomp: BATTLE.PRG _handleCombatCameraZoom (src/BATTLE/BATTLE.PRG/146C.c:6958)
void Battle_UpdateActionCameraZoom(void)
{
    SVECTOR sp18;
    VECTOR sp20;
    VECTOR sp30;

    if ((Field_sphericalCamera.delta.mode == 1)
        || (Field_sphericalCamera.delta.mode == 3)) {
        if (Battle_actionState->cameraZoomState == 0) {
            Battle_StepCameraTowardActionMidpoint(1);
            Battle_ActionCameraMidpoint(&sp18);
            Field_ComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);
            if ((((sp20.vx | sp20.vy | sp20.vz) == 0)
                    && (Field_sphericalCamera.delta.yaw == 0))
                && ((Field_sphericalCamera.delta.mode == 1)
                    || (Field_sphericalCamera.delta.mode == 3))) {
                Battle_StartActionPresentation();
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
                Battle_ActionCameraMidpoint(&sp18);
                Field_ComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);
                if ((((sp20.vx | sp20.vy | sp20.vz) == 0)
                        && (Field_sphericalCamera.delta.yaw == 0))
                    && ((Field_sphericalCamera.delta.mode == 1)
                        || (Field_sphericalCamera.delta.mode == 3))) {
                    Battle_StartActionPresentation();
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
                Battle_ActionCameraMidpoint(&sp18);
                Field_ComputeCameraTrackingStep(&sp18, &sp30, &sp20, 1, 0x28);
                if ((((sp20.vx | sp20.vy | sp20.vz) == 0)
                        && (Field_sphericalCamera.delta.yaw == 0))
                    && ((Field_sphericalCamera.delta.mode == 1)
                        || (Field_sphericalCamera.delta.mode == 3))) {
                    Battle_StartActionPresentation();
                }
            }
        } else if ((Field_sphericalCamera.delta.yaw == 0)
                   && ((Field_sphericalCamera.delta.mode == 1)
                       || (Field_sphericalCamera.delta.mode == 3))) {
            Battle_StartActionPresentation();
        }
    }
}

// decomp: BATTLE.PRG func_80078748 (src/BATTLE/BATTLE.PRG/146C.c:7025)
void Battle_WaitForActionEffectReady(void)
{
    Battle_ActionRequest* temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];
    if ((Menu_IsSceneNotificationPending() == 0)
        && ((Actors_ActorStateFlags(temp_s0->primaryTarget.target.targetActor) & 0x08000100) == 0x08000100)
        && (Content_FindFreeActorAssetRequest() == 0) && (Effects_CanStartEffectTrack() != 0)) {
        Battle_AdvanceActionEffectPhase();
    }
}

// decomp: BATTLE.PRG func_800787F0 (src/BATTLE/BATTLE.PRG/146C.c:7035)
void Battle_StartReadyActionAnimation(void)
{
    if (!(Effects_effectBusyFlags & 2)) {
        Battle_StartActionAnimation();
        Effects_StartEffectTrack(0);
    }
}

// decomp: BATTLE.PRG func_80079030 (src/BATTLE/BATTLE.PRG/146C.c:7354)
void Battle_StartFollowupActionEffects(void) { Battle_StartActionPresentation(); }

// decomp: BATTLE.PRG func_80079050 (src/BATTLE/BATTLE.PRG/146C.c:7356)
void Battle_WaitForActionResolution(void)
{
    if (Battle_Ai_RebuildActorActionPlans(&Battle_actionState->queue[Battle_actionState->queueIndex & 3]) == 0) {
        Battle_ReleaseQueuedActorAttacks();
        Battle_BeginActionTrailFade();
    }
}

// decomp: BATTLE.PRG func_800790BC (src/BATTLE/BATTLE.PRG/146C.c:7364)
void Battle_WaitForTargetRecovery(void)
{
    Battle_ActionRequest* var_s3;
    int j;
    int i;
    int var_s4;

    var_s4 = 1;

    for (i = 0; i < Battle_actionState->chainStep; ++i) {

        var_s3 = &Battle_actionState->queue[i % 4];

        for (j = 0; j < var_s3->targetCount; ++j) {
            if ((var_s3->targets[j].targetsPosition == 0)
                && ((((u_int (*)(int))Actors_IsActorActionAnimationBusy)(var_s3->targets[j].target.targetActor) + 1) > 1)) {
                var_s4 = 0;
            }
        }
    }

    if (var_s4 != 0) {

        var_s4 = 1;

        for (i = 0; i < Battle_actionState->chainStep; ++i) {

            var_s3 = &Battle_actionState->queue[i % 4];

            if (var_s3->primaryTarget.targetsPosition == 0) {
                Battle_BeginActorDefeatAnimation(var_s3->primaryTarget.target.targetActor);
            }

            for (j = 0; j < var_s3->targetCount; ++j) {
                if ((var_s3->targets[j].targetsPosition == 0)
                    && (Battle_BeginActorDefeatAnimation(var_s3->targets[j].target.targetActor) != 0)) {
                    var_s4 = 2;
                }
            }
        }

        if (var_s3->primaryTarget.target.targetActor != 0) {
            var_s4 = 0;
        }

        Battle_BeginPostActionRecovery(var_s4);
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

    if ((Battle_cameraEaseMode == 1) || (Battle_cameraEaseMode == 3)) {
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
    if ((Render_projectionDistance != 512) || (Battle_settledCameraDistance != Battle_actionState->preferredCameraDistance)
        || !Field_IsCameraLookAtOnAshley()) {
        return;
    }
    ready = 1;
    for (i = 0; i < Battle_actionState->chainStep; ++i) {
        action = &Battle_actionState->queue[i % 4];
        for (j = 0; j < action->targetCount; ++j) {
            if ((action->targets[j].targetsPosition == 0)
                && ((((u_int (*)(int))Actors_IsActorActionAnimationBusy)(action->targets[j].target.targetActor) + 1) >= 2)) {
                ready = 0;
            }
        }
    }
    if (!ready) {
        return;
    }
    Field_ResumeFieldAfterMenu();
    Battle_SetHudHidden(0);
    Actors_RestoreActorIdlePose(Battle_actionState->selection.primaryTarget.target.targetActor);
    for (i = 0; i < Battle_actionState->chainStep; ++i) {
        action = &Battle_actionState->queue[i % 4];
        for (j = 0; j < action->targetCount; ++j) {
            if (action->targets[j].targetsPosition == 0) {
                Battle_ResolveActorDefeat(
                    action->primaryTarget.target.targetActor, action->targets[j].target.targetActor, 1);
            }
        }
    }
    for (i = 0; i < Battle_actionState->chainStep; ++i) {
        action = &Battle_actionState->queue[i % 4];
        if (action->primaryTarget.targetsPosition == 0) {
            actor = action->primaryTarget.target.targetActor;
            Battle_ResolveActorDefeat(actor, actor, 1);
        }
    }
    if (Field_controlMode == 3) {
        return;
    }
    if (Event_EvaluateRoomEventTriggers(12, 0, 0, 0) == 1) {
        Event_BeginQueuedCutscene();
        return;
    }
    Items_OpenPendingEnemyLoot();
}

