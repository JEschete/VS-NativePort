// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#ifndef VS_BATTLE_DAMAGE_H_TYPES
#define VS_BATTLE_DAMAGE_H_TYPES
#include "Legacy/Psx.h"


#endif // VS_BATTLE_DAMAGE_H_TYPES

#if !defined(VS_TYPES_ONLY) && !defined(VS_BATTLE_DAMAGE_H_DECLS)
#define VS_BATTLE_DAMAGE_H_DECLS
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Hit.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

extern u_short Battle_bladePpDamageBonuses[12];
extern short (*Battle_statCalculators[27])(Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int);
extern Battle_Actor* Battle_actors[42];
extern u_int Battle_lastActionStatValue;
extern int Battle_criticalHitOccurred;
extern short Battle_limbArmorDpLoss[28];
int Battle_ResolveActorDefeat(int arg0, int arg1, int arg2);
short Battle_ApplyDamageGemMultiplier(
    Battle_ActionDefinition* action, short arg1, Battle_ActorStats* arg2, Battle_ActorStats* arg3);
void Battle_DistributeHitLimbDamage(Battle_ActionDefinition* action, Battle_HitResult* entity, short baseValue);
void Battle_DistributeLimbRecovery(Battle_ActionDefinition* action, Battle_HitResult* entity, short amount);
void Battle_IgnoreStatCalculator(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
short Battle_LastActionStatValue(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
short Battle_CalculatePhysicalHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int withRandMod, int applyStatuses);
short Battle_PhysicalDamageWithoutDebuffs(Battle_ActionDefinition* action,
    Battle_HitResult* source, Battle_HitResult* target, int nHit, int withRandMod);
short Battle_CalculateSpellHitDamage(Battle_ActionDefinition* action, Battle_HitResult* source,
    Battle_HitResult* target, int nHit, int addRandMod);
short Battle_ScalePreviousTargetHit(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
short Battle_ScaleSourceMissingHp(Battle_ActionDefinition* arg0, Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_ScaleTargetCurrentHp(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_WeaponPpWithChainOffset(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
short Battle_ScaleIncomingSpellDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_ScaleIncomingPhysicalDamage(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_ScaleSourceMissingMp(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
short Battle_CalculateSpellHealing(Battle_ActionDefinition* action, Battle_HitResult* source, Battle_HitResult* target, int nHit,
    int withRandMod);
short Battle_TargetMissingHp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)));
short Battle_TargetMissingMp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)));
short Battle_ScaleIncomingSpellEffect(Battle_ActionDefinition* action,
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_ScaleIncomingSpellCost(Battle_ActionDefinition* action, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_ScaleIncomingDamage(Battle_ActionDefinition* arg0, Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3,
    int arg4 __attribute__((unused)));
short Battle_SourceShieldPp(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
short Battle_ScaleLastActionStatValue(Battle_ActionDefinition* arg0 __attribute__((unused)), Battle_HitResult* arg1,
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)));
short Battle_PhysicalDamageWithDebuffs(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4);
short Battle_TargetRisk(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)));
short Battle_CalculateActionStatChange(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int hit, int arg4);

#endif // VS_BATTLE_DAMAGE_H_DECLS
