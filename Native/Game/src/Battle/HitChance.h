// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_HITCHANCE_H_TYPES
#define VS_BATTLE_HITCHANCE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_HITCHANCE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_HITCHANCE_H_DECLS)
#define VS_BATTLE_HITCHANCE_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"

extern int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
short Battle_AttackGemHitBonus(Battle_ActionDefinition* action, Battle_ActorStats* actor);
short Battle_DefenseGemHitBonus(Battle_ActionDefinition* action, Battle_ActorStats* actor, int hitNumber);
int Battle_IgnoreHitCallback(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
int Battle_PreviousHitSuccessThreshold(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
int Battle_GemHitChance(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int arg4 __attribute__((unused)));
int Battle_AgilityHitChance(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int addVariance);
int Battle_IntelligenceHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int hitNumber, int addVariance);
int Battle_CostAdjustedAgilityHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int hitNumber, int addVariance);
int Battle_RiskHitChance(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* source,
    Battle_HitResult* arg2 __attribute__((unused)), int hitNumber __attribute__((unused)),
    int arg4 __attribute__((unused)));
int Battle_ChainHitChance(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* target,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)));
int Battle_RollActionHit(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int addVariance);

#endif // VS_BATTLE_HITCHANCE_H_DECLS
