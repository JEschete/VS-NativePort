// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONFLOW_H_TYPES
#define VS_BATTLE_ACTIONFLOW_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_ACTIONFLOW_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ACTIONFLOW_H_DECLS)
#define VS_BATTLE_ACTIONFLOW_H_DECLS
#include "Battle/Action.h"
#include "Battle/Hit.h"
#include "Battle/Targeting.h"
#include "Menu/Framework/ActionSelection.h"

extern Menu_Framework_ActionSelection* Battle_actionSelection;
extern u_int Battle_actionFlowMode;
extern int Battle_recoveryRewardMode;
extern Battle_ActionState* Battle_actionState;
extern char Battle_spellCommitFlag;
void Battle_ReleaseQueuedActorAttacks(void);
void Battle_ResolveEnemyDefeat(int arg0, int actorId, int arg2);
int Battle_PlayWeaponHitSounds(int arg0);
void Battle_ApplyActionSourceEffects(int arg0, SVECTOR* arg1);
void Battle_ApplyActionTargetEffects(int, SVECTOR*);
void Battle_WaitForPostActionMovement(void);
void Battle_BeginPostActionRecovery(int arg0);
void Battle_BeginActionTrailFade(void);
void Battle_FinishBattleAbilitySequence(void);
void Battle_StartActionAnimation(void);
void Battle_AdvanceActionEffectPhase(void);
void Battle_CommitSelectedAction(int arg0);
void Battle_StartActionPresentation(void);
void Battle_CommitEnvironmentAction(int arg0 __attribute__((unused)));
void Battle_ResetActionPhase(void);
void Battle_BeginAshleyWeaponToggle(void);
void Battle_EndAshleyWeaponToggle(void);
int Battle_BeginActorDefeatAnimation(int arg0);
void Battle_ApplyLearnedActionUnlocks(void);

#endif // VS_BATTLE_ACTIONFLOW_H_DECLS
