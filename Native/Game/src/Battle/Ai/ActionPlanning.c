// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Action.h"
#include "Battle/ActionResolution.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/ConditionSnapshot.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/PlanningPrimitives.h"
#include "Battle/BattleTypes.h"
#include "Battle/Damage.h"
#include "Battle/Hit.h"
#include "Battle/StatusEffects.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Gimmicks.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"
#include "State/Flags.h"

// decomp: D_80069BBC (D_80069BBC@BATTLE/BATTLE.PRG)
u_int Battle_Ai_kTimedStatusFlags[344]; /* contents: module image (ModuleData.c) */

// decomp: D_800F58E8 (D_800F58E8@BATTLE/BATTLE.PRG)
Battle_ActorStats* Battle_Ai_planningStatsSnapshot;

// decomp: D_800F58EC (D_800F58EC@BATTLE/BATTLE.PRG)
Battle_Ai_CandidateEntryView (*Battle_Ai_planningPassCandidates)[16];

// decomp: D_800F58F0 (D_800F58F0@BATTLE/BATTLE.PRG)
int Battle_Ai_fallbackCandidateCount;

// decomp: D_800F58F4 (D_800F58F4@BATTLE/BATTLE.PRG)
int Battle_Ai_unusedPlanCounter;

// decomp: D_800F58F8 (D_800F58F8@BATTLE/BATTLE.PRG)
Battle_Ai_CandidateEntryView* Battle_Ai_fallbackCandidates;

// decomp: D_800F5900 (D_800F5900@BATTLE/BATTLE.PRG)
Battle_Ai_ReactionRecord* Battle_Ai_planningWorkspace;

// decomp: D_800F5904 (D_800F5904@BATTLE/BATTLE.PRG)
u_char (*Battle_Ai_candidateExclusions)[256];

// decomp: BATTLE.PRG func_800D85D8 (src/BATTLE/BATTLE.PRG/6FC00.c:515)
int Battle_Ai_RefreshActionCandidateReadiness(Battle_Ai_CandidateReadinessView* state)
{
    int i, changed = 0;
    unsigned char value;
    Battle_Ai_CandidateEntryView* entry;
    Battle_Ai_CandidateEntryView* candidate;
    Battle_Actor* actor;
    for (i = 0; i < state->candidateCount; i++) {
        entry = state->candidateOrder[i];
        if (entry->weight && entry->threshold <= Battle_Ai_roomAiState->readinessLevel) {
            if (!Battle_Ai_CanAiActionAffectActor(state, entry, entry->targetId)
                || !Battle_Ai_RefreshAiActionUsability(state, entry->row, entry->column)) {
                entry->weight = 0;
            }
        }
    }
    if (state->candidateCount > 0 && !state->candidateOrder[0]->weight) {
        changed = 1;
        for (i = 1; i < state->candidateCount; i++) {
            candidate = state->candidateOrder[i];
            if (candidate->weight) {
                state->candidateOrder[0] = candidate;
                Battle_Ai_SelectActionMovementMode(state);
                goto done;
            }
        }
        state->candidateCount = 0;
    }
done:
    if (!state->candidateCount) {
        actor = Battle_actors[0];
        while (actor) {
            if ((state->relations[actor->id].opaque0 >> 6) == 3) {
                break;
            }
            actor = actor->next;
        }
        value = 2;
        if (!actor) {
            value = 3;
        }
    } else {
        value = Battle_Ai_roomAiState->readinessLevel + 30 < state->candidateOrder[0]->threshold;
    }
    state->actionStatus = value;
    return changed;
}

// decomp: BATTLE.PRG func_800DC888 (src/BATTLE/BATTLE.PRG/74088.c:473)
void Battle_Ai_ApplySimulatedActionResults(Battle_Ai_ActionTiming* simulated_actor_rows)
{
    int i;
    Battle_Ai_ActionTiming* row;

    if (Battle_Ai_planningWorkspace->opaque4.opaque40 == 0) {
        Battle_ActionDefinition* action = &Battle_actionDefinitions[Battle_Ai_planningWorkspace->opaque0];

        row = &simulated_actor_rows[Battle_Ai_planningWorkspace->opaque4.opaque0];

        switch (action->type) {
        case Battle_ActionType_Spell:
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque8, &row->opaqueA, action->cost, 1);
            break;

        case Battle_ActionType_BreakArt:
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque4, &row->opaque6, action->cost, 1);
            break;

        case Battle_ActionType_BaseAttack:
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque10, &row->opaque12, action->cost, 1);
            break;
        }
    }

    for (i = -1; i < Battle_Ai_planningWorkspace->opaque4A; ++i) {
        Battle_Ai_EngagementActor* entry = i < 0 ? &Battle_Ai_planningWorkspace->opaque4 : &Battle_Ai_planningWorkspace->opaque4C[i];

        if (entry->opaque40 == 0) {
            row = &simulated_actor_rows[entry->opaque0];
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque4, &row->opaque6, entry->opaque4, entry->opaque1CBit0);
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque8, &row->opaqueA, entry->opaque6, entry->opaque1CBit2);
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque10, &row->opaque12, entry->opaque38, entry->opaque1EBit2);
            Battle_Ai_ApplySimulatedStatDelta(&row->opaque14, &row->opaque16, entry->opaque3C, entry->opaque1EBit6);
            row->opaqueC |= entry->opaque14;
            row->opaqueC &= ~entry->opaque18;
        }
    }
}

// decomp: BATTLE.PRG func_800DCAA0 (src/BATTLE/BATTLE.PRG/74088.c:513)
void Battle_Ai_SimulateActionCandidate(
    Battle_Ai_ActorAiState* ai_state, Battle_Ai_ActionTiming* simulated_actor_rows, Battle_Ai_ActionCandidate* candidate, int simulation_mode)
{
    SVECTOR simulated_source_position;
    SVECTOR target_position;
    int target_actor_slot = candidate->targetId;
    int action_available;

    if (simulation_mode != 3) {
        action_available =
            Battle_PreviewActionAgainstLimb(candidate->action, ai_state->actorId, target_actor_slot, candidate->targetLimb, Battle_Ai_planningWorkspace);
    } else {
        Actors_ActorRecord* actor = Actors_actorRecords[target_actor_slot];
        target_position.vx = actor->instance.position.vx;
        target_position.vz = actor->instance.position.vz;
        target_position.vy = actor->instance.position.vy - candidate->opaqueCBit7;
        *(int*)&simulated_source_position.vx = *(int*)&target_position.vx;
        simulated_source_position.vz = target_position.vz + Core_Math_SquareRootInteger(candidate->distanceSquared);
        action_available = ((int (*)(int, int, int, int, SVECTOR*, SVECTOR*, void*))Battle_PreviewActionAgainstActor)(
            candidate->action, ai_state->actorId, target_actor_slot, candidate->targetLimb, &simulated_source_position, &target_position, Battle_Ai_planningWorkspace);
    }
    if (action_available != 0) {
        Battle_Ai_ApplySimulatedActionResults(simulated_actor_rows);
    }
}

// decomp: BATTLE.PRG func_800DCBD8 (src/BATTLE/BATTLE.PRG/74088.c:539)
void Battle_Ai_UpdateCandidateHitWeight(Battle_Ai_ActionCandidate* candidate)
{
    int i;
    int threshold = 0;
    int id = candidate->targetId;

    for (i = 0; i < Battle_Ai_planningWorkspace->opaque4A; ++i) {
        Battle_Ai_EngagementActor* entry = &Battle_Ai_planningWorkspace->opaque4C[i];
        if ((entry->opaque40 == 0) && (id == Battle_Ai_planningWorkspace->opaque4C[i].opaque0)) {
            threshold = Battle_Ai_planningWorkspace->opaque4C[i].opaque2;
            break;
        }
    }

    candidate->weight = threshold + (threshold >> 2) + (threshold >> 5);
}

// decomp: BATTLE.PRG _getStatusScore (src/BATTLE/BATTLE.PRG/74088.c:565)
int Battle_Ai_ActorStatusConditionScore(Battle_Ai_ActionTiming* stats)
{
    static const signed char Battle_Ai_kStatusScores[] = { -51, -39, -39, -39, -26, -51, 50, -13, 12,
        -13, 12, 25, -51, -26, -45, -26, -39, 76, 19, -13, 12, 12, 12, 12, 12, 12, 12, 12,
        12, 0, 0, 0 };
    int score = 0;
    int i;
    int noCost = Battle_Ai_actorAiStates[stats->opaque0Bit0]->actionCostEnabled == 0;

    for (i = 0; i < 32; ++i) {
        if (stats->opaqueC & (1 << i) && (i != 12 || !noCost)) {
            score += Battle_Ai_kStatusScores[i];
        }
    }

    if (stats->opaque4 == 0) {
        score -= 128;
    }

    if (stats->opaque4 * 128 < stats->opaque6 * 44) {
        score -= 84;
    }

    return score;
}

// decomp: BATTLE.PRG _getHpRatio (src/BATTLE/BATTLE.PRG/74088.c:590)
int Battle_Ai_ActorHpRatio(Battle_Ai_ActionTiming* stats, int id)
{
    if (stats[id].opaque6 == 0) {
        return 0;
    }

    return stats[id].opaque4 * 128 / stats[id].opaque6;
}

// decomp: BATTLE.PRG _getMpRatio (src/BATTLE/BATTLE.PRG/74088.c:598)
int Battle_Ai_ActorMpRatio(Battle_Ai_ActionTiming* stats, int id)
{
    if (stats[id].opaqueA == 0) {
        return 0;
    }

    return stats[id].opaque8 * 128 / stats[id].opaqueA;
}

// decomp: BATTLE.PRG _getExtraRatio (src/BATTLE/BATTLE.PRG/74088.c:606)
int Battle_Ai_ActorEquipmentDpRatio(Battle_Ai_ActionTiming* stats, int id)
{
    int ratio = 0;

    if (stats[id].opaque12 != 0) {
        ratio = stats[id].opaque10 * 128 / stats[id].opaque12;
    }

    if (stats[id].opaque16 != 0) {
        ratio += stats[id].opaque14 * 128 / stats[id].opaque16;
    }

    return ratio;
}

// decomp: BATTLE.PRG func_800DCC94 (src/BATTLE/BATTLE.PRG/74088.c:620)
void Battle_Ai_EvaluateActionCandidate(
    Battle_Ai_ActorAiState* ai_state, Battle_Ai_ActionCandidate* candidate, int simulation_mode, Battle_ActionDefinition* action_definition)
{
    int special = 0;
    Battle_ActionAiFlags flags = { 0 };
    Battle_Actor* actor;
    Battle_Ai_ActionTiming* stats;

    if (action_definition != NULL) {
        flags = action_definition->aiFlags;
    }

    if (flags.rarelyChosen && (Core_Random_Rand() & 0x7F) < 115) {
        candidate->opaque0 = 0x80000000;
        return;
    }

    candidate->opaque0 = 0;
    candidate->weight = 128;
    stats = (Battle_Ai_ActionTiming*)((uintptr_t)Core_ScratchBase() + 0x18C);
    Battle_Ai_CaptureActorConditionTable(stats);

    if (ai_state->settingsModeLow && flags.opaque0Bit4) {
        special = 1;
        candidate->weight = 40;
    } else if (simulation_mode != 0) {
        Battle_Ai_SimulateActionCandidate(ai_state, stats, candidate, simulation_mode);
        Battle_Ai_UpdateCandidateHitWeight(candidate);
    }

    for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
        int id = actor->id;
        int kind = ai_state->relations[id].classification;
        int value;

        if (kind == 1) {
            continue;
        }

        value = Battle_Ai_ScoreActorCondition(Battle_Ai_ActorStatusConditionScore(&stats[id]), Battle_Ai_ActorHpRatio(stats, id),
            Battle_Ai_ActorMpRatio(stats, id), Battle_Ai_ActorEquipmentDpRatio(stats, id));

        if (kind != 2) {
            value = -value;
        }

        candidate->opaque0 += value;
    }

    if (special) {
        ++candidate->opaque0;
    }

    candidate->opaque0 = candidate->opaque0 * 128 + candidate->weight;
}

// decomp: BATTLE.PRG func_800DD604 (src/BATTLE/BATTLE.PRG/74088.c:899)
void Battle_Ai_SelectFallbackAction(Battle_Ai_ActorAiState* ai_state)
{
    u_int best = 0;
    int row;
    int col;
    Battle_Ai_ActionCandidate* choice = &ai_state->currentChoice;
    Battle_ActorStats* stats = ai_state->stats;

    for (row = 0; row < 6; ++row) {
        for (col = 0; col < 4; ++col) {
            int action = stats->limbActions[row][col].actionId;
            Battle_Ai_ActionSlotMetadata* config = &ai_state->actionSlots[row][col];
            Battle_Ai_CandidateFlags flags;

            if (!Battle_Ai_RefreshAiActionUsability(ai_state, row, col) || !config->special
                || best >= config->rangeSquared) {
                continue;
            }

            best = config->rangeSquared;
            choice->distanceSquared = best;
            choice->column = col;
            choice->row = row;
            choice->action = action;
            choice->opaque10.opaque0Bit1 = 0;
            flags = choice->opaque10;
            flags.opaque0Bit2 = Battle_actionDefinitions[action].aiFlags.opaque1Bit4;
            choice->weight = 128;
            flags.opaque0Bit0 = 0;
            flags.opaque0Bit3 = 0;
            choice->opaque10 = flags;
        }
    }

    if (best == 0) {
        choice->action = 0;
    }
}

// decomp: BATTLE.PRG func_800DD918 (src/BATTLE/BATTLE.PRG/74088.c:1007)
void Battle_Ai_EnumerateActionCandidates(Battle_Ai_ActorAiState* ai_state, int readiness_threshold)
{
    Battle_Ai_ActionCandidate* list = (Battle_Ai_ActionCandidate*)((uintptr_t)Core_ScratchBase() + 0x30C);
    Battle_Ai_ActionCandidate* candidate;
    int pass;
    int row;
    int col;
    int limb;
    int part;
    int last;
    Battle_Actor* actor;
    Battle_Ai_ActorAiState* target;

    Core_Memory_ClearAlignedWords(list, sizeof *list);
    list[0].distanceSquared = 256;
    list[0].threshold = readiness_threshold;
    Battle_Ai_EvaluateActionCandidate(ai_state, list, 0, NULL);
    Battle_Ai_CopyActionCandidate(&list[1], list);
    list[1].threshold = readiness_threshold;
    candidate = &list[1];

    for (row = 0; row < 6; ++row) {
        for (col = 0; col < 4; ++col) {
            int action = ai_state->stats->limbActions[row][col].actionId;
            Battle_Ai_ActionSlotMetadata* config = &ai_state->actionSlots[row][col];
            Battle_ActionDefinition* info = &Battle_actionDefinitions[action];
            int flag;

            if (action == 0 || !config->opaque0Bit7) {
                continue;
            }

            list[1].column = col;
            list[1].row = row;
            list[1].action = action;
            flag = info->aiFlags.radialArea;
            list[1].opaque10.opaque0Bit1 = info->aiFlags.shapedArea | flag;
            list[1].opaque10.opaque0Bit2 = info->aiFlags.opaque1Bit4;
            list[1].opaque10.opaque0Bit3 = config->opaque0Bit2;
            list[1].opaque10.opaque0Bit6 = config->radius;
            list[1].distanceSquared = config->rangeSquared;

            for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
                u_int id = actor->id;

                if (Battle_Ai_actorAiStates[id]->isDead) {
                    continue;
                }

                list[1].targetId = id;
                list[1].opaque10.opaque0Bit4 = Battle_Ai_IsActionHarmfulToActor(action, id);
                list[1].opaque10.opaque0Bit5 = Battle_Ai_IsActionBeneficialToActor(action, id);
                list[1].opaque10.opaque0Bit0 = config->longRange && list[1].opaque10.opaque0Bit4;

                if (!Battle_Ai_CanAiActionAffectActor(ai_state, candidate, id) || Battle_Ai_candidateExclusions[id][action]) {
                    continue;
                }

                switch (config->kind) {
                case 1:
                    Battle_Ai_EvaluateActionCandidate(ai_state, candidate, 1, info);
                    Battle_Ai_CacheFallbackActionCandidate(candidate, info);

                    if (list[1].opaque0 > list[0].opaque0) {
                        Battle_Ai_AppendActionCandidate(ai_state, candidate);
                    }
                    break;

                case 2:
                    last = -1;
                    Battle_Ai_CopyActionCandidate(&list[2], list);
                    Battle_Ai_CopyActionCandidate(&list[3], list);

                    for (limb = 0; limb < 6; ++limb) {
                        int chance;

                        if ((actor->stats->limbs + limb)->maxHp == 0) {
                            continue;
                        }

                        last = limb;
                        list[1].targetLimb = last;
                        Battle_Ai_EvaluateActionCandidate(ai_state, candidate, 2, info);
                        chance = list[1].weight;

                        if ((Core_Random_Rand() & 0x7F) < chance && list[1].opaque0 > list[2].opaque0) {
                            Battle_Ai_CopyActionCandidate(&list[2], candidate);
                        }

                        if (list[1].opaque0 > list[3].opaque0) {
                            Battle_Ai_CopyActionCandidate(&list[3], &list[1]);
                        }
                    }

                    part = Core_Random_Rand();
                    part = (part & 3) + ((part >> 2) & 1) + ((part >> 3) & 1);

                    if ((actor->stats->limbs + part)->maxHp == 0 && last >= 0) {
                        part = last;
                    }

                    if (part >= 0) {
                        list[1].targetLimb = part;
                        Battle_Ai_CacheFallbackActionCandidate(&list[1], info);
                    }

                    if (list[2].opaque0 > list[0].opaque0) {
                        Battle_Ai_AppendActionCandidate(ai_state, &list[2]);
                    } else if (list[0].opaque0 < list[3].opaque0) {
                        Battle_Ai_AppendActionCandidate(ai_state, &list[3]);
                    }
                    break;

                case 3:
                    target = Battle_Ai_actorAiStates[actor->id];
                    Battle_Ai_CopyActionCandidate(&list[2], list);
                    Battle_Ai_CopyActionCandidate(&list[3], list);
                    list[1].opaqueCBit7 = 0;

                    for (pass = 0; pass < 1; ++pass) {
                        int chance;

                        list[1].opaqueCBit7 = ABS(target->pointHeight);
                        Battle_Ai_EvaluateActionCandidate(ai_state, candidate, 3, info);
                        chance = list[1].weight;

                        if ((Core_Random_Rand() & 0x7F) < chance && list[1].opaque0 > list[2].opaque0) {
                            Battle_Ai_CopyActionCandidate(&list[2], candidate);
                        }

                        if (list[1].opaque0 > list[3].opaque0) {
                            Battle_Ai_CopyActionCandidate(&list[3], &list[1]);
                        }
                    }

                    Battle_Ai_CacheFallbackActionCandidate(&list[1], info);

                    if (list[2].opaque0 > list[0].opaque0) {
                        Battle_Ai_AppendActionCandidate(ai_state, &list[2]);
                    } else if (list[0].opaque0 < list[3].opaque0) {
                        Battle_Ai_AppendActionCandidate(ai_state, &list[3]);
                    }
                    break;
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_800DE030 (src/BATTLE/BATTLE.PRG/74088.c:1167)
void Battle_Ai_AdvancePlanningSimulation(int simulation_time_step)
{
    Battle_Actor* actor;
    int ticks = (simulation_time_step >> 5) + (simulation_time_step >> 9);

    Battle_Ai_roomAiState->readinessLevel += simulation_time_step;

    for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
        Battle_ActorStats* stats = actor->stats;
        int statuses = stats->statuses;
        Battle_Ai_ActorAiState* state;
        int i;
        int limb;
        int amount;
        int row;
        int column;

        if (Battle_Ai_roomAiState->readinessLevel > 2880) {
            for (limb = 0; limb < 6; ++limb) {
                Battle_LimbStats* limbs = &stats->limbs[limb];

                limbs->hp += 2;

                if (limbs->hp > limbs->maxHp) {
                    limbs->hp = limbs->maxHp;
                }
            }
        }

        if (stats->limbs[stats->mobilityLimb0].hp >= 2 || stats->limbs[stats->mobilityLimb1].hp >= 2) {
            statuses &= ~0x10;
        }

        amount = simulation_time_step / 240;

        if (amount > 0) {
            stats->currentHp += amount - 1;
        }

        if (stats->currentHp > stats->maxHp) {
            stats->currentHp = stats->maxHp;
        }

        amount = simulation_time_step / 180;

        if (amount > 0) {
            stats->currentMp += amount - 1;
        }

        if (stats->currentMp > stats->maxMp) {
            stats->currentMp = stats->maxMp;
        }

        if (statuses & 0x4000) {
            int scale = stats->maxHp * 5;

            stats->currentHp -= ticks / Battle_statusEffectParams[14].tickInterval * scale / 100;

            if (stats->currentHp < 0) {
                stats->currentHp = 0;
            }
        }

        if (statuses & 0x20000) {
            int scale = stats->maxHp * 5;

            stats->currentHp += ticks / Battle_statusEffectParams[17].tickInterval * scale / 100;

            if (stats->currentHp < 0) {
                stats->currentHp = 0;
            }
        }

        for (i = 0; i < 5; ++i) {
            if (statuses & Battle_Ai_kTimedStatusFlags[i]) {
                int duration = stats->statusTimers[i];

                duration = duration * 32 - duration * 2;

                if (Battle_Ai_roomAiState->readinessLevel >= duration) {
                    statuses &= ~Battle_Ai_kTimedStatusFlags[i];
                }
            }
        }

        stats->statuses = statuses;
        state = Battle_Ai_actorAiStates[actor->id];
        Battle_Ai_RefreshAiResourceRestrictions(state);

        for (row = 0; row < 6; ++row) {
            for (column = 0; column < 4; ++column) {
                Battle_Ai_RefreshAiActionUsability(state, row, column);
            }
        }
    }
}

// decomp: BATTLE.PRG snapshotActorStats (src/BATTLE/BATTLE.PRG/74088.c:1295)
int Battle_Ai_SnapshotPlanningActorStats(void)
{
    Battle_Actor* actor;

    Battle_Ai_planningStatsSnapshot = Core_Memory_AllocateFromHighEnd(sizeof(Battle_ActorStats) * 16);
    if (Battle_Ai_planningStatsSnapshot == NULL) {
        return 0;
    }
    for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
        Battle_Ai_CopyPlanningActorStats(actor->stats, &Battle_Ai_planningStatsSnapshot[actor->id]);
    }
    return 1;
}

// decomp: BATTLE.PRG restoreActorStats (src/BATTLE/BATTLE.PRG/74088.c:1309)
void Battle_Ai_RestorePlanningActorStats(void)
{
    Battle_Actor* actor;

    for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
        Battle_Ai_CopyPlanningActorStats(&Battle_Ai_planningStatsSnapshot[actor->id], actor->stats);
    }
    Core_Memory_FreeFromHighEnd(Battle_Ai_planningStatsSnapshot);
}

// decomp: src/BATTLE/BATTLE.PRG/74088.c:1264
#define MIN(a, b) ((a) > (b) ? (b) : (a))
// decomp: src/BATTLE/BATTLE.PRG/74088.c:1265
#define MAX(a, b) ((a) < (b) ? (b) : (a))
// decomp: BATTLE.PRG func_800DE3E4 (src/BATTLE/BATTLE.PRG/74088.c:1319)
void Battle_Ai_BuildActorActionPlan(Battle_Ai_ActionPlanView* state)
{
    int delays[3] = { 0, 455, 2900 };
    int quotas[3] = { 10, 3, 3 };
    int selected[3];
    int row;
    int column;
    int pass;
    int i;
    int j;
    int delay;
    int index;
    int minDelay;
    Battle_Actor* actor;
    Battle_ActorStats* stats = state->stats;
    int remaining;
    Battle_Ai_CandidateEntryView* entry;
    Battle_Ai_CandidateEntryView* rejected;
    Battle_Ai_CandidateEntryView* fallback;
    Battle_Actor* self;

    Battle_Ai_fallbackCandidateCount = 0;
    Battle_Ai_unusedPlanCounter = 0;

    for (row = 0; row < 6; ++row) {
        for (column = 0; column < 4; ++column) {
            for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
                Battle_Ai_candidateExclusions[actor->id][(u_short)stats->limbActions[row][column].actionId] = 0;
            }
        }
    }

    state->candidateCount = 0;
    if (state->actorId == 0) {
        return;
    }

    if (Battle_Ai_SnapshotPlanningActorStats()) {
        minDelay = MAX(state->minActionDelay, (u_short)(state->stats->actionRecoveryTimer / 35));
        Battle_Ai_roomAiState->readinessLevel = 0;

        for (pass = 0; pass < 3; ++pass) {
            delay = MAX(delays[pass], minDelay);
            Battle_Ai_AdvancePlanningSimulation(delay);
            Battle_Ai_EnumerateActionCandidates((Battle_Ai_ActionChoiceView*)state, (short)delay);
            Battle_Ai_SortActionCandidates(state);

            if (pass == 1) {
                remaining = 16 - selected[0] - quotas[2];
                if (remaining < 0) {
                    remaining = 0;
                }
                selected[1] = remaining;
                selected[pass] = MIN(selected[1], state->candidateCount);
            } else {
                selected[pass] = MIN(state->candidateCount, quotas[pass]);
            }

            for (i = 0; i < selected[pass]; ++i) {
                Battle_Ai_CopyActionCandidate(&Battle_Ai_planningPassCandidates[pass][i], state->candidateOrder[i]);
            }
            for (i = selected[pass]; i < state->candidateCount; ++i) {
                rejected = state->candidateOrder[i];
                Battle_Ai_candidateExclusions[rejected->targetId][rejected->action] = 0;
            }
            state->candidateCount = 0;
        }

        for (pass = 0; pass < 3; ++pass) {
            for (j = 0; j < selected[pass]; ++j) {
                index = state->candidateCount;
                entry = &state->candidates[index];
                Battle_Ai_CopyActionCandidate(entry, &Battle_Ai_planningPassCandidates[pass][j]);
                state->candidateOrder[index] = entry;
                state->candidateCount = index + 1;
                if (index + 1 >= 16) {
                    goto restore;
                }
            }
        }

        if (state->candidateCount == 0 && state->allowFallbackAction) {
            for (i = 0; i < Battle_Ai_fallbackCandidateCount; ++i) {
                fallback = &state->candidates[i];
                Battle_Ai_CopyActionCandidate(fallback, &Battle_Ai_fallbackCandidates[i]);
                fallback->weight = 128;
                state->candidateOrder[i] = fallback;
            }
            state->candidateCount = Battle_Ai_fallbackCandidateCount;
            Battle_Ai_SortActionCandidates(state);
        }
    restore:
        Battle_Ai_RestorePlanningActorStats();
        Battle_Ai_roomAiState->readinessLevel = 0;
    } else {
        Battle_Ai_EnumerateActionCandidates((Battle_Ai_ActionChoiceView*)state, 0);
        Battle_Ai_SortActionCandidates(state);
    }

    if (state->candidateCount >= 2) {
        for (pass = 0; pass < 3; ++pass) {
            if (selected[pass] != 0) {
                Battle_Ai_PromoteRandomizedActionCandidate(state, selected[pass]);
                break;
            }
        }
    }

    for (i = 0; i < state->candidateCount; ++i) {
        if (i > 0
            && (int)state->candidateOrder[i]->threshold - (int)state->candidateOrder[0]->threshold
                   < 80) {
            state->candidateOrder[i]->threshold = state->candidateOrder[0]->threshold + 80;
        }
        state->candidateOrder[i]->threshold += Core_Random_Rand() & 7;
    }

    Battle_Ai_RefreshActionCandidateReadiness((Battle_Ai_CandidateReadinessView*)state);
    if (state->candidateCount == 0) {
        state->planMode = 7;
    } else {
        Battle_Ai_SelectActionMovementMode((Battle_Ai_CandidateReadinessView*)state);
    }

    if (Battle_Ai_arenaOrbit != NULL) {
        self = state->actor;
        if (State_ScriptFlag(0xA6) == 4 && self->commandOpcode == 24 && self->commandArg0 == 4) {
            State_SetScriptFlag(0xA6, 5);
        }
        if (Battle_Ai_arenaOrbit->planSaved && (u_int)(self->subType - 172) < 2) {
            Battle_Ai_arenaOrbit->savedPlanCount = state->candidateCount;
            state->candidateCount = 0;
        }
    }
}
#undef MAX
#undef MIN

// decomp: BATTLE.PRG func_800DEB10 (src/BATTLE/BATTLE.PRG/74088.c:1458)
void Battle_Ai_ReconcileCompletedActionPlan(Battle_Ai_ReactionRecord* completed_action)
{
    int i;
    int j;
    Battle_Ai_ActorAiState* entry;
    Battle_Ai_EngagementData* command_actor;

    if (completed_action == NULL) {
        return;
    }

    if (completed_action->opaque4.opaque40 != 0) {
        return;
    }

    entry = Battle_Ai_actorAiStates[completed_action->opaque4.opaque0];
    command_actor = entry->actor;

    if (entry == NULL) {
        return;
    }

    if (command_actor->opaque9 != 24) {
        return;
    }

    for (i = 0; i < entry->candidateCount; ++i) {
        Battle_Ai_ActionCandidate** list = entry->candidateOrder;
        Battle_Ai_ActionCandidate* candidate = list[i];

        if ((completed_action->opaque0 == candidate->action) && (command_actor->opaqueC == candidate->row)
            && (command_actor->opaqueE == candidate->column)) {
            entry->opaqueFB = candidate->targetId;
            for (j = 0; j < completed_action->opaque4A; ++j) {
                Battle_Ai_EngagementActor* elem = &completed_action->opaque4C[j];

                if (elem->opaque40 == 0) {
                    Field_GimmickInstance* actor = Field_gimmickInstances[elem->opaque0];

                    if ((actor == NULL) || ((0x14 >> actor->objectInfo.objectId) & 1)) {
                        Battle_Ai_ForgetAiActionTarget(entry);
                        return;
                    }
                }
            }
        }
    }

    Battle_Ai_RememberAiActionTarget(entry);
}

