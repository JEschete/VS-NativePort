#pragma once
// Hand-maintained (tools/port/owned.csv).

namespace VS::Battle {
void BeginLiveRewardPause();
void EndLiveRewardPause();
void ResumeLiveAfterRewards();
void CheckLiveRewardUnlocks();
void ShowLivePendingRewardUnlock();
void OpenLivePendingLootRewards();
void OpenLiveRewardResults();
void UpdateLiveRewardResultsDialog();
void UpdateLiveLootResults();
void UpdateLiveAbilityUnlockDelay();
void UpdateLiveAbilityUnlockResults();
void UpdateLivePendingLootDialog();
}  // namespace VS::Battle
