// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorState.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Skeleton.h"
#include "Battle/Abilities.h"
#include "Battle/Action.h"
#include "Battle/ActionFlow.h"
#include "Battle/ActionResolution.h"
#include "Battle/Actor.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Feedback.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Battle/Targeting.h"
#include "Core/Random.h"
#include "Event/Triggers.h"
#include "Field/Interactions.h"
#include "Items/Equipment.h"
#include "Items/EquipmentAffinity.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Menu/Framework/ActorPreview.h"
#include "Menu/Score/ScoreScreen.h"
#include "Render/Palette.h"
#include "State/Timers.h"

// decomp: D_800E8204 (D_800E8204@BATTLE/BATTLE.PRG)
int Battle_limbStatusFlags[4]; /* contents: module image (ModuleData.c) */

// decomp: D_800E8490 (D_800E8490@BATTLE/BATTLE.PRG)
int Battle_vitalsTickCounter; /* contents: module image (ModuleData.c) */

// decomp: D_800E8494 (D_800E8494@BATTLE/BATTLE.PRG)
int Battle_regeneratingLimb; /* contents: module image (ModuleData.c) */

// decomp: D_800E8498 (D_800E8498@BATTLE/BATTLE.PRG)
int Battle_targetingRiskTimer; /* contents: module image (ModuleData.c) */

// decomp: vs_battle_sceneBuffer (vs_battle_sceneBuffer@BATTLE/BATTLE.PRG)
void* Battle_unusedSceneBuffer;

// decomp: vs_battle_menu9CursorMemory (vs_battle_menu9CursorMemory@BATTLE/BATTLE.PRG)
Menu_Score_CursorMemory Battle_unusedMenu9CursorMemory;

// decomp: vs_battle_submenuState (vs_battle_submenuState@BATTLE/BATTLE.PRG)
int Battle_unusedSubmenuState;

// decomp: D_800F4590 (D_800F4590@BATTLE/BATTLE.PRG)
Menu_Framework_PreviewPoseState* Battle_unusedGlobal1[1];

// decomp: D_800F4594 (D_800F4594@BATTLE/BATTLE.PRG)
int Battle_unusedGlobal2;

// decomp: vs_main_statusEffectParams (vs_main_statusEffectParams@SLUS_010.40)
Battle_StatusEffectParams Battle_statusEffectParams[32]; /* contents: module image (ModuleData.c) */

// decomp: BATTLE.PRG func_8007C31C (src/BATTLE/BATTLE.PRG/146C.c:8736)
void Battle_PreserveAshleyStatuses(void) { Battle_RemoveActorStatuses(0, Battle_characterState->stats); }

// decomp: BATTLE.PRG _setApplicableStatuses (src/BATTLE/BATTLE.PRG/146C.c:11195)
int Battle_QueueApplicableStatusChanges(int status_id, Battle_HitResult* entity)
{
    int inflict = 0;
    int remove = 0;
    Battle_StatusEffectParams* params = &Battle_statusEffectParams[status_id];
    Battle_ActorStats* target_stats = Battle_actors[entity->target.targetActor]->stats;

    if (target_stats->actorId != 128) {

        int current_statuses = target_stats->statuses;

        if (!(current_statuses & params->blockingStatuses)) {
            remove = params->statusFlags & ~target_stats->statusImmunities & (~current_statuses | params->cancelledStatuses);
            entity->removeStatuses |= remove;
            inflict = params->cancelledStatuses & target_stats->statuses;
            entity->inflictStatuses |= inflict;
        }
    }

    return remove | inflict;
}

// decomp: BATTLE.PRG _setInflictedStatuses (src/BATTLE/BATTLE.PRG/146C.c:11217)
int Battle_QueueStatusRemovalMask(int status_id, Battle_HitResult* entity)
{
    int ret = 0;
    Battle_StatusEffectParams* params = &Battle_statusEffectParams[status_id];

    if (Battle_actors[entity->target.targetActor]->stats->actorId != 128) {
        ret = params->statusFlags & Battle_actors[entity->target.targetActor]->stats->statuses;
        entity->inflictStatuses |= ret;
    }

    return ret;
}

// decomp: BATTLE.PRG func_80083998 (src/BATTLE/BATTLE.PRG/146C.c:12273)
int Battle_QueueStrengthDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(5, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_800839E8 (src/BATTLE/BATTLE.PRG/146C.c:12282)
int Battle_QueueIntelligenceDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(7, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083A38 (src/BATTLE/BATTLE.PRG/146C.c:12291)
int Battle_QueueAgilityDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(9, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083A88 (src/BATTLE/BATTLE.PRG/146C.c:12300)
int Battle_QueueEquipmentDown(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(19, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083AD8 (src/BATTLE/BATTLE.PRG/146C.c:12309)
int Battle_QueueHalfMovement(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(4, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083B28 (src/BATTLE/BATTLE.PRG/146C.c:12318)
int Battle_QueueStrengthUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(6, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12327)
int Battle_QueueIntelligenceUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(8, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12336)
int Battle_QueueAgilityUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(10, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12345)
int Battle_QueueEquipmentUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(20, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12354)
int Battle_QueueQuicken(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(11, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12363)
int Battle_QueueSilence(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(12, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12372)
int Battle_QueueMagicWard(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)),
    Battle_HitResult* target_hit __attribute__((unused)), int hit_index __attribute__((unused)),
    int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x12, target_hit);
}

// decomp: BATTLE.PRG func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12380)
int Battle_QueueRegeneration(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(17, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12389)
int Battle_QueueBanish(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action)
{
    Battle_QueueApplicableStatusChanges(0x1D, target_hit);
    if (execute_action != 0) {
        Battle_actors[target_hit->target.targetActor]->stats->isAnalyzed = 1;
    }
}

// decomp: BATTLE.PRG func_80083DEC (src/BATTLE/BATTLE.PRG/146C.c:12399)
int Battle_QueueParalysis(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(13, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083E3C (src/BATTLE/BATTLE.PRG/146C.c:12408)
int Battle_QueuePoison(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(14, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083E8C (src/BATTLE/BATTLE.PRG/146C.c:12417)
int Battle_QueueNumbness(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(15, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083EDC (src/BATTLE/BATTLE.PRG/146C.c:12426)
int Battle_QueueCurse(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(16, target_hit) != 0) {
        target_hit->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083F2C (src/BATTLE/BATTLE.PRG/146C.c:12435)
int Battle_QueueAirAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x15, target_hit);
}

// decomp: BATTLE.PRG func_80083F50 (src/BATTLE/BATTLE.PRG/146C.c:12442)
int Battle_QueueFireAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x16, target_hit);
}

// decomp: BATTLE.PRG func_80083F74 (src/BATTLE/BATTLE.PRG/146C.c:12449)
int Battle_QueueEarthAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x17, target_hit);
}

// decomp: BATTLE.PRG func_80083F98 (src/BATTLE/BATTLE.PRG/146C.c:12456)
int Battle_QueueWaterAttackUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x18, target_hit);
}

// decomp: BATTLE.PRG func_80083FBC (src/BATTLE/BATTLE.PRG/146C.c:12463)
int Battle_QueueAirDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x19, target_hit);
}

// decomp: BATTLE.PRG func_80083FE0 (src/BATTLE/BATTLE.PRG/146C.c:12470)
int Battle_QueueFireDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x1A, target_hit);
}

// decomp: BATTLE.PRG func_80084004 (src/BATTLE/BATTLE.PRG/146C.c:12477)
int Battle_QueueEarthDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x1B, target_hit);
}

// decomp: BATTLE.PRG func_80084028 (src/BATTLE/BATTLE.PRG/146C.c:12484)
int Battle_QueueWaterDefenseUp(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x1C, target_hit);
}

// decomp: BATTLE.PRG func_8008404C (src/BATTLE/BATTLE.PRG/146C.c:12491)
int Battle_QueueParalysisCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0xD, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_8008409C (src/BATTLE/BATTLE.PRG/146C.c:12500)
int Battle_QueuePoisonCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0xE, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_800840EC (src/BATTLE/BATTLE.PRG/146C.c:12509)
int Battle_QueueCurseCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0x10, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_8008413C (src/BATTLE/BATTLE.PRG/146C.c:12518)
int Battle_QueueNumbnessCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0xF, target_hit) != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_8008418C (src/BATTLE/BATTLE.PRG/146C.c:12527)
int Battle_QueueDispel(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    target_hit->inflictStatuses |=
        (Battle_actors[target_hit->target.targetActor]->stats->statuses & 0x1FFE1FE0);
}

// decomp: BATTLE.PRG func_800841C8 (src/BATTLE/BATTLE.PRG/146C.c:12535)
int Battle_QueueNegativeStatusCure(Battle_ActionDefinition* action_definition __attribute__((unused)),
    Battle_HitResult* source_hit __attribute__((unused)), Battle_HitResult* target_hit,
    int hit_index __attribute__((unused)), int execute_action __attribute__((unused)))
{
    int negative_status_mask = Battle_actors[target_hit->target.targetActor]->stats->statuses & 0xE000;
    target_hit->inflictStatuses |= negative_status_mask;

    if (negative_status_mask != 0) {
        target_hit->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80086754 (src/BATTLE/BATTLE.PRG/146C.c:13532)
void Battle_RemoveActorStatuses(int status_mask, Battle_ActorStats* actor_stats)
{
    int i;
    int statDecrease;
    int statIncrease;
    u_int element_status_mask;

    actor_stats->statuses &= ~status_mask;

    if (status_mask & 0x7E0) {
        if (!(actor_stats->statuses & 0x7E0)) {
            actor_stats->statusTimers[0] = 0;
        }
    }

    if (status_mask & 0x810) {
        if (!(actor_stats->statuses & 0x810)) {
            actor_stats->statusTimers[1] = 0;
        }
    }

    if (status_mask & 0x1FF80000) {
        if (!(actor_stats->statuses & 0x1FF80000)) {
            actor_stats->statusTimers[2] = 0;
        }
    }

    if (status_mask & 0x2000) {
        if (!(actor_stats->statuses & 0x2000)) {
            actor_stats->statusTimers[3] = 0;
        }
    }

    if (status_mask & 0x8000) {
        if (!(actor_stats->statuses & 0x8000)) {
            actor_stats->statusTimers[4] = 0;
        }
    }

    if (status_mask & 0x24000) {
        if (!(actor_stats->statuses & 0x24000)) {
            actor_stats->statusTimers[5] = 0;
        }
    }

    if (status_mask & 0x60) {
        actor_stats->strength = actor_stats->totalStrength;
    }

    if (status_mask & 0x180) {
        actor_stats->intelligence = actor_stats->totalIntelligence;
    }

    if (status_mask & 0x600) {
        actor_stats->agility = actor_stats->totalAgility;
    }

    if (status_mask & 0x10000) {
        actor_stats->strength = actor_stats->totalStrength;
        actor_stats->intelligence = actor_stats->totalIntelligence;
        actor_stats->agility = actor_stats->totalAgility;
    }

    if (status_mask & 0x8010) {
        if (status_mask & 0x10) {

            if ((actor_stats->mobilityLimb0 != 0) && (actor_stats->limbs[actor_stats->mobilityLimb0].maxHp != 0)
                && (actor_stats->limbs[actor_stats->mobilityLimb0].hp < 2)) {
                actor_stats->limbs[actor_stats->mobilityLimb0].hp = 2;
            }

            if ((actor_stats->mobilityLimb1 != 0) && (actor_stats->limbs[actor_stats->mobilityLimb1].maxHp != 0)
                && (actor_stats->limbs[actor_stats->mobilityLimb1].hp < 2)) {
                actor_stats->limbs[actor_stats->mobilityLimb1].hp = 2;
            }
        }

        if (!(actor_stats->statuses & 0x10)) {
            actor_stats->walkSpeed = actor_stats->baseWalkSpeed;
            actor_stats->runSpeed = actor_stats->baseRunSpeed;
        }

        Battle_Ai_SetActorVerticalMovementSteps(actor_stats->actorId, actor_stats->walkSpeed, actor_stats->runSpeed);
    }

    if ((status_mask & 0x800) && !(actor_stats->statuses & 0x8010)) {

        actor_stats->walkSpeed = actor_stats->baseWalkSpeed;
        actor_stats->runSpeed = actor_stats->baseRunSpeed;

        Battle_Ai_SetActorVerticalMovementSteps(actor_stats->actorId, actor_stats->walkSpeed, actor_stats->runSpeed);
    }

    if (status_mask & 0x180000) {
        if (actor_stats->weapon.blade.id != 0) {
            actor_stats->weapon.currentStr = actor_stats->weapon.baseStr;
            actor_stats->weapon.currentInt = actor_stats->weapon.baseInt;
        }

        if (actor_stats->shield.base.id != 0) {
            actor_stats->shield.currentStr = actor_stats->shield.baseStr;
            actor_stats->shield.currentInt = actor_stats->shield.baseInt;
        }

        for (i = 0; i < 6; ++i) {
            if (actor_stats->limbs[i].armor.armor.id != 0) {
                actor_stats->limbs[i].armor.currentStr = actor_stats->limbs[i].armor.baseStr;
                actor_stats->limbs[i].armor.currentInt = actor_stats->limbs[i].armor.baseInt;
            }
        }
    }

    element_status_mask = status_mask & 0x01E00000;

    if (element_status_mask != 0) {
        switch (element_status_mask) {
        case 0x200000:
            statIncrease = 1;
            statDecrease = 3;
            break;

        case 0x400000:
            statIncrease = 2;
            statDecrease = 4;
            break;

        case 0x800000:
            statIncrease = 3;
            statDecrease = 1;
            break;

        case 0x01000000:
            statIncrease = 4;
            statDecrease = 2;
            break;

        default:
            statDecrease = 7;
            statIncrease = 7;
            break;
        }

        if (statIncrease != 7) {
            if (actor_stats->weapon.blade.id != 0) {

                if ((actor_stats->weapon.blade.affinities[statIncrease] < 50)
                    && (Core_Random_NextBounded(8) == 0)) {

                    ++actor_stats->weapon.blade.affinities[statIncrease];

                    if (actor_stats->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(240, statIncrease + 6, 1);
                    }
                }

                if ((actor_stats->weapon.blade.affinities[statDecrease] > -100)
                    && (Core_Random_NextBounded(16) == 0)) {

                    --actor_stats->weapon.blade.affinities[statDecrease];

                    if (actor_stats->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(240, statDecrease + 6, 0x80000001);
                    }
                }
            }

            actor_stats->weapon.affinityModifiers[statIncrease] = 0;
            actor_stats->weapon.affinityModifiers[statDecrease] = 0;

            Items_RebuildWeaponClassAffinity(actor_stats);
        }
    }

    element_status_mask = status_mask & 0x1E000000;

    if (element_status_mask != 0) {

        switch (element_status_mask) {
        case 0x2000000:
            statIncrease = 1;
            statDecrease = 3;
            break;

        case 0x4000000:
            statIncrease = 2;
            statDecrease = 4;
            break;

        case 0x8000000:
            statIncrease = 3;
            statDecrease = 1;
            break;

        case 0x10000000:
            statIncrease = 4;
            statDecrease = 2;
            break;

        default:
            statDecrease = 7;
            statIncrease = 7;
            break;
        }

        if (statIncrease != 7) {
            if (actor_stats->shield.base.id != 0) {

                if ((actor_stats->shield.base.affinities[statIncrease] < 50)
                    && (Core_Random_NextBounded(8) == 0)) {

                    ++actor_stats->shield.base.affinities[statIncrease];

                    if (actor_stats->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(241, statIncrease + 6, 1);
                    }
                }

                if ((actor_stats->shield.base.affinities[statDecrease] > -100)
                    && (Core_Random_NextBounded(0x10) == 0)) {

                    --actor_stats->shield.base.affinities[statDecrease];

                    if (actor_stats->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(241, statDecrease + 6, 0x80000001);
                    }
                }
            }

            actor_stats->shield.affinityModifiers[statIncrease] = 0;
            actor_stats->shield.affinityModifiers[statDecrease] = 0;

            Items_RebuildShieldClassAffinity(actor_stats);

            for (i = 0; i < 6; ++i) {
                if (actor_stats->limbs[i].armor.armor.id != 0) {

                    if ((actor_stats->limbs[i].armor.armor.affinities[statIncrease] < 50)
                        && (Core_Random_NextBounded(8) == 0)) {

                        ++actor_stats->limbs[i].armor.armor.affinities[statIncrease];

                        if (actor_stats->actorId == 0) {
                            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(actor_stats->limbs[i].nameIndex, statIncrease + 6, 1);
                        }
                    }

                    if ((actor_stats->limbs[i].armor.armor.affinities[statDecrease] > -100)
                        && (Core_Random_NextBounded(16) == 0)) {

                        --actor_stats->limbs[i].armor.armor.affinities[statDecrease];

                        if (actor_stats->actorId == 0) {
                            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(
                                actor_stats->limbs[i].nameIndex, statDecrease + 6, 0x80000001);
                        }
                    }
                }

                actor_stats->limbs[i].armor.affinityModifiers[statIncrease] = 0;
                actor_stats->limbs[i].armor.affinityModifiers[statDecrease] = 0;

                Items_RebuildArmorClassAffinity(actor_stats, i);
            }
        }

        Battle_CommitEquipmentStatChanges();
    }
}

// decomp: BATTLE.PRG func_80086EA4 (src/BATTLE/BATTLE.PRG/146C.c:13801)
void Battle_ApplyWeaponElementEnchantment(Battle_ActorStats* actor_stats, int increased_affinity, int decreased_affinity, int modifier_amount)
{
    if (actor_stats->weapon.blade.id != 0) {
        actor_stats->weapon.classAffinityCurrent.affinities[0][increased_affinity] += modifier_amount;
        actor_stats->weapon.classAffinityCurrent.affinities[0][decreased_affinity] -= modifier_amount;
        actor_stats->weapon.affinityModifiers[increased_affinity] = modifier_amount;
        actor_stats->weapon.affinityModifiers[decreased_affinity] = -modifier_amount;
    }
}

// decomp: BATTLE.PRG func_80086EF4 (src/BATTLE/BATTLE.PRG/146C.c:13811)
void Battle_ApplyArmorElementEnchantment(Battle_ActorStats* actor_stats, int increased_affinity, int decreased_affinity, int modifier_amount)
{
    int i;

    if (actor_stats->shield.base.id != 0) {
        actor_stats->shield.classAffinityCurrent.affinities[0][increased_affinity] += modifier_amount;
        actor_stats->shield.classAffinityCurrent.affinities[0][decreased_affinity] -= modifier_amount;
        actor_stats->shield.affinityModifiers[increased_affinity] = modifier_amount;
        actor_stats->shield.affinityModifiers[decreased_affinity] = -modifier_amount;
    }

    for (i = 0; i < 6; ++i) {
        if (actor_stats->limbs[i].armor.armor.id != 0) {
            actor_stats->limbs[i].armor.classAffinityCurrent.affinities[0][increased_affinity] += modifier_amount;
            actor_stats->limbs[i].armor.classAffinityCurrent.affinities[0][decreased_affinity] -= modifier_amount;
            actor_stats->limbs[i].armor.affinityModifiers[increased_affinity] = modifier_amount;
            actor_stats->limbs[i].armor.affinityModifiers[decreased_affinity] = -modifier_amount;
        }
    }
}

// decomp: BATTLE.PRG func_80086FA8 (src/BATTLE/BATTLE.PRG/146C.c:13832)
void Battle_InflictActorStatuses(int status_mask, Battle_ActorStats* actor_stats)
{
    int i;
    int equipment_modifier_amount;
    u_int element_status_mask;

    actor_stats->statuses |= status_mask;

    if (status_mask & 0x7E0) {
        actor_stats->statusTimers[0] = Battle_statusParams5Duration;
    }

    if (status_mask & 0x810) {
        actor_stats->statusTimers[1] = Battle_statusParams11Duration;
    }

    if (status_mask & 0x1FF80000) {
        actor_stats->statusTimers[2] = Battle_statusParams19Duration;
    }

    if (status_mask & 0x2000) {
        actor_stats->statusTimers[3] = Battle_statusParams13Duration;
    }

    if (status_mask & 0x8000) {
        actor_stats->statusTimers[4] = Battle_statusParams13Duration;
    }

    if (status_mask & 0x4000) {
        actor_stats->statusTimers[5] = Battle_statusParams14TickInterval;
    } else if (status_mask & 0x20000) {
        actor_stats->statusTimers[5] = Battle_statusParams17TickInterval;
    }

    if (status_mask & 0x20) {
        actor_stats->strength = actor_stats->totalStrength - (actor_stats->totalStrength / 10);
    }

    if (status_mask & 0x40) {
        actor_stats->strength = actor_stats->totalStrength + (actor_stats->totalStrength / 10);
    }

    if (status_mask & 0x80) {
        actor_stats->intelligence = actor_stats->totalIntelligence - (actor_stats->totalIntelligence / 10);
    }

    if (status_mask & 0x100) {
        actor_stats->intelligence = actor_stats->totalIntelligence + (actor_stats->totalIntelligence / 10);
    }

    if (status_mask & 0x200) {
        actor_stats->agility = actor_stats->totalAgility - (actor_stats->totalAgility / 10);
    }
    if (status_mask & 0x400) {
        actor_stats->agility = actor_stats->totalAgility + (actor_stats->totalAgility / 10);
    }

    if (status_mask & 0x10000) {
        actor_stats->strength = actor_stats->totalStrength - (actor_stats->totalStrength / 10);
        actor_stats->intelligence = actor_stats->totalIntelligence - (actor_stats->totalIntelligence / 10);
        actor_stats->agility = actor_stats->totalAgility - (actor_stats->totalAgility / 10);
    }

    if (status_mask & 0x8810) {

        if (actor_stats->statuses & 0x8010) {
            actor_stats->walkSpeed = (actor_stats->baseWalkSpeed >> 1);
            actor_stats->runSpeed = actor_stats->baseRunSpeed >> 1;
        } else if (actor_stats->statuses & 0x800) {
            actor_stats->walkSpeed = actor_stats->baseWalkSpeed + (actor_stats->baseWalkSpeed >> 2);
            actor_stats->runSpeed = actor_stats->baseRunSpeed + (actor_stats->baseRunSpeed >> 2);
        }

        Battle_Ai_SetActorVerticalMovementSteps(actor_stats->actorId, actor_stats->walkSpeed, actor_stats->runSpeed);
    }

    if (status_mask & 0x80000) {

        equipment_modifier_amount = ((actor_stats->intelligence + actor_stats->weapon.currentInt) * 7) / 100;

        if (equipment_modifier_amount < 0) {
            equipment_modifier_amount = 0;
        }

        if (actor_stats->weapon.blade.id != 0) {
            actor_stats->weapon.currentStr -= equipment_modifier_amount;
        }

        if (actor_stats->shield.base.id != 0) {
            actor_stats->shield.currentStr -= equipment_modifier_amount;
        }

        for (i = 0; i < 6; ++i) {
            if (actor_stats->limbs[i].armor.armor.id != 0) {
                actor_stats->limbs[i].armor.currentStr -= equipment_modifier_amount;
            }
        }
    }

    if (status_mask & 0x100000) {

        equipment_modifier_amount = ((actor_stats->intelligence + actor_stats->weapon.currentInt) * 7) / 100;

        if (equipment_modifier_amount < 0) {
            equipment_modifier_amount = 0;
        }

        if (actor_stats->weapon.blade.id != 0) {
            actor_stats->weapon.currentStr += equipment_modifier_amount;
        }

        if (actor_stats->shield.base.id != 0) {
            actor_stats->shield.currentStr += equipment_modifier_amount;
        }

        for (i = 0; i < 6; ++i) {
            if (actor_stats->limbs[i].armor.armor.id != 0) {
                actor_stats->limbs[i].armor.currentStr += equipment_modifier_amount;
            }
        }
    }

    if (status_mask & 0x20000000) {
        *(int*)&actor_stats->actionRecoveryTimer |= 0x20000;
    }

    element_status_mask = status_mask & 0x01E00000;

    if (element_status_mask != 0) {

        equipment_modifier_amount = (actor_stats->intelligence + actor_stats->weapon.currentInt) / 2;

        if (equipment_modifier_amount < 0) {
            equipment_modifier_amount = 0;
        }

        switch (element_status_mask) {
        case 0x200000:
            Battle_ApplyWeaponElementEnchantment(actor_stats, 1, 3, equipment_modifier_amount);
            break;
        case 0x400000:
            Battle_ApplyWeaponElementEnchantment(actor_stats, 2, 4, equipment_modifier_amount);
            break;
        case 0x800000:
            Battle_ApplyWeaponElementEnchantment(actor_stats, 3, 1, equipment_modifier_amount);
            break;
        case 0x01000000:
            Battle_ApplyWeaponElementEnchantment(actor_stats, 4, 2, equipment_modifier_amount);
            break;
        }
    }

    element_status_mask = status_mask & 0x01E000000;

    if (element_status_mask) {

        equipment_modifier_amount = (actor_stats->intelligence + actor_stats->weapon.currentInt) / 2;

        if (equipment_modifier_amount < 0) {
            equipment_modifier_amount = 0;
        }

        switch (element_status_mask) {
        case 0x2000000:
            Battle_ApplyArmorElementEnchantment(actor_stats, 1, 3, equipment_modifier_amount);
            break;
        case 0x4000000:
            Battle_ApplyArmorElementEnchantment(actor_stats, 2, 4, equipment_modifier_amount);
            break;
        case 0x8000000:
            Battle_ApplyArmorElementEnchantment(actor_stats, 3, 1, equipment_modifier_amount);
            break;
        case 0x10000000:
            Battle_ApplyArmorElementEnchantment(actor_stats, 4, 2, equipment_modifier_amount);
            break;
        }
    }
}

// decomp: BATTLE.PRG func_80087578 (src/BATTLE/BATTLE.PRG/146C.c:14011)
void Battle_QueueActorStatChangeNumber(int actor_slot, int limb_tag, int encoded_amount, int label_flags)
{
    if ((Battle_actors[actor_slot]->roomSpawnIndex != 0x80) || (actor_slot == 0)) {
        ((void (*)(int actor_slot, int limb_tag, u_int encoded_amount, int label_flags))Battle_QueueActorValueLabel)(actor_slot, limb_tag, encoded_amount, label_flags);
    }
}

// decomp: BATTLE.PRG _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018)
void Battle_ApplyActorActionResult(Battle_HitResult* hit_result)
{
    int flag;
    u_int i;
    int targetActorId;
    Battle_ActorStats* actor;

    Battle_CommitEquipmentStatChanges();

    targetActorId = hit_result->target.targetActor;
    actor = Battle_actors[targetActorId]->stats;

    if (hit_result->target.impactKind == 4) {
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, 0, 2);
    }

    switch (hit_result->hpEffect) {
    case 2:

        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->target.hp, 0);

        actor->currentHp += hit_result->target.hp;

        if (actor->maxHp < actor->currentHp) {
            actor->currentHp = actor->maxHp;
        }

        for (i = 0; i < 6; ++i) {

            actor->limbs[i].hp += hit_result->limbs[i].effects.opaque0;

            if (actor->limbs[i].maxHp < actor->limbs[i].hp) {
                actor->limbs[i].hp = actor->limbs[i].maxHp;
            }
        }

        break;

    case 3:
    case 1:

        Battle_QueueActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->target.hp | 0x80000000, 0);

        if (actor->currentHp > hit_result->target.hp) {
            actor->currentHp -= hit_result->target.hp;
        } else {
            actor->currentHp = 0;
        }

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].hp > hit_result->limbs[i].effects.opaque0) {
                actor->limbs[i].hp -= hit_result->limbs[i].effects.opaque0;
            } else {
                actor->limbs[i].hp = 0;
            }
            if (actor->limbs[i].armor.currentDp > hit_result->limbs[i].effects.opaque2) {
                actor->limbs[i].armor.currentDp -= hit_result->limbs[i].effects.opaque2;
            } else {
                actor->limbs[i].armor.currentDp = 0;
            }
        }

        break;
    }

    switch (hit_result->mpEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->target.mp, 1);

        actor->currentMp += hit_result->target.mp;

        if (actor->maxMp < actor->currentMp) {
            actor->currentMp = actor->maxMp;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->target.mp | 0x80000000, 1);

        if (actor->currentMp > hit_result->target.mp) {
            actor->currentMp -= hit_result->target.mp;
        } else {
            actor->currentMp = 0;
        }
        break;
    }

    switch (hit_result->riskEffect) {
    case 1:
        break;

    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->riskChange | 0x80000000, 8);

        if (actor->risk > hit_result->riskChange) {
            actor->risk -= hit_result->riskChange;
        } else {
            actor->risk = 0;
        }
        break;
    }

    switch (hit_result->strengthEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->strengthChange, 5);

        actor->totalStrength += hit_result->strengthChange;

        if (actor->totalStrength > 999) {
            actor->totalStrength = 999;
        }

        actor->strength += hit_result->strengthChange;

        if (actor->strength > 999) {
            actor->strength = 999;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->strengthChange | 0x80000000, 5);

        if (actor->totalStrength > hit_result->strengthChange) {
            actor->totalStrength -= hit_result->strengthChange;
        } else {
            actor->totalStrength = 0;
        }

        if (actor->strength > hit_result->strengthChange) {
            actor->strength -= hit_result->strengthChange;
        } else {
            actor->strength = 0;
        }

        break;
    }

    switch (hit_result->intelligenceEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->intelligenceChange, 6);

        actor->totalIntelligence += hit_result->intelligenceChange;

        if (actor->totalIntelligence > 999) {
            actor->totalIntelligence = 999;
        }

        actor->intelligence += hit_result->intelligenceChange;

        if (actor->intelligence > 999) {
            actor->intelligence = 999;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->intelligenceChange | 0x80000000, 6);

        if (actor->totalIntelligence > hit_result->intelligenceChange) {
            actor->totalIntelligence -= hit_result->intelligenceChange;
        } else {
            actor->totalIntelligence = 0;
        }

        if (actor->intelligence > hit_result->intelligenceChange) {
            actor->intelligence -= hit_result->intelligenceChange;
        } else {
            actor->intelligence = 0;
        }

        break;
    }

    switch (hit_result->agilityEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->agilityChange, 7);

        actor->totalAgility += hit_result->agilityChange;

        if (actor->totalAgility > 999) {
            actor->totalAgility = 999;
        }

        actor->agility += hit_result->agilityChange;

        if (actor->agility > 999) {
            actor->agility = 999;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, hit_result->target.targetLimb, hit_result->agilityChange | 0x80000000, 7);

        if (actor->totalAgility > hit_result->agilityChange) {
            actor->totalAgility -= hit_result->agilityChange;
        } else {
            actor->totalAgility = 0;
        }

        if (actor->agility > hit_result->agilityChange) {
            actor->agility -= hit_result->agilityChange;
        } else {
            actor->agility = 0;
        }
        break;
    }

    switch (hit_result->maxHpEffect) {
    case 1:
        break;
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->maxHpChange, 3);

        actor->maxHp += hit_result->maxHpChange;

        if (actor->maxHp > 999) {
            actor->maxHp = 999;
        }
        break;
    }

    switch (hit_result->maxMpEffect) {
    case 1:
        break;
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, hit_result->target.targetLimb, hit_result->maxMpChange, 4);

        actor->maxMp += hit_result->maxMpChange;

        if (actor->maxMp > 999) {
            actor->maxMp = 999;
        }
        break;
    }

    switch (hit_result->weaponDpEffect) {
    case 1:
        break;
    case 2:
        actor->weapon.currentDp += hit_result->weaponDpChange;

        if (actor->weapon.maxDp < actor->weapon.currentDp) {
            actor->weapon.currentDp = actor->weapon.maxDp;
        }
        break;
    }

    switch (hit_result->weaponPpEffect) {
    case 2:
        actor->weapon.currentPp += hit_result->weaponPpChange;

        if (actor->weapon.maxPp < actor->weapon.currentPp) {
            actor->weapon.currentPp = actor->weapon.maxPp;
        }

        break;

    case 1:
        if (actor->weapon.currentPp > hit_result->weaponPpChange) {
            actor->weapon.currentPp -= hit_result->weaponPpChange;
        } else {
            actor->weapon.currentPp = 0;
        }

        break;
    }

    switch (hit_result->armorDpEffect) {
    case 1:
        if (actor->shield.currentDp > hit_result->armorDpChange) {
            actor->shield.currentDp -= hit_result->armorDpChange;
        } else {
            actor->shield.currentDp = 0;
        }

        if (actor->limbs[hit_result->target.targetLimb].armor.currentDp > hit_result->armorDpChange) {
            actor->limbs[hit_result->target.targetLimb].armor.currentDp -= hit_result->armorDpChange;
        } else {
            actor->limbs[hit_result->target.targetLimb].armor.currentDp = 0;
        }

        break;
    }

    switch (hit_result->shieldPpEffect) {
    case 1:
        if (actor->shield.currentPp > hit_result->shieldPpChange) {
            actor->shield.currentPp -= hit_result->shieldPpChange;
        } else {
            actor->shield.currentPp = 0;
        }

        break;
    }

    flag = 1;

    if (hit_result->inflictStatuses != 0) {

        for (i = 0; i < 32; ++i) {

            int status = hit_result->inflictStatuses & flag;

            if ((status != 0) && (actor->statuses & flag)) {

                if (targetActorId != 0) {

                    if (actor->currentHp != 0) {
                        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(targetActorId, 0, status, 1);
                    }

                } else {
                    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, status, 1);
                }

                Battle_RemoveActorStatuses(hit_result->inflictStatuses & flag, actor);
            }

            flag <<= 1;
        }
    }

    flag = 1;

    if (hit_result->removeStatuses != 0) {

        for (i = 0; i < 32; ++i) {

            int status = hit_result->removeStatuses & flag;

            if ((status != 0) && !(actor->statuses & flag)) {

                if (targetActorId != 0) {

                    if (actor->currentHp != 0) {
                        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(targetActorId, 0, status, 0);
                    }

                } else {
                    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, status, 0);
                }

                Battle_InflictActorStatuses(hit_result->removeStatuses & flag, actor);
            }

            flag <<= 1;
        }
    }
}

// decomp: BATTLE.PRG func_80087EF4 (src/BATTLE/BATTLE.PRG/146C.c:14376)
void Battle_UpdateActorStatusTimers(Battle_ActorStats* actor)
{
    int limb_recovered;
    int i;
    int statuses;

    statuses = 0;

    if ((actor->statusTimers[0] != 0) && (--actor->statusTimers[0] == 0)) {
        statuses = actor->statuses & 0x7E0;
    }

    if ((actor->statusTimers[1] != 0) && (--actor->statusTimers[1] == 0)) {
        statuses |= actor->statuses & 0x810;
    }

    if ((actor->statusTimers[2] != 0) && (--actor->statusTimers[2] == 0)) {
        statuses |= actor->statuses & 0x1FF80000;
    }

    if ((actor->statusTimers[3] != 0) && (--actor->statusTimers[3] == 0)) {
        statuses |= actor->statuses & 0x2000;
    }

    if ((actor->statusTimers[4] != 0) && (--actor->statusTimers[4] == 0)) {
        statuses |= actor->statuses & 0x8000;
    }

    if ((actor->statusTimers[5] != 0) && (--actor->statusTimers[5] == 0)) {

        int hp;

        if (actor->statuses & 0x4000) {

            actor->statusTimers[5] = Battle_statusParams14TickInterval;
            hp = (actor->currentHp / 20) + Core_Random_NextBounded(6);

            if (hp <= 0) {
                hp = 1;
            }

            if (actor->currentHp < hp) {
                actor->currentHp = 0;
            } else {
                actor->currentHp -= hp;
            }

            ((void (*)(int actor_slot, int limb_tag, u_int encoded_amount, int label_flags))Battle_QueueActorValueLabel)(actor->actorId, 0, hp | 0x80000000, 0);

        } else if (actor->statuses & 0x20000) {

            actor->statusTimers[5] = Battle_statusParams17TickInterval;
            hp = 2;
            hp = (actor->maxHp / 20) - hp + Core_Random_NextAveraged(5);
            actor->currentHp += hp;

            if (actor->maxHp < actor->currentHp) {
                actor->currentHp = actor->maxHp;
            }

            for (i = 0; i < 6; ++i) {
                if (actor->limbs[i].maxHp != 0) {
                    int healing_dividend = hp;

                    if (hp < 0) {
                        healing_dividend = hp + 7;
                    }

                    actor->limbs[i].hp += (healing_dividend >> 3);

                    if (actor->limbs[i].maxHp < actor->limbs[i].hp) {
                        actor->limbs[i].hp = actor->limbs[i].maxHp;
                    }
                }
            }

            ((void (*)(int actor_slot, int limb_tag, u_int encoded_amount, int label_flags))Battle_QueueActorValueLabel)(actor->actorId, 0, hp, 0);
        }
    }

    if (actor->statuses & 0x10) {

        limb_recovered = 0;

        if (actor->mobilityLimb0 != 0) {
            limb_recovered = actor->limbs[actor->mobilityLimb0].hp >= 2;
        }

        if ((actor->mobilityLimb1 != 0) && (actor->limbs[actor->mobilityLimb1].hp >= 2)) {
            limb_recovered = 1;
        }

        if (limb_recovered != 0) {
            statuses |= 0x10;
        }
    }

    if (actor->actorId == 0) {
        if (actor->statuses & 0xF) {

            for (i = 0; i < 4; ++i) {
                if (actor->statuses & Battle_limbStatusFlags[i]) {

                    limb_recovered = 0;

                    if (actor->mobilityLimb0 != 0) {
                        limb_recovered = actor->limbs[i].hp >= 2;
                    }

                    if (limb_recovered != 0) {
                        statuses |= Battle_limbStatusFlags[i];
                    }
                }
            }
        }
    }

    if (statuses != 0) {
        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(actor->actorId, 0, statuses, 1);
        Battle_RemoveActorStatuses(statuses, actor);
    }
}

// decomp: BATTLE.PRG func_80088554 (src/BATTLE/BATTLE.PRG/146C.c:14560)
int Battle_UpdateActorVitals(void)
{
    int update_result;
    int i;
    int pending_event_result = 0;

    for (i = 0; i < 16; ++i) {

        Battle_Actor* battle_actor = Battle_actors[i];
        update_result = 0;

        if ((battle_actor != NULL) && ((battle_actor->roomSpawnIndex != 0x80) || (i == 0))) {

            int actor_state_flags = Actors_ActorStateFlags(i);

            if (actor_state_flags & 0x01000000) {

                if (((u_int (*)(int))Actors_IsActorActionAnimationBusy)(i) != 1) {

                    update_result = Battle_ResolveActorDefeat(0, i, 0);

                    if ((update_result == 0) && (i != 0)) {
                        if (Event_EvaluateRoomEventTriggers(0xC, 0, NULL, 0) == 1) {
                            update_result = 1;
                        } else {
                            update_result = 2;
                        }
                    }
                }

            } else if ((actor_state_flags & 0x01002001) == 1) {

                Battle_ActorStats* actor_stats = battle_actor->stats;

                if (actor_stats != NULL) {

                    if (actor_stats->actionRecoveryTimer > 35) {
                        actor_stats->actionRecoveryTimer -= 35;
                    } else if (actor_stats->actionRecoveryTimer != 0) {
                        actor_stats->actionRecoveryTimer = 0;
                    }

                    if ((Battle_vitalsTickCounter % 30 == 0) && (actor_stats->currentHp != 0)) {

                        if (battle_actor->weaponDrawn & 1) {

                            if (Battle_vitalsTickCounter % 120 == 0) {
                                if (actor_stats->risk != 0) {
                                    --actor_stats->risk;
                                }
                            }

                            if (Battle_vitalsTickCounter % 180 == 0) {

                                if (actor_stats->currentMp < actor_stats->maxMp) {
                                    ++actor_stats->currentMp;
                                }

                                if (battle_actor->weaponDrawn & 1) {

                                    if (actor_stats->weapon.currentPp != 0) {
                                        --actor_stats->weapon.currentPp;
                                    }

                                    if (actor_stats->shield.currentPp != 0) {
                                        --actor_stats->shield.currentPp;
                                    }
                                }
                            }

                            if (Battle_vitalsTickCounter % 240 == 0) {

                                if (actor_stats->currentHp < actor_stats->maxHp) {
                                    if (actor_stats->currentHp != 0) {
                                        ++actor_stats->currentHp;
                                    }
                                }

                                if (actor_stats->limbs[Battle_regeneratingLimb].maxHp != 0) {
                                    if (actor_stats->limbs[Battle_regeneratingLimb].hp
                                        < actor_stats->limbs[Battle_regeneratingLimb].maxHp) {
                                        ++actor_stats->limbs[Battle_regeneratingLimb].hp;
                                    }
                                }
                            }

                        } else {

                            if (!(actor_stats->statuses & 8) || (Battle_vitalsTickCounter % 120 == 0)) {

                                if (actor_stats->risk != 0) {
                                    --actor_stats->risk;
                                }
                            }

                            if (Battle_vitalsTickCounter % 60 == 0) {

                                if (actor_stats->currentMp < actor_stats->maxMp) {
                                    ++actor_stats->currentMp;
                                }

                                if (battle_actor->weaponDrawn & 1) {

                                    if (actor_stats->weapon.currentPp != 0) {
                                        --actor_stats->weapon.currentPp;
                                    }

                                    if (actor_stats->shield.currentPp != 0) {
                                        --actor_stats->shield.currentPp;
                                    }
                                }
                            }

                            if (Battle_vitalsTickCounter % 120 == 0) {

                                if (actor_stats->currentHp < actor_stats->maxHp) {
                                    if (actor_stats->currentHp != 0) {
                                        ++actor_stats->currentHp;
                                    }
                                }

                                if (actor_stats->limbs[Battle_regeneratingLimb].maxHp != 0) {
                                    if (actor_stats->limbs[Battle_regeneratingLimb].hp
                                        < actor_stats->limbs[Battle_regeneratingLimb].maxHp) {
                                        ++actor_stats->limbs[Battle_regeneratingLimb].hp;
                                    }
                                }
                            }
                        }
                    }

                    if (Battle_vitalsTickCounter % 60 == 0) {

                        if (actor_stats->currentHp != 0) {
                            Battle_UpdateActorStatusTimers(actor_stats);
                        }

                        if (actor_stats->currentHp == 0) {

                            if (battle_actor->retainAfterDefeat == 0) {
                                Battle_BeginActorDefeatAnimation(i);
                            } else {

                                update_result = Battle_ResolveActorDefeat(0, i, 0);

                                if ((update_result == 0) && (i != 0)) {
                                    if (Event_EvaluateRoomEventTriggers(0xC, 0, NULL, 0) == 1) {
                                        update_result = 1;
                                    } else {
                                        update_result = 2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (update_result != 0) {
            if (pending_event_result == 0) {
                pending_event_result = update_result;
            } else if (update_result == 1) {
                pending_event_result = 1;
            }
        }
    }

    Battle_vitalsTickCounter = (Battle_vitalsTickCounter + State_tickSpeed) % 720;

    if (Battle_vitalsTickCounter % 240 == 0) {
        Battle_regeneratingLimb = (Battle_regeneratingLimb + 1) % 6;
    }

    update_result = State_UpdateEventTimer();

    if (update_result != 0) {
        pending_event_result = update_result;
    }

    return pending_event_result;
}

// decomp: BATTLE.PRG func_80088B6C (src/BATTLE/BATTLE.PRG/146C.c:14743)
int Battle_UpdateRoomActorVitals(void) { return Battle_UpdateActorVitals(); }

// decomp: BATTLE.PRG func_80088B8C (src/BATTLE/BATTLE.PRG/146C.c:14745)
void Battle_UpdateTargetingRiskPenalty(void)
{
    if ((Field_controlMode == 2) && (Battle_actionFlowMode < 4) && (Battle_actionFlowMode != 0)) {
        if ((Battle_targetingRiskTimer == 0)
            && ((
                (Battle_actors[Battle_actionState->selection.primaryTarget.target.targetActor]->stats->statuses
                    & 8)
                != 0))
            && (Battle_actionState->selection.primaryTarget.targetsPosition == 0)) {
            if (Battle_actionState->selection.primaryTarget.target.targetActor == 0) {
                if (Battle_characterState->stats->risk < 100) {
                    ++Battle_characterState->stats->risk;
                }
            }
        }
        Battle_targetingRiskTimer = (Battle_targetingRiskTimer + State_tickSpeed) % 30;
    }
}

// decomp: BATTLE.PRG func_80088CA0 (src/BATTLE/BATTLE.PRG/146C.c:14764)
void Battle_ResetTargetingRiskPenaltyTimer(void) { Battle_targetingRiskTimer = 0; }

