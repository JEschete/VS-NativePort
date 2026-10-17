// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
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
#include "Core/Math/FixedPoint.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Collision.h"
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
void (*Battle_actionEffectMediators[100])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* data: contents loaded in Step 5 */

// decomp: D_800F1A00 (D_800F1A00@BATTLE/BATTLE.PRG)
u_int Battle_hitPrerequisiteResult;

// decomp: D_800F1A04 (D_800F1A04@BATTLE/BATTLE.PRG)
u_int Battle_previousHitResult;

// decomp: BATTLE.PRG func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12820)
int Battle_AccumulateMpDamage(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    Battle_ActorStats* temp_s0 = Battle_actors[arg2->target.targetActor]->stats;
    short amount = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);

    if (temp_s0->currentMp < amount) {
        amount = temp_s0->currentMp;
        Battle_lastActionStatValue = temp_s0->currentMp;
    }

    arg2->target.mp += amount;
    arg2->mpEffect = 1;
}

// decomp: BATTLE.PRG func_80084D44 (src/BATTLE/BATTLE.PRG/146C.c:12835)
int Battle_AccumulateWeaponPpDrain(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_actors[arg2->target.targetActor]->stats->weapon.blade.id != 0) {
        u_short amount =
            Battle_actors[arg2->target.targetActor]->stats->weapon.currentPp;
        arg2->weaponPpChange += amount;
        arg2->weaponPpEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80084DA8 (src/BATTLE/BATTLE.PRG/146C.c:12847)
int Battle_AccumulateShieldPpDrain(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_ActorStats* actor = Battle_actors[arg2->target.targetActor]->stats;

    if ((Battle_actionState->selection.primaryTarget.hpEffect == 1) || (Battle_actionState->selection.primaryTarget.hpEffect == 3)) {
        if (actor->shield.base.id != 0) {
            short amount = actor->shield.currentPp;
            arg2->shieldPpChange += amount;
            arg2->shieldPpEffect = 1;
        }
    }
}

// decomp: BATTLE.PRG func_80084E28 (src/BATTLE/BATTLE.PRG/146C.c:12862)
int Battle_AccumulateHpRestoration(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    short amount = Battle_CalculateActionStatChange(action, arg1, arg2, arg3, arg4);
    arg2->target.hp += amount;
    arg2->hpEffect = 2;

    if (arg2->targetsPosition == 0) {
        Battle_DistributeLimbRecovery(action, arg2, amount);
    }
}

// decomp: BATTLE.PRG func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12874)
int Battle_AccumulateMpRestoration(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    short temp_v0 = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
    arg2->target.mp = arg2->target.mp + temp_v0;
    arg2->mpEffect = 2;
}

// decomp: BATTLE.PRG func_80084EEC (src/BATTLE/BATTLE.PRG/146C.c:12882)
int Battle_AccumulateWeaponPpRestoration(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    short temp_v0 = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
    arg2->weaponPpChange += temp_v0;
    arg2->weaponPpEffect = 2;
}

// decomp: BATTLE.PRG func_80084F40 (src/BATTLE/BATTLE.PRG/146C.c:12890)
int Battle_AccumulateWeaponDpRestoration(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    short temp_v0 = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
    arg2->limbs[6].effects.opaque0 += temp_v0;
    arg2->weaponDpEffect = 2;
}

// decomp: BATTLE.PRG func_80084F94 (src/BATTLE/BATTLE.PRG/146C.c:12898)
int Battle_AccumulateRiskReduction(
    Battle_ActionDefinition* arg0, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int arg4)
{
    short temp_v0 = Battle_CalculateActionStatChange(arg0, arg1, arg2, arg3, arg4);
    int var_v0 = temp_v0 << 0x10;
    if (var_v0 != 0) {
        arg2->riskChange += temp_v0;
        arg2->riskEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80084FF0 (src/BATTLE/BATTLE.PRG/146C.c:12909)
int Battle_IgnoreActionEffect(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
}

// decomp: BATTLE.PRG func_80085008 (src/BATTLE/BATTLE.PRG/146C.c:12930)
void Battle_DeriveLimbInjuryStatuses(Battle_HitResult* arg0)
{
    int i;
    int var_v1;
    short var_a1;
    u_short var_a2;
    Battle_ActorStats* temp_s3;

    temp_s3 = Battle_actors[arg0->target.targetActor]->stats;

    switch (arg0->hpEffect) {
    case 2:
        var_a2 = temp_s3->limbs[temp_s3->mobilityLimb0].hp + arg0->limbs[temp_s3->mobilityLimb0].packed;
        var_a1 = temp_s3->limbs[temp_s3->mobilityLimb1].hp + arg0->limbs[temp_s3->mobilityLimb1].packed;
        break;
    case 3:
    case 1:
        var_a2 = temp_s3->limbs[temp_s3->mobilityLimb0].hp - arg0->limbs[temp_s3->mobilityLimb0].packed;
        var_a1 = temp_s3->limbs[temp_s3->mobilityLimb1].hp - arg0->limbs[temp_s3->mobilityLimb1].packed;
        break;
    default:
        var_a2 = temp_s3->limbs[temp_s3->mobilityLimb0].hp;
        var_a1 = temp_s3->limbs[temp_s3->mobilityLimb1].hp;
        break;
    }
    if (!(temp_s3->statuses & 0x10)) {
        if ((temp_s3->mobilityLimb0 | temp_s3->mobilityLimb1) != 0) {
            var_v1 = 0;
            if (temp_s3->mobilityLimb0 != 0) {
                var_v1 = (var_a2 << 0x10) > 0x10000;
            }
            if ((temp_s3->mobilityLimb1 != 0) && (var_a1 >= 2)) {
                var_v1 = 1;
            }
            if (var_v1 == 0) {
                Battle_QueueApplicableStatusChanges(4, arg0);
            }
        }
    } else {
        if ((temp_s3->mobilityLimb0 | temp_s3->mobilityLimb1) != 0) {
            var_v1 = 0;
            if (temp_s3->mobilityLimb0 != 0) {
                var_v1 = (var_a2 << 0x10) > 0x10000;
            }
            if ((temp_s3->mobilityLimb1 != 0) && (var_a1 >= 2)) {
                var_v1 = 1;
            }
            if (var_v1 != 0) {
                Battle_QueueStatusRemovalMask(4, arg0);
            }
        }
    }
    if (arg0->target.targetActor == 0) {
        for (i = 0; i < 4; ++i) {
            switch (arg0->hpEffect) {
            case 2:
                var_a2 = temp_s3->limbs[i].hp + arg0->limbs[i].packed;
                break;
            case 3:
            case 1:
                var_a2 = temp_s3->limbs[i].hp - arg0->limbs[i].packed;
                break;
            default:
                var_a2 = temp_s3->limbs[i].hp;
                break;
            }
            if (!(temp_s3->statuses & Battle_statusEffectParams[Battle_limbInjuryStatusIds[i]].statusFlags)) {
                var_v1 = var_a2 << 0x10;
                var_v1 = 0x10000 < var_v1;
                if (!var_v1) {
                    Battle_QueueApplicableStatusChanges(Battle_limbInjuryStatusIds[i], arg0);
                }
            } else {
                var_v1 = (var_a2 << 0x10) > 0x10000;
                if (var_v1) {
                    Battle_QueueStatusRemovalMask(Battle_limbInjuryStatusIds[i], arg0);
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_80085390 (src/BATTLE/BATTLE.PRG/146C.c:13012)
void Battle_ApplyActionHitEffect(
    Battle_ActionDefinition* action, Battle_HitResult* arg1, Battle_HitResult* arg2, int arg3, int hit)
{
    if ((arg2->targetsPosition == 0)
        && (Battle_actors[arg2->target.targetActor]->stats->actorId == 0x80)) {
        arg2->target.hp += 999;
        arg2->hpEffect = 1;
    } else if (action->type == Battle_ActionType_Spell) {
        if ((arg2->targetsPosition == 0)
            && (Battle_actors[arg2->target.targetActor]->stats->statuses & 0x80040000)) {
            Battle_lastActionStatValue = 0;
        } else {
            Battle_actionEffectMediators[action->hits[hit].effectId](
                action, arg1, arg2, hit, arg3);
        }
        if (arg2->targetsPosition == 0) {
            if (Battle_actors[arg2->target.targetActor]->stats->actorId != 0x80) {
                arg2->inflictStatuses |=
                    Battle_actors[arg2->target.targetActor]->stats->statuses & 0x61000;
            }
        }
    } else {
        Battle_actionEffectMediators[action->hits[hit].effectId](
            action, arg1, arg2, hit, arg3);
    }
    if ((arg3 != 0) && (arg1->targetsPosition == 0) && (arg1->target.targetActor == 0)
        && (arg2->targetsPosition == 0)
        && (Battle_actors[arg2->target.targetActor]->stats->actorId != 0x80)
        && (action->type == Battle_ActionType_Ability || action->type == Battle_ActionType_BreakArt
            || action->type == Battle_ActionType_BaseAttack)
        && (Battle_actors[arg2->target.targetActor]->stats->currentHp <= arg2->target.hp)
        && (arg2->target.targetActor != 0)
        && (Battle_actors[arg2->target.targetActor]->killCounted == 0)) {
        int temp_s0;
        Battle_actors[arg2->target.targetActor]->killCounted = 1;
        temp_s0 = Battle_characterState->equippedWeaponCategory % 10;
        if ((State_ScriptFlag(0x82) >= 0xA)
            && (Audio_Seq_unusedArtsStatus.artsLearned[temp_s0] < 4)
            && (Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[temp_s0]
                < Battle_breakArtPointRequirements[temp_s0][Audio_Seq_unusedArtsStatus
                        .artsLearned[temp_s0]])) {
            ++Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[temp_s0];
        }
    }
}

// decomp: BATTLE.PRG func_800856F8 (src/BATTLE/BATTLE.PRG/146C.c:13058)
void Battle_ClearActionResult(void* arg0) { Core_Memory_ZeroAlignedWords(arg0, 0x84C); }

// decomp: BATTLE.PRG func_80085718 (src/BATTLE/BATTLE.PRG/146C.c:13060)
void Battle_InitializeHitResult(Battle_HitResult* arg0)
{
    Core_Memory_ZeroAlignedWords(arg0, sizeof *arg0);
    arg0->target.impactKind = 6;
    *(&arg0->inflictStatuses + 1) = 0;
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
void Battle_PreviewActorActionCost(int arg0, int arg1)
{
    Battle_ActionCostAndAvailability(arg0, Battle_actors[arg1]->stats, 0);
}

// decomp: BATTLE.PRG func_800859B4 (src/BATTLE/BATTLE.PRG/146C.c:13172)
void Battle_AddActionRecoveryDelay(u_int arg0, Battle_ActorStats* arg1, int arg2)
{
    Battle_ActionDefinition* new_var = &Battle_actionDefinitions[arg0];

    if ((arg2 != 0) && ((arg0 - 0x16) >= 0x20)) {
        if (new_var->recoveryDelay == 0xFF) {
            arg1->actionRecoveryTimer += (arg1->weapon.recoveryDelay + arg1->recoveryDelayBonus) << 8;
        } else {
            int new_var2 = new_var->recoveryDelay + arg1->recoveryDelayBonus;
            arg1->actionRecoveryTimer += new_var2 << 8;
        }
    }
}

// decomp: BATTLE.PRG func_80085A34 (src/BATTLE/BATTLE.PRG/146C.c:13186)
void Battle_FinalizeHitOutcome(Battle_HitStatusView* arg0)
{
    int i;
    u_int var_a1;

    if (arg0->opaque40 == 0) {
        if (arg0->opaque2 == 0xFF) {
            arg0->opaque2 = 0;
        }
        if (arg0->opaque3 == 6) {
            var_a1 = arg0->opaque1C;
            if (var_a1 != 0) {
                for (i = 0; i < 9; var_a1 /= 4, ++i) {
                    switch (var_a1 & 3) {
                    case 2:
                        arg0->opaque3 = 5;
                        return;
                    case 1:
                        arg0->opaque3 = 0;
                        return;
                    case 3:
                        arg0->opaque3 = 1;
                        return;
                    }
                }
            } else if ((arg0->opaque14 | arg0->opaque18) == 0) {
                arg0->opaque3 = 4;
                arg0->opaque2 = 0;
            }
        }
    }
}

// decomp: BATTLE.PRG func_80085B10 (src/BATTLE/BATTLE.PRG/146C.c:13221)
void Battle_ResolveActionTargets(int action, Battle_ActionRequest* result, Battle_ActionRequest* source, int arg3)
{
    SVECTOR origin;
    SVECTOR position;
    SVECTOR impact;
    Battle_ActionDefinition* spec;
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
    __typeof__(spec->hits[0]) params;

    spec = &Battle_actionDefinitions[action];
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
    if (spec->hits[0].affinity != 0) {
        best = spec->hits[0].affinity - 1;
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

    params = spec->hits[0];
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
        switch (spec->hits[hit].prerequisiteFunctionId) {
        case 6:
        case 7:
        case 8:
            Battle_ApplyActionHitEffect(spec, &result->primaryTarget, result->targets, arg3, hit);
            break;

        case 3:
            if (result->primaryTarget.targetsPosition != 0) {
                break;
            }
            entry = &result->primaryTarget;
            stats = Battle_actors[result->primaryTarget.target.targetActor]->stats;
            Battle_hitPrerequisiteResult = 1;
            Battle_previousHitResult = Battle_RollActionHit(spec, entry, entry, hit, arg3);
            if ((Battle_previousHitResult != 0) || (arg3 == 0)) {
                Battle_ApplyActionHitEffect(spec, entry, entry, arg3, hit);
                if (arg3 && stats->cancelsHitResults) {
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
                if (arg3 && (*(int*)&spec->recoveryDelay & 0x4000)) {
                    if (entry->targetsPosition == 0) {
                        Battle_ActionTargetLimbPosition(
                            entry->target.targetActor, entry->target.targetLimb, &position);
                    } else {
                        position = *(SVECTOR*)&entry->target;
                    }
                    redirected = Field_TraceActorAndTerrainCollision(
                        &origin, &position, &impact, result->primaryTarget.target.targetActor);
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
                Battle_hitPrerequisiteResult = Battle_CheckActionHitPrerequisite(spec, (char*)entry, hit);
                if (Battle_hitPrerequisiteResult != 0) {
                    Battle_previousHitResult = Battle_RollActionHit(spec, user, entry, hit, arg3);
                    if ((Battle_previousHitResult != 0) || (arg3 == 0)) {

                        Battle_ApplyActionHitEffect(spec, user, entry, arg3, hit);
                        target = entry->target.targetActor;
                        if ((Battle_actors[target]->roomSpawnIndex == 0x80) && (target != 0)
                            && stats->cancelsHitResults) {
                            limb = (u_char)entry->target.targetLimb;
                            Battle_InitializeHitResult(entry);
                            entry->target.targetActor = target;
                            entry->target.impactKind = 4;
                            entry->target.targetLimb = limb;
                        }
                        if (arg3 && stats->cancelsHitResults) {
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
    if ((Battle_actionState->queueIndex == 0) && arg3) {
        *source = *result;
    }
    if (arg3 && (attacker != NULL)) {
        Battle_AddActionRecoveryDelay(action, attacker, 1);
        Battle_ActionCostAndAvailability(action, attacker, 1);
    }
}

// decomp: BATTLE.PRG func_8008631C (src/BATTLE/BATTLE.PRG/146C.c:13421)
int Battle_PreviewActionAgainstLimb(int arg0, int arg1, int targetActor, int targetLimb, void* arg4)
{
    Battle_ActionRequest sp10;

    sp10.targets[0].target.targetActor = targetActor;
    sp10.actionId = arg0;
    sp10.primaryTarget.targetsPosition = 0;
    sp10.primaryTarget.target.targetActor = arg1;
    sp10.targetCount = 1;
    sp10.targets[0].targetsPosition = 0;
    sp10.targets[0].target.targetLimb = targetLimb;
    Battle_ResolveActionTargets(arg0, arg4, &sp10, 0);
    return Battle_ActionCostAndAvailability(arg0, Battle_actors[arg1]->stats, 0) & 0xFF000000;
}

// decomp: BATTLE.PRG func_800863A4 (src/BATTLE/BATTLE.PRG/146C.c:13436)
int Battle_PreviewActionAgainstActor(int arg0, int arg1, int targetActor, int arg3, SVECTOR* arg4,
    SVECTOR* arg5, Battle_ActionRequest* arg6)
{
    Battle_ActionRequest sp10;
    Battle_TargetVolume sp860;
    SVECTOR sp870;
    int var_s2;
    int temp_s0;
    int temp_s0_2;
    int temp_s1;
    int temp_v0;
    int i;

    sp10.actionId = arg0;
    sp10.primaryTarget.targetsPosition = 0;
    sp10.primaryTarget.target.targetActor = arg1;
    sp10.targetCount = 0;
    temp_v0 = (Battle_actionDefinitions[arg0].category);

    switch (temp_v0) {
    case 11:
    case 12:
        sp860.kind = Battle_actionDefinitions[arg0].shape;
        sp860.opaque1 = 1;
        sp860.scale.extents[0] = Battle_actionDefinitions[arg0].rangeX;
        sp860.scale.extents[1] = Battle_actionDefinitions[arg0].rangeY;
        sp860.scale.extents[2] = Battle_actionDefinitions[arg0].rangeZ;
        temp_s1 = arg5->vx - arg4->vx;
        temp_s0 = arg5->vz - arg4->vz;
        sp860.angle = Core_Gte_Atan2(temp_s1, temp_s0);
        temp_s0_2 = arg5->vy - arg4->vy;
        sp860.scale.extents[3] =
            (Core_Gte_Atan2(temp_s0_2, Core_Math_SquareRootInteger((temp_s1 * temp_s1) + (temp_s0 * temp_s0)))
                / 16)
            + 0x40;
        *(SVECTOR*)sp860.position = *arg4;
        break;

    case 2:
    case 4:
    case 6:
    case 8:
        sp860.kind = Battle_actionDefinitions[arg0].areaShape & 7;
        sp860.opaque1 = 1;
        sp860.scale.extents[0] = Battle_actionDefinitions[arg0].areaRangeX;
        sp860.scale.extents[1] = Battle_actionDefinitions[arg0].areaRangeY;
        sp860.scale.extents[2] = Battle_actionDefinitions[arg0].areaRangeZ;
        sp860.angle = 0;
        sp860.scale.extents[3] = Battle_actionDefinitions[arg0].areaShape & 0xF8;
        *(SVECTOR*)sp860.position = *arg5;
        break;
    }

    switch (Battle_actionDefinitions[arg0].category) {
    case 6:
    case 8:
    case 11:
    case 12:
        var_s2 = 0;

        for (i = 0; i < 6; ++i) {
            if ((Actors_ActorLowGroupBonePosition(targetActor, i, &sp870, 2) >= 0)
                && (Battle_IsPointInTargetVolume(&sp860, (u_short*)&sp870) != 0)) {
                sp10.targets[var_s2].targetsPosition = 0;
                sp10.targets[var_s2].target.targetActor = targetActor;
                sp10.targets[var_s2].target.targetLimb = i;
                ++var_s2;
            }
        }

        sp10.targetCount = var_s2;
        break;

    case 5:
    case 7:
        sp10.targets[0].targetsPosition = 0;
        sp10.targets[0].target.targetActor = targetActor;
        sp10.targets[0].target.targetLimb = arg3;
        sp10.targetCount = 1;
        break;

    case 1:
    case 2:
    case 3:
    case 4:
        sp10.targets[0].targetsPosition = 0;
        sp10.targets[0].target.targetActor = targetActor;
        sp10.targets[0].target.targetLimb = Battle_actors[targetActor]->stats->defaultTargetLimb;
        sp10.targetCount = 1;
        break;
    }

    Battle_ResolveActionTargets(arg0, arg6, &sp10, 0);
    return Battle_ActionCostAndAvailability(arg0, Battle_actors[arg1]->stats, 0) & 0xFF000000;
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

