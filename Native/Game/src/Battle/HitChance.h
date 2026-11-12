// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_HITCHANCE_H_TYPES
#define VS_BATTLE_HITCHANCE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_HITCHANCE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_HITCHANCE_H_DECLS)
#define VS_BATTLE_HITCHANCE_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

extern int (*Battle_hitFunctions[8])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
int Battle_IgnoreHitCallback(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
int Battle_AgilityHitChance(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int addVariance);
int Battle_IntelligenceHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int hitNumber, int addVariance);
int Battle_CostAdjustedAgilityHitChance(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int hitNumber, int addVariance);
int Battle_RollActionHit(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target,
    int hitNumber, int addVariance);

#endif // VS_BATTLE_HITCHANCE_H_DECLS
