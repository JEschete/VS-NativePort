// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/AttackPrerequisites.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/HitChance.h"
#include "Battle/Sphere.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/WordCopy.h"
#include "Field/SegmentCollision.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Flags.h"
#include "State/Progress.h"

// decomp: D_800E8200 (D_800E8200@BATTLE/BATTLE.PRG)
u_char Battle_limbInjuryStatusIds[20]; /* contents: module image (ModuleData.c) */

// decomp: _actionEffectMediators (_actionEffectMediators@BATTLE/BATTLE.PRG)
void (*Battle_actionEffectMediators[70])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.c) */

// decomp: D_800F1A00 (D_800F1A00@BATTLE/BATTLE.PRG)
u_int Battle_hitPrerequisiteResult;

// decomp: D_800F1A04 (D_800F1A04@BATTLE/BATTLE.PRG)
u_int Battle_previousHitResult;

// decomp: BATTLE.PRG func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12820)
int Battle_AccumulateMpDamage(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    Battle_ActorStats* target_stats = Battle_actors[target_hit->target.targetActor]->stats;
    short amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);

    if (target_stats->currentMp < amount) {
        amount = target_stats->currentMp;
        Battle_lastActionStatValue = target_stats->currentMp;
    }

    target_hit->target.mp += amount;
    target_hit->mpEffect = 1;
}

// decomp: BATTLE.PRG func_80084D44 (src/BATTLE/BATTLE.PRG/146C.c:12835)
int Battle_AccumulateWeaponPpDrain(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_actors[target_hit->target.targetActor]->stats->weapon.blade.id != 0) {
        u_short amount =
            Battle_actors[target_hit->target.targetActor]->stats->weapon.currentPp;
        target_hit->weaponPpChange += amount;
        target_hit->weaponPpEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80084DA8 (src/BATTLE/BATTLE.PRG/146C.c:12847)
int Battle_AccumulateShieldPpDrain(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_ActorStats* actor = Battle_actors[target_hit->target.targetActor]->stats;

    if ((Battle_actionState->selection.primaryTarget.hpEffect == 1) || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
        if (actor->shield.base.id != 0) {
            short amount = actor->shield.currentPp;
            target_hit->shieldPpChange += amount;
            target_hit->shieldPpEffect = 1;
        }
    }
}

// decomp: BATTLE.PRG func_80084E28 (src/BATTLE/BATTLE.PRG/146C.c:12862)
int Battle_AccumulateHpRestoration(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short amount = Battle_CalculateActionStatChange(action, source_hit, target_hit, hit_index, execute_action);
    target_hit->target.hp += amount;
    target_hit->hpEffect = 2;

    if (target_hit->targetsPosition == 0) {
        Battle_DistributeLimbRecovery(action, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12874)
int Battle_AccumulateMpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->target.mp = target_hit->target.mp + restoration_amount;
    target_hit->mpEffect = 2;
}

// decomp: BATTLE.PRG func_80084EEC (src/BATTLE/BATTLE.PRG/146C.c:12882)
int Battle_AccumulateWeaponPpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->weaponPpChange += restoration_amount;
    target_hit->weaponPpEffect = 2;
}

// decomp: BATTLE.PRG func_80084F40 (src/BATTLE/BATTLE.PRG/146C.c:12890)
int Battle_AccumulateWeaponDpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->limbs[6].effects.opaque0 += restoration_amount;
    target_hit->weaponDpEffect = 2;
}

// decomp: BATTLE.PRG func_80084F94 (src/BATTLE/BATTLE.PRG/146C.c:12898)
int Battle_AccumulateRiskReduction(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = Battle_CalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    int shifted_restoration_amount = restoration_amount << 0x10;
    if (shifted_restoration_amount != 0) {
        target_hit->riskChange += restoration_amount;
        target_hit->riskEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80084FF0 (src/BATTLE/BATTLE.PRG/146C.c:12909)
int Battle_IgnoreActionEffect(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
}

// decomp: BATTLE.PRG func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12930)
void Battle_DeriveLimbInjuryStatuses(Battle_HitResult* hit_result)
{
    int i;
    int limb_health_test;
    short projected_secondary_mobility_hp;
    u_short projected_limb_hp;
    Battle_ActorStats* target_stats;

    target_stats = Battle_actors[hit_result->target.targetActor]->stats;

    switch (hit_result->hpEffect) {
    case 2:
        projected_limb_hp = target_stats->limbs[target_stats->mobilityLimb0].hp + hit_result->limbs[target_stats->mobilityLimb0].packed;
        projected_secondary_mobility_hp = target_stats->limbs[target_stats->mobilityLimb1].hp + hit_result->limbs[target_stats->mobilityLimb1].packed;
        break;
    case 3:
    case 1:
        projected_limb_hp = target_stats->limbs[target_stats->mobilityLimb0].hp - hit_result->limbs[target_stats->mobilityLimb0].packed;
        projected_secondary_mobility_hp = target_stats->limbs[target_stats->mobilityLimb1].hp - hit_result->limbs[target_stats->mobilityLimb1].packed;
        break;
    default:
        projected_limb_hp = target_stats->limbs[target_stats->mobilityLimb0].hp;
        projected_secondary_mobility_hp = target_stats->limbs[target_stats->mobilityLimb1].hp;
        break;
    }
    if (!(target_stats->statuses & 0x10)) {
        if ((target_stats->mobilityLimb0 | target_stats->mobilityLimb1) != 0) {
            limb_health_test = 0;
            if (target_stats->mobilityLimb0 != 0) {
                limb_health_test = (projected_limb_hp << 0x10) > 0x10000;
            }
            if ((target_stats->mobilityLimb1 != 0) && (projected_secondary_mobility_hp >= 2)) {
                limb_health_test = 1;
            }
            if (limb_health_test == 0) {
                Battle_QueueApplicableStatusChanges(4, hit_result);
            }
        }
    } else {
        if ((target_stats->mobilityLimb0 | target_stats->mobilityLimb1) != 0) {
            limb_health_test = 0;
            if (target_stats->mobilityLimb0 != 0) {
                limb_health_test = (projected_limb_hp << 0x10) > 0x10000;
            }
            if ((target_stats->mobilityLimb1 != 0) && (projected_secondary_mobility_hp >= 2)) {
                limb_health_test = 1;
            }
            if (limb_health_test != 0) {
                Battle_QueueStatusRemovalMask(4, hit_result);
            }
        }
    }
    if (hit_result->target.targetActor == 0) {
        for (i = 0; i < 4; ++i) {
            switch (hit_result->hpEffect) {
            case 2:
                projected_limb_hp = target_stats->limbs[i].hp + hit_result->limbs[i].packed;
                break;
            case 3:
            case 1:
                projected_limb_hp = target_stats->limbs[i].hp - hit_result->limbs[i].packed;
                break;
            default:
                projected_limb_hp = target_stats->limbs[i].hp;
                break;
            }
            if (!(target_stats->statuses & Battle_statusEffectParams[Battle_limbInjuryStatusIds[i]].statusFlags)) {
                limb_health_test = projected_limb_hp << 0x10;
                limb_health_test = 0x10000 < limb_health_test;
                if (!limb_health_test) {
                    Battle_QueueApplicableStatusChanges(Battle_limbInjuryStatusIds[i], hit_result);
                }
            } else {
                limb_health_test = (projected_limb_hp << 0x10) > 0x10000;
                if (limb_health_test) {
                    Battle_QueueStatusRemovalMask(Battle_limbInjuryStatusIds[i], hit_result);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_80085390 (src/BATTLE/BATTLE.PRG/146C.c:13012)
void Battle_ApplyActionHitEffect(
    Battle_ActionDefinition* action, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int execute_action, int hit)
{
    if ((target_hit->targetsPosition == 0)
        && (Battle_actors[target_hit->target.targetActor]->stats->actorId == 0x80)) {
        target_hit->target.hp += 999;
        target_hit->hpEffect = 1;
    } else if (action->type == Battle_ActionType_Spell) {
        if ((target_hit->targetsPosition == 0)
            && (Battle_actors[target_hit->target.targetActor]->stats->statuses & 0x80040000)) {
            Battle_lastActionStatValue = 0;
        } else {
            Battle_actionEffectMediators[action->hits[hit].effectId](
                action, source_hit, target_hit, hit, execute_action);
        }
        if (target_hit->targetsPosition == 0) {
            if (Battle_actors[target_hit->target.targetActor]->stats->actorId != 0x80) {
                target_hit->inflictStatuses |=
                    Battle_actors[target_hit->target.targetActor]->stats->statuses & 0x61000;
            }
        }
    } else {
        Battle_actionEffectMediators[action->hits[hit].effectId](
            action, source_hit, target_hit, hit, execute_action);
    }
    if ((execute_action != 0) && (source_hit->targetsPosition == 0) && (source_hit->target.targetActor == 0)
        && (target_hit->targetsPosition == 0)
        && (Battle_actors[target_hit->target.targetActor]->stats->actorId != 0x80)
        && (action->type == Battle_ActionType_Ability || action->type == Battle_ActionType_BreakArt
            || action->type == Battle_ActionType_BaseAttack)
        && (Battle_actors[target_hit->target.targetActor]->stats->currentHp <= target_hit->target.hp)
        && (target_hit->target.targetActor != 0)
        && (Battle_actors[target_hit->target.targetActor]->killCounted == 0)) {
        int weapon_category;
        Battle_actors[target_hit->target.targetActor]->killCounted = 1;
        weapon_category = Battle_characterState->equippedWeaponCategory % 10;
        if ((State_ScriptFlag(0x82) >= 0xA)
            && (Audio_Seq_unusedArtsStatus.artsLearned[weapon_category] < 4)
            && (Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[weapon_category]
                < Battle_breakArtPointRequirements[weapon_category][Audio_Seq_unusedArtsStatus
                        .artsLearned[weapon_category]])) {
            ++Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[weapon_category];
        }
    }
}

// decomp: BATTLE.PRG func_800856F8 (src/BATTLE/BATTLE.PRG/146C.c:13058)
void Battle_ClearActionResult(void* action_result) { Core_Memory_ZeroAlignedWords(action_result, sizeof(Battle_ActionRequest)); }

// decomp: BATTLE.PRG func_80085718 (src/BATTLE/BATTLE.PRG/146C.c:13060)
void Battle_InitializeHitResult(Battle_HitResult* hit_result)
{
    Core_Memory_ZeroAlignedWords(hit_result, sizeof *hit_result);
    hit_result->target.impactKind = 6;
    *(&hit_result->inflictStatuses + 1) = 0;
}

// decomp: BATTLE.PRG _getActionCost (src/BATTLE/BATTLE.PRG/146C.c:13075)
int Battle_ActionCostAndAvailability(u_int actionId, Battle_ActorStats* actor, int updateStat)
{
    int requirementsMet;
    int actionType;
    int cost;
    Battle_ActionDefinition* action;
    int statValue;

    action = &Battle_actionDefinitions[actionId];
    cost = action->cost;
    actionType = action->type;

    switch (actionType) {
    case Battle_ActionType_Spell:
        statValue = actor->currentMp;
        break;
    case Battle_ActionType_Ability:
        statValue = 100 - actor->risk;
        break;
    case Battle_ActionType_BreakArt:
        statValue = actor->currentHp;
        ++cost;
        break;
    case 4:
        statValue = actor->weapon.currentPp;
        break;
    case Battle_ActionType_BaseAttack:
        cost = actor->weapon.risk;
        switch (actor->weapon.actionType) {
        case Battle_ActionType_Spell:
            statValue = actor->currentMp;
            break;
        case Battle_ActionType_Ability:
            statValue = 100 - actor->risk;
            break;
        case Battle_ActionType_BreakArt:
            statValue = actor->currentHp;
            ++cost;
            break;
        case 4:
            statValue = actor->weapon.currentPp;
            break;
        }
        actionType = actor->weapon.actionType;
        break;
    default:
        statValue = 0;
        break;
    }

    if (actionType == Battle_ActionType_Ability) {
        if (Battle_actionState->chainStep >= 10) {
            if ((actionId - 22) < 18) {
                cost += (Battle_actionState->chainStep * Battle_actionState->chainStep) / 10;
            }
        }
    }
    if (statValue >= cost) {
        if (actionType == Battle_ActionType_BreakArt) {
            --cost;
        }
        statValue -= cost;
        requirementsMet = 1;
    } else {
        if (actionType == Battle_ActionType_Ability) {
            statValue = 0;
            requirementsMet = 1;
        } else {
            requirementsMet = 0;
        }
    }

    if (updateStat != 0) {
        switch (actionType) {
        case Battle_ActionType_Spell:
            actor->currentMp = statValue;
            break;
        case Battle_ActionType_Ability:
            actor->risk = 100 - statValue;
            break;
        case Battle_ActionType_BreakArt:
            actor->currentHp = statValue;
            break;
        case 4:
            actor->weapon.currentPp = statValue;
            break;
        }
    }

    return (requirementsMet << 24) + (actionType << 16) + cost;
}

// decomp: BATTLE.PRG func_80085978 (src/BATTLE/BATTLE.PRG/146C.c:13167)
void Battle_PreviewActorActionCost(int action_id, int actor_slot)
{
    Battle_ActionCostAndAvailability(action_id, Battle_actors[actor_slot]->stats, 0);
}

// decomp: BATTLE.PRG func_800859B4 (src/BATTLE/BATTLE.PRG/146C.c:13172)
void Battle_AddActionRecoveryDelay(u_int action_id, Battle_ActorStats* actor_stats, int execute_action)
{
    Battle_ActionDefinition* action_definition = &Battle_actionDefinitions[action_id];

    if ((execute_action != 0) && ((action_id - 0x16) >= 0x20)) {
        if (action_definition->recoveryDelay == 0xFF) {
            actor_stats->actionRecoveryTimer += (actor_stats->weapon.recoveryDelay + actor_stats->recoveryDelayBonus) << 8;
        } else {
            int recovery_delay = action_definition->recoveryDelay + actor_stats->recoveryDelayBonus;
            actor_stats->actionRecoveryTimer += recovery_delay << 8;
        }
    }
}

// decomp: BATTLE.PRG func_80085A34 (src/BATTLE/BATTLE.PRG/146C.c:13186)
void Battle_FinalizeHitOutcome(Battle_HitStatusView* hit_status)
{
    int i;
    u_int remaining_effect_flags;

    if (hit_status->opaque40 == 0) {
        if (hit_status->opaque2 == 0xFF) {
            hit_status->opaque2 = 0;
        }
        if (hit_status->opaque3 == 6) {
            remaining_effect_flags = hit_status->opaque1C;
            if (remaining_effect_flags != 0) {
                for (i = 0; i < 9; remaining_effect_flags /= 4, ++i) {
                    switch (remaining_effect_flags & 3) {
                    case 2:
                        hit_status->opaque3 = 5;
                        return;
                    case 1:
                        hit_status->opaque3 = 0;
                        return;
                    case 3:
                        hit_status->opaque3 = 1;
                        return;
                    }
                }
            } else if ((hit_status->opaque14 | hit_status->opaque18) == 0) {
                hit_status->opaque3 = 4;
                hit_status->opaque2 = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80085B10 (src/BATTLE/BATTLE.PRG/146C.c:13221)
void Battle_ResolveActionTargets(int action, Battle_ActionRequest* result, Battle_ActionRequest* source, int execute_action)
{
    SVECTOR origin;
    SVECTOR position;
    SVECTOR impact;
    Battle_ActionDefinition* action_definition;
    Battle_ActorStats* attacker;
    Battle_ActorStats* stats;
    Battle_HitResult* user;
    Battle_HitResult* entry;
    int i;
    int hit;
    int redirected;
    int best;
    short max;
    int target;
    int limb;
    __typeof__(action_definition->hits[0]) params;

    action_definition = &Battle_actionDefinitions[action];
    Battle_ClearActionResult(result);
    result->actionId = action;
    Battle_InitializeHitResult(&result->primaryTarget);
    result->primaryTarget.targetsPosition = source->primaryTarget.targetsPosition;
    if (result->primaryTarget.targetsPosition == 0) {
        result->primaryTarget.target.targetActor = source->primaryTarget.target.targetActor;
        result->primaryTarget.target.targetLimb = source->primaryTarget.target.targetLimb;
        attacker = Battle_actors[result->primaryTarget.target.targetActor]->stats;
        Actors_ActorBonePosition(result->primaryTarget.target.targetActor, 0, &origin, 0);
    } else {
        attacker = NULL;
    }

    result->targetCount = source->targetCount;
    for (i = 0; i < source->targetCount; ++i) {
        Battle_InitializeHitResult(&result->targets[i]);
        result->targets[i].targetsPosition = source->targets[i].targetsPosition;
        if (result->targets[i].targetsPosition == 0) {
            result->targets[i].target.targetActor = source->targets[i].target.targetActor;
            result->targets[i].target.targetLimb = source->targets[i].target.targetLimb;
        } else if ((u_int)result->targets[i].targetsPosition < 6) {
            result->targets[i].target = source->targets[i].target;
        }
    }
    result->focusPosition = source->focusPosition;

    best = 0;
    max = 0;
    if (action_definition->hits[0].affinity != 0) {
        best = action_definition->hits[0].affinity - 1;
    } else if (attacker != NULL) {
        if ((Battle_actors[attacker->actorId]->weaponDrawn & 1)
            && attacker->weapon.blade.id) {
            for (i = 0; i < 7; ++i) {
                if (max < attacker->weapon.classAffinityCurrent.affinities[0][i]) {
                    max = attacker->weapon.classAffinityCurrent.affinities[0][i];
                    best = i;
                }
            }
        }
    } else {
        best = 0;
    }
    result->effectVariant = best;

    params = action_definition->hits[0];
    if ((params.statCalculatorId == 2) || (params.statCalculatorId == 25)) {
        if (params.damageType == 0) {
            if (attacker->weapon.blade.id) {
                result->damageType = attacker->weapon.damageType;
            } else {
                result->damageType = 1;
            }
        } else {
            result->damageType = params.damageType;
        }
    } else {
        result->damageType = params.damageType;
    }
    hit = 0;
    user = &result->primaryTarget;
    Battle_lastActionStatValue = 0;
    Battle_previousHitResult = 0;
    Battle_hitPrerequisiteResult = 0;
    for (; hit < 2; ++hit) {
        switch (action_definition->hits[hit].prerequisiteFunctionId) {
        case 6:
        case 7:
        case 8:
            Battle_ApplyActionHitEffect(action_definition, &result->primaryTarget, result->targets, execute_action, hit);
            break;

        case 3:
            if (result->primaryTarget.targetsPosition != 0) {
                break;
            }
            entry = &result->primaryTarget;
            stats = Battle_actors[result->primaryTarget.target.targetActor]->stats;
            Battle_hitPrerequisiteResult = 1;
            Battle_previousHitResult = Battle_RollActionHit(action_definition, entry, entry, hit, execute_action);
            if ((Battle_previousHitResult != 0) || (execute_action == 0)) {
                Battle_ApplyActionHitEffect(action_definition, entry, entry, execute_action, hit);
                if (execute_action && stats->cancelsHitResults) {
                    Battle_InitializeHitResult(entry);
                    entry->target.impactKind = 4;
                }
                Battle_DeriveLimbInjuryStatuses(entry);
            }
            break;

        case 0:
            break;

        default:
            for (i = 0; i < result->targetCount; ++i) {
                entry = &result->targets[i];
                redirected = 0;
                if (execute_action && (action_definition->resetsHitResults)) {
                    if (entry->targetsPosition == 0) {
                        Battle_ActionTargetLimbPosition(
                            entry->target.targetActor, entry->target.targetLimb, &position);
                    } else {
                        position = *(SVECTOR*)&entry->target;
                    }
                    redirected = Field_TraceActorAndTerrainCollision(
                        &origin, &position, &impact, result->primaryTarget.target.targetActor) != NULL;
                    if (redirected) {
                        if (impact.pad == 0) {
                            entry->targetsPosition = 1;
                            *(SVECTOR*)&entry->target = impact;
                            entry->target.mp = 0x80;
                            result->animationParameter = 1;
                        } else {
                            if ((u_short)impact.pad < 17) {
                                target = impact.pad;
                                if ((target - 1) != entry->target.targetActor) {
                                    entry->target.targetActor = impact.pad - 1;
                                    entry->target.targetLimb =
                                        Battle_actors[entry->target.targetActor]
                                            ->stats->defaultTargetLimb;
                                    result->animationParameter = 1;
                                }
                            }
                            redirected = 0;
                        }
                    }
                }
                if (redirected) {
                    continue;
                }
                if (entry->targetsPosition != 0) {
                    continue;
                }
                stats = Battle_actors[entry->target.targetActor]->stats;
                Battle_hitPrerequisiteResult = Battle_CheckActionHitPrerequisite(action_definition, (char*)entry, hit);
                if (Battle_hitPrerequisiteResult != 0) {
                    Battle_previousHitResult = Battle_RollActionHit(action_definition, user, entry, hit, execute_action);
                    if ((Battle_previousHitResult != 0) || (execute_action == 0)) {

                        Battle_ApplyActionHitEffect(action_definition, user, entry, execute_action, hit);
                        target = entry->target.targetActor;
                        if ((Battle_actors[target]->roomSpawnIndex == 0x80) && (target != 0)
                            && stats->cancelsHitResults) {
                            limb = (u_char)entry->target.targetLimb;
                            Battle_InitializeHitResult(entry);
                            entry->target.targetActor = target;
                            entry->target.impactKind = 4;
                            entry->target.targetLimb = limb;
                        }
                        if (execute_action && stats->cancelsHitResults) {
                            limb = (u_char)entry->target.targetLimb;
                            target = entry->target.targetActor;
                            Battle_InitializeHitResult(entry);
                            entry->target.impactKind = 4;
                            entry->target.targetActor = target;
                            entry->target.targetLimb = limb;
                        }
                        Battle_DeriveLimbInjuryStatuses(entry);
                    }
                } else if (hit == 0) {
                    entry->target.hitThreshold = 0xFF;
                }
            }
            break;
        }
    }

    Battle_FinalizeHitOutcome((Battle_HitStatusView*)&result->primaryTarget);
    for (i = 0; i < result->targetCount; ++i) {
        Battle_FinalizeHitOutcome((Battle_HitStatusView*)&result->targets[i]);
    }
    if ((Battle_actionState->queueIndex == 0) && execute_action) {
        *source = *result;
    }
    if (execute_action && (attacker != NULL)) {
        Battle_AddActionRecoveryDelay(action, attacker, 1);
        Battle_ActionCostAndAvailability(action, attacker, 1);
    }
}

// decomp: BATTLE.PRG func_8008631C (src/BATTLE/BATTLE.PRG/146C.c:13421)
int Battle_PreviewActionAgainstLimb(int action_id, int source_actor_slot, int targetActor, int targetLimb, void* result_out)
{
    Battle_ActionRequest preview_request;

    preview_request.targets[0].target.targetActor = targetActor;
    preview_request.actionId = action_id;
    preview_request.primaryTarget.targetsPosition = 0;
    preview_request.primaryTarget.target.targetActor = source_actor_slot;
    preview_request.targetCount = 1;
    preview_request.targets[0].targetsPosition = 0;
    preview_request.targets[0].target.targetLimb = targetLimb;
    Battle_ResolveActionTargets(action_id, result_out, &preview_request, 0);
    return Battle_ActionCostAndAvailability(action_id, Battle_actors[source_actor_slot]->stats, 0) & 0xFF000000;
}

// decomp: BATTLE.PRG func_800863A4 (src/BATTLE/BATTLE.PRG/146C.c:13436)
int Battle_PreviewActionAgainstActor(int action_id, int source_actor_slot, int targetActor, int target_limb, SVECTOR* source_position,
    SVECTOR* target_position, Battle_ActionRequest* result_out)
{
    Battle_ActionRequest preview_request;
    Battle_TargetVolume target_volume;
    SVECTOR limb_position;
    int target_count;
    int delta_z;
    int delta_y;
    int delta_x;
    int action_category;
    int i;

    preview_request.actionId = action_id;
    preview_request.primaryTarget.targetsPosition = 0;
    preview_request.primaryTarget.target.targetActor = source_actor_slot;
    preview_request.targetCount = 0;
    action_category = (Battle_actionDefinitions[action_id].category);

    switch (action_category) {
    case 11:
    case 12:
        target_volume.kind = Battle_actionDefinitions[action_id].shape;
        target_volume.opaque1 = 1;
        target_volume.scale.extents[0] = Battle_actionDefinitions[action_id].rangeX;
        target_volume.scale.extents[1] = Battle_actionDefinitions[action_id].rangeY;
        target_volume.scale.extents[2] = Battle_actionDefinitions[action_id].rangeZ;
        delta_x = target_position->vx - source_position->vx;
        delta_z = target_position->vz - source_position->vz;
        target_volume.angle = Core_Gte_Atan2(delta_x, delta_z);
        delta_y = target_position->vy - source_position->vy;
        target_volume.scale.extents[3] =
            (Core_Gte_Atan2(delta_y, Core_Math_SquareRootInteger((delta_x * delta_x) + (delta_z * delta_z)))
                / 16)
            + 0x40;
        *(SVECTOR*)target_volume.position = *source_position;
        break;

    case 2:
    case 4:
    case 6:
    case 8:
        target_volume.kind = Battle_actionDefinitions[action_id].areaShape & 7;
        target_volume.opaque1 = 1;
        target_volume.scale.extents[0] = Battle_actionDefinitions[action_id].areaRangeX;
        target_volume.scale.extents[1] = Battle_actionDefinitions[action_id].areaRangeY;
        target_volume.scale.extents[2] = Battle_actionDefinitions[action_id].areaRangeZ;
        target_volume.angle = 0;
        target_volume.scale.extents[3] = Battle_actionDefinitions[action_id].areaShape & 0xF8;
        *(SVECTOR*)target_volume.position = *target_position;
        break;
    }

    switch (Battle_actionDefinitions[action_id].category) {
    case 6:
    case 8:
    case 11:
    case 12:
        target_count = 0;

        for (i = 0; i < 6; ++i) {
            if ((Actors_ActorLowGroupBonePosition(targetActor, i, &limb_position, 2) >= 0)
                && (Battle_IsPointInTargetVolume(&target_volume, (u_short*)&limb_position) != 0)) {
                preview_request.targets[target_count].targetsPosition = 0;
                preview_request.targets[target_count].target.targetActor = targetActor;
                preview_request.targets[target_count].target.targetLimb = i;
                ++target_count;
            }
        }

        preview_request.targetCount = target_count;
        break;

    case 5:
    case 7:
        preview_request.targets[0].targetsPosition = 0;
        preview_request.targets[0].target.targetActor = targetActor;
        preview_request.targets[0].target.targetLimb = target_limb;
        preview_request.targetCount = 1;
        break;

    case 1:
    case 2:
    case 3:
    case 4:
        preview_request.targets[0].targetsPosition = 0;
        preview_request.targets[0].target.targetActor = targetActor;
        preview_request.targets[0].target.targetLimb = Battle_actors[targetActor]->stats->defaultTargetLimb;
        preview_request.targetCount = 1;
        break;
    }

    Battle_ResolveActionTargets(action_id, result_out, &preview_request, 0);
    return Battle_ActionCostAndAvailability(action_id, Battle_actors[source_actor_slot]->stats, 0) & 0xFF000000;
}

// decomp: BATTLE.PRG vs_battle_getActionFlags (src/BATTLE/BATTLE.PRG/146C.c:15566)
int Battle_ActorActionRestrictionFlags(int actorId, int actionId)
{
    Battle_ActionDefinition* action = &Battle_actionDefinitions[actionId];
    Battle_ActorStats* actor = Battle_actors[actorId]->stats;
    int ret = actor->actionRecoveryTimer != 0;

    if (!(Battle_ActionCostAndAvailability(actionId, actor, 0) & 0xFF000000)) {
        ret |= 2;
    }

    if ((actorId == 0) && (!(action->unlocked << 15))) {
        ret |= 4;
    }

    if (actionId < 141) {
        if (actor->statuses & 0x1000) {
            ret |= 4;
        }
    } else if (actionId < 224) {
        if (Battle_actors[actorId]->weaponDrawn == 0) {
            ret |= 4;
        }

        if (actor->statuses & 0x2000) {
            ret |= 4;
        }
    }
    return ret;
}

