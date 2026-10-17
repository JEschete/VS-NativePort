// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/Animation.h"
#include "Actors/Lifecycle.h"
#include "Audio/Ambience.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Hit.h"
#include "Battle/Rewards.h"
#include "Battle/Targeting.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/GameMode.h"
#include "Field/Gimmicks.h"
#include "Field/Map.h"
#include "Field/RoomRendering.h"
#include "Items/Loot.h"
#include "Menu/Framework/ActionSelection.h"
#include "Menu/Framework/Framework.h"
#include "Menu/TextBox.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: D_800F1860 (D_800F1860@BATTLE/BATTLE.PRG)
int Battle_lootMenuOpened;

// decomp: D_800F1864 (D_800F1864@BATTLE/BATTLE.PRG)
int Battle_rewardsFollowAction;

// decomp: BATTLE.PRG func_800797BC (src/BATTLE/BATTLE.PRG/146C.c:7516)
void Battle_UpdateLootResults(void)
{
    Menu_Framework_UpdateMenuOverlay();
    if (Menu_Framework_ActionSelectionState()->menuStatus != 1) {
        Field_ResumeFieldAfterMenu();
        Menu_Framework_ReleaseMenuRows();
        Battle_BeginPendingAbilityReward();
    }
}

// decomp: BATTLE.PRG func_8007980C (src/BATTLE/BATTLE.PRG/146C.c:7526)
void Battle_UpdateAbilityUnlockDelay(void)
{
    if (!(Effects_effectBusyFlags & 3)) {
        Battle_GrantPendingAbilityUnlock();
    }
}

// decomp: BATTLE.PRG func_8007983C (src/BATTLE/BATTLE.PRG/146C.c:7533)
void Battle_UpdateAbilityUnlockResults(void)
{
    Menu_Framework_UpdateMenuOverlay();
    if ((Menu_Framework_ActionSelectionState()->menuStatus != 1) && (Menu_IsSceneNotificationPending() == 0)) {
        Field_ResumeFieldAfterMenu();
        Menu_Framework_ReleaseMenuRows();
        Menu_ReleaseSceneNotification();
        Battle_BeginPendingAbilityReward();
    }
}

// decomp: BATTLE.PRG func_8007B410 (src/BATTLE/BATTLE.PRG/146C.c:8287)
void Battle_BeginRewardPause(void)
{
    Render_TransformRoomPalettes(0xD, 4, -4, -4, 0);
    Render_SetClutAnimationEnabled(0);
    Actors_UnfreezeAllActorAnimations();
    Field_SetRoomAnimationsPaused(1);
    Field_SetMinimapVisible(0);
    Audio_SetRoomAmbienceDucked(1);
    Field_isInteractionPaused = 1;
}

// decomp: BATTLE.PRG func_8007B470 (src/BATTLE/BATTLE.PRG/146C.c:8298)
void Battle_EndRewardPause(void)
{
    Render_SetClutAnimationEnabled(1);
    Render_TransformSceneColors(8, 4, 0, 0, 0);
    Actors_FreezeAllActorAnimations();
    Field_SetRoomAnimationsPaused(0);
    Audio_SetRoomAmbienceDucked(0);
    Field_isInteractionPaused = 0;
}

// decomp: BATTLE.PRG func_8007B4C4 (src/BATTLE/BATTLE.PRG/146C.c:8308)
void Battle_ResumeAfterRewards(void)
{
    Battle_actionFlowMode = 2;
    Battle_EndRewardPause();
    if (Battle_rewardsFollowAction != 0) {
        Actors_RestoreActorIdlePose(0);
    }
}

// decomp: BATTLE.PRG func_8007B508 (src/BATTLE/BATTLE.PRG/146C.c:8317)
void Battle_CheckRewardUnlocks(void)
{
    if (!(State_ScriptFlag(0xB6)) || ((State_ScriptFlag(0xB6)) == 2)
        || ((State_ScriptFlag(0xB6)) == 3)) {
        if (Battle_PendingBattleAbilityUnlocks() != 0) {
            Battle_actionFlowMode = 5;
            if (Battle_rewardsFollowAction != 0) {
                ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
                Effects_SpawnAbilityLearnedEffect();
            }
            Field_SetMovingPlatformsPaused(1);
            return;
        }
    }
    if ((Battle_actionState->selection.actionId != 0)
        && !((Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked))
        && (Battle_actionState->selection.primaryTarget.targetsPosition == 0) && (Battle_actionState->chainContinued == 0)
        && (Battle_actionState->selection.primaryTarget.target.targetActor == 0)) {
        Battle_actionFlowMode = 5;
        if (Battle_rewardsFollowAction != 0) {
            ((void (*)(int, short*, int))Actors_PlayActorActionPose)(0, NULL, 6);
            Effects_SpawnAbilityLearnedEffect();
        }
        Field_SetMovingPlatformsPaused(1);
        return;
    }
    Battle_ResumeAfterRewards();
}

// decomp: BATTLE.PRG func_8007B63C (src/BATTLE/BATTLE.PRG/146C.c:8346)
void Battle_ShowPendingRewardUnlock(void)
{
    u_int temp_v0_2;

    if ((Battle_actionState->selection.actionId != 0)
        && !((Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked))
        && (Battle_actionState->selection.primaryTarget.targetsPosition == 0) && (Battle_actionState->chainContinued == 0)
        && (Battle_actionState->selection.primaryTarget.target.targetActor == 0)) {
        Battle_actionFlowMode = 6;
        Battle_BeginRewardPause();
        Menu_ShowSceneNotification(0xC, Battle_actionState->selection.actionId, 1);
        Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked = 1;
        return;
    }
    temp_v0_2 = Battle_PendingBattleAbilityUnlocks();
    Battle_actionFlowMode = 6;
    Battle_BeginRewardPause();
    Menu_Framework_OpenAbilityUnlockMenu(temp_v0_2 >> 0x10, temp_v0_2 & 0xFFFF);
}

// decomp: BATTLE.PRG func_8007B764 (src/BATTLE/BATTLE.PRG/146C.c:8366)
void Battle_OpenPendingLootRewards(void)
{
    if ((State_ScriptFlag(0xAB)) >= 3) {
        if (Items_lootListHead != NULL) {
            Menu_Framework_OpenLootListMenu(Items_lootListHead);
            Battle_actionFlowMode = 4;
            Battle_BeginRewardPause();
            return;
        }
        Battle_CheckRewardUnlocks();
        return;
    }
    Battle_ResumeAfterRewards();
}

// decomp: BATTLE.PRG func_8007B7DC (src/BATTLE/BATTLE.PRG/146C.c:8381)
void Battle_OpenRewardResults()
{
    if (State_ScriptFlag(0xAB) >= 3) {
        Menu_Framework_OpenChallengeMenu();
        Battle_actionFlowMode = 3;
        Battle_BeginRewardPause();
        return;
    }
    Battle_OpenPendingLootRewards();
}

// decomp: BATTLE.PRG func_8007B834 (src/BATTLE/BATTLE.PRG/146C.c:8392)
void Battle_UpdateRewardResultsDialog(void)
{
    Menu_Framework_UpdateMenuOverlay();
    if (Menu_Framework_ActionSelectionState()->menuStatus != 1) {
        Battle_EndRewardPause();
        Menu_Framework_ReleaseMenuRows();
        Battle_OpenPendingLootRewards();
    }
}

// decomp: BATTLE.PRG func_8007B884 (src/BATTLE/BATTLE.PRG/146C.c:8402)
void Battle_UpdatePendingLootDialog(void)
{
    Menu_Framework_UpdateMenuOverlay();
    if (Menu_Framework_ActionSelectionState()->menuStatus != 1) {
        if (Battle_rewardsFollowAction != 0) {
            Battle_EndRewardPause();
        }
        Menu_Framework_ReleaseMenuRows();
        if (Battle_lootMenuOpened != 0) {
            Battle_ResumeAfterRewards();
        } else {
            Battle_CheckRewardUnlocks();
        }
    }
}

// decomp: BATTLE.PRG func_8007B908 (src/BATTLE/BATTLE.PRG/146C.c:8418)
void Battle_WaitForRewardUnlockEffects(void)
{
    if (!(Effects_effectBusyFlags & 3)) {
        Battle_ShowPendingRewardUnlock();
    }
}

// decomp: BATTLE.PRG func_8007B938 (src/BATTLE/BATTLE.PRG/146C.c:8425)
void Battle_UpdateRewardUnlockDialog(void)
{
    Menu_Framework_UpdateMenuOverlay();
    if ((Menu_Framework_ActionSelectionState()->menuStatus != 1) && (Menu_IsSceneNotificationPending() == 0)) {
        Battle_EndRewardPause();
        Menu_Framework_ReleaseMenuRows();
        Menu_ReleaseSceneNotification();
        Battle_CheckRewardUnlocks();
    }
}

// decomp: BATTLE.PRG func_8007B9A0 (src/BATTLE/BATTLE.PRG/146C.c:8436)
void Battle_BeginEventRewardResults(void)
{
    Battle_lootMenuOpened = 0;
    Battle_rewardsFollowAction = 0;
    Battle_OpenRewardResults();
}

// decomp: BATTLE.PRG func_8007B9CC (src/BATTLE/BATTLE.PRG/146C.c:8443)
void Battle_BeginPostActionRewards(void)
{
    Battle_lootMenuOpened = 0;
    Battle_rewardsFollowAction = 1;
    Battle_OpenPendingLootRewards();
}

// decomp: BATTLE.PRG func_8007B9FC (src/BATTLE/BATTLE.PRG/146C.c:8450)
int Battle_UpdateRewardSequence(void)
{
    switch (Battle_actionFlowMode) {
    case 3:
        Battle_UpdateRewardResultsDialog();
        return 1;
    case 4:
        Battle_UpdatePendingLootDialog();
        return 1;
    case 5:
        Battle_WaitForRewardUnlockEffects();
        return 1;
    case 6:
        Battle_UpdateRewardUnlockDialog();
        return 1;
    default:
        return 0;
    }
}

