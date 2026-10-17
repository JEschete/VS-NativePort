// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_REWARDS_H_TYPES
#define VS_BATTLE_REWARDS_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_REWARDS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_REWARDS_H_DECLS)
#define VS_BATTLE_REWARDS_H_DECLS

extern int Battle_lootMenuOpened;
extern int Battle_rewardsFollowAction;
void Battle_UpdateLootResults(void);
void Battle_UpdateAbilityUnlockDelay(void);
void Battle_UpdateAbilityUnlockResults(void);
void Battle_BeginRewardPause(void);
void Battle_EndRewardPause(void);
void Battle_ResumeAfterRewards(void);
void Battle_CheckRewardUnlocks(void);
void Battle_ShowPendingRewardUnlock(void);
void Battle_OpenPendingLootRewards(void);
void Battle_OpenRewardResults();
void Battle_UpdateRewardResultsDialog(void);
void Battle_UpdatePendingLootDialog(void);
void Battle_WaitForRewardUnlockEffects(void);
void Battle_UpdateRewardUnlockDialog(void);
void Battle_BeginEventRewardResults(void);
void Battle_BeginPostActionRewards(void);
int Battle_UpdateRewardSequence(void);

#endif // VS_BATTLE_REWARDS_H_DECLS
