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
_Static_assert(sizeof(Battle_HitTargetWord) == 0x8, "Battle_HitTargetWord must keep its PS1 layout");

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
extern void (*Battle_actionEffectMediators[70])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern u_int Battle_hitPrerequisiteResult;
extern u_int Battle_previousHitResult;
int Battle_AccumulateMpDamage(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_AccumulateWeaponPpDrain(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_AccumulateShieldPpDrain(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_AccumulateHpRestoration(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_AccumulateMpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_AccumulateWeaponPpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_AccumulateWeaponDpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_AccumulateRiskReduction(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_IgnoreActionEffect(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
void Battle_DeriveLimbInjuryStatuses(Battle_HitResult* hit_result);
void Battle_ApplyActionHitEffect(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int execute_action, int hit);
void Battle_ClearActionResult(void* action_result);
void Battle_InitializeHitResult(Battle_HitResult* hit_result);
int Battle_ActionCostAndAvailability(u_int actionId, Battle_ActorStats* actor, int updateStat);
void Battle_PreviewActorActionCost(int action_id, int actor_slot);
void Battle_AddActionRecoveryDelay(u_int action_id, Battle_ActorStats* actor_stats, int execute_action);
void Battle_FinalizeHitOutcome(Battle_HitStatusView* hit_status);
void Battle_ResolveActionTargets(int action, Battle_ActionRequest* result, Battle_ActionRequest* source, int execute_action);
int Battle_PreviewActionAgainstLimb(int action_id, int source_actor_slot, int targetActor, int targetLimb, void* result_out);
int Battle_PreviewActionAgainstActor(int action_id, int source_actor_slot, int targetActor, int target_limb, SVECTOR* source_position,
    SVECTOR* target_position, Battle_ActionRequest* result_out);
int Battle_ActorActionRestrictionFlags(int actorId, int actionId);

#endif // VS_BATTLE_ACTIONRESOLUTION_H_DECLS
