// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_STATUSEFFECTS_H_TYPES
#define VS_BATTLE_STATUSEFFECTS_H_TYPES
#include "Legacy/Psx.h"

typedef struct Battle_StatusEffectParams Battle_StatusEffectParams;
typedef struct Battle_StatusEffectTick Battle_StatusEffectTick;
typedef struct Battle_StatusEffectTickTable Battle_StatusEffectTickTable;

// decomp: D_8004EF20 (D_8004EF20@SLUS_010.40): a view into another global
#define Battle_statusParams5Duration (*(u_char*)(&Battle_statusEffectParams[5].duration))

// decomp: D_8004EF80 (D_8004EF80@SLUS_010.40): a view into another global
#define Battle_statusParams11Duration (*(u_char*)(&Battle_statusEffectParams[11].duration))

// decomp: D_8004EFA0 (D_8004EFA0@SLUS_010.40): a view into another global
#define Battle_statusParams13Duration (*(u_char*)(&Battle_statusEffectParams[13].duration))

// decomp: D_8004EFB2 (D_8004EFB2@SLUS_010.40): a view into another global
#define Battle_statusParams14TickInterval (*(u_char*)(&Battle_statusEffectParams[14].tickInterval))

// decomp: D_8004EFE2 (D_8004EFE2@SLUS_010.40): a view into another global
#define Battle_statusParams17TickInterval (*(u_char*)(&Battle_statusEffectParams[17].tickInterval))

// decomp: D_8004F000 (D_8004F000@SLUS_010.40): a view into another global
#define Battle_statusParams19Duration (*(u_char*)(&Battle_statusEffectParams[19].duration))

// decomp: vs_battle_statuses (src/BATTLE/BATTLE.PRG/146C.h:485)
enum Battle_StatusFlag {
    Battle_StatusFlag_StrDown = 1 << 0,
    Battle_StatusFlag_StrUp = 1 << 1,
    Battle_StatusFlag_IntDown = 1 << 2,
    Battle_StatusFlag_IntUp = 1 << 3,
    Battle_StatusFlag_AgiDown = 1 << 4,
    Battle_StatusFlag_AgiUp = 1 << 5,
    Battle_StatusFlag_Quicken = 1 << 6,
    Battle_StatusFlag_Silent = 1 << 7,
    Battle_StatusFlag_Paralysis = 1 << 8,
    Battle_StatusFlag_Poison = 1 << 9,
    Battle_StatusFlag_Numbness = 1 << 10,
    Battle_StatusFlag_Curse = 1 << 11,
    Battle_StatusFlag_Regen = 1 << 12,
    Battle_StatusFlag_Ward = 1 << 13,
    Battle_StatusFlag_EquDown = 1 << 14,
    Battle_StatusFlag_EquUp = 1 << 15,
    Battle_StatusFlag_AirAtkUp = 1 << 16,
    Battle_StatusFlag_FireAtkUp = 1 << 17,
    Battle_StatusFlag_EarthAtkUp = 1 << 18,
    Battle_StatusFlag_WaterAtkUp = 1 << 19,
    Battle_StatusFlag_AirDefUp = 1 << 20,
    Battle_StatusFlag_FireDefUp = 1 << 21,
    Battle_StatusFlag_EarthDefUp = 1 << 22,
    Battle_StatusFlag_WaterDefUp = 1 << 23
};
typedef enum Battle_StatusFlag Battle_StatusFlag;

// decomp: statusEffectTick (src/BATTLE/BATTLE.PRG/74088.c:1162)
typedef struct Battle_StatusEffectTick {
    int reserved0;
    u_short reserved4;
    u_short reserved6;
    int reserved8;
    int reservedC;
} Battle_StatusEffectTick;
_Static_assert(sizeof(Battle_StatusEffectTick) == 0x10, "Battle_StatusEffectTick must keep its PS1 layout");

// decomp: statusEffectTable (src/BATTLE/BATTLE.PRG/74088.c:1165)
typedef struct Battle_StatusEffectTickTable {
    Battle_StatusEffectTick reserved0[1];
} Battle_StatusEffectTickTable;
_Static_assert(sizeof(Battle_StatusEffectTickTable) == 0x10, "Battle_StatusEffectTickTable must keep its PS1 layout");

// decomp: vs_main_statusEffectParams_t (src/SLUS_010.40/main.h:115)
typedef struct Battle_StatusEffectParams {
    int statusFlags;
    u_short duration;
    u_short tickInterval;
    int cancelledStatuses;
    int blockingStatuses;
} Battle_StatusEffectParams;
_Static_assert(sizeof(Battle_StatusEffectParams) == 0x10, "Battle_StatusEffectParams must keep its PS1 layout");

#endif // VS_BATTLE_STATUSEFFECTS_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_STATUSEFFECTS_H_DECLS)
#define VS_BATTLE_STATUSEFFECTS_H_DECLS
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Score/ScoreScreen.h"

extern int Battle_limbStatusFlags[4];
extern int Battle_vitalsTickCounter;
extern int Battle_regeneratingLimb;
extern int Battle_targetingRiskTimer;
extern void* Battle_unusedSceneBuffer;
extern Menu_Score_CursorMemory Battle_unusedMenu9CursorMemory;
extern int Battle_unusedSubmenuState;
extern Menu_Framework_PreviewPoseState* Battle_unusedGlobal1[1];
extern int Battle_unusedGlobal2;
extern Battle_StatusEffectParams Battle_statusEffectParams[32];
void Battle_PreserveAshleyStatuses(void);
int Battle_QueueApplicableStatusChanges(int status_id, Battle_HitResult* entity);
int Battle_QueueStatusRemovalMask(int status_id, Battle_HitResult* entity);
int Battle_QueueStrengthDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueIntelligenceDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueAgilityDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueEquipmentDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueHalfMovement(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueStrengthUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueIntelligenceUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueAgilityUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueEquipmentUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueQuicken(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueSilence(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueMagicWard(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)));
int Battle_QueueRegeneration(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueBanish(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action);
int Battle_QueueParalysis(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueuePoison(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueNumbness(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueCurse(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueAirAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueFireAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueEarthAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueWaterAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueAirDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueFireDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueEarthDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueWaterDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueParalysisCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueuePoisonCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueCurseCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueNumbnessCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueDispel(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
int Battle_QueueNegativeStatusCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)));
void Battle_RemoveActorStatuses(int status_mask, Battle_ActorStats* actor_stats);
void Battle_ApplyWeaponElementEnchantment(Battle_ActorStats* actor_stats, int increased_affinity, int decreased_affinity, int modifier_amount);
void Battle_ApplyArmorElementEnchantment(Battle_ActorStats* actor_stats, int increased_affinity, int decreased_affinity, int modifier_amount);
void Battle_InflictActorStatuses(int status_mask, Battle_ActorStats* actor_stats);
void Battle_QueueActorStatChangeNumber(int actor_slot, int limb_tag, int encoded_amount, int label_flags);
void Battle_ApplyActorActionResult(Battle_HitResult* hit_result);
void Battle_UpdateActorStatusTimers(Battle_ActorStats* actor);
int Battle_UpdateActorVitals(void);
int Battle_UpdateRoomActorVitals(void);
void Battle_UpdateTargetingRiskPenalty(void);
void Battle_ResetTargetingRiskPenaltyTimer(void);

#endif // VS_BATTLE_STATUSEFFECTS_H_DECLS
