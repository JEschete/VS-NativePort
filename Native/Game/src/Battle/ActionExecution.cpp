// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorStateLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Battle/ActionOutcomesLive.hpp"
#include "Battle/ActionPhasesLive.hpp"
#include "Battle/ActionPresentationLive.hpp"
#include "Battle/ActorVitalsLive.hpp"
#include "Battle/Ai/PlanningWorkspaceLive.hpp"
#include "Content/ActorAssetsLive.hpp"
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
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
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
