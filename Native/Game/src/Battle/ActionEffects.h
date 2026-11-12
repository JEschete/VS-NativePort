// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_ACTIONEFFECTS_H_TYPES
#define VS_BATTLE_ACTIONEFFECTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_SoundEventTargets Battle_SoundEventTargets;

#ifndef VS_TYPES_ONLY
#define VS_TYPES_ONLY
#define VS_BATTLE_ACTIONEFFECTS_H_SET_TYPES_ONLY
#endif
#include "Effects/Runtime.h"
#ifdef VS_BATTLE_ACTIONEFFECTS_H_SET_TYPES_ONLY
#undef VS_TYPES_ONLY
#undef VS_BATTLE_ACTIONEFFECTS_H_SET_TYPES_ONLY
#endif

// decomp: soundEventTargets (src/BATTLE/BATTLE.PRG/5BF94.c:637)
typedef struct Battle_SoundEventTargets {
    u_short reserved0;
    u_char reserved2, reserved3;
    char reserved4[12];
    Effects_ActionEffectTarget reserved10[0];
} Battle_SoundEventTargets;

#endif // VS_BATTLE_ACTIONEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_ACTIONEFFECTS_H_DECLS)
#define VS_BATTLE_ACTIONEFFECTS_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"

int Battle_QueueSpellTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueuePhysicalTriggeredRecovery(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueuePhysicalAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueAirAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueFireAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueEarthAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueWaterAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueLightAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueDarkAffinityRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueDamageTriggeredRecovery(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);
int Battle_QueueHitDamageAndEquipmentWear(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action);

#endif // VS_BATTLE_ACTIONEFFECTS_H_DECLS
