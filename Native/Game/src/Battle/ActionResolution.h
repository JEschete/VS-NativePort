// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONRESOLUTION_H_TYPES
#define VS_BATTLE_ACTIONRESOLUTION_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_HitTargetWord Battle_HitTargetWord;

// decomp: func_8008631C_t2 (src/BATTLE/BATTLE.PRG/146C.c:37)
typedef struct Battle_HitTargetWord {
    u_char targetActor;
    signed char targetLimb;
    u_char hitThreshold;
    u_char impactKind;
    short hp;
    short mp;
} Battle_HitTargetWord;

#endif // VS_BATTLE_ACTIONRESOLUTION_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ACTIONRESOLUTION_H_DECLS)
#define VS_BATTLE_ACTIONRESOLUTION_H_DECLS
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

extern u_char Battle_limbInjuryStatusIds[20];
extern void (*Battle_actionEffectMediators[100])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern u_int Battle_hitPrerequisiteResult;
extern u_int Battle_previousHitResult;
int Battle_AccumulateMpDamage(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
int Battle_AccumulateWeaponPpDrain(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)));
int Battle_AccumulateShieldPpDrain(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)));
int Battle_AccumulateHpRestoration(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
int Battle_AccumulateMpRestoration(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
int Battle_AccumulateWeaponPpRestoration(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
int Battle_AccumulateWeaponDpRestoration(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
int Battle_AccumulateRiskReduction(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
int Battle_IgnoreActionEffect(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
void Battle_DeriveLimbInjuryStatuses(Battle_HitResult* arg0);
void Battle_ApplyActionHitEffect(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int hit);
void Battle_ClearActionResult(void* arg0);
void Battle_InitializeHitResult(Battle_HitResult* arg0);
int Battle_ActionCostAndAvailability(u_int actionId, Battle_ActorStats* actor, int updateStat);
void Battle_PreviewActorActionCost(int arg0, int arg1);
void Battle_AddActionRecoveryDelay(u_int arg0, Battle_ActorStats* arg1, int arg2);
void Battle_FinalizeHitOutcome(Battle_HitStatusView* arg0);
void Battle_ResolveActionTargets(int action, Battle_ActionRequest* result, Battle_ActionRequest* source, int arg3);
int Battle_PreviewActionAgainstLimb(int arg0, int arg1, int targetActor, int targetLimb, void* arg4);
int Battle_PreviewActionAgainstActor(int arg0, int arg1, int targetActor, int arg3, SVECTOR* arg4,
    SVECTOR* arg5, Battle_ActionRequest* arg6);
int Battle_ActorActionRestrictionFlags(int actorId, int actionId);

#endif // VS_BATTLE_ACTIONRESOLUTION_H_DECLS
