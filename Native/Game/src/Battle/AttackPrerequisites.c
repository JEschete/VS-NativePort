// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/Actor.h"
#include "Battle/AttackPrerequisites.h"
#include "Battle/Damage.h"
#include "Battle/Enemy.h"
#include "Battle/StatusEffects.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: _attackPrerequisiteFunctions (_attackPrerequisiteFunctions@BATTLE/BATTLE.PRG)
int (*Battle_attackPrerequisiteFunctions[40])(Battle_ActionDefinition*, u_char*); /* contents: module image, relocated (ModulePointers.c) */

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEffect (src/BATTLE/BATTLE.PRG/146C.c:10133)
int Battle_IsStatusEffectBlocked(int debuff, u_char* target_result_bytes)
{
    Battle_StatusEffectParams* params = &Battle_statusEffectParams[debuff];
    Battle_ActorStats* target_stats = Battle_actors[*target_result_bytes]->stats;

    if ((!(params->statusFlags & target_stats->statusImmunities)) && (!(params->blockingStatuses & target_stats->statuses))) {
        return 0;
    }
    return 1;
}

// decomp: BATTLE.PRG _getEnemyClass (src/BATTLE/BATTLE.PRG/146C.c:10144)
int Battle_TargetEnemyClass(u_char* target_result_bytes) { return Battle_actors[*target_result_bytes]->stats->enemyClass; }

// decomp: BATTLE.PRG _hitPrerequisiteNever0 (src/BATTLE/BATTLE.PRG/146C.c:10146)
int Battle_RejectAttackPrerequisite(
    Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes __attribute__((unused)))
{
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteAlways0 (src/BATTLE/BATTLE.PRG/146C.c:10152)
int Battle_AcceptAttackPrerequisite(
    Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes __attribute__((unused)))
{
    return 1;
}

// decomp: BATTLE.PRG _hitPrerequisiteUndead (src/BATTLE/BATTLE.PRG/146C.c:10170)
int Battle_IsAttackTargetUndead(Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_TargetEnemyClass(target_result_bytes) == Battle_EnemyClass_Undead;
}

// decomp: BATTLE.PRG _hitPrerequisiteNotUndead (src/BATTLE/BATTLE.PRG/146C.c:10175)
int Battle_IsAttackTargetLiving(Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_TargetEnemyClass(target_result_bytes) != Battle_EnemyClass_Undead;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyStrDown (src/BATTLE/BATTLE.PRG/146C.c:10198)
int Battle_CanApplyStrengthDown(
    Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(5, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyIntDown (src/BATTLE/BATTLE.PRG/146C.c:10204)
int Battle_CanApplyIntelligenceDown(
    Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(7, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAglDown (src/BATTLE/BATTLE.PRG/146C.c:10210)
int Battle_CanApplyAgilityDown(
    Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(9, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEquipDown (src/BATTLE/BATTLE.PRG/146C.c:10216)
int Battle_CanApplyEquipmentDown(Battle_ActionDefinition* action_definition, u_char* target_result_bytes)
{
    int i;
    int hasEquipment;
    Battle_ActorStats* actor;

    if (Battle_IsStatusEffectBlocked(19, target_result_bytes) != 0) {
        return 0;
    }

    hasEquipment = 0;
    actor = Battle_actors[*target_result_bytes]->stats;

    for (i = 0; i < 6; ++i) {
        if (actor->limbs[i].armor.armor.id != 0) {
            hasEquipment = 1;
        }
    }

    if (actor->shield.base.id != 0) {
        hasEquipment = 1;
    }

    if (actor->weapon.blade.id != 0) {
        hasEquipment = 1;
    }

    if (hasEquipment != 0) {
        return 1;
    }

    return i = 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyHalfMove (src/BATTLE/BATTLE.PRG/146C.c:10250)
int Battle_CanApplyHalfMovement(
    Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(4, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyStrUp (src/BATTLE/BATTLE.PRG/146C.c:10256)
int Battle_CanApplyStrengthUp(Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(6, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyIntUp (src/BATTLE/BATTLE.PRG/146C.c:10261)
int Battle_CanApplyIntelligenceUp(Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(8, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAglUp (src/BATTLE/BATTLE.PRG/146C.c:10266)
int Battle_CanApplyAgilityUp(Battle_ActionDefinition* action_definition __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(10, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEquipUp (src/BATTLE/BATTLE.PRG/146C.c:10271)
int Battle_CanApplyEquipmentUp(Battle_ActionDefinition* action_definition, u_char* target_result_bytes)
{
    int i;
    int hasEquipment;
    Battle_ActorStats* actor;

    if (Battle_IsStatusEffectBlocked(20, target_result_bytes) != 0) {
        return 0;
    }

    hasEquipment = 0;
    actor = Battle_actors[*target_result_bytes]->stats;

    for (i = 0; i < 6; ++i) {
        if (actor->limbs[i].armor.armor.id != 0) {
            hasEquipment = 1;
        }
    }

    if (actor->shield.base.id != 0) {
        hasEquipment = 1;
    }

    if (actor->weapon.blade.id != 0) {
        hasEquipment = 1;
    }

    if (hasEquipment != 0) {
        return 1;
    }

    return i = 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyQuicken (src/BATTLE/BATTLE.PRG/146C.c:10305)
int Battle_CanApplyQuicken(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(11, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplySilence (src/BATTLE/BATTLE.PRG/146C.c:10311)
int Battle_CanApplySilence(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(12, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyMagicWard (src/BATTLE/BATTLE.PRG/146C.c:10317)
int Battle_CanApplyMagicWard(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(18, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyRegen (src/BATTLE/BATTLE.PRG/146C.c:10323)
int Battle_CanApplyRegeneration(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(17, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyParalysis (src/BATTLE/BATTLE.PRG/146C.c:10329)
int Battle_CanApplyParalysis(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(13, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyPoison (src/BATTLE/BATTLE.PRG/146C.c:10335)
int Battle_CanApplyPoison(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(14, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyNumbness (src/BATTLE/BATTLE.PRG/146C.c:10341)
int Battle_CanApplyNumbness(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(15, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyCurse (src/BATTLE/BATTLE.PRG/146C.c:10347)
int Battle_CanApplyCurse(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(16, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAirAttackUp (src/BATTLE/BATTLE.PRG/146C.c:10353)
int Battle_CanApplyAirAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if ((Battle_IsStatusEffectBlocked(21, target_result_bytes) == 0)
        && (Battle_actors[*target_result_bytes]->stats->weapon.blade.id != 0)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyFireAttackUp (src/BATTLE/BATTLE.PRG/146C.c:10363)
int Battle_CanApplyFireAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if ((Battle_IsStatusEffectBlocked(22, target_result_bytes) == 0)
        && (Battle_actors[*target_result_bytes]->stats->weapon.blade.id != 0)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEarthAttackUp (src/BATTLE/BATTLE.PRG/146C.c:10373)
int Battle_CanApplyEarthAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if ((Battle_IsStatusEffectBlocked(23, target_result_bytes) == 0)
        && (Battle_actors[*target_result_bytes]->stats->weapon.blade.id != 0)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyWaterAttackUp (src/BATTLE/BATTLE.PRG/146C.c:10383)
int Battle_CanApplyWaterAttackUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if ((Battle_IsStatusEffectBlocked(24, target_result_bytes) == 0)
        && (Battle_actors[*target_result_bytes]->stats->weapon.blade.id != 0)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyAirDefenseUp (src/BATTLE/BATTLE.PRG/146C.c:10393)
int Battle_CanApplyAirDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if (Battle_IsStatusEffectBlocked(25, target_result_bytes) == 0) {
        int i;
        int hasArmor = 0;
        Battle_ActorStats* actor = Battle_actors[*target_result_bytes]->stats;

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].armor.armor.id != 0) {
                hasArmor = 1;
            }
        }

        if (actor->shield.base.id != 0) {
            hasArmor = 1;
        }

        if (hasArmor != 0) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyFireDefenseUp (src/BATTLE/BATTLE.PRG/146C.c:10418)
int Battle_CanApplyFireDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if (Battle_IsStatusEffectBlocked(26, target_result_bytes) == 0) {
        int i;
        int hasArmor = 0;
        Battle_ActorStats* actor = Battle_actors[*target_result_bytes]->stats;

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].armor.armor.id != 0) {
                hasArmor = 1;
            }
        }

        if (actor->shield.base.id != 0) {
            hasArmor = 1;
        }
        if (hasArmor != 0) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyEarthDefenseUp (src/BATTLE/BATTLE.PRG/146C.c:10442)
int Battle_CanApplyEarthDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if (Battle_IsStatusEffectBlocked(27, target_result_bytes) == 0) {
        int i;
        int hasArmor = 0;
        Battle_ActorStats* actor = Battle_actors[*target_result_bytes]->stats;

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].armor.armor.id != 0) {
                hasArmor = 1;
            }
        }

        if (actor->shield.base.id != 0) {
            hasArmor = 1;
        }
        if (hasArmor != 0) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyWaterDefenseUp (src/BATTLE/BATTLE.PRG/146C.c:10466)
int Battle_CanApplyWaterDefenseUp(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if (Battle_IsStatusEffectBlocked(28, target_result_bytes) == 0) {
        int i;
        int hasArmor = 0;
        Battle_ActorStats* actor = Battle_actors[*target_result_bytes]->stats;

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].armor.armor.id != 0) {
                hasArmor = 1;
            }
        }

        if (actor->shield.base.id != 0) {
            hasArmor = 1;
        }

        if (hasArmor != 0) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyBanish (src/BATTLE/BATTLE.PRG/146C.c:10491)
int Battle_CanApplyBanish(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    return Battle_IsStatusEffectBlocked(29, target_result_bytes) == 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyExorcise (src/BATTLE/BATTLE.PRG/146C.c:10497)
int Battle_CanApplyExorcise(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if ((Battle_IsStatusEffectBlocked(30, target_result_bytes) == 0)
        && (Battle_TargetEnemyClass(target_result_bytes) == Battle_EnemyClass_Undead)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteCanApplyDrain (src/BATTLE/BATTLE.PRG/146C.c:10507)
int Battle_CanApplyDrain(
    Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes)
{
    if ((Battle_IsStatusEffectBlocked(30, target_result_bytes) == 0)
        && (Battle_TargetEnemyClass(target_result_bytes) != Battle_EnemyClass_Undead)) {
        return 1;
    }
    return 0;
}

// decomp: BATTLE.PRG _hitPrerequisiteUnused0 (src/BATTLE/BATTLE.PRG/146C.c:10517)
int Battle_IgnoreAttackPrerequisite(Battle_ActionDefinition* action __attribute__((unused)), u_char* target_result_bytes) { }

// decomp: BATTLE.PRG _canPerformAttack (src/BATTLE/BATTLE.PRG/146C.c:10523)
int Battle_CheckActionHitPrerequisite(Battle_ActionDefinition* action, u_char* target_result_bytes, int hitNumber)
{
    if (hitNumber != 0) {
        if ((action->hits[hitNumber].prerequisiteFunctionId == 1) && (target_result_bytes[3] == 4)) {
            return 0;
        }
    }
    return Battle_attackPrerequisiteFunctions[action->hits[hitNumber]
            .prerequisiteFunctionId](action, target_result_bytes);
}

