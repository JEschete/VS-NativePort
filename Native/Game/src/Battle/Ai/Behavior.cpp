// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
extern "C" {
#include "Legacy/Psx.h"
#include "Actors/ActorModelSlot.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorCommands.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/DecisionCallbacks.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/GroundNavigation.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/FrameTiming.h"
#include "Core/Math/ScalarMath.h"
#include "Field/Gimmicks.h"
#include "Field/TerrainHeight.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: BATTLE.PRG func_800D9E18 (src/BATTLE/BATTLE.PRG/71618.c:511)
int Battle_Ai_MoveForActionCandidate(Battle_Ai_CandidateMovementView* state, Battle_Ai_CandidateEntryView* entry)
{
    int id, flag;
    unsigned int distance, limit;
    int active = 0;
    short point[3];
    int x, z, y;
    if (!state->candidateCount) {
        return 0;
    }
    id = entry->targetId;
    if (id == 16 || !((Battle_Ai_CandidateMovementView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[id]) {
        return 0;
    }
    switch (state->planMode) {
    case 1:
    normal:
        limit = 0x1F80;
        if (!(state->movementFlags & 8)) {
            limit = Core_Math_SquareRootInteger(entry->distanceSquared) + (state->maxRange << 5);
        }
        limit = limit * limit;
        if ((state->relations[id].bytes[0] >> 6) == 2) {
            distance = entry->distanceSquared;
            flag = 0;
            active = 1;
        } else {
            if (Battle_Ai_CanApproachEngagedTarget((Battle_Ai_ActorAiState *)((void*)state), (Battle_Ai_ActionCandidate *)((void*)entry))) {
                distance = entry->distanceSquared;
                active = 1;
            } else {
                distance = limit;
                {
                    unsigned int minimum = 0x64000;
                    if (distance < minimum) {
                        distance = minimum;
                    }
                }
            }
            flag = 1;
        }
        distance = Core_Math_SquareRootInteger(distance);
        if (active) {
            Battle_Ai_roomAiState->approachMoves++;
            if (state->distancesSquared[id] < limit
                && (state->relations[entry->targetId].word & 1)) {
                Battle_Ai_TryEngageActor((Battle_Ai_ActorAiState *)((void*)state));
            }
        }
    move:
        if (Battle_Ai_ApproachActor((Battle_Ai_ActorAiState *)((void*)state), id, distance, flag)) {
            return 1;
        }
        return Battle_Ai_TrackActorPosition((Battle_Ai_ActorAiState *)((void*)state), id) != 0;
    case 2:
        if (!Battle_Ai_IsActionCooldownReady((Battle_Ai_ActorAiState *)((void*)state), (Battle_Ai_ActionCandidate *)((void*)entry))) {
            goto normal;
        }
        if (entry->opaque10Bit1) {
            point[0] = (state->plannedTile.bytes[0] << 7) + 64;
            point[2] = (state->plannedTile.bytes[2] << 7) + 64;
            point[1] = Field_GroundHeightAtWorldPosition(point[0], point[2]);
            distance =
                Core_Math_SquareRootInteger(Core_Math_SquaredDistanceWithHeightOffsets(point, (short*)&Actors_actorRecords[id]->instance.position,
                    state->pointHeight, ((Battle_Ai_CandidateMovementView*)Battle_Ai_actorAiStates[id])->pointHeight));
            distance =
                (int)distance < Core_Math_SquareRootInteger(entry->distanceSquared) - entry->range;
            if (distance) {
                goto normal;
            }
        }
        x = (state->destination.bytes[0] << 7) + 64;
        z = (state->destination.bytes[2] << 7) + 64;
        y = Field_GroundHeightAtWorldPosition(x, z);
        if (Core_Math_SquaredVectorLength(
                x - state->position[0], y - state->position[1], z - state->position[2])
            <= 0xFFFF) {
            state->movementTarget = id;
            Battle_Ai_WanderAroundTile(state, state->plannedTile.word, 2);
        } else {
            state->opaque1 = 1;
            Battle_Ai_MoveToTile(state, state->plannedTile.word, 256);
        }
        return 1;
    case 4:
        return Battle_Ai_FleeVisibleEnemies(state, 0);
    case 7:
        Battle_Ai_DisengageActor((Battle_Ai_ActorAiState *)((void*)state));
        id = 0;
        flag = 1;
        if ((state->relations[id].bytes[0] >> 6) == 3) {
            distance = 640;
            goto move;
        }
        break;
    }
    return 0;
}

// decomp: BATTLE.PRG func_800DA1D4 (src/BATTLE/BATTLE.PRG/71618.c:610)
int Battle_Ai_UpdateCombatPursuit(Battle_Ai_ActorAiState* ai_state)
{
    int i;
    int id = -1;
    u_int nearest = -1;
    int flag;
    int distance;
    Battle_Ai_ActionCandidate* entry;
    Battle_Actor* actor;

    switch (ai_state->actionStatus) {
    case 0:
        entry = ai_state->candidateOrder[0];
        id = entry->targetId;

        if (Battle_Ai_MoveForActionCandidate((Battle_Ai_CandidateMovementView *)((void*)ai_state), (Battle_Ai_CandidateEntryView *)((void*)entry))) {
            return 1;
        }

        for (i = 1; i < ai_state->candidateCount; ++i) {
            entry = ai_state->candidateOrder[i];

            if (entry->threshold < Battle_Ai_roomAiState->readinessLevel && entry->targetId != id) {
                return Battle_Ai_MoveForActionCandidate((Battle_Ai_CandidateMovementView *)((void*)ai_state), (Battle_Ai_CandidateEntryView *)((void*)entry));
            }
        }

        return 0;

    case 1:
        Battle_Ai_DisengageActor(ai_state);

        if (ai_state->relations[0].classification == 3) {
            if (Battle_Ai_ApproachActor(ai_state, 0, 640, 1)) {
                return 1;
            }

            if (Battle_Ai_TrackActorPosition(ai_state, 0)) {
                return 1;
            }
        }

        id = ai_state->candidateOrder[0]->targetId;

        if (ai_state->relations[id].classification == 3) {
            flag = 1;
            distance = 640;
        } else {
            if (ai_state->candidateCount < 2) {
                return 0;
            }

            flag = 0;
            distance = Core_Math_SquareRootInteger(ai_state->candidateOrder[1]->distanceSquared);
        }

        if (Battle_Ai_ApproachActor(ai_state, id, distance, flag)) {
            return 1;
        }

        if (Battle_Ai_TrackActorPosition(ai_state, id)) {
            return 1;
        }
        // Fallthrough

    case 2:
        Battle_Ai_DisengageActor(ai_state);

        if (ai_state->relations[0].classification == 3) {
            if (Battle_Ai_ApproachActor(ai_state, 0, 640, 1)) {
                return 1;
            }

            if (Battle_Ai_TrackActorPosition(ai_state, 0)) {
                return 1;
            }
        }

        for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
            if (ai_state->relations[actor->id].classification == 3
                && ai_state->distancesSquared[actor->id] < nearest) {
                nearest = ai_state->distancesSquared[actor->id];
                id = actor->id;
            }
        }

        if (id >= 0) {
            if (Battle_Ai_ApproachActor(ai_state, id, 640, 1)) {
                return 1;
            }

            if (Battle_Ai_TrackActorPosition(ai_state, id)) {
                return 1;
            }
        }
        break;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800DA4BC (src/BATTLE/BATTLE.PRG/71618.c:788)
int Battle_Ai_TryApproachPlannedTarget(Battle_Ai_ActorAiState* ai_state, Battle_Ai_ActionCandidate* candidate)
{
    if (Battle_Ai_CanApproachEngagedTarget(ai_state, candidate)) {
        int id = candidate->targetId;
        int distance = Core_Math_SquareRootInteger(candidate->distanceSquared);
        int range = distance + ai_state->maxRange * 32;

        range *= range;
        ++Battle_Ai_roomAiState->approachMoves;

        if (ai_state->distancesSquared[id] < range && ai_state->relations[candidate->targetId].active) {
            Battle_Ai_TryEngageActor(ai_state);
        }

        if (Battle_Ai_ApproachActor(ai_state, id, distance, 1)) {
            return 1;
        }

        if (Battle_Ai_TrackActorPosition(ai_state, id)) {
            return 1;
        }
    }

    return 0;
}

// decomp: BATTLE.PRG func_800DA5CC (src/BATTLE/BATTLE.PRG/71618.c:814)
int Battle_Ai_UpdateCloseFollowBehavior(Battle_Ai_EscortView* state)
{
    Battle_Ai_EscortEntryView* entry = state->candidateOrder[0];
    int target;

    if (!state->actionStatus && Battle_Ai_TryApproachPlannedTarget((Battle_Ai_ActorAiState *)((void*)state), (Battle_Ai_ActionCandidate *)((void*)entry))) {
        return 1;
    }
    target = state->linkedActor;
    if (target < 16) {
        if (Battle_Ai_ApproachActor((Battle_Ai_ActorAiState *)((void*)state), target, 288, 1)) {
            if (state->opaqueE9 == 1 && state->planMode != 7) {
                state->movementTarget = ((Battle_Ai_EscortTargetView*)&entry->opaqueC)->target;
                Battle_Ai_ReleaseActorAiReservations((Battle_Ai_ActorAiState *)((void*)state));
                ((void (*)(Battle_Ai_ActorAiState*))Battle_Ai_UpdateMovementRecovery)((Battle_Ai_ActorAiState *)((void*)state));
            }
            return 1;
        }
        if (Battle_Ai_TrackActorPosition((Battle_Ai_ActorAiState *)((void*)state), target)) {
            return 1;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800DA6A4 (src/BATTLE/BATTLE.PRG/71618.c:839)
int Battle_Ai_UpdateEnemyPursuitBehavior(Battle_Ai_EscortView* state)
{
    int target;

    if (state->opaqueE) {
        return Battle_Ai_FleeVisibleEnemies((Battle_Ai_CandidateMovementView *)((void*)state), 1);
    }
    target = Battle_Ai_FindActorByRelationship(state, 0, 3);
    if (target >= 0 && Battle_Ai_FindActorByRelationship(state, 0, 2) >= 0
        && Battle_Ai_ApproachActor((Battle_Ai_ActorAiState *)((void*)state), target, 1024, 1)) {
        return 1;
    }
    return Battle_Ai_UpdateCombatPursuit((Battle_Ai_ActorAiState *)((void*)state));
}

// decomp: BATTLE.PRG func_800DA738 (src/BATTLE/BATTLE.PRG/71618.c:854)
int Battle_Ai_UpdateGuardMovementBehavior(Battle_Ai_EscortView* state)
{
    Battle_Ai_GuardTileWord point;
    int oldFlag, candidate;
    unsigned char* pointPtr;
    int centerX, centerZ;
    int angleA, angleB, cosA, cosB, sinA, dot;
    Battle_Ai_TilePosition tile;
    unsigned int valid;
    int target;
    int active = 0;
    Battle_Ai_GuardModelView** models;
    Battle_Ai_GuardModelView* first;
    Battle_Ai_GuardModelView* second;

    candidate = Battle_Ai_FindActorByRelationship(state, &point, 3);
    target = state->linkedActor;
    valid = (unsigned int)~candidate >> 31;
    if (state->actorId != target && target != 16) {
        if (((Battle_Ai_EscortView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[target]) {
            active = ((unsigned char)state->relations[target] >> 6) == 2;
        }
    }
    oldFlag = state->opaqueF;
    state->opaqueF = active;
    if (valid) {
        if (active) {
            if ((state->relations[target] >> 5) & 1) {
                models = (Battle_Ai_GuardModelView**)&Actors_actorRecords[target];
                first = *models;
                pointPtr = point.bytes;
                tile = first->tile;
                angleA = Core_Gte_Atan2((point.bytes[0] << 7) - (centerX = first->x - 64),
                    (point.bytes[2] << 7) - (centerZ = first->z - 64));
                second = *models;
                angleB = Core_Gte_Atan2(state->x - second->x, state->z - second->z);
                cosA = Core_Gte_Cos(angleA);
                cosB = Core_Gte_Cos(angleB);
                sinA = Core_Gte_Sin(angleA);
                dot = cosA * cosB + sinA * Core_Gte_Sin(angleB);
                dot >>= 12;
                if (state->opaque44) {
                    state->opaque44--;
                }
                if (dot >= -1200 || state->opaque44) {
                    Battle_Ai_ProjectClearRadialDestination((Battle_Ai_MoveTargetView *)((void*)state), tile,
                        Core_Gte_Atan2(point.bytes[0] - (tile.word & 255),
                            pointPtr[2] - ((tile.word >> 16) & 255)),
                        320);
                    ((unsigned char*)state)[1] = 1;
                    Battle_Ai_DispatchAiLocomotionMode(state, 0, 0);
                    if (!state->opaque44) {
                        state->opaque44 = 10;
                    }
                    return 1;
                }
            }
        } else {
            goto fallback;
        }
    }
    if (active) {
        if (valid
            && ((Battle_Ai_EscortView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[target]->distancesSquared[candidate] <= 0x23FFF) {
            state->movementTarget = target;
            ((void (*)(Battle_Ai_ActorAiState*))Battle_Ai_UpdateMovementRecovery)((Battle_Ai_ActorAiState *)((void*)state));
            return 1;
        }
        if (Battle_Ai_ApproachActor((Battle_Ai_ActorAiState *)((void*)state), target, 320, 1)) {
            return 1;
        }
        if (Battle_Ai_TrackActorPosition((Battle_Ai_ActorAiState *)((void*)state), target)) {
            return 1;
        }
    }
fallback:
    if (valid) {
        if (oldFlag) {
            if ((unsigned int)Battle_Ai_NextActorRandomMasked(128) < 128) {
                state->opaque18 = 1;
            }
            ((void (*)(Battle_Ai_ActionExecutionView* ai_state, int awareness_marker, int duration_frames))Battle_Ai_SetTemporaryAwarenessIndicator)((Battle_Ai_ActionExecutionView *)((void*)state), 6, 0);
        }
        if (Battle_Ai_FleeVisibleEnemies((Battle_Ai_CandidateMovementView *)((void*)state), 1)) {
            return 1;
        }
    }
    return Battle_Ai_FleeVisibleEnemies((Battle_Ai_CandidateMovementView *)((void*)state), 1);
}

// decomp: BATTLE.PRG func_800DAAD8 (src/BATTLE/BATTLE.PRG/71618.c:958)
int Battle_Ai_UpdateEscortCombatBehavior(Battle_Ai_EscortView* state)
{
    Battle_Ai_EscortEntryView* entry = state->candidateOrder[0];
    int target;
    Battle_Ai_EscortActorView* actor;

    if (!state->actionStatus && Battle_Ai_TryApproachPlannedTarget((Battle_Ai_ActorAiState *)((void*)state), (Battle_Ai_ActionCandidate *)((void*)entry))) {
        return 1;
    }
    target = state->linkedActor;
    if (target < 16) {
        if (Battle_Ai_ApproachActor((Battle_Ai_ActorAiState *)((void*)state), target, 384, 0)) {
            if (state->opaqueE9 == 1 && state->planMode != 7) {
                state->movementTarget = ((Battle_Ai_EscortTargetView*)&entry->opaqueC)->target;
                Battle_Ai_ReleaseActorAiReservations((Battle_Ai_ActorAiState *)((void*)state));
                ((void (*)(Battle_Ai_ActorAiState*))Battle_Ai_UpdateMovementRecovery)((Battle_Ai_ActorAiState *)((void*)state));
            }
            return 1;
        }
        if (Battle_Ai_TrackActorPosition((Battle_Ai_ActorAiState *)((void*)state), target)) {
            return 1;
        }
    }
    for (actor = (Battle_Ai_EscortActorView*)Battle_actors[0]; actor; actor = actor->next) {
        if (((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode != 3
            && ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].category) {
            ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode = 3;
            if (Battle_Ai_FleeVisibleEnemies((Battle_Ai_CandidateMovementView *)((void*)state), 0)) {
                ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode =
                    ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].oldMode;
                return 1;
            }
            ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].mode =
                ((Battle_Ai_EscortRelationsView*)state)->relations[actor->id].oldMode;
        }
    }
    return 0;
}

// decomp: BATTLE.PRG func_800DAC80 (src/BATTLE/BATTLE.PRG/71618.c:1006)
int Battle_Ai_TryExecuteActionCandidate(Battle_Ai_ActorAiState* ai_state, Battle_Ai_ActionCandidate* candidate)
{
    if (!Battle_Ai_CanExecuteActionCandidate(ai_state, candidate)) {
        return 0;
    }

    Battle_Ai_TryEngageActor(ai_state);

    if (Battle_Ai_IsEngagementReady(ai_state)) {
        Actors_ActorRecord* actor;

        ai_state->currentCandidate = candidate;
        Battle_Ai_candidateEngagedThisUpdate = 1;
        actor = Actors_actorRecords[candidate->targetId];

        if (actor != NULL && actor->instance.locomotionState) {
            candidate->targetLimb = Battle_Ai_NextActorRandomMasked(2);
            candidate->opaqueCBit7 = 217;
        }

        Battle_Ai_IssueActorCommand(ai_state, 15);

        if (Battle_Ai_DoesActionRequireCooldown(ai_state, candidate)) {
            ai_state->minActionDelay = ai_state->usesRoomActionDelay == 0 ? Battle_Ai_roomAiState->defaultActionDelay : 50;
        }

        return 1;
    }

    return 0;
}

// decomp: BATTLE.PRG func_800DAD9C (src/BATTLE/BATTLE.PRG/71618.c:1093)
int Battle_Ai_TryExecutePlannedActions(Battle_Ai_ActorAiState* ai_state)
{
    int i;
    int j;
    Battle_Ai_ActionCandidate* entry;

    for (i = 0; i < ai_state->candidateCount; ++i) {
        entry = ai_state->candidateOrder[i];

        if (Battle_Ai_TryExecuteActionCandidate(ai_state, entry)) {
            return 1;
        }
    }

    entry = &ai_state->currentChoice;

    if (entry->action) {
        for (j = 0; j < 4; ++j) {
            Battle_Actor* actor = Battle_Ai_roomAiState->specialActors[j];

            if (actor != NULL) {
                Field_GimmickInstance* model;

                entry->targetId = actor->id;
                model = Field_gimmickInstances[actor->id];

                if (model->parent && model->mode == 0 && Battle_Ai_TryExecuteActionCandidate(ai_state, entry)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

// decomp: BATTLE.PRG func_800DAED0 (src/BATTLE/BATTLE.PRG/71618.c:1128)
void Battle_Ai_UpdateActorDecisions(void)
{
    Battle_Ai_DispatchActorView* actor;
    Battle_Ai_DecisionDispatchView* state;
    int value, height, coordinate;
    for (actor = ((Battle_Ai_DispatchActorView*)Battle_actors[0])->next; actor;
        actor = actor->next) {
        state = ((Battle_Ai_DecisionDispatchView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[actor->id];
        if (!state->opaque14 || state->opaque1D) {
            value = Core_ElapsedFrameCounterTicks();
            if (Battle_Ai_frameTickBudget < value) {
                Battle_Ai_IssueActorCommand((Battle_Ai_ActorAiState *)((void*)state), 17);
            } else {
                *(int*)((uintptr_t)&Battle_Ai_FrameState()->lastBudgetTick) = value;
                if (state->opaque14 && state->opaque1D) {
                    if (Battle_Ai_actorAiStates[0]) {
                        if ((((Battle_Ai_DispatchModelView*)Actors_actorRecords[0])->opaque8 & 0x180000)
                            && (coordinate =
                                    ((Battle_Ai_DispatchModelView*)Actors_actorRecords[0])->position[1],
                                state->position[1] < coordinate)) {
                            height = -((Battle_Ai_DecisionDispatchView*)Battle_Ai_actorAiStates[0])->bodyHeight;
                        } else {
                            height = ((Battle_Ai_DecisionDispatchView*)Battle_Ai_actorAiStates[0])->pointHeight;
                        }
                    } else {
                        height = -64;
                    }
                    state->distance = Core_Math_SquaredDistanceWithHeightOffsets(state->position,
                        ((Battle_Ai_DispatchActorView*)Battle_actors[0])->model->position,
                        state->pointHeight, height);
                    Battle_Ai_TryExecutePlannedActions((Battle_Ai_ActorAiState *)((void*)state));
                } else {
                    coordinate = state->tileX;
                    state->movementTarget = 16;
                    state->savedDestination = -1;
                    /* Clear the packed header halfword through its raw address. */
                    *(unsigned short*)((char*)state + 12) = 0;
                    state->opaque0 = 0;
                    state->opaque4 = 0;
                    state->opaque8 = 0;
                    value =
                        (*(unsigned short (**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))[state->tileZ][coordinate]
                        & 15;
                    state->opaque154 = value;
                    state->opaque132 = value;
                    *(int*)((uintptr_t)&Battle_Ai_FrameState()->directionCache) = 0;
                    Battle_Ai_DispatchMovementBehavior(state);
                    Battle_Ai_UpdateActorLocomotionMode(state);
                }
            }
        }
    }
}

}  // extern "C"
