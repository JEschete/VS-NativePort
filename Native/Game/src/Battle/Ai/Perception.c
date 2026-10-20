// Copied from the decomp by tools/port/copy_to_native.py; see each item's decomp line.
#include "Legacy/Psx.h"
#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Action.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/DecisionPrimitives.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/OrbitingBoss.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Ai/PlanningPrimitives.h"
#include "Battle/Damage.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Timing.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Render/Palette.h"

// decomp: D_800F16F4 (D_800F16F4@BATTLE/BATTLE.PRG)
int Battle_Ai_frameTickBudget; /* contents: module image (ModuleData.c) */

// decomp: D_800F58D4 (D_800F58D4@BATTLE/BATTLE.PRG)
int Battle_Ai_secondPerceptionTarget;

// decomp: D_800F58E0 (D_800F58E0@BATTLE/BATTLE.PRG)
int Battle_Ai_candidateEngagedThisUpdate;

// decomp: BATTLE.PRG func_800D8260 (src/BATTLE/BATTLE.PRG/6FA60.c:407)
void Battle_Ai_SetTemporaryAwarenessIndicator(Battle_Ai_ActorAiState* arg0, int arg1, int arg2)
{
    arg0->opaque138 = arg1;
    arg0->opaque12A = arg2;
}

// decomp: src/BATTLE/BATTLE.PRG/6FC00.c:644
#define RELATION_STATES ((Battle_Ai_PerceptionView**)((uintptr_t)&Battle_Ai_FrameState()->actors))
// decomp: BATTLE.PRG checkExtent (src/BATTLE/BATTLE.PRG/6FC00.c:646)
int Battle_Ai_IsTargetOccludedAtExtent(
    SVECTOR* from, SVECTOR* to, Battle_Ai_PerceptionView* state, int id)
{
    Battle_Ai_PerceptionView* other = RELATION_STATES[id];
    int extent;

    if (other) {
        extent = other->bodyHeight;
    } else {
        extent = 128;
    }
    return Field_IsTerrainSegmentBlocked(from, to, extent, state->bodyHeight);
}
#undef RELATION_STATES

// decomp: src/BATTLE/BATTLE.PRG/6FC00.c:644
#define RELATION_STATES ((Battle_Ai_PerceptionView**)((uintptr_t)&Battle_Ai_FrameState()->actors))
// decomp: BATTLE.PRG checkHeight (src/BATTLE/BATTLE.PRG/6FC00.c:660)
int Battle_Ai_IsTargetOccludedAtHeight(
    SVECTOR* from, SVECTOR* to, Battle_Ai_PerceptionView* state, int id)
{
    Battle_Ai_PerceptionView* other = RELATION_STATES[id];
    int height;
    int own;

    if (other) {
        height = ABS(other->pointHeight);
    } else {
        height = 64;
    }
    own = state->pointHeight;
    if (own < 0) {
        own = -own;
    }
    return Field_IsTerrainSegmentBlocked(from, to, height, own);
}
#undef RELATION_STATES

// decomp: src/BATTLE/BATTLE.PRG/6FC00.c:644
#define RELATION_STATES ((Battle_Ai_PerceptionView**)((uintptr_t)&Battle_Ai_FrameState()->actors))
// decomp: BATTLE.PRG func_800D87E8 (src/BATTLE/BATTLE.PRG/6FC00.c:679)
void Battle_Ai_UpdateTargetPerception(Battle_Ai_PerceptionView* state, int id)
{
    int height;
    int active;
    int previous;
    int visible;
    Battle_Ai_TargetRelation relation;
    Battle_Ai_PerceivedObjectView* object;
    Battle_Ai_PerceptionView* other;
    Battle_Actor* actor;
    int owner;
    int blocked;
    int heading;
    int angle;
    int distance;
    int x;
    int z;
    u_short radius;
    u_int targetDistance;
    u_int root;

    {
        Battle_Ai_PerceptionView* target = (Battle_Ai_PerceptionView*)Battle_Ai_actorAiStates[id];
        Battle_Ai_PerceivedObjectView* targetObject;

        if (target) {
            targetObject = (Battle_Ai_PerceivedObjectView*)Actors_actorRecords[id];
            if ((targetObject->opaque8.word & 0x180000)
                && targetObject->position.vy > state->position.vy) {
                height = -target->bodyHeight;
            } else {
                height = ((Battle_Ai_PerceptionView*)Battle_Ai_actorAiStates[id])->pointHeight;
            }
        } else {
            height = -64;
        }
    }
    state->distancesSquared[id] = Core_Math_SquaredDistanceWithHeightOffsets((short*)&state->position,
        (short*)&((Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model)->position, state->pointHeight,
        height);

    if (Field_gimmickInstances[id]
        || ((object = (Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model),
            (object->tile.word & 0xFF00FF) == (state->tile.word & 0xFF00FF))
        || ((object->opaque8.word & 0x180000)
            && (object->lastTile.word & 0xFF00FF) == (state->tile.word & 0xFF00FF))) {
        state->relations[id].word |= 0x40000004;
    } else {
        if (++state->phase & 1) {
            state->relations[id].bits.outOfReach =
                !Battle_Ai_IsTargetOccludedAtExtent(&object->position, &state->position, state, id);
        } else {
            state->relations[id].bits.hidden =
                !Battle_Ai_IsTargetOccludedAtHeight(&object->position, &state->position, state, id);
        }
        if (state->relations[id].bits.hidden) {
            state->relations[id].bits.outOfReach = 1;
        }
    }

    {
        Battle_Ai_PerceivedObjectView* object = state->model;
        state->relations[id].bits.inView =
            Battle_Ai_TargetFacingCosine(&state->position,
                &((Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model)->position,
                object->facing + object->opaque14, state->distancesSquared[id])
            >= (int)state->viewCosine;
    }

    /* Copied as a raw word, which keeps the local addressable. */
    *(u_int*)&relation = *(u_int*)&state->relations[id];
    if (state->actorId == id) {
        relation.bits.active = 1;
        relation.bits.visible = 1;
        relation.bits.wasActive = 0;
    } else {
        active = 0;
        if (state->distancesSquared[id] <= state->farDistanceSquared && relation.bits.inView
            && id != state->actorId && (state->settingsBit15 || relation.bits.outOfReach)) {
            active = 1;
        }
        relation.bits.active = active;
        {
            int timer = relation.bits.timer;
            if (timer) {
                relation.bits.timer = timer - 1;
                relation.bits.active = 1;
            }
        }
        previous = 0;
        if (!relation.bits.active) {
            previous = state->relations[id].bits.active;
        }
        relation.bits.wasActive = previous;
        visible = 0;
        if (relation.bits.active
            || (state->distancesSquared[id] < state->nearDistanceSquared && relation.bits.outOfReach)) {
            visible = 1;
        }
        relation.bits.visible = visible;
    }

    {
        Battle_Ai_PerceivedObjectView* object = (Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model;
        if ((*(u_short(**)[32])((uintptr_t)&Battle_Ai_FrameState()->tileFlags))[object->tile.tile.tileZ][object->tile.tile.tileX] & 0x2000) {
            relation.bits.onMarkedTile = 1;
        } else {
            relation.bits.onMarkedTile = relation.bits.active;
        }
    }
    *(u_int*)&state->relations[id] = *(u_int*)&relation;

    blocked = 0;
    radius = 63;
    {
        Battle_Ai_PerceivedObjectView* own = state->model;
        x = own->position.vx;
        z = own->position.vz;
    }
    owner = state->actorId;
    targetDistance = state->distancesSquared[id];
    heading = Core_Gte_Atan2(((Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model)->position.vx - x,
        ((Battle_Ai_PerceivedObjectView*)Battle_actors[id]->model)->position.vz - z);
    other = RELATION_STATES[id];
    if (other) {
        radius = other->radius;
    }
    for (actor = Battle_actors[0]; actor != NULL; actor = actor->next) {
        int candidate = actor->id;
        if (candidate == owner || candidate == id
            || state->distancesSquared[candidate] > targetDistance) {
            continue;
        }
        {
            Battle_Ai_PerceivedObjectView* object = (Battle_Ai_PerceivedObjectView*)Actors_actorRecords[candidate];
            angle = Core_Gte_Atan2(object->position.vx - x, object->position.vz - z) - heading;
        }
        if (Core_Gte_Cos(angle) < 0) {
            continue;
        }
        root = Core_Math_SquareRootInteger(state->distancesSquared[candidate]);
        distance = (int)(root * Core_Gte_Sin(angle)) >> 12;
        if (distance < 0) {
            distance = -distance;
        }
        if ((u_int)distance < radius) {
            blocked = 1;
            break;
        }
    }

    {
        Battle_Ai_TargetRelation final;

        final.word = state->relations[id].word & 0xF7FFFFFF;
        final.word |= blocked << 27;
        state->relations[id] = final;
        if (state->actorId != id && RELATION_STATES[id]) {
            if (final.bits.classificationTimer) {
                final.bits.classificationTimer--;
                if (!final.bits.classificationTimer) {
                    final.bits.classification = final.bits.previousClassification;
                }
            }
            if (state->settingsBit23 && final.bits.classification == 3) {
                if (final.bits.active) {
                    final.bits.counter = 0;
                } else {
                    final.bits.counter++;
                    if (final.bits.counter >= 121) {
                        final.bits.classification = 1;
                    }
                }
            }
            state->relations[id] = final;
        }
    }
}
#undef RELATION_STATES

// decomp: BATTLE.PRG func_800D8EF4 (src/BATTLE/BATTLE.PRG/6FC00.c:983)
void Battle_Ai_UpdateActorPerception(void)
{
    Battle_Ai_PerceptionActorView* actor;
    Battle_Ai_PerceptionUpdateView* state;
    Battle_Ai_PerceivedObjectView* object;
    int flags;
    int id;
    int targetId;
    int i;
    int arg;
    int alert;
    Battle_Actor* other;
    u_int target;
    Battle_Ai_PerceptionStatsView* stats;
    u_int x;
    Battle_Ai_HpPairView hp;
    int current;
    int max;
    u_int z;

    Battle_Ai_candidateEngagedThisUpdate = 0;
    Battle_Ai_roomAiState->approachMoves = 0;
    if (Battle_Ai_roomAiState->readinessLevel < 0xFFFE) {
        Battle_Ai_roomAiState->readinessLevel++;
    }

    for (actor = (Battle_Ai_PerceptionActorView*)Battle_actors[0]; actor != NULL;
        actor = actor->next) {
        flags = actor->opaque8;
        actor->opaqueBBit0 = 0;
        id = actor->id;
        state = ((Battle_Ai_PerceptionUpdateView**)((uintptr_t)&Battle_Ai_FrameState()->actors))[id];
        state->opaque1D = 0;
        object = state->model;
        state->statusFlags = actor->stats->actionRecoveryTimer;

        if (flags & 0x20) {
            Battle_Ai_DisengageActor((Battle_Ai_ActorAiState*)state);
            Battle_Ai_ResetActorAwarenessIndicator((struct Battle_Ai_PlanEvaluationView*)state);
            Battle_Ai_UnlinkEngagedActor((Battle_Ai_ActorAiState*)state);
        skip:
            state->opaque14 = 1;
            continue;
        }
        Battle_Ai_UpdateTemporaryAwarenessIndicator(state);
        Battle_Ai_TickActorDecisionTimers(state);
        if (flags & 0x80) {
            goto skip;
        }
        ((void (*)(struct Battle_Ai_EngagementView*))Battle_Ai_UpdateEngagementBuildup)((struct Battle_Ai_EngagementView*)state);
        if ((flags & 0x40) || state->opaque18C
            || (object->opaque8.bits.skipStateA | object->opaque8.bits.skipStateB | object->opaque5ADBit7)
            || id == 0) {
            goto skip;
        }
        if (object->opaque8.bits.forcesPerception) {
            state->opaque1D = 1;
            Battle_Ai_UpdateTargetPerception((Battle_Ai_PerceptionView*)state, 0);
            goto skip;
        }

        arg = Core_ElapsedFrameCounterTicks();
        if (Battle_Ai_frameTickBudget < arg) {
            goto idle;
        }
        *(int*)((uintptr_t)&Battle_Ai_FrameState()->lastBudgetTick) = arg;
        if (state->opaque18) {
            if (state->opaque12A != 0 && state->opaque138 == 6) {
            idle:
                Battle_Ai_RefreshAiTargetTileWhenTracking(state);
                goto skip;
            }
            state->opaque18 = 0;
        }
        if (state->opaque19A) {
            state->opaque19A--;
            ((void (*)(Battle_Ai_ActorAiState*))Battle_Ai_UpdateMovementRecovery)((Battle_Ai_MovementRecoveryView*)state);
            Battle_Ai_UpdateTargetPerception((Battle_Ai_PerceptionView*)state, 0);
            state->opaque1D = 1;
            if (state->opaque46 >= 2) {
                state->opaque46--;
            }
            goto idle;
        }

        state->opaque14 = 0;
        if ((state->tileSource->position & 0xFF00FF) == (state->tile & 0xFF00FF)) {
            if (state->opaque46 && --state->opaque46 == 0) {
                state->previousTile.word = state->tileSource->position;
                state->opaqueFB = 16;
                state->opaque1C = 1;
                state->opaque45 = 40;
            }
            if (state->opaque45 && --state->opaque45 == 0) {
                if (++state->detourStep >= 5) {
                    state->detourStep = 1;
                }
                state->opaque45 = 40;
            }
            state->opaque8 = 1;
        } else {
            state->opaque46 = 0xA8C;
            state->opaque8 = 0;
            if (state->opaque1C) {
                Battle_Ai_ForgetAiActionTarget((Battle_Ai_ActionExecutionView*)state);
                state->opaque1C = 0;
            }
        }

        if (state->opaque124 != 8 && actor->opaque9 != 8 && state->opaque124 != actor->opaque9) {
            Battle_Ai_SetActorAiReactionWithDelay(state, 1, 64);
        }
        state->opaque124 = actor->opaque9;
        state->tile = state->model->tile.word;
        x = state->positionSource->position;
        z = state->positionSource->opaque4;
        state->opaque4C = x;
        state->opaque50 = z;
        state->opaque120 = (((z & 0xFFFF) >> 6) << 16) | ((x & 0xFFFF) >> 6);
        if ((0x12600 >> state->opaque124) & 1) {
            if (Battle_Ai_IsMovementHistoryStalled(state)) {
                state->historyMask = 0;
                state->opaque19A = 30;
                goto idle;
            }
            state->historyX.samples[state->historyIndex] = state->opaque4C;
            state->historyZ.samples[state->historyIndex] = state->opaque50;
            state->historyMask |= 1 << state->historyIndex;
            state->historyIndex = (state->historyIndex + 1) & 7;
        } else {
            state->historyMask = 0;
        }

        if (state->opaque190 > 0) {
            state->opaque190--;
        }

        arg = 0;
        for (i = 0; i < 2; i++) {
            Battle_Ai_UpdateTargetPerception((Battle_Ai_PerceptionView*)state, arg);
            arg = Battle_Ai_secondPerceptionTarget;
            if (arg == 0) {
                break;
            }
        }

        Battle_Ai_RefreshAiResourceRestrictions(state);
        if (Battle_Ai_RefreshActionCandidateReadiness((Battle_Ai_CandidateReadinessView*)state)) {
            goto idle;
        }

        alert = 0;
        stats = (Battle_Ai_PerceptionStatsView*)state->actor->stats;
        hp = stats->hp;
        current = hp.current;
        max = hp.max;
        if (state->settingsBits28 != 0) {
            if (state->settingsBits28 == 1) {
                alert = current * 4 < max;
            } else if (state->settingsBits28 == 2) {
                if (current * 2 < max) {
                    alert = 1;
                }
            } else if (state->opaque114) {
                alert = 1;
            } else {
                for (other = Battle_actors[0]; other != NULL; other = other->next) {
                    target = state->relations[other->id];
                    if (((target >> 5) & 1) && ((target >> 6) & 3) == 3) {
                        alert = 1;
                        break;
                    }
                }
            }
            if (state->actionStatus == 2) {
                alert = 1;
            }
        }
        state->opaqueE = alert;
        if (state->opaque156) {
            state->opaque156--;
        }
        if (state->opaque1C == 0 && state->previousTile.word >= 0) {
            targetId = state->previousTile.bytes[3];
            if (state->opaque40 == 0 || --state->opaque40 == 0 || Actors_actorRecords[targetId] == NULL
                || (((Battle_Ai_PerceivedObjectView*)Actors_actorRecords[targetId])->tile.word & 0xFF00FF)
                       != (state->opaque3C & 0xFF00FF)) {
                Battle_Ai_ForgetAiActionTarget((Battle_Ai_ActionExecutionView*)state);
            }
        }
    }
}

// decomp: BATTLE.PRG func_800DBB2C (src/BATTLE/BATTLE.PRG/72B70.s:0) [asm-file: not yet ported]
int Battle_Ai_FindActorByRelationship(void*, void*, int)
{
    VS_UNPORTED("Battle::Ai::FindActorByRelationship");
    return 0;
}

// decomp: BATTLE.PRG func_800DBF00 (src/BATTLE/BATTLE.PRG/73700.c:414)
void Battle_Ai_UnlinkEngagedActor(Battle_Ai_ActorAiState* arg0)
{
    Battle_Ai_EngagementData* data = arg0->actor;
    Battle_Ai_ActorAiState* node;

    arg0->opaque12F = 0;
    data->opaqueBBit2 = 2;

    if (arg0 == Battle_Ai_roomAiState->engagedTail) {
        data->opaqueBBit2 = 2;
        Battle_Ai_roomAiState->engagedTail = Battle_Ai_roomAiState->engagedTail->nextEngaged != NULL ? Battle_Ai_roomAiState->engagedTail->nextEngaged
                                                              : Battle_Ai_roomAiState->engagedHead;
    }

    if (Battle_Ai_roomAiState->engagedHead == arg0) {
        Battle_Ai_roomAiState->engagedHead = arg0->nextEngaged;
    } else {
        for (node = Battle_Ai_roomAiState->engagedHead; node != NULL; node = node->nextEngaged) {
            if (node->nextEngaged == arg0) {
                node->nextEngaged = arg0->nextEngaged;
                return;
            }
        }
    }

    if (Battle_Ai_roomAiState->engagedHead == NULL) {
        Battle_Ai_roomAiState->engagedTail = NULL;
    }
}

// decomp: BATTLE.PRG func_800DBFE4 (src/BATTLE/BATTLE.PRG/73700.c:463)
void Battle_Ai_UpdateEngagementBuildup(Battle_Ai_ActorAiState* arg0)
{
    if (arg0->opaque8E != 0 && ++arg0->opaque8E != 0) {
        int frame;

        if (arg0->opaque8E < 8) {
            frame = 0;
        } else if (arg0->opaque8E < 16) {
            frame = 1;
        } else if (arg0->opaque8E < 24) {
            frame = 2;
        } else {
            frame = 3;
        }

        if (arg0->opaque8E <= 120) {
            Battle_Ai_SetActorAwarenessIndicator(arg0->actorId, frame + 1);
            return;
        }

        Battle_Ai_RememberAiActionTarget(arg0);
        Battle_Ai_DisengageActor(arg0);
    }

    if (arg0->opaque9A != 0) {
        if (--arg0->opaque9A != 0) {
            Battle_Ai_SetActorAwarenessIndicator(arg0->actorId, 4);
        } else {
            Battle_Ai_SetActorAwarenessIndicator(arg0->actorId, 0);
        }
    }

    if (arg0->opaque98 != 0) {
        --arg0->opaque98;
    }
}

// decomp: BATTLE.PRG func_800DC0D0 (src/BATTLE/BATTLE.PRG/73700.c:499)
void Battle_Ai_AnimateEngagementPhase(void)
{
    Battle_Ai_EngagementView* head = (Battle_Ai_EngagementView*)Battle_Ai_roomAiState->engagedHead;
    Battle_Ai_EngagementView* state;
    int changed = 0;
    Battle_Ai_EngagementActorView* actor;
    if (head) {
        state = head;
        do {
            actor = state->actor;
            if ((state->targetFlags >> 6) != 2) {
                if (state->phase == 0) {
                    state->direction = 1;
                    actor->mode = 1;
                    changed = 1;
                } else if (state->phase == 16) {
                    state->direction = -1;
                    actor->mode = 2;
                    changed = 1;
                }
                state->phase += state->direction;
                if (changed) {
                    break;
                }
            }
            state = state->nextEngaged;
        } while (state);
    }
}

// decomp: BATTLE.PRG func_800DC19C (src/BATTLE/BATTLE.PRG/7399C.s:0) [asm-file: not yet ported]
void Battle_Ai_EndAshleyAlliance(Battle_Ai_ReactionView*)
{
    VS_UNPORTED("Battle::Ai::EndAshleyAlliance");
}

// decomp: BATTLE.PRG func_800DC210 (src/BATTLE/BATTLE.PRG/7399C.s:0) [asm-file: not yet ported]
void Battle_Ai_TryBeginAshleyAlliance(Battle_Ai_ReactionView*, int)
{
    VS_UNPORTED("Battle::Ai::TryBeginAshleyAlliance");
}

// decomp: BATTLE.PRG func_800DC784 (src/BATTLE/BATTLE.PRG/73C84.s:0) [asm-file: not yet ported]
void Battle_Ai_MakeTargetHostile(Battle_Ai_ReactionView*, int)
{
    VS_UNPORTED("Battle::Ai::MakeTargetHostile");
}

// decomp: BATTLE.PRG func_800DC810 (src/BATTLE/BATTLE.PRG/73C84.s:0) [asm-file: not yet ported]
void Battle_Ai_ApplyActionRelationshipChanges(Battle_Ai_ReactionRecord*)
{
    VS_UNPORTED("Battle::Ai::ApplyActionRelationshipChanges");
}

// decomp: BATTLE.PRG func_800DD000 (src/BATTLE/BATTLE.PRG/74088.c:716)
void Battle_Ai_ApplyActorActionReaction(Battle_Ai_ReactionView* state, Battle_Ai_StatApplicationView* app)
{
    int own, source, action, i, bad;
    Battle_Ai_ReactionView* other;
    Battle_Ai_StatDeltaView* entry;
    Battle_Ai_ReactionActionView* info;
    if (!app || app->first.skip) {
        return;
    }
    own = state->actorId;
    if (!own) {
        return;
    }
    info = (void*)&Battle_actionDefinitions[app->action];
    bad = 0;
    if (((info->effect1 >> 7) & 63) == 3 || ((info->effect2 >> 7) & 63) == 3) {
        bad = 1;
    }
    if (bad) {
        return;
    }
    source = app->first.actor;
    action = app->action;
    other = (void*)Battle_Ai_actorAiStates[source];
    for (i = 0; i < app->count; i++) {
        entry = &app->entries[i];
        if (entry->skip || entry->actor != own || source == own) {
            continue;
        }
        if (state->behaviorMode == 1) {
            state->behaviorMode = 2;
        }
        if (Battle_Ai_IsActionBeneficialToActor(action, state->actorId)) {
            if (source == 0 && state->usesRoomActionDelay == 0 && state->settingsBit27) {
                Battle_Ai_TryBeginAshleyAlliance(state, 0);
            } else if ((other->relations[0].bytes.firstByte >> 6) == 3) {
                Battle_Ai_EndAshleyAlliance(state);
            }
        }
        state->relations[source].reactionBits.timer = 28;
        if (Battle_Ai_IsActionHarmfulToActor(action, state->actorId)) {
            if (Battle_Ai_arenaOrbit && source == 0 && (entry->modes & 3) != 2 && entry->hp > 0) {
                ((Battle_Ai_ReactionOrbitView*)Battle_Ai_arenaOrbit)->isOrbiting = 0;
            }
            if (source == 0 && state->settingsBit27) {
                Battle_Ai_EndAshleyAlliance(state);
            }
            if (state->settingsBit25 && (state->settingsBit26 == 0 || source == 0)) {
                Battle_Ai_MakeTargetHostile(state, source);
            } else if (state->relations[source].reactionBits.classification != 3) {
                if (other->relations[own].reactionBits.counter) {
                    other->relations[own].reactionBits.counter = 0;
                    other->relations[own].reactionBits.classification = other->relations[own].reactionBits.previousClassification;
                } else {
                    state->relations[source].reactionBits.counter = 127;
                    state->relations[source].reactionBits.previousClassification =
                        state->relations[source].reactionBits.classification;
                    if (state->settingsBit26) {
                        Battle_Ai_MakeTargetHostile(state, source);
                    }
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_800DD344 (src/BATTLE/BATTLE.PRG/74088.c:782)
void Battle_Ai_PropagateActorRelationships(Battle_Ai_ReactionView* state)
{
    Battle_Ai_ActorListNodeView* actor;
    Battle_Ai_ActorListNodeView* candidate;
    Battle_Ai_ActorListNodeView* ally;
    Battle_Ai_ReactionView* other;
    Battle_Ai_ReactionView* firstOther;
    Battle_Ai_ReactionView* allyState;
    int id, classification;
    unsigned int otherClass;
    if (state->usesRoomActionDelay) {
        for (actor = (void*)Battle_actors[0]; actor; actor = actor->next) {
            id = actor->id;
            firstOther = (void*)Battle_Ai_actorAiStates[id];
            if (id == state->actorId || id == 0 || firstOther->settingsBits21 == 3) {
                classification = 2;
            } else if (!firstOther->usesRoomActionDelay) {
                if ((firstOther->relations[0].bytes.firstByte >> 6) == 3) {
                    classification = 3;
                    state->relations[id].reactionBits.counter = 0;
                } else {
                    classification = 1;
                }
            } else {
                classification = 1;
            }
            state->relations[id].reactionBits.classification = classification;
        }
    } else if (state->settingsByte3Bit0) {
        for (actor = (void*)Battle_actors[0]; actor; actor = actor->next) {
            otherClass = state->relations[actor->id].bytes.firstByte >> 6;
            other = (void*)Battle_Ai_actorAiStates[actor->id];
            if (otherClass == 2 && (other->relations[state->actorId].bytes.firstByte >> 6) == 2) {
                for (candidate = (void*)Battle_actors[0]; candidate;
                    candidate = candidate->next) {
                    id = candidate->id;
                    if (state->relations[id].reactionBits.classification == 1
                        && (state->relations[id].word & 1)
                        && (other->relations[id].bytes.firstByte >> 6) == 3) {
                        for (ally = (void*)Battle_actors[0]; ally; ally = ally->next) {
                            allyState = (void*)Battle_Ai_actorAiStates[ally->id];
                            if ((state->relations[ally->id].bytes.firstByte >> 6) == 2
                                && (allyState->relations[state->actorId].bytes.firstByte >> 6) == 2
                                && (allyState->relations[id].reactionBits.classification == 2
                                    || (allyState->relations[id].reactionBits.counter
                                        && allyState->relations[id].reactionBits.previousClassification == 2))) {
                                break;
                            }
                        }
                        if (!ally) {
                            Battle_Ai_MakeTargetHostile(state, id);
                        }
                    }
                }
            }
        }
    }
}

// decomp: BATTLE.PRG func_800DEEA4 (src/BATTLE/BATTLE.PRG/74088.c:1596)
void Battle_Ai_NotifyActionRelationshipChanges(Battle_Ai_ReactionRecord* arg0)
{
    if (arg0 != NULL) {
        if (arg0->opaque4.opaque40 == 0) {
            Battle_Ai_ActorAiState* entry = Battle_Ai_actorAiStates[arg0->opaque4.opaque0];

            if (entry != NULL) {
                entry->opaque9A = 0;
            }
        }

        Battle_Ai_ApplyActionRelationshipChanges(arg0);
    }
}

// decomp: BATTLE.PRG func_800E437C (src/BATTLE/BATTLE.PRG/7B5DC.s:0) [asm-file: not yet ported]
int Battle_Ai_TargetFacingCosine(SVECTOR*, SVECTOR*, int, u_int)
{
    VS_UNPORTED("Battle::Ai::TargetFacingCosine");
    return 0;
}

