// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/RewardFlowLive.hpp"
#include "Menu/Framework/FrameworkLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Battle/ActionFlow.h"
#include "Battle/Rewards.h"
#include "Effects/Runtime.h"
#include "Menu/TextBox.h"

// decomp: D_800F1860 (D_800F1860@BATTLE/BATTLE.PRG)
int Battle_lootMenuOpened;

// decomp: D_800F1864 (D_800F1864@BATTLE/BATTLE.PRG)
int Battle_rewardsFollowAction;

// decomp: BATTLE.PRG func_8007B908 (src/BATTLE/BATTLE.PRG/146C.c:8418)
void Battle_WaitForRewardUnlockEffects(void)
{
    if (!(Effects_effectBusyFlags & 3)) {
        VS::Battle::ShowLivePendingRewardUnlock();
    }
}

// decomp: BATTLE.PRG func_8007B938 (src/BATTLE/BATTLE.PRG/146C.c:8425)
void Battle_UpdateRewardUnlockDialog(void)
{
    VS::Menu::Framework::UpdateLiveMenuOverlay();
    if ((VS::Menu::Framework::LiveActionSelectionState()->menuStatus != 1) && (Menu_IsSceneNotificationPending() == 0)) {
        VS::Battle::EndLiveRewardPause();
        VS::Menu::Framework::ReleaseLiveMenuRows();
        Menu_ReleaseSceneNotification();
        VS::Battle::CheckLiveRewardUnlocks();
    }
}

// decomp: BATTLE.PRG func_8007B9A0 (src/BATTLE/BATTLE.PRG/146C.c:8436)
void Battle_BeginEventRewardResults(void)
{
    Battle_lootMenuOpened = 0;
    Battle_rewardsFollowAction = 0;
    VS::Battle::OpenLiveRewardResults();
}

// decomp: BATTLE.PRG func_8007B9CC (src/BATTLE/BATTLE.PRG/146C.c:8443)
void Battle_BeginPostActionRewards(void)
{
    Battle_lootMenuOpened = 0;
    Battle_rewardsFollowAction = 1;
    VS::Battle::OpenLivePendingLootRewards();
}

// decomp: BATTLE.PRG func_8007B9FC (src/BATTLE/BATTLE.PRG/146C.c:8450)
int Battle_UpdateRewardSequence(void)
{
    switch (Battle_actionFlowMode) {
    case 3:
        VS::Battle::UpdateLiveRewardResultsDialog();
        return 1;
    case 4:
        VS::Battle::UpdateLivePendingLootDialog();
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
