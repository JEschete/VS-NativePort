// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/DamagePoliciesLive.hpp"
#include "Battle/HitCalculationsLive.hpp"
#include "Battle/StatusQueueLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Audio/Seq/Channel.h"
#include "Battle/AbilityState.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Battle/TargetingState.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Progress.h"

// decomp: D_800E8200 (D_800E8200@BATTLE/BATTLE.PRG)
u_char Battle_limbInjuryStatusIds[20]; /* contents: module image (ModuleData.cpp) */

// decomp: _actionEffectMediators (_actionEffectMediators@BATTLE/BATTLE.PRG)
void (*Battle_actionEffectMediators[70])(
    Battle_ActionDefinition*, Battle_HitResult*, Battle_HitResult*, int, int); /* contents: module image, relocated (ModulePointers.cpp) */

// decomp: D_800F1A00 (D_800F1A00@BATTLE/BATTLE.PRG)
u_int Battle_hitPrerequisiteResult;

// decomp: D_800F1A04 (D_800F1A04@BATTLE/BATTLE.PRG)
u_int Battle_previousHitResult;

// decomp: BATTLE.PRG func_80084CAC (src/BATTLE/BATTLE.PRG/146C.c:12820)
int Battle_AccumulateMpDamage(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    Battle_ActorStats* target_stats = Battle_actors[target_hit->target.targetActor]->stats;
    short amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);

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
    short amount = VS::Battle::LiveCalculateActionStatChange(action, source_hit, target_hit, hit_index, execute_action);
    target_hit->target.hp += amount;
    target_hit->hpEffect = 2;

    if (target_hit->targetsPosition == 0) {
        VS::Battle::LiveDistributeLimbRecovery(action, target_hit, amount);
    }
}

// decomp: BATTLE.PRG func_80084EA0 (src/BATTLE/BATTLE.PRG/146C.c:12874)
int Battle_AccumulateMpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->target.mp = target_hit->target.mp + restoration_amount;
    target_hit->mpEffect = 2;
}

// decomp: BATTLE.PRG func_80084EEC (src/BATTLE/BATTLE.PRG/146C.c:12882)
int Battle_AccumulateWeaponPpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->weaponPpChange += restoration_amount;
    target_hit->weaponPpEffect = 2;
}

// decomp: BATTLE.PRG func_80084F40 (src/BATTLE/BATTLE.PRG/146C.c:12890)
int Battle_AccumulateWeaponDpRestoration(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
    target_hit->limbs[6].effects.opaque0 += restoration_amount;
    target_hit->weaponDpEffect = 2;
}

// decomp: BATTLE.PRG func_80084F94 (src/BATTLE/BATTLE.PRG/146C.c:12898)
int Battle_AccumulateRiskReduction(
    Battle_ActionDefinition* action_definition, Battle_HitResult* source_hit, Battle_HitResult* target_hit, int hit_index, int execute_action)
{
    short restoration_amount = VS::Battle::LiveCalculateActionStatChange(action_definition, source_hit, target_hit, hit_index, execute_action);
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
                VS::Battle::QueueLiveApplicableStatusChanges(4, hit_result);
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
                VS::Battle::QueueLiveStatusRemovalMask(4, hit_result);
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
                    VS::Battle::QueueLiveApplicableStatusChanges(Battle_limbInjuryStatusIds[i], hit_result);
                }
            } else {
                limb_health_test = (projected_limb_hp << 0x10) > 0x10000;
                if (limb_health_test) {
                    VS::Battle::QueueLiveStatusRemovalMask(Battle_limbInjuryStatusIds[i], hit_result);
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
        if ((VS::State::LiveScriptFlag(0x82) >= 0xA)
            && (Audio_Seq_unusedArtsStatus.artsLearned[weapon_category] < 4)
            && (Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[weapon_category]
                < Battle_breakArtPointRequirements[weapon_category][Audio_Seq_unusedArtsStatus
                        .artsLearned[weapon_category]])) {
            ++Audio_Seq_unusedArtsStatus.kills.weaponCategoryKills[weapon_category];
        }
    }
}

}  // extern "C"
