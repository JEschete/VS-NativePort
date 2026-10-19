// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
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
int Battle_QueueApplicableStatusChanges(int arg0, Battle_HitResult* entity)
{
    int inflict = 0;
    int remove = 0;
    Battle_StatusEffectParams* params = &Battle_statusEffectParams[arg0];
    Battle_ActorStats* temp_a2 = Battle_actors[entity->target.targetActor]->stats;

    if (temp_a2->actorId != 128) {

        int temp_t0 = temp_a2->statuses;

        if (!(temp_t0 & params->blockingStatuses)) {
            remove = params->statusFlags & ~temp_a2->statusImmunities & (~temp_t0 | params->cancelledStatuses);
            entity->removeStatuses |= remove;
            inflict = params->cancelledStatuses & temp_a2->statuses;
            entity->inflictStatuses |= inflict;
        }
    }

    return remove | inflict;
}

// decomp: BATTLE.PRG _setInflictedStatuses (src/BATTLE/BATTLE.PRG/146C.c:11217)
int Battle_QueueStatusRemovalMask(int arg0, Battle_HitResult* entity)
{
    int ret = 0;
    Battle_StatusEffectParams* params = &Battle_statusEffectParams[arg0];

    if (Battle_actors[entity->target.targetActor]->stats->actorId != 128) {
        ret = params->statusFlags & Battle_actors[entity->target.targetActor]->stats->statuses;
        entity->inflictStatuses |= ret;
    }

    return ret;
}

// decomp: BATTLE.PRG func_80083998 (src/BATTLE/BATTLE.PRG/146C.c:12273)
int Battle_QueueStrengthDown(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(5, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_800839E8 (src/BATTLE/BATTLE.PRG/146C.c:12282)
int Battle_QueueIntelligenceDown(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(7, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083A38 (src/BATTLE/BATTLE.PRG/146C.c:12291)
int Battle_QueueAgilityDown(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(9, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083A88 (src/BATTLE/BATTLE.PRG/146C.c:12300)
int Battle_QueueEquipmentDown(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(19, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083AD8 (src/BATTLE/BATTLE.PRG/146C.c:12309)
int Battle_QueueHalfMovement(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(4, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083B28 (src/BATTLE/BATTLE.PRG/146C.c:12318)
int Battle_QueueStrengthUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(6, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083B78 (src/BATTLE/BATTLE.PRG/146C.c:12327)
int Battle_QueueIntelligenceUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(8, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083BC8 (src/BATTLE/BATTLE.PRG/146C.c:12336)
int Battle_QueueAgilityUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(10, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083C18 (src/BATTLE/BATTLE.PRG/146C.c:12345)
int Battle_QueueEquipmentUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(20, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083C68 (src/BATTLE/BATTLE.PRG/146C.c:12354)
int Battle_QueueQuicken(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(11, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083CB8 (src/BATTLE/BATTLE.PRG/146C.c:12363)
int Battle_QueueSilence(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(12, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083D08 (src/BATTLE/BATTLE.PRG/146C.c:12372)
int Battle_QueueMagicWard(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)),
    Battle_HitResult* arg2 __attribute__((unused)), int arg3 __attribute__((unused)),
    int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x12, arg2);
}

// decomp: BATTLE.PRG func_80083D2C (src/BATTLE/BATTLE.PRG/146C.c:12380)
int Battle_QueueRegeneration(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(17, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80083D7C (src/BATTLE/BATTLE.PRG/146C.c:12389)
int Battle_QueueBanish(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4)
{
    Battle_QueueApplicableStatusChanges(0x1D, arg2);
    if (arg4 != 0) {
        Battle_actors[arg2->target.targetActor]->stats->isAnalyzed = 1;
    }
}

// decomp: BATTLE.PRG func_80083DEC (src/BATTLE/BATTLE.PRG/146C.c:12399)
int Battle_QueueParalysis(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(13, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083E3C (src/BATTLE/BATTLE.PRG/146C.c:12408)
int Battle_QueuePoison(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(14, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083E8C (src/BATTLE/BATTLE.PRG/146C.c:12417)
int Battle_QueueNumbness(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(15, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083EDC (src/BATTLE/BATTLE.PRG/146C.c:12426)
int Battle_QueueCurse(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueApplicableStatusChanges(16, arg2) != 0) {
        arg2->statStatusEffect = 1;
    }
}

// decomp: BATTLE.PRG func_80083F2C (src/BATTLE/BATTLE.PRG/146C.c:12435)
int Battle_QueueAirAttackUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x15, arg2);
}

// decomp: BATTLE.PRG func_80083F50 (src/BATTLE/BATTLE.PRG/146C.c:12442)
int Battle_QueueFireAttackUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x16, arg2);
}

// decomp: BATTLE.PRG func_80083F74 (src/BATTLE/BATTLE.PRG/146C.c:12449)
int Battle_QueueEarthAttackUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x17, arg2);
}

// decomp: BATTLE.PRG func_80083F98 (src/BATTLE/BATTLE.PRG/146C.c:12456)
int Battle_QueueWaterAttackUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x18, arg2);
}

// decomp: BATTLE.PRG func_80083FBC (src/BATTLE/BATTLE.PRG/146C.c:12463)
int Battle_QueueAirDefenseUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x19, arg2);
}

// decomp: BATTLE.PRG func_80083FE0 (src/BATTLE/BATTLE.PRG/146C.c:12470)
int Battle_QueueFireDefenseUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x1A, arg2);
}

// decomp: BATTLE.PRG func_80084004 (src/BATTLE/BATTLE.PRG/146C.c:12477)
int Battle_QueueEarthDefenseUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x1B, arg2);
}

// decomp: BATTLE.PRG func_80084028 (src/BATTLE/BATTLE.PRG/146C.c:12484)
int Battle_QueueWaterDefenseUp(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    Battle_QueueApplicableStatusChanges(0x1C, arg2);
}

// decomp: BATTLE.PRG func_8008404C (src/BATTLE/BATTLE.PRG/146C.c:12491)
int Battle_QueueParalysisCure(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0xD, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_8008409C (src/BATTLE/BATTLE.PRG/146C.c:12500)
int Battle_QueuePoisonCure(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0xE, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_800840EC (src/BATTLE/BATTLE.PRG/146C.c:12509)
int Battle_QueueCurseCure(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0x10, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_8008413C (src/BATTLE/BATTLE.PRG/146C.c:12518)
int Battle_QueueNumbnessCure(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    if (Battle_QueueStatusRemovalMask(0xF, arg2) != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_8008418C (src/BATTLE/BATTLE.PRG/146C.c:12527)
int Battle_QueueDispel(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    arg2->inflictStatuses |=
        (Battle_actors[arg2->target.targetActor]->stats->statuses & 0x1FFE1FE0);
}

// decomp: BATTLE.PRG func_800841C8 (src/BATTLE/BATTLE.PRG/146C.c:12535)
int Battle_QueueNegativeStatusCure(Battle_ActionDefinition* arg0 __attribute__((unused)),
    Battle_HitResult* arg1 __attribute__((unused)), Battle_HitResult* arg2,
    int arg3 __attribute__((unused)), int arg4 __attribute__((unused)))
{
    int temp_v1 = Battle_actors[arg2->target.targetActor]->stats->statuses & 0xE000;
    arg2->inflictStatuses |= temp_v1;

    if (temp_v1 != 0) {
        arg2->statStatusEffect = 2;
    }
}

// decomp: BATTLE.PRG func_80086754 (src/BATTLE/BATTLE.PRG/146C.c:13532)
void Battle_RemoveActorStatuses(int arg0, Battle_ActorStats* arg1)
{
    int i;
    int statDecrease;
    int statIncrease;
    u_int temp_v1_2;

    arg1->statuses &= ~arg0;

    if (arg0 & 0x7E0) {
        if (!(arg1->statuses & 0x7E0)) {
            arg1->statusTimers[0] = 0;
        }
    }

    if (arg0 & 0x810) {
        if (!(arg1->statuses & 0x810)) {
            arg1->statusTimers[1] = 0;
        }
    }

    if (arg0 & 0x1FF80000) {
        if (!(arg1->statuses & 0x1FF80000)) {
            arg1->statusTimers[2] = 0;
        }
    }

    if (arg0 & 0x2000) {
        if (!(arg1->statuses & 0x2000)) {
            arg1->statusTimers[3] = 0;
        }
    }

    if (arg0 & 0x8000) {
        if (!(arg1->statuses & 0x8000)) {
            arg1->statusTimers[4] = 0;
        }
    }

    if (arg0 & 0x24000) {
        if (!(arg1->statuses & 0x24000)) {
            arg1->statusTimers[5] = 0;
        }
    }

    if (arg0 & 0x60) {
        arg1->strength = arg1->totalStrength;
    }

    if (arg0 & 0x180) {
        arg1->intelligence = arg1->totalIntelligence;
    }

    if (arg0 & 0x600) {
        arg1->agility = arg1->totalAgility;
    }

    if (arg0 & 0x10000) {
        arg1->strength = arg1->totalStrength;
        arg1->intelligence = arg1->totalIntelligence;
        arg1->agility = arg1->totalAgility;
    }

    if (arg0 & 0x8010) {
        if (arg0 & 0x10) {

            if ((arg1->mobilityLimb0 != 0) && (arg1->limbs[arg1->mobilityLimb0].maxHp != 0)
                && (arg1->limbs[arg1->mobilityLimb0].hp < 2)) {
                arg1->limbs[arg1->mobilityLimb0].hp = 2;
            }

            if ((arg1->mobilityLimb1 != 0) && (arg1->limbs[arg1->mobilityLimb1].maxHp != 0)
                && (arg1->limbs[arg1->mobilityLimb1].hp < 2)) {
                arg1->limbs[arg1->mobilityLimb1].hp = 2;
            }
        }

        if (!(arg1->statuses & 0x10)) {
            arg1->walkSpeed = arg1->baseWalkSpeed;
            arg1->runSpeed = arg1->baseRunSpeed;
        }

        Battle_Ai_SetActorVerticalMovementSteps(arg1->actorId, arg1->walkSpeed, arg1->runSpeed);
    }

    if ((arg0 & 0x800) && !(arg1->statuses & 0x8010)) {

        arg1->walkSpeed = arg1->baseWalkSpeed;
        arg1->runSpeed = arg1->baseRunSpeed;

        Battle_Ai_SetActorVerticalMovementSteps(arg1->actorId, arg1->walkSpeed, arg1->runSpeed);
    }

    if (arg0 & 0x180000) {
        if (arg1->weapon.blade.id != 0) {
            arg1->weapon.currentStr = arg1->weapon.baseStr;
            arg1->weapon.currentInt = arg1->weapon.baseInt;
        }

        if (arg1->shield.base.id != 0) {
            arg1->shield.currentStr = arg1->shield.baseStr;
            arg1->shield.currentInt = arg1->shield.baseInt;
        }

        for (i = 0; i < 6; ++i) {
            if (arg1->limbs[i].armor.armor.id != 0) {
                arg1->limbs[i].armor.currentStr = arg1->limbs[i].armor.baseStr;
                arg1->limbs[i].armor.currentInt = arg1->limbs[i].armor.baseInt;
            }
        }
    }

    temp_v1_2 = arg0 & 0x01E00000;

    if (temp_v1_2 != 0) {
        switch (temp_v1_2) {
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
            if (arg1->weapon.blade.id != 0) {

                if ((arg1->weapon.blade.affinities[statIncrease] < 50)
                    && (Core_Random_NextBounded(8) == 0)) {

                    ++arg1->weapon.blade.affinities[statIncrease];

                    if (arg1->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(240, statIncrease + 6, 1);
                    }
                }

                if ((arg1->weapon.blade.affinities[statDecrease] > -100)
                    && (Core_Random_NextBounded(16) == 0)) {

                    --arg1->weapon.blade.affinities[statDecrease];

                    if (arg1->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(240, statDecrease + 6, 0x80000001);
                    }
                }
            }

            arg1->weapon.affinityModifiers[statIncrease] = 0;
            arg1->weapon.affinityModifiers[statDecrease] = 0;

            Items_RebuildWeaponClassAffinity(arg1);
        }
    }

    temp_v1_2 = arg0 & 0x1E000000;

    if (temp_v1_2 != 0) {

        switch (temp_v1_2) {
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
            if (arg1->shield.base.id != 0) {

                if ((arg1->shield.base.affinities[statIncrease] < 50)
                    && (Core_Random_NextBounded(8) == 0)) {

                    ++arg1->shield.base.affinities[statIncrease];

                    if (arg1->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(241, statIncrease + 6, 1);
                    }
                }

                if ((arg1->shield.base.affinities[statDecrease] > -100)
                    && (Core_Random_NextBounded(0x10) == 0)) {

                    --arg1->shield.base.affinities[statDecrease];

                    if (arg1->actorId == 0) {
                        ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(241, statDecrease + 6, 0x80000001);
                    }
                }
            }

            arg1->shield.affinityModifiers[statIncrease] = 0;
            arg1->shield.affinityModifiers[statDecrease] = 0;

            Items_RebuildShieldClassAffinity(arg1);

            for (i = 0; i < 6; ++i) {
                if (arg1->limbs[i].armor.armor.id != 0) {

                    if ((arg1->limbs[i].armor.armor.affinities[statIncrease] < 50)
                        && (Core_Random_NextBounded(8) == 0)) {

                        ++arg1->limbs[i].armor.armor.affinities[statIncrease];

                        if (arg1->actorId == 0) {
                            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(arg1->limbs[i].nameIndex, statIncrease + 6, 1);
                        }
                    }

                    if ((arg1->limbs[i].armor.armor.affinities[statDecrease] > -100)
                        && (Core_Random_NextBounded(16) == 0)) {

                        --arg1->limbs[i].armor.armor.affinities[statDecrease];

                        if (arg1->actorId == 0) {
                            ((void (*)(int, int, int))Battle_QueueEquipmentStatChange)(
                                arg1->limbs[i].nameIndex, statDecrease + 6, 0x80000001);
                        }
                    }
                }

                arg1->limbs[i].armor.affinityModifiers[statIncrease] = 0;
                arg1->limbs[i].armor.affinityModifiers[statDecrease] = 0;

                Items_RebuildArmorClassAffinity(arg1, i);
            }
        }

        Battle_CommitEquipmentStatChanges();
    }
}

// decomp: BATTLE.PRG func_80086EA4 (src/BATTLE/BATTLE.PRG/146C.c:13801)
void Battle_ApplyWeaponElementEnchantment(Battle_ActorStats* arg0, int arg1, int arg2, int arg3)
{
    if (arg0->weapon.blade.id != 0) {
        arg0->weapon.classAffinityCurrent.affinities[0][arg1] += arg3;
        arg0->weapon.classAffinityCurrent.affinities[0][arg2] -= arg3;
        arg0->weapon.affinityModifiers[arg1] = arg3;
        arg0->weapon.affinityModifiers[arg2] = -arg3;
    }
}

// decomp: BATTLE.PRG func_80086EF4 (src/BATTLE/BATTLE.PRG/146C.c:13811)
void Battle_ApplyArmorElementEnchantment(Battle_ActorStats* arg0, int arg1, int arg2, int arg3)
{
    int i;

    if (arg0->shield.base.id != 0) {
        arg0->shield.classAffinityCurrent.affinities[0][arg1] += arg3;
        arg0->shield.classAffinityCurrent.affinities[0][arg2] -= arg3;
        arg0->shield.affinityModifiers[arg1] = arg3;
        arg0->shield.affinityModifiers[arg2] = -arg3;
    }

    for (i = 0; i < 6; ++i) {
        if (arg0->limbs[i].armor.armor.id != 0) {
            arg0->limbs[i].armor.classAffinityCurrent.affinities[0][arg1] += arg3;
            arg0->limbs[i].armor.classAffinityCurrent.affinities[0][arg2] -= arg3;
            arg0->limbs[i].armor.affinityModifiers[arg1] = arg3;
            arg0->limbs[i].armor.affinityModifiers[arg2] = -arg3;
        }
    }
}

// decomp: BATTLE.PRG func_80086FA8 (src/BATTLE/BATTLE.PRG/146C.c:13832)
void Battle_InflictActorStatuses(int arg0, Battle_ActorStats* arg1)
{
    int i;
    int var_a3;
    u_int temp_a0;

    arg1->statuses |= arg0;

    if (arg0 & 0x7E0) {
        arg1->statusTimers[0] = Battle_statusParams5Duration;
    }

    if (arg0 & 0x810) {
        arg1->statusTimers[1] = Battle_statusParams11Duration;
    }

    if (arg0 & 0x1FF80000) {
        arg1->statusTimers[2] = Battle_statusParams19Duration;
    }

    if (arg0 & 0x2000) {
        arg1->statusTimers[3] = Battle_statusParams13Duration;
    }

    if (arg0 & 0x8000) {
        arg1->statusTimers[4] = Battle_statusParams13Duration;
    }

    if (arg0 & 0x4000) {
        arg1->statusTimers[5] = Battle_statusParams14TickInterval;
    } else if (arg0 & 0x20000) {
        arg1->statusTimers[5] = Battle_statusParams17TickInterval;
    }

    if (arg0 & 0x20) {
        arg1->strength = arg1->totalStrength - (arg1->totalStrength / 10);
    }

    if (arg0 & 0x40) {
        arg1->strength = arg1->totalStrength + (arg1->totalStrength / 10);
    }

    if (arg0 & 0x80) {
        arg1->intelligence = arg1->totalIntelligence - (arg1->totalIntelligence / 10);
    }

    if (arg0 & 0x100) {
        arg1->intelligence = arg1->totalIntelligence + (arg1->totalIntelligence / 10);
    }

    if (arg0 & 0x200) {
        arg1->agility = arg1->totalAgility - (arg1->totalAgility / 10);
    }
    if (arg0 & 0x400) {
        arg1->agility = arg1->totalAgility + (arg1->totalAgility / 10);
    }

    if (arg0 & 0x10000) {
        arg1->strength = arg1->totalStrength - (arg1->totalStrength / 10);
        arg1->intelligence = arg1->totalIntelligence - (arg1->totalIntelligence / 10);
        arg1->agility = arg1->totalAgility - (arg1->totalAgility / 10);
    }

    if (arg0 & 0x8810) {

        if (arg1->statuses & 0x8010) {
            arg1->walkSpeed = (arg1->baseWalkSpeed >> 1);
            arg1->runSpeed = arg1->baseRunSpeed >> 1;
        } else if (arg1->statuses & 0x800) {
            arg1->walkSpeed = arg1->baseWalkSpeed + (arg1->baseWalkSpeed >> 2);
            arg1->runSpeed = arg1->baseRunSpeed + (arg1->baseRunSpeed >> 2);
        }

        Battle_Ai_SetActorVerticalMovementSteps(arg1->actorId, arg1->walkSpeed, arg1->runSpeed);
    }

    if (arg0 & 0x80000) {

        var_a3 = ((arg1->intelligence + arg1->weapon.currentInt) * 7) / 100;

        if (var_a3 < 0) {
            var_a3 = 0;
        }

        if (arg1->weapon.blade.id != 0) {
            arg1->weapon.currentStr -= var_a3;
        }

        if (arg1->shield.base.id != 0) {
            arg1->shield.currentStr -= var_a3;
        }

        for (i = 0; i < 6; ++i) {
            if (arg1->limbs[i].armor.armor.id != 0) {
                arg1->limbs[i].armor.currentStr -= var_a3;
            }
        }
    }

    if (arg0 & 0x100000) {

        var_a3 = ((arg1->intelligence + arg1->weapon.currentInt) * 7) / 100;

        if (var_a3 < 0) {
            var_a3 = 0;
        }

        if (arg1->weapon.blade.id != 0) {
            arg1->weapon.currentStr += var_a3;
        }

        if (arg1->shield.base.id != 0) {
            arg1->shield.currentStr += var_a3;
        }

        for (i = 0; i < 6; ++i) {
            if (arg1->limbs[i].armor.armor.id != 0) {
                arg1->limbs[i].armor.currentStr += var_a3;
            }
        }
    }

    if (arg0 & 0x20000000) {
        *(int*)&arg1->actionRecoveryTimer |= 0x20000;
    }

    temp_a0 = arg0 & 0x01E00000;

    if (temp_a0 != 0) {

        var_a3 = (arg1->intelligence + arg1->weapon.currentInt) / 2;

        if (var_a3 < 0) {
            var_a3 = 0;
        }

        switch (temp_a0) {
        case 0x200000:
            Battle_ApplyWeaponElementEnchantment(arg1, 1, 3, var_a3);
            break;
        case 0x400000:
            Battle_ApplyWeaponElementEnchantment(arg1, 2, 4, var_a3);
            break;
        case 0x800000:
            Battle_ApplyWeaponElementEnchantment(arg1, 3, 1, var_a3);
            break;
        case 0x01000000:
            Battle_ApplyWeaponElementEnchantment(arg1, 4, 2, var_a3);
            break;
        }
    }

    temp_a0 = arg0 & 0x01E000000;

    if (temp_a0) {

        var_a3 = (arg1->intelligence + arg1->weapon.currentInt) / 2;

        if (var_a3 < 0) {
            var_a3 = 0;
        }

        switch (temp_a0) {
        case 0x2000000:
            Battle_ApplyArmorElementEnchantment(arg1, 1, 3, var_a3);
            break;
        case 0x4000000:
            Battle_ApplyArmorElementEnchantment(arg1, 2, 4, var_a3);
            break;
        case 0x8000000:
            Battle_ApplyArmorElementEnchantment(arg1, 3, 1, var_a3);
            break;
        case 0x10000000:
            Battle_ApplyArmorElementEnchantment(arg1, 4, 2, var_a3);
            break;
        }
    }
}

// decomp: BATTLE.PRG func_80087578 (src/BATTLE/BATTLE.PRG/146C.c:14011)
void Battle_QueueActorStatChangeNumber(int arg0, int arg1, int arg2, int arg3)
{
    if ((Battle_actors[arg0]->roomSpawnIndex != 0x80) || (arg0 == 0)) {
        ((void (*)(int arg0, int arg1, u_int arg2, int arg3))Battle_QueueActorValueLabel)(arg0, arg1, arg2, arg3);
    }
}

// decomp: BATTLE.PRG _applyBattleAbilityEffect (src/BATTLE/BATTLE.PRG/146C.c:14018)
void Battle_ApplyActorActionResult(Battle_HitResult* arg0)
{
    int flag;
    u_int i;
    int targetActorId;
    Battle_ActorStats* actor;

    Battle_CommitEquipmentStatChanges();

    targetActorId = arg0->target.targetActor;
    actor = Battle_actors[targetActorId]->stats;

    if (arg0->target.impactKind == 4) {
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, 0, 2);
    }

    switch (arg0->hpEffect) {
    case 2:

        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->target.hp, 0);

        actor->currentHp += arg0->target.hp;

        if (actor->maxHp < actor->currentHp) {
            actor->currentHp = actor->maxHp;
        }

        for (i = 0; i < 6; ++i) {

            actor->limbs[i].hp += arg0->limbs[i].effects.opaque0;

            if (actor->limbs[i].maxHp < actor->limbs[i].hp) {
                actor->limbs[i].hp = actor->limbs[i].maxHp;
            }
        }

        break;

    case 3:
    case 1:

        Battle_QueueActorStatChangeNumber(
            targetActorId, arg0->target.targetLimb, arg0->target.hp | 0x80000000, 0);

        if (actor->currentHp > arg0->target.hp) {
            actor->currentHp -= arg0->target.hp;
        } else {
            actor->currentHp = 0;
        }

        for (i = 0; i < 6; ++i) {
            if (actor->limbs[i].hp > arg0->limbs[i].effects.opaque0) {
                actor->limbs[i].hp -= arg0->limbs[i].effects.opaque0;
            } else {
                actor->limbs[i].hp = 0;
            }
            if (actor->limbs[i].armor.currentDp > arg0->limbs[i].effects.opaque2) {
                actor->limbs[i].armor.currentDp -= arg0->limbs[i].effects.opaque2;
            } else {
                actor->limbs[i].armor.currentDp = 0;
            }
        }

        break;
    }

    switch (arg0->mpEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->target.mp, 1);

        actor->currentMp += arg0->target.mp;

        if (actor->maxMp < actor->currentMp) {
            actor->currentMp = actor->maxMp;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, arg0->target.targetLimb, arg0->target.mp | 0x80000000, 1);

        if (actor->currentMp > arg0->target.mp) {
            actor->currentMp -= arg0->target.mp;
        } else {
            actor->currentMp = 0;
        }
        break;
    }

    switch (arg0->riskEffect) {
    case 1:
        break;

    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->riskChange | 0x80000000, 8);

        if (actor->risk > arg0->riskChange) {
            actor->risk -= arg0->riskChange;
        } else {
            actor->risk = 0;
        }
        break;
    }

    switch (arg0->strengthEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->strengthChange, 5);

        actor->totalStrength += arg0->strengthChange;

        if (actor->totalStrength > 999) {
            actor->totalStrength = 999;
        }

        actor->strength += arg0->strengthChange;

        if (actor->strength > 999) {
            actor->strength = 999;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, arg0->target.targetLimb, arg0->strengthChange | 0x80000000, 5);

        if (actor->totalStrength > arg0->strengthChange) {
            actor->totalStrength -= arg0->strengthChange;
        } else {
            actor->totalStrength = 0;
        }

        if (actor->strength > arg0->strengthChange) {
            actor->strength -= arg0->strengthChange;
        } else {
            actor->strength = 0;
        }

        break;
    }

    switch (arg0->intelligenceEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->intelligenceChange, 6);

        actor->totalIntelligence += arg0->intelligenceChange;

        if (actor->totalIntelligence > 999) {
            actor->totalIntelligence = 999;
        }

        actor->intelligence += arg0->intelligenceChange;

        if (actor->intelligence > 999) {
            actor->intelligence = 999;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, arg0->target.targetLimb, arg0->intelligenceChange | 0x80000000, 6);

        if (actor->totalIntelligence > arg0->intelligenceChange) {
            actor->totalIntelligence -= arg0->intelligenceChange;
        } else {
            actor->totalIntelligence = 0;
        }

        if (actor->intelligence > arg0->intelligenceChange) {
            actor->intelligence -= arg0->intelligenceChange;
        } else {
            actor->intelligence = 0;
        }

        break;
    }

    switch (arg0->agilityEffect) {
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->agilityChange, 7);

        actor->totalAgility += arg0->agilityChange;

        if (actor->totalAgility > 999) {
            actor->totalAgility = 999;
        }

        actor->agility += arg0->agilityChange;

        if (actor->agility > 999) {
            actor->agility = 999;
        }

        break;

    case 1:
        Battle_QueueActorStatChangeNumber(
            targetActorId, arg0->target.targetLimb, arg0->agilityChange | 0x80000000, 7);

        if (actor->totalAgility > arg0->agilityChange) {
            actor->totalAgility -= arg0->agilityChange;
        } else {
            actor->totalAgility = 0;
        }

        if (actor->agility > arg0->agilityChange) {
            actor->agility -= arg0->agilityChange;
        } else {
            actor->agility = 0;
        }
        break;
    }

    switch (arg0->maxHpEffect) {
    case 1:
        break;
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->maxHpChange, 3);

        actor->maxHp += arg0->maxHpChange;

        if (actor->maxHp > 999) {
            actor->maxHp = 999;
        }
        break;
    }

    switch (arg0->maxMpEffect) {
    case 1:
        break;
    case 2:
        Battle_QueueActorStatChangeNumber(targetActorId, arg0->target.targetLimb, arg0->maxMpChange, 4);

        actor->maxMp += arg0->maxMpChange;

        if (actor->maxMp > 999) {
            actor->maxMp = 999;
        }
        break;
    }

    switch (arg0->weaponDpEffect) {
    case 1:
        break;
    case 2:
        actor->weapon.currentDp += arg0->weaponDpChange;

        if (actor->weapon.maxDp < actor->weapon.currentDp) {
            actor->weapon.currentDp = actor->weapon.maxDp;
        }
        break;
    }

    switch (arg0->weaponPpEffect) {
    case 2:
        actor->weapon.currentPp += arg0->weaponPpChange;

        if (actor->weapon.maxPp < actor->weapon.currentPp) {
            actor->weapon.currentPp = actor->weapon.maxPp;
        }

        break;

    case 1:
        if (actor->weapon.currentPp > arg0->weaponPpChange) {
            actor->weapon.currentPp -= arg0->weaponPpChange;
        } else {
            actor->weapon.currentPp = 0;
        }

        break;
    }

    switch (arg0->armorDpEffect) {
    case 1:
        if (actor->shield.currentDp > arg0->armorDpChange) {
            actor->shield.currentDp -= arg0->armorDpChange;
        } else {
            actor->shield.currentDp = 0;
        }

        if (actor->limbs[arg0->target.targetLimb].armor.currentDp > arg0->armorDpChange) {
            actor->limbs[arg0->target.targetLimb].armor.currentDp -= arg0->armorDpChange;
        } else {
            actor->limbs[arg0->target.targetLimb].armor.currentDp = 0;
        }

        break;
    }

    switch (arg0->shieldPpEffect) {
    case 1:
        if (actor->shield.currentPp > arg0->shieldPpChange) {
            actor->shield.currentPp -= arg0->shieldPpChange;
        } else {
            actor->shield.currentPp = 0;
        }

        break;
    }

    flag = 1;

    if (arg0->inflictStatuses != 0) {

        for (i = 0; i < 32; ++i) {

            int status = arg0->inflictStatuses & flag;

            if ((status != 0) && (actor->statuses & flag)) {

                if (targetActorId != 0) {

                    if (actor->currentHp != 0) {
                        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(targetActorId, 0, status, 1);
                    }

                } else {
                    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, status, 1);
                }

                Battle_RemoveActorStatuses(arg0->inflictStatuses & flag, actor);
            }

            flag <<= 1;
        }
    }

    flag = 1;

    if (arg0->removeStatuses != 0) {

        for (i = 0; i < 32; ++i) {

            int status = arg0->removeStatuses & flag;

            if ((status != 0) && !(actor->statuses & flag)) {

                if (targetActorId != 0) {

                    if (actor->currentHp != 0) {
                        ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(targetActorId, 0, status, 0);
                    }

                } else {
                    ((void (*)(int, int, int, int))Battle_QueueActorStatusLabels)(0, 0, status, 0);
                }

                Battle_InflictActorStatuses(arg0->removeStatuses & flag, actor);
            }

            flag <<= 1;
        }
    }
}

// decomp: BATTLE.PRG func_80087EF4 (src/BATTLE/BATTLE.PRG/146C.c:14376)
void Battle_UpdateActorStatusTimers(Battle_ActorStats* actor)
{
    int var_a0_2;
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

            ((void (*)(int arg0, int arg1, u_int arg2, int arg3))Battle_QueueActorValueLabel)(actor->actorId, 0, hp | 0x80000000, 0);

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
                    int var_v1 = hp;

                    if (hp < 0) {
                        var_v1 = hp + 7;
                    }

                    actor->limbs[i].hp += (var_v1 >> 3);

                    if (actor->limbs[i].maxHp < actor->limbs[i].hp) {
                        actor->limbs[i].hp = actor->limbs[i].maxHp;
                    }
                }
            }

            ((void (*)(int arg0, int arg1, u_int arg2, int arg3))Battle_QueueActorValueLabel)(actor->actorId, 0, hp, 0);
        }
    }

    if (actor->statuses & 0x10) {

        var_a0_2 = 0;

        if (actor->mobilityLimb0 != 0) {
            var_a0_2 = actor->limbs[actor->mobilityLimb0].hp >= 2;
        }

        if ((actor->mobilityLimb1 != 0) && (actor->limbs[actor->mobilityLimb1].hp >= 2)) {
            var_a0_2 = 1;
        }

        if (var_a0_2 != 0) {
            statuses |= 0x10;
        }
    }

    if (actor->actorId == 0) {
        if (actor->statuses & 0xF) {

            for (i = 0; i < 4; ++i) {
                if (actor->statuses & Battle_limbStatusFlags[i]) {

                    var_a0_2 = 0;

                    if (actor->mobilityLimb0 != 0) {
                        var_a0_2 = actor->limbs[i].hp >= 2;
                    }

                    if (var_a0_2 != 0) {
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
    int var_s1;
    int i;
    int var_s5 = 0;

    for (i = 0; i < 16; ++i) {

        Battle_Actor* temp_s3 = Battle_actors[i];
        var_s1 = 0;

        if ((temp_s3 != NULL) && ((temp_s3->roomSpawnIndex != 0x80) || (i == 0))) {

            int temp_v0 = Actors_ActorStateFlags(i);

            if (temp_v0 & 0x01000000) {

                if (((u_int (*)(int))Actors_IsActorActionAnimationBusy)(i) != 1) {

                    var_s1 = Battle_ResolveActorDefeat(0, i, 0);

                    if ((var_s1 == 0) && (i != 0)) {
                        if (Event_EvaluateRoomEventTriggers(0xC, 0, NULL, 0) == 1) {
                            var_s1 = 1;
                        } else {
                            var_s1 = 2;
                        }
                    }
                }

            } else if ((temp_v0 & 0x01002001) == 1) {

                Battle_ActorStats* temp_s0 = temp_s3->stats;

                if (temp_s0 != NULL) {

                    if (temp_s0->actionRecoveryTimer > 35) {
                        temp_s0->actionRecoveryTimer -= 35;
                    } else if (temp_s0->actionRecoveryTimer != 0) {
                        temp_s0->actionRecoveryTimer = 0;
                    }

                    if ((Battle_vitalsTickCounter % 30 == 0) && (temp_s0->currentHp != 0)) {

                        if (temp_s3->weaponDrawn & 1) {

                            if (Battle_vitalsTickCounter % 120 == 0) {
                                if (temp_s0->risk != 0) {
                                    --temp_s0->risk;
                                }
                            }

                            if (Battle_vitalsTickCounter % 180 == 0) {

                                if (temp_s0->currentMp < temp_s0->maxMp) {
                                    ++temp_s0->currentMp;
                                }

                                if (temp_s3->weaponDrawn & 1) {

                                    if (temp_s0->weapon.currentPp != 0) {
                                        --temp_s0->weapon.currentPp;
                                    }

                                    if (temp_s0->shield.currentPp != 0) {
                                        --temp_s0->shield.currentPp;
                                    }
                                }
                            }

                            if (Battle_vitalsTickCounter % 240 == 0) {

                                if (temp_s0->currentHp < temp_s0->maxHp) {
                                    if (temp_s0->currentHp != 0) {
                                        ++temp_s0->currentHp;
                                    }
                                }

                                if (temp_s0->limbs[Battle_regeneratingLimb].maxHp != 0) {
                                    if (temp_s0->limbs[Battle_regeneratingLimb].hp
                                        < temp_s0->limbs[Battle_regeneratingLimb].maxHp) {
                                        ++temp_s0->limbs[Battle_regeneratingLimb].hp;
                                    }
                                }
                            }

                        } else {

                            if (!(temp_s0->statuses & 8) || (Battle_vitalsTickCounter % 120 == 0)) {

                                if (temp_s0->risk != 0) {
                                    --temp_s0->risk;
                                }
                            }

                            if (Battle_vitalsTickCounter % 60 == 0) {

                                if (temp_s0->currentMp < temp_s0->maxMp) {
                                    ++temp_s0->currentMp;
                                }

                                if (temp_s3->weaponDrawn & 1) {

                                    if (temp_s0->weapon.currentPp != 0) {
                                        --temp_s0->weapon.currentPp;
                                    }

                                    if (temp_s0->shield.currentPp != 0) {
                                        --temp_s0->shield.currentPp;
                                    }
                                }
                            }

                            if (Battle_vitalsTickCounter % 120 == 0) {

                                if (temp_s0->currentHp < temp_s0->maxHp) {
                                    if (temp_s0->currentHp != 0) {
                                        ++temp_s0->currentHp;
                                    }
                                }

                                if (temp_s0->limbs[Battle_regeneratingLimb].maxHp != 0) {
                                    if (temp_s0->limbs[Battle_regeneratingLimb].hp
                                        < temp_s0->limbs[Battle_regeneratingLimb].maxHp) {
                                        ++temp_s0->limbs[Battle_regeneratingLimb].hp;
                                    }
                                }
                            }
                        }
                    }

                    if (Battle_vitalsTickCounter % 60 == 0) {

                        if (temp_s0->currentHp != 0) {
                            Battle_UpdateActorStatusTimers(temp_s0);
                        }

                        if (temp_s0->currentHp == 0) {

                            if (temp_s3->retainAfterDefeat == 0) {
                                Battle_BeginActorDefeatAnimation(i);
                            } else {

                                var_s1 = Battle_ResolveActorDefeat(0, i, 0);

                                if ((var_s1 == 0) && (i != 0)) {
                                    if (Event_EvaluateRoomEventTriggers(0xC, 0, NULL, 0) == 1) {
                                        var_s1 = 1;
                                    } else {
                                        var_s1 = 2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (var_s1 != 0) {
            if (var_s5 == 0) {
                var_s5 = var_s1;
            } else if (var_s1 == 1) {
                var_s5 = 1;
            }
        }
    }

    Battle_vitalsTickCounter = (Battle_vitalsTickCounter + State_tickSpeed) % 720;

    if (Battle_vitalsTickCounter % 240 == 0) {
        Battle_regeneratingLimb = (Battle_regeneratingLimb + 1) % 6;
    }

    var_s1 = State_UpdateEventTimer();

    if (var_s1 != 0) {
        var_s5 = var_s1;
    }

    return var_s5;
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

