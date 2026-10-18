// Compile-time checks of the x64 layout decisions the copy depends on (tools/port/copy_fixups.py TYPE_FIXES and
// FIXES). A failure here means a fix no longer holds after a ledger or decomp change.
#include "Legacy/Psx.h"
#include "Actors/ActorLoading.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelRendering.h"
#include "Battle/Actor.h"
#include "Battle/Damage.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ConditionSnapshot.h"
#include "Core/Memory/WordCopy.h"
#include "Field/Gimmicks.h"
#include "Field/GimmickMotion.h"

#include <stddef.h>
#include <string.h>

// An actor record placed at core.model (Content_ParseActorModel) must end before the weapon model slots.
_Static_assert(offsetof(Actors_ActorAllocation, modelSlots) - offsetof(Actors_ActorAllocation, core.model) >=
                   sizeof(Actors_ActorRecord),
    "the actor record overlaps the weapon model slots");

// Actors_SpawnRoomEnemy: `actor + 1` is the stats block of the allocation.
_Static_assert(offsetof(Actors_ActorAllocation, core.actor.stats) == sizeof(Battle_Actor),
    "stats do not follow the Battle_Actor");

// On-disc structs keep the file layout (ONDISC_OFFSETS): the SHP header is 64 bytes, the bones follow.
_Static_assert(sizeof(Actors_ShpModel) == 64, "SHP header layout");
_Static_assert(offsetof(Actors_ShpModel, bones) == 64, "SHP bones offset");
_Static_assert(offsetof(Actors_SeqAnimationBank, headers) == 8, "SEQ bank header layout");
_Static_assert(sizeof(((Actors_ActorRecord*)0)->animationHeader) == sizeof(void*), "SEQ bank cache must be pointer-sized");

// Runtime views must follow the native object, including pointers hidden in the original PS1 padding.
#define SAME_FIELD(View, member, Record, field) \
    _Static_assert(offsetof(View, member) == offsetof(Record, field), "native view mismatch: " #View "." #member)
SAME_FIELD(Battle_Ai_AiActorView, actorId, Battle_Actor, id);
SAME_FIELD(Battle_Ai_AiActorView, aiControlFlags, Battle_Actor, kind);
SAME_FIELD(Battle_Ai_AiActorView, stats, Battle_Actor, stats);
SAME_FIELD(Battle_Ai_AiActorView, model, Battle_Actor, model);
SAME_FIELD(Battle_Ai_SettingsActorView, opaque34, Battle_Actor, reserved34);
SAME_FIELD(Battle_Ai_CreationModelView, radius, Actors_ActorRecord, collisionRadius);
SAME_FIELD(Battle_Ai_CreationModelView, opaque63E, Actors_ActorRecord, collisionHeight);
SAME_FIELD(Battle_Ai_AiCreationView, actor, Battle_Ai_ActorAiState, actor);
SAME_FIELD(Battle_Ai_AiCreationView, model, Battle_Ai_ActorAiState, model);
SAME_FIELD(Battle_Ai_AiCreationView, stats, Battle_Ai_ActorAiState, stats);
SAME_FIELD(Battle_Ai_AiCreationView, actorId, Battle_Ai_ActorAiState, actorId);
SAME_FIELD(Battle_Ai_AiCreationView, pointHeight, Battle_Ai_ActorAiState, pointHeight);
SAME_FIELD(Battle_Ai_SettingsView, decisionCallback, Battle_Ai_ActorAiState, nativePointer68);
SAME_FIELD(Battle_Ai_PerceptionView, relations, Battle_Ai_ActorAiState, relations);
SAME_FIELD(Battle_Ai_CandidateReadinessView, candidateOrder, Battle_Ai_ActorAiState, candidateOrder);
SAME_FIELD(Battle_Ai_CandidateReadinessView, candidateCount, Battle_Ai_ActorAiState, candidateCount);
SAME_FIELD(Field_GimmickPhysicsView, nativePointer68, Field_GimmickInstance, model);
SAME_FIELD(Field_GimmickPhysicsView, frame, Field_GimmickInstance, objectInfo.supportIndex);
SAME_FIELD(Field_GimmickPhysicsView, carrier, Field_GimmickInstance, objectInfo.carrier);
SAME_FIELD(Field_GimmickMotionView, nativePointer68, Field_GimmickInstance, model);
SAME_FIELD(Field_GimmickMotionView, frame, Field_GimmickInstance, objectInfo.supportIndex);
SAME_FIELD(Field_GimmickMotionView, state, Field_GimmickInstance, pulsedPalette);
SAME_FIELD(Field_GimmickMotionView, savedState, Field_GimmickInstance, basePalette);
_Static_assert(sizeof(Battle_Ai_AiCreationView) <= sizeof(Battle_Ai_ActorAiState), "creation view exceeds storage");
_Static_assert(sizeof(Battle_Ai_actorAiStates) / sizeof(Battle_Ai_actorAiStates[0]) == 16, "AI table extent");
_Static_assert(sizeof(Battle_Ai_roomHasWaypoints) == sizeof(void*), "room auxiliary-data pointer width");
_Static_assert(sizeof(Battle_Ai_ActorAiState) % 4 == 0, "AI clear requires whole words");
#undef SAME_FIELD

static int TestDecision(Battle_Ai_ActorAiState* state) { return state->actorId; }

int Tests_ActorConditionSnapshot(void)
{
    Battle_Actor first = {0}, second = {0};
    Battle_ActorStats firstStats = {0}, secondStats = {0};
    Battle_Ai_ActorAiState firstAi = {0}, secondAi = {0};
    Battle_Actor* savedHead = Battle_actors[0];
    Battle_Ai_ActorAiState* savedFirst = Battle_Ai_actorAiStates[2];
    Battle_Ai_ActorAiState* savedSecond = Battle_Ai_actorAiStates[5];
    struct { unsigned before; Battle_Ai_ActionTiming rows[16]; unsigned after; } output;
    memset(&output, 0xA5, sizeof output);
    first.id = 2;
    first.next = &second;
    first.stats = &firstStats;
    second.id = 5;
    second.stats = &secondStats;
    firstStats.currentHp = 20;
    firstStats.maxHp = 100;
    firstStats.currentMp = 15;
    firstStats.maxMp = 60;
    firstStats.statuses = (1 << 6) | (1 << 12);
    firstStats.weapon.currentDp = 10;
    firstStats.weapon.maxDp = 40;
    firstStats.shield.currentDp = 30;
    firstStats.shield.maxDp = 60;
    secondStats.maxHp = 120;
    secondStats.statuses = (1 << 0) | (1 << 12);
    secondAi.actionCostEnabled = 1;
    Battle_actors[0] = &first;
    Battle_Ai_actorAiStates[2] = &firstAi;
    Battle_Ai_actorAiStates[5] = &secondAi;
    Battle_Ai_CaptureActorConditionTable(output.rows);
    const int passed = output.rows[2].opaque0Bit0 == 2 && output.rows[5].opaque0Bit0 == 5
        && output.rows[2].opaque4 == 20 && output.rows[2].opaque6 == 100
        && output.rows[2].opaque8 == 15 && output.rows[2].opaqueA == 60
        && output.rows[2].opaque10 == 10 && output.rows[2].opaque12 == 40
        && output.rows[2].opaque14 == 30 && output.rows[2].opaque16 == 60
        && Battle_Ai_ActorStatusConditionScore(&output.rows[2]) == -34
        && Battle_Ai_ActorStatusConditionScore(&output.rows[5]) == -314
        && output.before == 0xA5A5A5A5u && output.after == 0xA5A5A5A5u
        && output.rows[1].opaqueC == 0xA5A5A5A5u;
    Battle_actors[0] = savedHead;
    Battle_Ai_actorAiStates[2] = savedFirst;
    Battle_Ai_actorAiStates[5] = savedSecond;
    return passed;
}

int Tests_GimmickViewsRoundTrip(void)
{
    Field_GimmickInstance object = {0};
    Actors_ShpModel model = {0};
    Field_GimmickPhysicsView* physics = (void*)&object;
    Field_GimmickMotionView* motion = (void*)&object;
    object.model = &model;
    object.objectInfo.supportIndex = 13;
    object.objectInfo.material = 5;
    motion->y = -64;
    int passed = physics->nativePointer68 == &model && motion->nativePointer68 == &model
        && physics->frame == 13 && motion->frame == 13 && motion->material == 5
        && object.positionY == -64;
    physics->frame = 14;
    physics->carrier = 3;
    passed = passed && motion->frame == 14 && object.objectInfo.supportIndex == 14
        && object.objectInfo.carrier == 3 && object.model == &model;
    return passed;
}

int Tests_AiViewsRoundTrip(void)
{
    struct {
        unsigned long long before;
        Battle_Ai_ActorAiState state;
        unsigned long long after;
    } guarded;
    Battle_Actor actor = {0};
    Actors_ActorRecord model = {0};
    Battle_ActorStats stats = {0};
    Battle_Ai_AiCreationView* creation = (void*)&guarded.state;
    Battle_Ai_SettingsView* settings = (void*)&guarded.state;
    Battle_Ai_PerceptionUpdateView* update = (void*)&guarded.state;
    Battle_Ai_CandidateReadinessView* planning = (void*)&guarded.state;
    Battle_Ai_AiActorView* actorView = (void*)&actor;
    Battle_Ai_CreationModelView* modelView = (void*)&model;
    unsigned int i;
    memset(&guarded, 0xA5, sizeof guarded);
    Core_Memory_ClearAlignedWords(&guarded.state, sizeof guarded.state);
    if (guarded.before != 0xA5A5A5A5A5A5A5A5ull || guarded.after != 0xA5A5A5A5A5A5A5A5ull) return 0;
    for (i = 0; i < sizeof guarded.state; ++i) {
        if (((unsigned char*)&guarded.state)[i] != 0) return 0;
    }
    actor.id = 2;
    actor.kind = 5;
    actor.stats = &stats;
    actor.model = &model.instance;
    model.instance.position.vy = -300;
    model.collisionRadius = 61;
    model.collisionHeight = 124;
    if (actorView->actorId != 2 || actorView->aiControlFlags != 5 || (void*)actorView->model != &model ||
        (void*)actorView->stats != &stats || modelView->position[1] != -300 || modelView->radius != 61 ||
        modelView->opaque63E != 124) return 0;
    creation->actor = actorView;
    creation->model = modelView;
    creation->stats = (void*)&stats;
    creation->actorId = 2;
    creation->positionSource = modelView->position;
    creation->tileSource = &modelView->tile.word;
    creation->radius = 61;
    creation->collisionRadius = 62;
    settings->decisionCallback = TestDecision;
    settings->farDistanceSquared = 0x240000;
    planning->candidateOrder[15] = (void*)&guarded.state.candidates[15];
    planning->candidateCount = 16;
    return (void*)guarded.state.actor == &actor && (void*)guarded.state.model == &model &&
        guarded.state.stats == &stats && guarded.state.actorId == 2 && guarded.state.radius == 61 &&
        settings->collisionRadius == 62 && creation->farDistanceSquared == 0x240000 &&
        (void*)update->positionSource == creation->positionSource && (void*)update->tileSource == creation->tileSource &&
        settings->decisionCallback(&guarded.state) == 2 && guarded.state.candidateCount == 16 &&
        guarded.state.candidateOrder[15] == &guarded.state.candidates[15] &&
        guarded.before == 0xA5A5A5A5A5A5A5A5ull && guarded.after == 0xA5A5A5A5A5A5A5A5ull;
}

int Tests_LayoutAssertsLinked(void) { return 1; }
