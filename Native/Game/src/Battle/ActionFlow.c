// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorAllocation.h"
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
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
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
void Battle_ResolveEnemyDefeat(int arg0, int actorId, int arg2)
{
    Battle_Actor* enemy = Battle_actors[actorId];

    if ((enemy->roomSpawnIndex == 0x80) || (enemy->defeated != 0)) {
        return;
    }

    if ((arg2 != 0) && (arg0 == 0)) {
        State_RecordEnemyKillScore(enemy);
        State_UpdateWeaponKillStreak(0);
    }

    Items_QueueEnemyLoot(enemy);

    enemy->defeated = 1;
}

// decomp: BATTLE.PRG func_8006C5AC (src/BATTLE/BATTLE.PRG/146C.c:1955)
int Battle_PlayWeaponHitSounds(int arg0)
{
    Actors_ActorLocationQuery sp10;
    SVECTOR sp20;
    Battle_ActionRequest* temp_s3;
    int i;

    temp_s3 = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    for (i = 0; i < temp_s3->targetCount; ++i) {
        if (temp_s3->targets[i].targetsPosition == 0) {

            Actors_ActorPlacement(temp_s3->targets[i].target.targetActor, &sp10);

            sp20.vx = sp10.location.position.vx;
            sp20.vz = sp10.location.position.vz;
            sp20.vy = sp10.location.position.vy;

            switch (temp_s3->targets[i].target.impactKind) {
            case 0:
            case 1:
            case 2:
            case 4:
                switch (Battle_actors[arg0]->equippedWeaponCategory) {
                case 1:
                    Audio_PlayWorldSfx(0x7E, 0x47, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x48, &sp20);
                    break;

                case 2:
                    Audio_PlayWorldSfx(0x7E, 0x49, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x4A, &sp20);
                    break;

                case 3:
                    Audio_PlayWorldSfx(0x7E, 0x4B, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x4C, &sp20);
                    break;

                case 4:
                    Audio_PlayWorldSfx(0x7E, 0x50, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x51, &sp20);
                    break;

                case 5:
                    Audio_PlayWorldSfx(0x7E, 0x52, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x53, &sp20);
                    break;

                case 6:
                    Audio_PlayWorldSfx(0x7E, 0x4D, &sp20);
                    break;

                case 7:
                    Audio_PlayWorldSfx(0x7E, 0x4E, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x4F, &sp20);
                    break;

                case 8:
                    Audio_PlayWorldSfx(0x7E, 0x54, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x55, &sp20);
                    break;

                case 10:
                    Audio_PlayWorldSfx(0x7E, 0x5B, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x5C, &sp20);
                    break;

                case 9:
                    Audio_PlayWorldSfx(0x7E, 0x69, &sp20);
                    Audio_PlayWorldSfx(0x7E, 0x6A, &sp20);
                    break;
                }

                break;

            case 3:
                Audio_PlayWorldSfx(0x7E, 0x33, &sp20);
                break;

            case 5:
                break;
            }
        }
    }

    return 1;
}

// decomp: BATTLE.PRG func_8006CA20 (src/BATTLE/BATTLE.PRG/146C.c:2106)
void Battle_ApplyActionSourceEffects(int arg0, SVECTOR* arg1)
{
    Battle_ActionRequest* temp_s0;

    if (arg0 >= 16) {
        return;
    }

    temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if ((temp_s0->primaryTarget.target.targetActor == arg0) && (temp_s0->primaryTarget.targetsPosition == 0)) {

        switch (temp_s0->primaryTarget.target.impactKind) {
        case 2:
        case 3:
        case 4:
            break;

        case 0:
        case 1:
            Battle_PlayActorDamageReaction(arg0, &temp_s0->focusPosition);
            break;

        case 5:
            if (arg0 == 0) {
                ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
            }

            break;
        }

        Battle_ApplyActorActionResult(&temp_s0->primaryTarget);
    }
}

// decomp: BATTLE.PRG func_8006CB04 (src/BATTLE/BATTLE.PRG/146C.c:2142) [INCLUDE_ASM: not yet ported]
void Battle_ApplyActionTargetEffects(int, SVECTOR*)
{
    VS_UNPORTED("Battle::ApplyActionTargetEffects");
}

// decomp: BATTLE.PRG func_8006F5FC (src/BATTLE/BATTLE.PRG/146C.c:3228)
void Battle_WaitForPostActionMovement(void)
{
    Field_controlMode = 12;
    Battle_actionFlowMode = 0;
    Actors_RestoreActorIdlePose(0);
}

// decomp: BATTLE.PRG func_8006FBCC (src/BATTLE/BATTLE.PRG/146C.c:3388)
void Battle_BeginPostActionRecovery(int arg0)
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
    Battle_recoveryRewardMode = arg0;
    State_tickSpeed = 2;
    Battle_actionFlowMode = 0xB;
}

// decomp: BATTLE.PRG func_8006FCBC (src/BATTLE/BATTLE.PRG/146C.c:3408)
void Battle_BeginActionTrailFade(void)
{
    Battle_ActionRequest* temp_s0;

    Battle_actionFlowMode = 10;
    temp_s0 = &Battle_actionState->queue[0];
    Effects_FadeWeaponTrails(temp_s0->primaryTarget.target.targetActor,
        Actors_FindActorBoneTag(temp_s0->primaryTarget.target.targetActor, 0xF0, 0));
}

// decomp: BATTLE.PRG func_8006FD0C (src/BATTLE/BATTLE.PRG/146C.c:3418)
void Battle_FinishBattleAbilitySequence(void) { Battle_actionFlowMode = 9; }

// decomp: BATTLE.PRG func_80070278 (src/BATTLE/BATTLE.PRG/146C.c:3510)
void Battle_StartActionAnimation(void)
{
    Battle_ActionRequest* temp_s0;
    int temp_a3;

    Battle_actionState->abilityWindow = 0;
    Battle_actionState->abilityTimer = 0;
    temp_s0 = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if (((temp_s0->actionId - 0x16u) < 0x20) && (temp_s0->primaryTarget.targetsPosition == 0)
        && (temp_s0->primaryTarget.target.targetActor == 0)) {
        State_SetScriptFlag(0xB8, 1);
    }

    temp_a3 = Battle_actionState->queueIndex;

    if (temp_a3 == 0) {
        if (!(Battle_actionDefinitions[temp_s0->actionId].returnsToMenuOnCancel)) {
            Battle_PlayMeleeAttackAnimation(
                temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, temp_s0->animationParameter);
            Battle_actionState->abilityWindow =
                Actors_AnimationBattleWindowFrame(temp_s0->primaryTarget.target.targetActor) * 2;
        } else {
            if (temp_s0->actionId >= 0x8D) {
                if (temp_s0->actionId < 0xB8) {
                    Battle_PlayBreakArtAnimation(
                        temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
                } else if (temp_s0->actionId < 0xE0) {
                    Battle_PlaySpecialActionAnimation(
                        temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
                } else {
                    Battle_PlaySpellCastAnimation(
                        temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
                }
            } else {
                Battle_PlaySpellCastAnimation(
                    temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 1);
            }
            Battle_actionState->abilityWindow = 0;
            State_tickSpeed = 4;
        }
    } else {
        if (temp_s0->actionId < 0x28) {
            Battle_PlayChainAbilityAnimation(temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition,
                Input_settings.mappedChainAbilities[Battle_actionState->chainAbilitySlot - 1] - 0x16,
                temp_a3);
        } else {
            Battle_PlayDefenseAbilityAnimation(temp_s0->primaryTarget.target.targetActor, &Battle_actionState->selection.focusPosition, 0);
        }
        Battle_actionState->abilityWindow =
            Actors_AnimationBattleWindowFrame(temp_s0->primaryTarget.target.targetActor) * 2;
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
void Battle_CommitSelectedAction(int arg0)
{
    int i;
    int temp_v1_2;
    u_char temp_s0;
    int var_a0;

    Field_StorePreferredActionCameraDistance();
    temp_s0 = Battle_actionState->selectedPreview;
    ((void (*)())Battle_ClearSphereTargets)();
    Battle_ClearPrimarySphereVolume();
    Battle_ClearSecondarySphereVolume();
    Battle_FreeSphere();

    if (arg0 != 0) {
        if (temp_s0 == 0xFF) {
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
    if (arg0 == 0) {
        Battle_CollectEnvironmentActionTargets();
    } else if (arg0 == 2) {
        Battle_SetAllActionTargets();
    } else {
        Battle_SetSingleActionTarget();
    }

    Battle_ResolveActionTargets(Battle_actionState->selection.actionId, Battle_actionState->queue, &Battle_actionState->selection, 1);

    if ((Battle_actionState->selection.primaryTarget.target.targetActor == 0)
        && ((temp_v1_2 = Battle_actionState->selection.actionId, (temp_v1_2 == 1))
            || ((temp_v1_2 - 0xB8) < 0x28U))
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
int Battle_BeginActorDefeatAnimation(int arg0)
{
    int ret;

    Battle_Ai_RefreshActorAiReservations(arg0);
    ret = 0;

    if (Battle_actors[arg0] != NULL) {
        Battle_actors[arg0]->commandState = 0;
        if (Battle_actors[arg0]->stats->currentHp == 0) {
            Battle_SetActorMarker(arg0, 0);
            Battle_actors[arg0]->commandState = 0x20;
            if (Battle_actors[arg0]->retainAfterDefeat == 0) {
                Actors_BeginActorDeath(arg0, &Battle_actionState->rangeOrigin, 1);
                Actors_SetActorAnimationLocked(arg0, 1);
                if ((Battle_actors[arg0]->roomSpawnIndex != 0x80) || (arg0 == 0)) {
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
    int var_v0;

    for (i = 0; i < 256; ++i) {
        var_v0 = i;
        if (i < 0) {
            var_v0 = i + 7;
        }
        if (State_learnedActionFlags[var_v0 >> 3] & (0x80 >> (i - ((var_v0 >> 3) * 8)))) {
            Battle_actionDefinitions[i].unlocked = 1;
        } else {
            Battle_actionDefinitions[i].unlocked = 0;
        }
    }
}

