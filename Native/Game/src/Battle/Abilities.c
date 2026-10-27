// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "ActorAssets.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/Location.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionMenu.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Perception.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Input/Buttons.h"
#include "Input/Controller.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Palette.h"
#include "State/Flags.h"
#include "State/Progress.h"
#include "State/Settings.h"
#include "State/Timers.h"

// decomp: vs_battle_characterState (vs_battle_characterState@BATTLE/BATTLE.PRG)
Battle_Actor* Battle_characterState;

// decomp: vs_main_artsPointsRequirements (vs_main_artsPointsRequirements@SLUS_010.40)
u_short Battle_breakArtPointRequirements[15][4]; /* contents: module image (ModuleData.c) */

// decomp: vs_main_battleAbilitiesPointsRequirements (vs_main_battleAbilitiesPointsRequirements@SLUS_010.40)
u_short Battle_battleAbilityPointRequirements[150]; /* contents: module image (ModuleData.c) */

// decomp: D_8005FFB0 (D_8005FFB0@SLUS_010.40)
short Battle_abilityInputResult;

// decomp: BATTLE.PRG func_80069C6C (src/BATTLE/BATTLE.PRG/146C.c:735)
void Battle_ArmDefenseAbilityTimingWindow(int target_actor_slot)
{
    Battle_HitResult* queued_source_hit;

    if (target_actor_slot != 0) {
        return;
    }

    queued_source_hit = &Battle_actionState->queue[Battle_actionState->queueIndex & 3].primaryTarget;

    if (queued_source_hit->target.targetActor == 0) {
        return;
    }

    // TODO: Remove dead code
    switch (queued_source_hit->target.targetActor != 0) {
    case 0:
        return;
    }

    if ((Battle_actionState->abilityWindow == 0) && (Battle_CanTriggerBattleAbility(0) != 0)) {
        Battle_actionState->abilityWindow =
            Battle_actionState->abilityTimer + State_tickSpeed * 2;
    }
}

// decomp: BATTLE.PRG _isArtOrAbilityUnlocked (src/BATTLE/BATTLE.PRG/146C.c:3270)
int Battle_PendingBattleAbilityUnlocks(void)
{
    int category;
    int unlockArt;
    int unlockBattleAbility;
    u_int artsLearned;

    unlockBattleAbility = 0;
    category = Battle_characterState->equippedWeaponCategory % 10;
    artsLearned = Audio_Seq_unusedArtsStatus.artsLearned[category];
    unlockArt = 0;

    if (artsLearned < 4) {
        unlockArt = Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[category]
                 >= Battle_breakArtPointRequirements[category][artsLearned];
    }
    if (unlockArt == 0) {
        if (Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked < 22) {
            if (Audio_Seq_unusedArtsStatus.kills.totalKills
                >= Battle_battleAbilityPointRequirements[Audio_Seq_unusedArtsStatus.kills
                        .battleAbilitiesUnlocked]) {
                unlockBattleAbility = 1;
            }
        }
    }
    return (unlockArt << 0x10) | unlockBattleAbility;
}

// decomp: BATTLE.PRG func_8006F89C (src/BATTLE/BATTLE.PRG/146C.c:3312)
void Battle_BeginPendingAbilityReward(void)
{
    switch (Battle_recoveryRewardMode) {
    case 0:
        Battle_WaitForPostActionMovement();
        break;
    case 2:
        if (Battle_PendingBattleAbilityUnlocks() != 0) {
            Battle_actionFlowMode = 0xD;
            ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
            Effects_SpawnAbilityLearnedEffect();
            Field_SetMovingPlatformsPaused(1);
        } else if ((Battle_actionState->selection.actionId != 0)
                   && !(Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked)
                   && (Battle_actionState->selection.primaryTarget.targetsPosition == 0) && (Battle_actionState->chainContinued == 0)
                   && (Battle_actionState->selection.primaryTarget.target.targetActor == 0)) {
            Battle_actionFlowMode = 0xD;
            ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
            Effects_SpawnAbilityLearnedEffect();
            Field_SetMovingPlatformsPaused(1);
        } else {
            Battle_WaitForPostActionMovement();
        }
        break;
    default:
        if ((Battle_actionState->selection.actionId != 0)
            && !(Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked)
            && (Battle_actionState->selection.primaryTarget.targetsPosition == 0) && (Battle_actionState->chainContinued == 0)
            && (Battle_actionState->selection.primaryTarget.target.targetActor == 0)) {
            Battle_actionFlowMode = 0xD;
            ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
            Effects_SpawnAbilityLearnedEffect();
            Field_SetMovingPlatformsPaused(1);
        } else {
            Battle_WaitForPostActionMovement();
        }
        break;
    }
}

// decomp: BATTLE.PRG func_8006FA20 (src/BATTLE/BATTLE.PRG/146C.c:3352)
void Battle_GrantPendingAbilityUnlock(void)
{
    u_int pending_unlocks;

    if ((Battle_actionState->selection.actionId != 0)
        && !((Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked))
        && (Battle_actionState->selection.primaryTarget.targetsPosition == 0) && (Battle_actionState->chainContinued == 0)
        && (Battle_actionState->selection.primaryTarget.target.targetActor == 0)) {
        Battle_actionFlowMode = 0xE;
        Field_PauseFieldForMenu();
        Menu_ShowSceneNotification(0xC, (int)Battle_actionState->selection.actionId, 1);
        Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked = 1;
        return;
    }
    pending_unlocks = Battle_PendingBattleAbilityUnlocks();
    Battle_actionFlowMode = 14;
    Field_PauseFieldForMenu();
    Menu_Framework_OpenAbilityUnlockMenu(pending_unlocks >> 0x10, pending_unlocks & 0xFFFF);
}

// decomp: BATTLE.PRG func_8006FD1C (src/BATTLE/BATTLE.PRG/146C.c:3420)
void Battle_AdvanceBattleAbilitySequence(void)
{
    Battle_HitResult saved;
    Actors_ActorLocationQuery position;
    Battle_ActionRequest* action;
    int i;
    int found;

    State_tickSpeed = 2;
    action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];
    if ((action->actionId >= 0x8D) && (action->actionId < 0xB8)) {
        Content_ReleaseActorSequence(action->primaryTarget.target.targetActor, 5);
    } else if ((action->actionId >= 0xB8) && (action->actionId < 0xE0)) {
        Content_ReleaseActorSequence(action->primaryTarget.target.targetActor, 4);
    }

    if (Battle_actionState->queueIndex >= 999) {
        Battle_FinishBattleAbilitySequence();
        return;
    }

    if (Battle_actionState->abilityInputSucceeded == 1) {
        if (Battle_actors[action->targets[0].target.targetActor]->stats->currentHp != 0) {
            ((void (*)(Battle_ActionRequest*))Battle_Ai_NotifyActionRelationshipChanges)(action);
            Battle_actionFlowMode = 8;
            ++Battle_actionState->chainStep;
            ++Battle_actionState->queueIndex;
            Battle_ResolveActionTargets(Input_settings.mappedChainAbilities[Battle_actionState->chainAbilitySlot - 1],
                &Battle_actionState->queue[Battle_actionState->queueIndex & 3], &Battle_actionState->selection, 1);
            Battle_QueueChainCount(Battle_actionState->queueIndex);
            if (Audio_Seq_unusedScoredata.maxChain < Battle_actionState->queueIndex) {
                Audio_Seq_unusedScoredata.maxChain = Battle_actionState->queueIndex;
            }
        } else {
            Battle_FinishBattleAbilitySequence();
        }
        if (((u_char)State_ScriptFlag(0x82) >= 10)
            && (Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked < 22)
            && (Audio_Seq_unusedArtsStatus.kills.totalKills < 0xFFFF)) {
            ++Audio_Seq_unusedArtsStatus.kills.totalKills;
        }
    } else if (Battle_actionState->abilityInputSucceeded == 2) {
        ((void (*)(Battle_ActionRequest*))Battle_Ai_NotifyActionRelationshipChanges)(action);
        Battle_actionFlowMode = 8;
        ++Battle_actionState->chainStep;
        saved = Battle_actionState->selection.primaryTarget;
        found = 0;
        for (i = 0; i < Battle_actionState->selection.targetCount; ++i) {
            if ((Battle_actionState->selection.targets[i].target.targetActor == 0)
                && (Battle_actionState->selection.targets[i].hpEffect
                    || Battle_actionState->selection.targets[i].mpEffect)) {
                Battle_actionState->selection.primaryTarget = Battle_actionState->selection.targets[i];
                found = 1;
            }
        }
        if (!found) {
            Battle_actionState->selection.primaryTarget = Battle_actionState->selection.targets[0];
        }
        Battle_actionState->selection.primaryTarget.target.mp = 0;
        Battle_actionState->selection.primaryTarget.target.hp = 0;
        for (i = 0; i < Battle_actionState->selection.targetCount; ++i) {
            if (Battle_actionState->selection.targets[i].target.targetActor
                == Battle_actionState->selection.primaryTarget.target.targetActor) {
                Battle_actionState->selection.primaryTarget.target.hp += Battle_actionState->selection.targets[i].target.hp;
                Battle_actionState->selection.primaryTarget.target.mp += Battle_actionState->selection.targets[i].target.mp;
            }
        }
        Battle_actionState->selection.targets[0] = saved;
        Battle_actionState->selection.targets[0].target.targetLimb =
            Battle_actors[Battle_actionState->selection.targets[0].target.targetActor]->stats->defaultTargetLimb;
        Battle_actionState->selection.targetCount = 1;
        Actors_ActorPlacement(saved.target.targetActor, &position);
        Battle_actionState->selection.focusPosition.vx = position.location.position.vx;
        Battle_actionState->selection.focusPosition.vz = position.location.position.vz;
        Battle_actionState->selection.focusPosition.vy = position.location.position.vy;
        ++Battle_actionState->queueIndex;
        action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];
        Battle_actionState->chainContinued = 1;
        Battle_ResolveActionTargets(Input_settings.mappedDefenseAbilities[Battle_actionState->chainAbilitySlot - 1],
            action, &Battle_actionState->selection, 1);
        if ((Audio_Seq_unusedArtsStatus.kills.battleAbilitiesUnlocked < 22)
            && (Audio_Seq_unusedArtsStatus.kills.totalKills < 0xFFFF)) {
            ++Audio_Seq_unusedArtsStatus.kills.totalKills;
        }
    } else {
        Battle_FinishBattleAbilitySequence();
    }
}

// decomp: BATTLE.PRG func_80078828 (src/BATTLE/BATTLE.PRG/146C.c:7043)
int Battle_CanTriggerBattleAbility(int ability_input_slot)
{
    Battle_ActionRequest* queued_action;
    int hasMappedAbility;
    int ret;
    int i;

    queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];
    ret = 1;

    if ((queued_action->primaryTarget.targetsPosition == 0) && (queued_action->targetCount != 0)
        && (queued_action->targets[0].targetsPosition == 0)) {

        if (queued_action->primaryTarget.target.targetActor == 0) {

            hasMappedAbility = 0;

            if (ability_input_slot == 0) {

                for (i = 0; i < 3; ++i) {

                    ret = 1;

                    if (Input_settings.mappedChainAbilities[i] != 0) {
                        hasMappedAbility = 1;
                    }
                }

                if (hasMappedAbility == 0) {
                    ret = 0;
                }
            }

        } else {

            hasMappedAbility = 0;

            if (ability_input_slot == 0) {

                for (i = 0; i < 3; ++i) {
                    if (Input_settings.mappedDefenseAbilities[i] != 0) {
                        hasMappedAbility = 1;
                    }
                }

                if (hasMappedAbility == 0) {
                    ret = 0;
                }
            }
        }

        if (queued_action->targets[0].target.targetActor == 0) {
            Actors_ActorStateFlags(0);
        } else if (Battle_actors[queued_action->targets[0].target.targetActor]->roomSpawnIndex == 0x80) {
            ret = 0;
        }

        if (queued_action->primaryTarget.target.targetActor != 0) {
            if (queued_action->targets[0].target.targetActor != 0) {
                ret = 0;
            }
        } else if (queued_action->actionId >= 40) {
            ret = 0;
        }

        if ((queued_action->actionId - 40u) < 14) {
            ret = 0;
        }

        if (queued_action->primaryTarget.target.targetActor == queued_action->targets[0].target.targetActor) {
            ret = 0;
        }

        if (ability_input_slot != 0) {

            if (queued_action->primaryTarget.target.targetActor == 0) {
                if (!(Battle_ActionCostAndAvailability(Input_settings.mappedChainAbilities[ability_input_slot - 1],
                          Battle_characterState->stats, 0)
                        & 0xFF000000)) {
                    ret = 0;
                }
            } else {
                if (!(Battle_ActionCostAndAvailability(Input_settings.mappedDefenseAbilities[ability_input_slot - 1],
                          Battle_characterState->stats, 0)
                        & 0xFF000000)) {
                    ret = 0;
                }
            }

            if (Battle_actionState->previousChainInput == ability_input_slot) {
                ret = 0;
            }
        }

        if (Battle_actionState->abilityInputAttempted != 0) {
            ret = 0;
        }
    } else {
        ret = 0;
    }

    return ret;
}

// decomp: BATTLE.PRG func_80078AB4 (src/BATTLE/BATTLE.PRG/146C.c:7147)
void Battle_UpdateBattleAbilityTiming(void)
{
    int timing_result;
    int i;
    int margin;
    int button;
    int isMapped;
    int abilityTimer;
    u_int* pressed_buttons;

    Battle_ActionRequest* queued_action = &Battle_actionState->queue[Battle_actionState->queueIndex & 3];

    if (queued_action->primaryTarget.target.targetActor == 0) {
        margin = State_ScriptFlag(1) ? 8 : 10;
    } else {
        margin = State_ScriptFlag(1) ? 8 : 10;
        if (queued_action->actionId >= 54) {
            margin += 4;
        }
    }

    margin -= Battle_actionState->queueIndex / 3;

    if (margin < 2) {
        margin = 2;
    }

    abilityTimer = Battle_actionState->abilityTimer;
    const Actors_ActorRecord* actingActor = Actors_actorRecords[queued_action->primaryTarget.target.targetActor];
    Core_Debugf("combat-flow", "animation id=%u state=%u active=%u held=%u timer=%d window=%u effects=%u action=%u",
        (unsigned)actingActor->animationId, (unsigned)actingActor->instance.actionAnimationState,
        (unsigned)actingActor->instance.currentAnimation, (unsigned)actingActor->pose.holdingLastFrame,
        abilityTimer, (unsigned)Battle_actionState->abilityWindow,
        (unsigned)Effects_effectBusyFlags, (unsigned)queued_action->actionId);

    if (Battle_actionState->abilityWindow != 0) {
        if (abilityTimer
            == (Battle_actionState->abilityWindow - (State_tickSpeed * 2))) {
            if (Battle_CanTriggerBattleAbility(0) != 0) {
                do {
                    Battle_ShowAbilityTimingCue((margin * 2) / State_tickSpeed);
                } while (0);
            }
        }
    }

    Battle_actionState->abilityTimer += State_tickSpeed;

    pressed_buttons = &Input_buttonsPressed.allButtons;

    if (*pressed_buttons & (PADRup | PADRright | PADRleft)) {

        if (*pressed_buttons & PADRleft) {
            button = 3;
        } else if (*pressed_buttons & PADRup) {
            button = 2;
        } else if (*pressed_buttons & PADRright) {
            button = 1;
        }

        isMapped = 0;

        if (queued_action->primaryTarget.targetsPosition == 0) {
            if (queued_action->primaryTarget.target.targetActor == 0) {
                isMapped = Input_settings.mappedChainAbilities[button - 1] != 0;
            } else if (Input_settings.mappedDefenseAbilities[button - 1] != 0) {
                isMapped = 1;
            }
        }

        if (isMapped != 0) {

            if (Battle_CanTriggerBattleAbility(button) != 0) {

                if (Battle_actionState->abilityWindow == 0) {
                    Battle_QueueAbilityTimingResult(1);
                    Battle_actionState->abilityInputAttempted = 1;
                } else if (queued_action->primaryTarget.targetsPosition == 0) {

                    if ((Battle_actionState->abilityWindow - margin)
                        >= abilityTimer) {
                        Battle_QueueAbilityTimingResult(0);
                        Battle_actionState->abilityInputSucceeded = 0;
                        Battle_actionState->abilityInputAttempted = 1;
                    } else if (Battle_actionState->abilityWindow >= abilityTimer) {
                        Battle_QueueAbilityTimingResult(1);
                        Battle_actionState->abilityInputSucceeded = 0;
                        Battle_actionState->abilityInputAttempted = 1;
                    } else if ((Battle_actionState->abilityWindow + margin)
                               >= abilityTimer) {

                        Battle_QueueAbilityTimingResult(2);

                        if (Battle_actionState->abilityInputAttempted == 0) {

                            int v =
                                (~Battle_actionState->previousChainButtons & (PADRup | PADRright | PADRleft));

                            if (Input_buttonsPressed.allButtons & v) {

                                if (queued_action->primaryTarget.target.targetActor == 0) {

                                    if ((Battle_actionState->previousChainInput != button)
                                        && (Input_settings
                                                .mappedChainAbilities[button - 1]
                                            != 0)) {

                                        Battle_actionState->chainAbilitySlot =
                                            Battle_actionState->previousChainInput = button;
                                        Battle_actionState->abilityInputSucceeded = 1;
                                        Battle_actionState->abilityInputAttempted = 1;

                                    } else {
                                        Battle_actionState->abilityInputSucceeded = 0;
                                        Battle_actionState->abilityInputAttempted = 1;
                                    }

                                } else {

                                    for (i = 0; i < queued_action->targetCount; ++i) {

                                        if ((queued_action->targets[0].target.targetActor == 0)
                                            && (queued_action->targets[0].targetsPosition == 0)) {

                                            if ((Battle_actionState->previousChainInput != button)
                                                && (Input_settings
                                                        .mappedDefenseAbilities[button
                                                                                - 1]
                                                    != 0)) {

                                                Battle_actionState->chainAbilitySlot =
                                                    Battle_actionState->previousChainInput =
                                                        button;
                                                Battle_actionState->abilityInputSucceeded =
                                                    2;
                                                Battle_actionState->abilityInputAttempted =
                                                    1;

                                                if (Audio_Seq_unusedScoredata.chainStreak
                                                    <= 0xFFFEU) {
                                                    ++Audio_Seq_unusedScoredata.chainStreak;
                                                }

                                            } else {
                                                Battle_actionState->abilityInputSucceeded =
                                                    0;
                                                Battle_actionState->abilityInputAttempted =
                                                    1;
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    } else {

                        timing_result = 4;

                        if ((Battle_actionState->abilityWindow + (margin * 2))
                            >= abilityTimer) {
                            timing_result = 3;
                        }

                        Battle_QueueAbilityTimingResult(timing_result);

                        Battle_actionState->abilityInputSucceeded = 0;
                        Battle_actionState->abilityInputAttempted = 1;
                    }

                    if (Battle_actionState->abilityInputSucceeded == 0) {
                        Battle_abilityInputResult = 0;
                    }

                } else {

                    if (!(Effects_effectBusyFlags & 3)) {
                        Battle_AdvanceBattleAbilitySequence();
                    }

                    return;
                }

            } else if (Battle_actionState->abilityInputAttempted == 0) {
                Battle_actionState->abilityInputSucceeded = 0;
                Battle_actionState->abilityInputAttempted = 1;
            }
        }
    }

    if (queued_action->primaryTarget.targetsPosition == 0) {

        if (((u_int (*)(int))Actors_IsActorActionAnimationBusy)(queued_action->primaryTarget.target.targetActor) != 0) {
            return;
        }

        if ((Battle_actionState->abilityInputAttempted != 0)
            || (Battle_actionState->chainAbilitySlot == 0)) {
            Actors_RestoreActorIdlePose(queued_action->primaryTarget.target.targetActor);
        }

        if ((queued_action->targets[0].targetsPosition == 0)
            && (Battle_actors[queued_action->targets[0].target.targetActor]->stats->currentHp
                == 0)) {
            Actors_RestoreActorIdlePose(queued_action->primaryTarget.target.targetActor);
        }
    }

    if (!(Effects_effectBusyFlags & 3)) {
        Battle_AdvanceBattleAbilitySequence();
    }
}

// decomp: BATTLE.PRG vs_battle_isActionUnlocked (src/BATTLE/BATTLE.PRG/146C.c:9241)
int Battle_IsActionUnlocked(u_int action_id)
{
    if ((action_id - 1) < 0xFF) {
        return Battle_actionDefinitions[action_id].unlocked;
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_setActionUnlocked (src/BATTLE/BATTLE.PRG/146C.c:9249)
void Battle_UnlockAction(u_int action_id)
{
    if ((action_id - 1) < 0xFF) {
        Battle_actionDefinitions[action_id].unlocked = 1;
    }
}

// decomp: BATTLE.PRG vs_battle_isSpellClassUnlocked (src/BATTLE/BATTLE.PRG/5BF94.c:3631)
int Battle_HasUnlockedSpellClass(int spellClass)
{
    int i;

    for (i = 0; i < Battle_spellClassCounts[spellClass]; ++i) {
        if (Battle_actionDefinitions[Battle_spellClassSpellIds[spellClass][i]].unlocked) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _breakArtsUnlocked (src/BATTLE/BATTLE.PRG/5BF94.c:3643)
int Battle_HasUnlockedWeaponBreakArt(void)
{
    int i;
    for (i = 0; i < 4; ++i) {
        if ((Battle_actionDefinitions
                    [0xB8 + ((Battle_characterState->equippedWeaponCategory - 1) * 4)
                        + i]
                        .unlocked)) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG vs_battle_chainAbilitiesUnlocked (src/BATTLE/BATTLE.PRG/5BF94.c:3657)
int Battle_HasUnlockedBattleAbility(int defense)
{
    int i;
    Battle_ActionDefinition* action_definitions;

    for (i = 0; i < 14; ++i) {
        action_definitions = Battle_actionDefinitions;
        if (action_definitions[defense == 0 ? Battle_chainAbilityActionIds[i]
                            : Battle_defenseAbilityActionIds[i]]
                .unlocked) {
            return 1;
        }
    }
    return 0;
}

