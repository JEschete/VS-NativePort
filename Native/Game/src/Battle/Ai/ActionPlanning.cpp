// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Battle/Ai/DecisionPrimitivesLive.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "State/ScriptFlagsLive.hpp"
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/PlanningPrimitivesLive.h"
#include "Battle/Damage.h"
#include "Field/Gimmicks.h"
#include "Items/Equipment.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Items/Weapons.h"
#include "Render/Palette.h"

// decomp: D_80069BBC (D_80069BBC@BATTLE/BATTLE.PRG)
u_int Battle_Ai_kTimedStatusFlags[344]; /* contents: module image (ModuleData.cpp) */

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

    VS::Core::Memory::ClearLiveAlignedWords(list, sizeof *list);
    list[0].distanceSquared = 256;
    list[0].threshold = readiness_threshold;
    Battle_Ai_EvaluateActionCandidateEntry(ai_state, list, 0, NULL);
    VS::Battle::Ai::CopyLiveActionCandidate(&list[1], list);
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
                list[1].opaque10.opaque0Bit4 = VS::Battle::Ai::IsLiveActionHarmfulToActor(action, id);
                list[1].opaque10.opaque0Bit5 = VS::Battle::Ai::IsLiveActionBeneficialToActor(action, id);
                list[1].opaque10.opaque0Bit0 = config->longRange && list[1].opaque10.opaque0Bit4;

                if (!VS::Battle::Ai::CanLiveAiActionAffectActor(ai_state, candidate, id) || Battle_Ai_candidateExclusions[id][action]) {
                    continue;
                }

                switch (config->kind) {
                case 1:
                    Battle_Ai_EvaluateActionCandidateEntry(ai_state, candidate, 1, info);
                    VS::Battle::Ai::CacheLiveFallbackActionCandidate(candidate, info);

                    if (list[1].opaque0 > list[0].opaque0) {
                        VS::Battle::Ai::AppendLiveActionCandidate(ai_state, candidate);
                    }
                    break;

                case 2:
                    last = -1;
                    VS::Battle::Ai::CopyLiveActionCandidate(&list[2], list);
                    VS::Battle::Ai::CopyLiveActionCandidate(&list[3], list);

                    for (limb = 0; limb < 6; ++limb) {
                        int chance;

                        if ((actor->stats->limbs + limb)->maxHp == 0) {
                            continue;
                        }

                        last = limb;
                        list[1].targetLimb = last;
                        Battle_Ai_EvaluateActionCandidateEntry(ai_state, candidate, 2, info);
                        chance = list[1].weight;

                        if ((Core_Random_Rand() & 0x7F) < chance && list[1].opaque0 > list[2].opaque0) {
                            VS::Battle::Ai::CopyLiveActionCandidate(&list[2], candidate);
                        }

                        if (list[1].opaque0 > list[3].opaque0) {
                            VS::Battle::Ai::CopyLiveActionCandidate(&list[3], &list[1]);
                        }
                    }

                    part = Core_Random_Rand();
                    part = (part & 3) + ((part >> 2) & 1) + ((part >> 3) & 1);

                    if ((actor->stats->limbs + part)->maxHp == 0 && last >= 0) {
                        part = last;
                    }

                    if (part >= 0) {
                        list[1].targetLimb = part;
                        VS::Battle::Ai::CacheLiveFallbackActionCandidate(&list[1], info);
                    }

                    if (list[2].opaque0 > list[0].opaque0) {
                        VS::Battle::Ai::AppendLiveActionCandidate(ai_state, &list[2]);
                    } else if (list[0].opaque0 < list[3].opaque0) {
                        VS::Battle::Ai::AppendLiveActionCandidate(ai_state, &list[3]);
                    }
                    break;

                case 3:
                    target = Battle_Ai_actorAiStates[actor->id];
                    VS::Battle::Ai::CopyLiveActionCandidate(&list[2], list);
                    VS::Battle::Ai::CopyLiveActionCandidate(&list[3], list);
                    list[1].opaqueCBit7 = 0;

                    for (pass = 0; pass < 1; ++pass) {
                        int chance;

                        list[1].opaqueCBit7 = ABS(target->pointHeight);
                        Battle_Ai_EvaluateActionCandidateEntry(ai_state, candidate, 3, info);
                        chance = list[1].weight;

                        if ((Core_Random_Rand() & 0x7F) < chance && list[1].opaque0 > list[2].opaque0) {
                            VS::Battle::Ai::CopyLiveActionCandidate(&list[2], candidate);
                        }

                        if (list[1].opaque0 > list[3].opaque0) {
                            VS::Battle::Ai::CopyLiveActionCandidate(&list[3], &list[1]);
                        }
                    }

                    VS::Battle::Ai::CacheLiveFallbackActionCandidate(&list[1], info);

                    if (list[2].opaque0 > list[0].opaque0) {
                        VS::Battle::Ai::AppendLiveActionCandidate(ai_state, &list[2]);
                    } else if (list[0].opaque0 < list[3].opaque0) {
                        VS::Battle::Ai::AppendLiveActionCandidate(ai_state, &list[3]);
                    }
                    break;
                }
            }
        }
    }
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
    int selected[3] = { 0, 0, 0 };
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

    if (VS::Battle::Ai::SnapshotLivePlanningActorStats()) {
        minDelay = MAX(state->minActionDelay, (u_short)(state->stats->actionRecoveryTimer / 35));
        Battle_Ai_roomAiState->readinessLevel = 0;

        for (pass = 0; pass < 3; ++pass) {
            delay = MAX(delays[pass], minDelay);
            Battle_Ai_AdvancePlanningSimulationEntry(delay);
            Battle_Ai_EnumerateActionCandidates((Battle_Ai_ActorAiState *)((Battle_Ai_ActionChoiceView*)state), (short)delay);
            VS::Battle::Ai::SortLiveActionCandidates(state);

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
                VS::Battle::Ai::CopyLiveActionCandidate((Battle_Ai_ActionCandidate *)(&Battle_Ai_planningPassCandidates[pass][i]), (Battle_Ai_ActionCandidate *)(state->candidateOrder[i]));
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
                VS::Battle::Ai::CopyLiveActionCandidate((Battle_Ai_ActionCandidate *)(entry), (Battle_Ai_ActionCandidate *)(&Battle_Ai_planningPassCandidates[pass][j]));
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
                VS::Battle::Ai::CopyLiveActionCandidate((Battle_Ai_ActionCandidate *)(fallback), (Battle_Ai_ActionCandidate *)(&Battle_Ai_fallbackCandidates[i]));
                fallback->weight = 128;
                state->candidateOrder[i] = fallback;
            }
            state->candidateCount = Battle_Ai_fallbackCandidateCount;
            VS::Battle::Ai::SortLiveActionCandidates(state);
        }
    restore:
        VS::Battle::Ai::RestoreLivePlanningActorStats();
        Battle_Ai_roomAiState->readinessLevel = 0;
    } else {
        Battle_Ai_EnumerateActionCandidates((Battle_Ai_ActorAiState *)((Battle_Ai_ActionChoiceView*)state), 0);
        VS::Battle::Ai::SortLiveActionCandidates(state);
    }

    if (state->candidateCount >= 2) {
        for (pass = 0; pass < 3; ++pass) {
            if (selected[pass] != 0) {
                VS::Battle::Ai::PromoteLiveRandomizedActionCandidate(state, selected[pass]);
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

    Battle_Ai_RefreshActionCandidateReadinessEntry((Battle_Ai_CandidateReadinessView*)state);
    if (state->candidateCount == 0) {
        state->planMode = 7;
    } else {
        VS::Battle::Ai::SelectLiveActionMovementMode((Battle_Ai_CandidateReadinessView*)state);
    }

    if (Battle_Ai_arenaOrbit != NULL) {
        self = state->actor;
        if (VS::State::LiveScriptFlag(0xA6) == 4 && self->commandOpcode == 24 && self->commandArg0 == 4) {
            VS::State::SetLiveScriptFlag(0xA6, 5);
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

    VS::Battle::Ai::RememberLiveAiActionTarget(entry);
}

}  // extern "C"
