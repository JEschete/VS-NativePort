// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Lifecycle.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Audio/Sfx.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorStateHelpers.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/BattleMode.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Hud.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/Actors.h"
#include "Field/Camera.h"
#include "Field/GameMode.h"
#include "Field/Interactions.h"
#include "Field/Map.h"
#include "Field/RoomRendering.h"
#include "Input/Controller.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Loot.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Palette.h"
#include "State/Flags.h"
#include "State/Progress.h"
#include "State/Score.h"
#include "State/Settings.h"
#include "State/Timers.h"

// decomp: D_800F18EC (D_800F18EC@BATTLE/BATTLE.PRG)
Menu_Framework_ActionSelection* Battle_actionSelection;

// decomp: _cameraMode (_cameraMode@BATTLE/BATTLE.PRG)
u_int Battle_actionFlowMode;

// decomp: D_800F18FC (D_800F18FC@BATTLE/BATTLE.PRG)
int Battle_recoveryRewardMode;

// decomp: D_800F19CC (D_800F19CC@BATTLE/BATTLE.PRG)
Battle_ActionState* Battle_actionState;

// decomp: D_8005FFAF (D_8005FFAF@SLUS_010.40)
char Battle_spellCommitFlag;

// decomp: BATTLE.PRG func_80069D14 (src/BATTLE/BATTLE.PRG/146C.c:761)
void Battle_ReleaseQueuedActorAttacks(void)
{
    int i;

    for (i = 0; i < 16; ++i) {

        Battle_Actor* actor = Battle_actors[i];

        if ((actor != NULL) && (actor->commandState == 0x80)
            && (Battle_actors[i]->commandOpcode == 0x18)) {
            actor->commandState = 8;
        }
    }
}

// decomp: BATTLE.PRG _enemyDefeated (src/BATTLE/BATTLE.PRG/146C.c:1824)
void Battle_ResolveEnemyDefeat(int source_actor_slot, int actorId, int count_kill)
{
    Battle_Actor* enemy = Battle_actors[actorId];

    if ((enemy->roomSpawnIndex == 0x80) || (enemy->defeated != 0)) {
        return;
    }

    if ((count_kill != 0) && (source_actor_slot == 0)) {
        State_RecordEnemyKillScore(enemy);
        State_UpdateWeaponKillStreak(0);
    }

    Items_QueueEnemyLoot(enemy);

    enemy->defeated = 1;
}

// decomp: BATTLE.PRG func_8006C5AC (src/BATTLE/BATTLE.PRG/146C.c:1955)
int Battle_PlayWeaponHitSounds(int source_actor_slot)
{
    Actors_ActorLocationQuery target_placement;
    SVECTOR hit_position;
    Battle_ActionRequest* queued_action;
    int i;

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    for (i = 0; i < queued_action->targetCount; ++i) {
        if (queued_action->targets[i].targetsPosition == 0) {

            Actors_ActorPlacement(queued_action->targets[i].target.targetActor, &target_placement);

            hit_position.vx = target_placement.location.position.vx;
            hit_position.vz = target_placement.location.position.vz;
            hit_position.vy = target_placement.location.position.vy;

            switch (queued_action->targets[i].target.impactKind) {
            case 0:
            case 1:
            case 2:
            case 4:
                switch (Battle_actors[source_actor_slot]->equippedWeaponCategory) {
                case 1:
                    Audio_PlayWorldSfx(0x7E, 0x47, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x48, &hit_position);
                    break;

                case 2:
                    Audio_PlayWorldSfx(0x7E, 0x49, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x4A, &hit_position);
                    break;

                case 3:
                    Audio_PlayWorldSfx(0x7E, 0x4B, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x4C, &hit_position);
                    break;

                case 4:
                    Audio_PlayWorldSfx(0x7E, 0x50, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x51, &hit_position);
                    break;

                case 5:
                    Audio_PlayWorldSfx(0x7E, 0x52, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x53, &hit_position);
                    break;

                case 6:
                    Audio_PlayWorldSfx(0x7E, 0x4D, &hit_position);
                    break;

                case 7:
                    Audio_PlayWorldSfx(0x7E, 0x4E, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x4F, &hit_position);
                    break;

                case 8:
                    Audio_PlayWorldSfx(0x7E, 0x54, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x55, &hit_position);
                    break;

                case 10:
                    Audio_PlayWorldSfx(0x7E, 0x5B, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x5C, &hit_position);
                    break;

                case 9:
                    Audio_PlayWorldSfx(0x7E, 0x69, &hit_position);
                    Audio_PlayWorldSfx(0x7E, 0x6A, &hit_position);
                    break;
                }

                break;

            case 3:
                Audio_PlayWorldSfx(0x7E, 0x33, &hit_position);
                break;

            case 5:
                break;
            }
        }
    }

    return 1;
}

// decomp: BATTLE.PRG func_8006CA20 (src/BATTLE/BATTLE.PRG/146C.c:2106)
void Battle_ApplyActionSourceEffects(int source_actor_slot, SVECTOR* arg1)
{
    Battle_ActionRequest* queued_action;

    if (source_actor_slot >= 16) {
        return;
    }

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if ((queued_action->primaryTarget.target.targetActor == source_actor_slot) && (queued_action->primaryTarget.targetsPosition == 0)) {

        switch (queued_action->primaryTarget.target.impactKind) {
        case 2:
        case 3:
        case 4:
            break;

        case 0:
        case 1:
            Battle_PlayActorDamageReaction(source_actor_slot, &queued_action->focusPosition);
            break;

        case 5:
            if (source_actor_slot == 0) {
                ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
            }

            break;
        }

        Battle_ApplyActorActionResult(&queued_action->primaryTarget);
    }
}

// decomp: BATTLE.PRG func_8006F5FC (src/BATTLE/BATTLE.PRG/146C.c:3228)
void Battle_WaitForPostActionMovement(void)
{
    Field_controlMode = 12;
    Battle_actionFlowMode = 0;
    Actors_RestoreActorIdlePose(0);
}

// decomp: BATTLE.PRG func_8006FBCC (src/BATTLE/BATTLE.PRG/146C.c:3388)
void Battle_BeginPostActionRecovery(int recovery_reward_mode)
{
    int i;

    for (i = 0; i < 16; ++i) {
        if ((i != Battle_actionState->selection.primaryTarget.target.targetActor)
            && !(Actors_ActorStateFlags(i) & 0x01000000) && (Actors_ActorStateFlags(i) & 1)) {
            Actors_RestoreActorIdlePose(i);
        }
    }
    Render_TransformRoomPalettes(8, 4, 0, 0, 0);
    Actors_FreezeAllActorAnimations();
    Render_SetClutAnimationEnabled(1);
    Field_SetRoomAnimationsPaused(0);
    Battle_SetActorMarkersHidden(0);
    Battle_recoveryRewardMode = recovery_reward_mode;
    State_tickSpeed = 2;
    Battle_actionFlowMode = 0xB;
}

// decomp: BATTLE.PRG func_8006FCBC (src/BATTLE/BATTLE.PRG/146C.c:3408)
void Battle_BeginActionTrailFade(void)
{
    Battle_ActionRequest* initial_action;

    Battle_actionFlowMode = 10;
    initial_action = &Battle_actionState->queue[0];
    Effects_FadeWeaponTrails(initial_action->primaryTarget.target.targetActor,
        Actors_FindActorBoneTag(initial_action->primaryTarget.target.targetActor, 0xF0, 0));
}

// decomp: BATTLE.PRG func_8006FD0C (src/BATTLE/BATTLE.PRG/146C.c:3418)
void Battle_FinishBattleAbilitySequence(void) { Battle_actionFlowMode = 9; }

// decomp: BATTLE.PRG func_80070278 (src/BATTLE/BATTLE.PRG/146C.c:3510)
void Battle_StartActionAnimation(void)
{
    Battle_ActionRequest* queued_action;
    int chain_index;

    Battle_actionState->abilityWindow = 0;
    Battle_actionState->abilityTimer = 0;
    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if (((queued_action->actionId - 0x16u) < 0x20) && (queued_action->primaryTarget.targetsPosition == 0)
        && (queued_action->primaryTarget.target.targetActor == 0)) {
        State_SetScriptFlag(0xB8, 1);
    }

    chain_index = Battle_actionState->queueIndex;

    if (chain_index == 0) {
        if (!(Battle_actionDefinitions[queued_action->actionId].returnsToMenuOnCancel)) {
            Battle_PlayMeleeAttackAnimation(
                queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, queued_action->animationParameter);
            Battle_actionState->abilityWindow =
                Actors_AnimationBattleWindowFrame(queued_action->primaryTarget.target.targetActor) * 2;
        } else {
            if (queued_action->actionId >= 0x8D) {
                if (queued_action->actionId < 0xB8) {
                    Battle_PlayBreakArtAnimation(
                        queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
                } else if (queued_action->actionId < 0xE0) {
                    Battle_PlaySpecialActionAnimation(
                        queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
                } else {
                    Battle_PlaySpellCastAnimation(
                        queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
                }
            } else {
                Battle_PlaySpellCastAnimation(
                    queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
            }
            Battle_actionState->abilityWindow = 0;
            State_tickSpeed = 4;
        }
    } else {
        if (queued_action->actionId < 0x28) {
            Battle_PlayChainAbilityAnimation(queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition,
                Input_settings.mappedChainAbilities[Battle_actionState->chainAbilitySlot - 1] - 0x16,
                chain_index);
        } else {
            Battle_PlayDefenseAbilityAnimation(queued_action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 0);
        }
        Battle_actionState->abilityWindow =
            Actors_AnimationBattleWindowFrame(queued_action->primaryTarget.target.targetActor) * 2;
    }

    Audio_StopSfx(0x7E, 0x34);
    Audio_StopSfx(0x7E, 0x35);
    Audio_StopSfx(0x7E, 0x36);
    Menu_ReleaseSceneNotification();

    Battle_actionState->chainAbilitySlot = 0;
    Battle_actionFlowMode = 7;
}

// decomp: BATTLE.PRG func_800704B0 (src/BATTLE/BATTLE.PRG/146C.c:3572)
void Battle_AdvanceActionEffectPhase(void)
{
    Battle_actionFlowMode = 6;
    Effects_ClearActionEffectMode();
}

// decomp: BATTLE.PRG func_80070F28 (src/BATTLE/BATTLE.PRG/146C.c:3775)
void Battle_CommitSelectedAction(int target_selection_mode)
{
    int i;
    int action_id;
    u_char selected_preview;
    int var_a0;

    Field_StorePreferredActionCameraDistance();
    selected_preview = Battle_actionState->selectedPreview;
    ((void (*)(...))Battle_ClearSphereTargets)();
    Battle_ClearPrimarySphereVolume();
    Battle_ClearSecondarySphereVolume();
    Battle_FreeSphere();

    if (target_selection_mode != 0) {
        if (selected_preview == 0xFF) {
            if (Battle_actionState->selection.primaryTarget.target.targetActor != 0) {
                Menu_Framework_ReleaseMenuRows();
                Battle_Ai_ResetActorAiReactionDelay(Battle_actionState->selection.primaryTarget.target.targetActor);
                Battle_actionState->selection.actionId = 0;
                Battle_BeginPostActionRecovery(0);
                return;
            }
            if (!(Battle_actionDefinitions[Battle_actionState->selection.actionId].returnsToMenuOnCancel)) {
                Menu_Framework_ReleaseMenuRows();
                Battle_actionState->selection.actionId = 0;
                Battle_BeginPostActionRecovery(0);
                return;
            }
            Battle_actionState->selection.actionId = 0;
            Battle_ReturnToBattleActionMenu();
            return;
        }
    }

    Battle_actionState->chainStep = 1;
    Battle_actionState->queueIndex = 0;

    if (Battle_actionState->selection.primaryTarget.target.targetActor == 0) {
        if (Battle_actionSelection->selectionKind != 7) {
            if (Battle_actionSelection->selectionKind == 8) {
                Items_ConsumeMiscItem(Battle_actionSelection->executeAbility.halves[1]);
            }
        }
        Battle_actionSelection->selectionKind = 0;
    } else {
        Battle_Ai_DisengageActorById(Battle_actionState->selection.primaryTarget.target.targetActor);
    }
    Menu_Framework_ReleaseMenuRows();
    if (target_selection_mode == 0) {
        Battle_CollectEnvironmentActionTargets();
    } else if (target_selection_mode == 2) {
        Battle_SetAllActionTargets();
    } else {
        Battle_SetSingleActionTarget();
    }

    Battle_ResolveActionTargets(Battle_actionState->selection.actionId, Battle_actionState->queue, &Battle_actionState->selection, 1);

    if ((Battle_actionState->selection.primaryTarget.target.targetActor == 0)
        && ((action_id = Battle_actionState->selection.actionId, (action_id == 1))
            || ((action_id - 0xB8) < 0x28U))
        && (Battle_actionState->selection.targets[0].targetsPosition == 0)
        && (Battle_actors[Battle_actionState->selection.targets[0].target.targetActor]->roomSpawnIndex
            != 0x80)) {
        if (Battle_characterState->equippedWeaponCategory == 0xA) {

            if (Audio_Seq_unusedScoredata.weaponAttacks[0] <= 0xFFFEU) {
                ++Audio_Seq_unusedScoredata.weaponAttacks[0];
            }

        } else {
            if (Audio_Seq_unusedScoredata
                    .weaponAttacks[Battle_characterState->equippedWeaponCategory]
                <= 0xFFFEU) {
                ++Audio_Seq_unusedScoredata
                      .weaponAttacks[Battle_characterState->equippedWeaponCategory];
            }
        }
    }

    if ((Battle_ActionTargetDistance() >= 0x1C1)
        && (((Battle_actionDefinitions[Battle_actionState->selection.actionId].opaqueDBit4)) == 1)) {
        Battle_actionState->cameraZoomState = 0;
    } else {
        Battle_actionState->cameraZoomState = (Battle_actionDefinitions[Battle_actionState->selection.actionId].opaqueDBit4);
    }

    Battle_actionState->previousChainButtons = 0;
    Battle_actionState->chainAbilitySlot = Battle_actionState->previousChainInput = 0;
    Battle_actionState->chainContinued = 0;

    if (Battle_actionState->selection.actionId >= 55) {
        Battle_spellCommitFlag = 0;

        do {
        } while (0);

        switch (Battle_actionState->selection.actionId) {
        case 0x4D:
        case 0x60:
        case 0xE0:
        case 0xE1:
        case 0xE2:
        case 0xE3:
        case 0xE8:
        case 0xE9:
        case 0xEA:
        case 0xEB:
            if (Audio_Seq_unusedScoredata.healCount < 0x3E8U) {
                Audio_Seq_unusedScoredata.healCount += 1;
            }
        }
    }

    Battle_actionFlowMode = 4;

    for (i = 0; i < 16; ++i) {
        if (i != Battle_actionState->selection.primaryTarget.target.targetActor) {
            Actors_RestoreActorIdlePose(i);
        }
    }

    Render_SetClutAnimationEnabled(1);
    Render_TransformSceneColors(8, 4, 0, 0, 0);
    Actors_FreezeAllActorAnimations();
    Battle_SetActorMarkersHidden(1);
    Field_SetMinimapVisible(0);
    Field_SetRoomAnimationsPaused(0);
}

// decomp: BATTLE.PRG func_8007138C (src/BATTLE/BATTLE.PRG/146C.c:3906)
void Battle_StartActionPresentation(void)
{
    Content_ActorAssetRequest object;
    Effects_ActionEffectRequest request;
    Actors_ActorLocationQuery actorPosition;
    SVECTOR soundPosition;
    Battle_ActionRequest* action;
    int mode;
    int i;
    int j;
    int duplicate;
    int count;

    Battle_actionState->abilityInputSucceeded = 0;

    if (Battle_characterState->stats->statuses & 0x8000) {
        Battle_actionState->abilityInputAttempted = 1;
    } else {
        Battle_actionState->abilityInputAttempted = 0;
    }

    action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if (!Battle_actionDefinitions[action->actionId].returnsToMenuOnCancel) {
        Battle_StartActionAnimation();
        return;
    }

    Actors_ActorPlacement(action->primaryTarget.target.targetActor, &actorPosition);
    soundPosition.vx = actorPosition.location.position.vx;
    soundPosition.vz = actorPosition.location.position.vz;
    soundPosition.vy = actorPosition.location.position.vy;

    if (action->actionId < 141) {
        Battle_PlaySpellCastAnimation(action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 0);
        Audio_PlayWorldSfx(0x7E, 0x34, &soundPosition);

        if (!action->primaryTarget.target.targetActor) {
            State_SetScriptFlag(0xB7, 1);
        }

        mode = 2;
    } else if (action->actionId < 184) {
        Battle_PlayBreakArtAnimation(action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 0);
        Audio_PlayWorldSfx(0x7E, 0x36, &soundPosition);
        mode = 3;
    } else if (action->actionId < 224) {
        Battle_PlaySpecialActionAnimation(action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 0);
        Audio_PlayWorldSfx(0x7E, 0x34, &soundPosition);

        if (!action->primaryTarget.target.targetActor) {
            State_SetScriptFlag(0xB9, 1);
        }

        mode = 3;
    } else {
        Battle_PlaySpellCastAnimation(action->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 0);
        Audio_PlayWorldSfx(0x7E, 0x35, &soundPosition);
        mode = 1;
    }

    request.effectId = Battle_actionDefinitions[action->actionId].effectId;
    request.otherCount = 0;
    request.source.type = 4;
    request.source.value.actor.actor = action->primaryTarget.target.targetActor;
    request.source.value.actor.eventActorSlot = 0;
    request.source.value.actor.part =
        Actors_FindActorBoneTag(action->primaryTarget.target.targetActor, 255, 2);

    count = 0;

    for (i = 0; i < action->targetCount; ++i) {
        if (!action->targets[i].targetsPosition) {
            duplicate = 0;

            for (j = 0; j < count; ++j) {
                if (!action->targets[j].targetsPosition
                    && request.targets[j].value.actor.actor
                           == action->targets[i].target.targetActor) {
                    request.targets[j].value.actor.part = 0;
                    duplicate = 1;
                }
            }
            if (duplicate) {
                continue;
            }

            request.targets[count].type = 4;
            request.targets[count].value.actor.actor = action->targets[i].target.targetActor;
            request.targets[count].value.actor.eventActorSlot = 0;
            request.targets[count].value.actor.part = Actors_FindActorBoneTag(
                action->targets[i].target.targetActor, action->targets[i].target.targetLimb, 2);
            // BUG: tests the limb of the entry at the output index
            if (action->targets[count].target.targetLimb == -1) {
                request.targets[count].value.actor.part = Actors_FindActorBoneTag(
                    action->targets[i].target.targetActor,
                    Battle_actors[action->targets[i].target.targetActor]->stats->defaultTargetLimb, 2);
            } else {
                request.targets[count].value.actor.part =
                    Actors_FindActorBoneTag(action->targets[i].target.targetActor,
                        action->targets[i].target.targetLimb, 2);
            }

            ++count;
        } else {
            request.targets[count].type = 5;
            request.targets[count].value.position = *(SVECTOR*)&action->targets[i].target;
            ++count;
        }
    }

    request.count = count;

    for (i = 0; i < 16; ++i) {
        if (Battle_actors[i]
            && (Battle_actors[i]->allocationKind == 1 || Battle_actors[i]->allocationKind == 2)) {
            duplicate = 0;

            for (j = 0; j < action->targetCount; ++j) {
                // BUG: indexes the hit list with the actor index
                if (!action->targets[j].targetsPosition && i == action->targets[i].target.targetActor) {
                    duplicate = 1;
                    break;
                }
            }
            if (i == action->primaryTarget.target.targetActor) {
                duplicate = 1;
            }
            if (!duplicate) {
                request.others[request.otherCount] = i;
                ++request.otherCount;
            }
        }
    }

    copyVector(&request.position, &action->focusPosition);

    if (request.effectId == 39 && action->targets[0].targetsPosition == 4) {
        request.effectId = 36;
    }

    Effects_BeginActionEffect(&request, mode, action->effectVariant);

    if (action->actionId >= 141 && action->actionId < 224) {
        Battle_Actor* actor = Battle_actors[action->primaryTarget.target.targetActor];

        object.assetKind = 7;
        object.targetSlot = action->primaryTarget.target.targetActor;
        object.modelId = 0;

        if (action->actionId < 184) {
            object.subKind = 5;
            object.modelId =
                actor->stats->limbActions[(u_short)actor->commandArg0][(u_short)(actor->commandArg0 >> 16)]
                    .effectModelId;
        } else if (object.targetSlot) {
            object.subKind = 5;
            object.modelId =
                actor->stats->limbActions[(u_short)actor->commandArg0][(u_short)(actor->commandArg0 >> 16)]
                    .effectModelId;
        } else {
            object.subKind = 4;
            object.modelId = (action->actionId - 184) % 4;
        }

        object.material = actor->equippedWeaponCategory;
        Content_QueueActorAssetRequest(&object);
    }

    Battle_SetHudHidden(1);

    if (Input_settings.information) {
        Menu_ShowSceneNotification(0xB, action->actionId, 1);
    }

    Battle_actionFlowMode = 5;
}

// decomp: BATTLE.PRG func_800719DC (src/BATTLE/BATTLE.PRG/146C.c:4084)
void Battle_CommitEnvironmentAction(int arg0 __attribute__((unused))) { Battle_CommitSelectedAction(0); }

// decomp: BATTLE.PRG func_80072B9C (src/BATTLE/BATTLE.PRG/146C.c:4414)
void Battle_ResetActionPhase(void) { Battle_actionFlowMode = 0; }

// decomp: BATTLE.PRG func_800734C0 (src/BATTLE/BATTLE.PRG/146C.c:4646)
void Battle_BeginAshleyWeaponToggle(void)
{
    int i;

    for (i = 0; i < 16; ++i) {
        Actors_RestoreActorIdlePose(i);
    }
    Battle_SetActorBattleMode(0, 2);
    Field_controlMode = 10;
}

// decomp: BATTLE.PRG func_8007350C (src/BATTLE/BATTLE.PRG/146C.c:4657)
void Battle_EndAshleyWeaponToggle(void) { Field_EnterFieldExplorationMode(); }

// decomp: BATTLE.PRG func_80074A20 (src/BATTLE/BATTLE.PRG/146C.c:5315)
int Battle_BeginActorDefeatAnimation(int actor_slot)
{
    int ret;

    Battle_Ai_RefreshActorAiReservations(actor_slot);
    ret = 0;

    if (Battle_actors[actor_slot] != NULL) {
        Battle_actors[actor_slot]->commandState = 0;
        if (Battle_actors[actor_slot]->stats->currentHp == 0) {
            Battle_SetActorMarker(actor_slot, 0);
            Battle_actors[actor_slot]->commandState = 0x20;
            if (Battle_actors[actor_slot]->retainAfterDefeat == 0) {
                Actors_BeginActorDeath(actor_slot, &Battle_actionState->rangeOrigin, 1);
                Actors_SetActorAnimationLocked(actor_slot, 1);
                if ((Battle_actors[actor_slot]->roomSpawnIndex != 0x80) || (actor_slot == 0)) {
                    ret = 1;
                }
            }
        }
    }
    return ret;
}

// decomp: INITBTL.PRG func_800F9818 (src/BATTLE/INITBTL.PRG/18.c:240)
void Battle_ApplyLearnedActionUnlocks(void)
{
    int i;
    int learned_flag_index;

    for (i = 0; i < 256; ++i) {
        learned_flag_index = i;
        if (i < 0) {
            learned_flag_index = i + 7;
        }
        if (State_learnedActionFlags[learned_flag_index >> 3] & (0x80 >> (i - ((learned_flag_index >> 3) * 8)))) {
            Battle_actionDefinitions[i].unlocked = 1;
        } else {
            Battle_actionDefinitions[i].unlocked = 0;
        }
    }
}

}  // extern "C"
