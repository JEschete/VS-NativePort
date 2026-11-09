// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Actors/ActorRosterLive.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Audio/AmbienceLive.hpp"
#include "Battle/AbilitiesLive.hpp"
#include "Field/GameModeLive.hpp"
#include "Field/MapLive.hpp"
#include "Field/RoomRenderingLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
#include "Render/PaletteAnimationLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionTable.h"
#include "Battle/Hit.h"
#include "Battle/Rewards.h"
#include "Battle/TargetingState.h"
#include "Effects/ActionEffects.h"
#include "Effects/Runtime.h"
#include "Field/GameModeLiveState.h"
#include "Field/PlatformPlacementLive.h"
#include "Items/LootLiveState.h"
#include "Menu/TextBox.h"

// decomp: D_800F1860 (D_800F1860@BATTLE/BATTLE.PRG)
int Battle_lootMenuOpened;

// decomp: D_800F1864 (D_800F1864@BATTLE/BATTLE.PRG)
int Battle_rewardsFollowAction;

// decomp: BATTLE.PRG func_800797BC (src/BATTLE/BATTLE.PRG/146C.c:7516)
void Battle_UpdateLootResults(void)
{
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    if (VS::Menu::Framework::LiveActionSelectionState()->menuStatus != 1) {
        VS::Field::LiveResumeFieldAfterMenu();
        VS::Menu::Framework::ReleaseLiveMenuRows();
        VS::Battle::LiveBeginPendingAbilityReward();
    }
}

// decomp: BATTLE.PRG func_8007980C (src/BATTLE/BATTLE.PRG/146C.c:7526)
void Battle_UpdateAbilityUnlockDelay(void)
{
    if (!(Effects_effectBusyFlags & 3)) {
        VS::Battle::LiveGrantPendingAbilityUnlock();
    }
}

// decomp: BATTLE.PRG func_8007983C (src/BATTLE/BATTLE.PRG/146C.c:7533)
void Battle_UpdateAbilityUnlockResults(void)
{
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    if ((VS::Menu::Framework::LiveActionSelectionState()->menuStatus != 1) && (Menu_IsSceneNotificationPending() == 0)) {
        VS::Field::LiveResumeFieldAfterMenu();
        VS::Menu::Framework::ReleaseLiveMenuRows();
        Menu_ReleaseSceneNotification();
        VS::Battle::LiveBeginPendingAbilityReward();
    }
}

// decomp: BATTLE.PRG func_8007B410 (src/BATTLE/BATTLE.PRG/146C.c:8287)
void Battle_BeginRewardPause(void)
{
    VS::Render::TransformLiveRoomPalettes(0xD, 4, -4, -4, 0);
    VS::Render::SetLiveClutAnimationEnabled(0);
    VS::Actors::UnfreezeLiveActorAnimations();
    VS::Field::LiveSetRoomAnimationsPaused(1);
    VS::Field::SetLiveMinimapVisible(0);
    VS::Audio::LiveSetRoomAmbienceDucked(1);
    Field_isInteractionPaused = 1;
}

// decomp: BATTLE.PRG func_8007B470 (src/BATTLE/BATTLE.PRG/146C.c:8298)
void Battle_EndRewardPause(void)
{
    VS::Render::SetLiveClutAnimationEnabled(1);
    VS::Render::TransformLiveSceneColors(8, 4, 0, 0, 0);
    VS::Actors::FreezeLiveActorAnimations();
    VS::Field::LiveSetRoomAnimationsPaused(0);
    VS::Audio::LiveSetRoomAmbienceDucked(0);
    Field_isInteractionPaused = 0;
}

// decomp: BATTLE.PRG func_8007B4C4 (src/BATTLE/BATTLE.PRG/146C.c:8308)
void Battle_ResumeAfterRewards(void)
{
    Battle_actionFlowMode = 2;
    Battle_EndRewardPause();
    if (Battle_rewardsFollowAction != 0) {
        VS::Actors::RestoreLiveActorIdlePose(0);
    }
}

// decomp: BATTLE.PRG func_8007B508 (src/BATTLE/BATTLE.PRG/146C.c:8317)
void Battle_CheckRewardUnlocks(void)
{
    if (!(VS::State::LiveScriptFlag(0xB6)) || ((VS::State::LiveScriptFlag(0xB6)) == 2)
        || ((VS::State::LiveScriptFlag(0xB6)) == 3)) {
        if (VS::Battle::LivePendingBattleAbilityUnlocks() != 0) {
            Battle_actionFlowMode = 5;
            if (Battle_rewardsFollowAction != 0) {
                VS::Actors::PlayLiveActorActionPose(0, NULL, 6);
                Effects_SpawnAbilityLearnedEffect();
            }
            Field_SetMovingPlatformsPausedEntry(1);
            return;
        }
    }
    if ((Battle_actionState->selection.actionId != 0)
        && !((Battle_actionDefinitions[Battle_actionState->selection.actionId].unlocked))
        && (Battle_actionState->selection.primaryTarget.targetsPosition == 0) && (Battle_actionState->chainContinued == 0)
        && (Battle_actionState->selection.primaryTarget.target.targetActor == 0)) {
        Battle_actionFlowMode = 5;
        if (Battle_rewardsFollowAction != 0) {
            VS::Actors::PlayLiveActorActionPose(0, NULL, 6);
            Effects_SpawnAbilityLearnedEffect();
        }
        Field_SetMovingPlatformsPausedEntry(1);
        return;
    }
    Battle_ResumeAfterRewards();
}

// decomp: BATTLE.PRG func_8007B63C (src/BATTLE/BATTLE.PRG/146C.c:8346)
void Battle_ShowPendingRewardUnlock(void)
{
    u_int pending_unlocks;

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
    pending_unlocks = VS::Battle::LivePendingBattleAbilityUnlocks();
    Battle_actionFlowMode = 6;
    Battle_BeginRewardPause();
    VS::Menu::Framework::OpenLiveAbilityUnlockMenu(pending_unlocks >> 0x10, pending_unlocks & 0xFFFF);
}

// decomp: BATTLE.PRG func_8007B764 (src/BATTLE/BATTLE.PRG/146C.c:8366)
void Battle_OpenPendingLootRewards(void)
{
    if ((VS::State::LiveScriptFlag(0xAB)) >= 3) {
        if (Items_lootListHead != NULL) {
            VS::Menu::Framework::OpenLiveLootListMenu(Items_lootListHead);
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
    if (VS::State::LiveScriptFlag(0xAB) >= 3) {
        VS::Menu::Framework::OpenLiveChallengeMenu();
        Battle_actionFlowMode = 3;
        Battle_BeginRewardPause();
        return;
    }
    Battle_OpenPendingLootRewards();
}

// decomp: BATTLE.PRG func_8007B834 (src/BATTLE/BATTLE.PRG/146C.c:8392)
void Battle_UpdateRewardResultsDialog(void)
{
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    if (VS::Menu::Framework::LiveActionSelectionState()->menuStatus != 1) {
        Battle_EndRewardPause();
        VS::Menu::Framework::ReleaseLiveMenuRows();
        Battle_OpenPendingLootRewards();
    }
}

// decomp: BATTLE.PRG func_8007B884 (src/BATTLE/BATTLE.PRG/146C.c:8402)
void Battle_UpdatePendingLootDialog(void)
{
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    if (VS::Menu::Framework::LiveActionSelectionState()->menuStatus != 1) {
        if (Battle_rewardsFollowAction != 0) {
            Battle_EndRewardPause();
        }
        VS::Menu::Framework::ReleaseLiveMenuRows();
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
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    if ((VS::Menu::Framework::LiveActionSelectionState()->menuStatus != 1) && (Menu_IsSceneNotificationPending() == 0)) {
        Battle_EndRewardPause();
        VS::Menu::Framework::ReleaseLiveMenuRows();
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

}  // extern "C"
