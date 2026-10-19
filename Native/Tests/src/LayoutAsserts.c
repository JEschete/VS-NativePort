// Compile-time checks of the x64 layout decisions the copy depends on
// (tools/port/copy_fixups.py TYPE_FIXES and FIXES). A failure here means a fix
// no longer holds after a ledger or decomp change.
#include <stddef.h>
#include <string.h>

#include "Actors/ActorLoading.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelRendering.h"
#include "Audio/SoundBanks.h"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/ConditionSnapshot.h"
#include "Battle/Ai/DecisionCallbacks.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Relations.h"
#include "Battle/Damage.h"
#include "Battle/TargetVolume.h"
#include "Battle/Targeting.h"
#include "Core/Memory/WordCopy.h"
#include "DiscReader.h"
#include "Effects/EffectResources.h"
#include "Effects/Runtime.h"
#include "Field/GimmickMotion.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"

int TestsRoomSfxBankBinding(void) {
  const Audio_SoundState saved_state = Audio_soundState;
  Audio_soundState = (Audio_SoundState){0};
  unsigned char bank = 0;
  unsigned char replacement = 0;
  int passed = Audio_BindRoomSfxBank((uintptr_t)&bank) == 1 &&
               Audio_soundState.sfxBankData[2] == &bank;
  passed = passed && Audio_BindRoomSfxBank((uintptr_t)&replacement) == 0 &&
           Audio_soundState.sfxBankData[2] == &bank &&
           Audio_soundState.sfxBankData[0] == NULL &&
           Audio_soundState.sfxBankData[1] == NULL;
  for (size_t slot = 0; slot < sizeof Audio_soundState.musicHandles /
                                   sizeof Audio_soundState.musicHandles[0];
       ++slot) {
    passed = passed && Audio_soundState.musicHandles[slot] == 0 &&
             Audio_soundState.musicData[slot] == NULL &&
             Audio_soundState.musicRequests[slot] == NULL;
  }
  Audio_soundState = saved_state;
  return passed;
}

int TestsDiscReadSizeContract(void) {
  const Content_DiscDrive saved = Content_discDriveState;
  Content_discDriveState = (Content_DiscDrive){0};
  Content_discDriveState.byteCount = 7;
  unsigned char destination[2049];
  int passed = Content_BeginDiscRead(1, UINT32_MAX, destination) == 0 &&
               Content_discDriveState.state == Content_DiscDriveState_Idle &&
               Content_discDriveState.byteCount == 7 &&
               Content_discDriveState.destination == NULL;
  passed = passed &&
           Content_BeginDiscRead(1, sizeof destination, destination) == 1 &&
           Content_discDriveState.state == Content_DiscDriveState_ReadInit &&
           Content_discDriveState.byteCount == sizeof destination &&
           Content_discDriveState.sectorCount == 2 &&
           Content_discDriveState.destination == destination;
  passed = passed && Content_BeginDiscRead(2, 0, NULL) == 0 &&
           Content_discDriveState.startSector == 1;
  Content_discDriveState = saved;
  return passed;
}

_Static_assert(sizeof(Battle_ActionDefinition) == 52,
               "action definition disc stride");
_Static_assert(offsetof(Battle_ActionDefinition, cost) == 3,
               "action cost offset");
_Static_assert(offsetof(Battle_ActionDefinition, aiFlags) == 14,
               "action AI flags offset");
_Static_assert(offsetof(Battle_ActionDefinition, hits) == 20,
               "action hit definitions offset");
_Static_assert(offsetof(Battle_ActionDefinition, name) == 28,
               "action name offset");
_Static_assert(sizeof(Battle_TargetSphereSetupView) >=
                   sizeof(Battle_TargetSphere),
               "sphere allocation extent");
_Static_assert(offsetof(Battle_TargetSphereSetupView, wireVertices) ==
                   offsetof(Battle_TargetSphere, wireVertices),
               "sphere vertex view offset");
_Static_assert(offsetof(Battle_TargetSphereSetupView, wireEdges) ==
                   offsetof(Battle_TargetSphere, wireEdges),
               "sphere edge view offset");
_Static_assert(offsetof(Battle_TargetSphereLabelView, opaque990) ==
                   offsetof(Battle_TargetSphere, labels),
               "sphere label view offset");

// An actor record placed at core.model (Content_ParseActorModel) must end
// before the weapon model slots.
_Static_assert(offsetof(Actors_ActorAllocation, modelSlots) -
                       offsetof(Actors_ActorAllocation, core.model) >=
                   sizeof(Actors_ActorRecord),
               "the actor record overlaps the weapon model slots");

// Actors_SpawnRoomEnemy: `actor + 1` is the stats block of the allocation.
_Static_assert(offsetof(Actors_ActorAllocation, core.actor.stats) ==
                   sizeof(Battle_Actor),
               "stats do not follow the Battle_Actor");

// On-disc structs keep the file layout (ONDISC_OFFSETS): the SHP header is 64
// bytes, the bones follow.
_Static_assert(sizeof(Actors_ShpModel) == 64, "SHP header layout");
_Static_assert(offsetof(Actors_ShpModel, bones) == 64, "SHP bones offset");
_Static_assert(offsetof(Actors_SeqAnimationBank, headers) == 8,
               "SEQ bank header layout");
_Static_assert(sizeof(((Actors_ActorRecord*)0)->animationHeader) ==
                   sizeof(void*),
               "SEQ bank cache must be pointer-sized");

// Runtime views must follow the native object, including pointers hidden in the
// original PS1 padding.
#define SAME_FIELD(View, member, Record, field)                     \
  _Static_assert(offsetof(View, member) == offsetof(Record, field), \
                 "native view mismatch: " #View "." #member)
SAME_FIELD(Battle_Ai_AiActorView, actorId, Battle_Actor, id);
SAME_FIELD(Battle_Ai_AiActorView, aiControlFlags, Battle_Actor, kind);
SAME_FIELD(Battle_Ai_AiActorView, stats, Battle_Actor, stats);
SAME_FIELD(Battle_Ai_AiActorView, model, Battle_Actor, model);
SAME_FIELD(Battle_Ai_SettingsActorView, opaque34, Battle_Actor, reserved34);
SAME_FIELD(Battle_Ai_CreationModelView, radius, Actors_ActorRecord,
           collisionRadius);
SAME_FIELD(Battle_Ai_CreationModelView, opaque63E, Actors_ActorRecord,
           collisionHeight);
SAME_FIELD(Battle_Ai_AiCreationView, actor, Battle_Ai_ActorAiState, actor);
SAME_FIELD(Battle_Ai_AiCreationView, model, Battle_Ai_ActorAiState, model);
SAME_FIELD(Battle_Ai_AiCreationView, stats, Battle_Ai_ActorAiState, stats);
SAME_FIELD(Battle_Ai_AiCreationView, actorId, Battle_Ai_ActorAiState, actorId);
SAME_FIELD(Battle_Ai_AiCreationView, pointHeight, Battle_Ai_ActorAiState,
           pointHeight);
SAME_FIELD(Battle_Ai_SettingsView, decisionCallback, Battle_Ai_ActorAiState,
           nativePointer68);
SAME_FIELD(Battle_Ai_PerceptionView, relations, Battle_Ai_ActorAiState,
           relations);
SAME_FIELD(Battle_Ai_CandidateReadinessView, candidateOrder,
           Battle_Ai_ActorAiState, candidateOrder);
SAME_FIELD(Battle_Ai_CandidateReadinessView, candidateCount,
           Battle_Ai_ActorAiState, candidateCount);
SAME_FIELD(Field_GimmickPhysicsView, nativePointer68, Field_GimmickInstance,
           model);
SAME_FIELD(Field_GimmickPhysicsView, frame, Field_GimmickInstance,
           objectInfo.supportIndex);
SAME_FIELD(Field_GimmickPhysicsView, carrier, Field_GimmickInstance,
           objectInfo.carrier);
SAME_FIELD(Field_GimmickMotionView, nativePointer68, Field_GimmickInstance,
           model);
SAME_FIELD(Field_GimmickMotionView, frame, Field_GimmickInstance,
           objectInfo.supportIndex);
SAME_FIELD(Field_GimmickMotionView, state, Field_GimmickInstance,
           pulsedPalette);
SAME_FIELD(Field_GimmickMotionView, savedState, Field_GimmickInstance,
           basePalette);
_Static_assert(sizeof(Battle_Ai_AiCreationView) <=
                   sizeof(Battle_Ai_ActorAiState),
               "creation view exceeds storage");
_Static_assert(sizeof(Battle_Ai_actorAiStates) /
                       sizeof(Battle_Ai_actorAiStates[0]) ==
                   16,
               "AI table extent");
_Static_assert(sizeof(Battle_Ai_roomHasWaypoints) == sizeof(void*),
               "room auxiliary-data pointer width");
_Static_assert(sizeof(Battle_Ai_ActorAiState) % 4 == 0,
               "AI clear requires whole words");
#undef SAME_FIELD

static int TestDecision(Battle_Ai_ActorAiState* state) {
  return state->actorId;
}

static int TestMovementDecision(Battle_Ai_ActorAiState* state) {
  ++state->opaque44;
  return 1;
}

int Tests_AiMovementDispatch(void) {
  Battle_Ai_ActorAiState state = {0};
  Battle_Ai_SettingsView* settings = (void*)&state;
  state.previousTile = -1;
  settings->decisionCallback = TestMovementDecision;
  Battle_Ai_DispatchMovementBehavior((void*)&state);
  const int passed = state.opaque44 == 1 && state.detourStep == 0;
  Battle_Ai_UpdateActorLocomotionMode((void*)&state);
  return passed && state.behaviorMode == 0;
}

int Tests_AiRelations(void) {
  Battle_Actor actors[3] = {{0}};
  Battle_Ai_ActorAiState states[3] = {{0}};
  const unsigned ids[3] = {0, 2, 5};
  Battle_Actor* savedHead = Battle_actors[0];
  Battle_Ai_ActorAiState* savedStates[3];
  for (unsigned index = 0; index < 3; ++index) {
    savedStates[index] = Battle_Ai_actorAiStates[ids[index]];
    actors[index].id = ids[index];
    actors[index].next = index + 1 < 3 ? &actors[index + 1] : NULL;
    states[index].actorId = ids[index];
    states[index].savedRelationOptions = -1;
    Battle_Ai_actorAiStates[ids[index]] = &states[index];
  }
  states[0].settingsBits21 = 3;
  states[1].settingsBits21 = 0;
  states[2].settingsBits21 = 2;
  states[1].relations[5].visible = 1;
  Battle_actors[0] = actors;
  Battle_Ai_InitializeActorAiRelations((void*)&states[1]);
  int passed = states[1].relations[0].classification == 3 &&
               states[0].relations[2].classification == 3 &&
               states[1].relations[5].classification == 2 &&
               states[2].relations[2].classification == 2 &&
               states[1].relations[5].visible == 1;
  for (unsigned index = 0; index < 3; ++index) {
    passed = passed && states[index].relations[ids[index]].classification == 2;
  }
  states[1].savedRelationOptions = 0x1234;
  states[1].relations[0].active = 1;
  states[1].relations[0].visible = 1;
  states[1].relations[0].counter = 37;
  Battle_Ai_ApplySavedActorAiRelations();
  passed =
      passed && states[1].relations[0].classification == ((0x1234 >> 1) & 3) &&
      states[1].relations[0].previousClassification == ((0x1234 >> 3) & 3) &&
      states[1].relations[0].classificationTimer == ((0x1234 >> 6) & 0x7F) &&
      states[1].relations[0].active == 0 &&
      states[1].relations[0].visible == 1 &&
      states[1].relations[0].counter == 37 &&
      states[0].relations[2].classification == 3;
  Battle_actors[0] = savedHead;
  for (unsigned index = 0; index < 3; ++index) {
    Battle_Ai_actorAiStates[ids[index]] = savedStates[index];
  }
  return passed;
}

int Tests_AiMovementSetup(void) {
  Battle_Ai_ActorAiState state = {0};
  state.decisionDelay = 2;
  state.minActionDelay = 1;
  Battle_Ai_TickActorDecisionTimers((void*)&state);
  int passed = state.decisionDelay == 1 && state.minActionDelay == 0;
  Battle_Ai_TickActorDecisionTimers((void*)&state);
  Battle_Ai_TickActorDecisionTimers((void*)&state);
  passed = passed && state.decisionDelay == 0 && state.minActionDelay == 0;
  const u_short flags[3] = {0x1000, 0x800, 0x100};
  const u_char modes[3] = {1, 2, 0};
  for (size_t index = 0; index < sizeof flags / sizeof flags[0]; ++index) {
    state.locomotionFlags = flags[index];
    Battle_Ai_InitializeActorLocomotionMode((void*)&state);
    passed = passed && state.locomotionMode == modes[index] &&
             state.movementFlags == (flags[index] | (flags[index] >> 8));
  }
  Battle_Ai_ActorAiState* saved = Battle_Ai_actorAiStates[2];
  Battle_Ai_actorAiStates[2] = &state;
  Battle_Ai_SetActorVerticalMovementSteps(2, 8, 12);
  passed = passed && state.walkStep == 8 && state.runStep == 12 &&
           state.maxRange == 8;
  Battle_Ai_SetActorVerticalMovementSteps(2, 16, 6);
  passed = passed && state.walkStep == 16 && state.runStep == 6 &&
           state.maxRange == 6;
  Battle_Ai_actorAiStates[2] = NULL;
  Battle_Ai_SetActorVerticalMovementSteps(2, 4, 8);
  passed = passed && state.walkStep == 16 && state.runStep == 6;
  Battle_Ai_actorAiStates[2] = saved;
  return passed;
}

int Tests_TargetSphereViews(void) {
  Battle_TargetSphereSetupView setup = {0};
  Battle_TargetSphere* sphere = (void*)&setup;
  Battle_TargetSphereLabelView* labels = (void*)&setup;
  Battle_TargetPreview targets[2] = {{0}};
  char text[] = "limb";
  setup.targetList0 = &targets[0];
  setup.targetList1 = &targets[1];
  sphere->labels[23].text = text;
  setup.wireEdges[769] = 0x1234;
  return sphere->targetLists[0] == &targets[0] &&
         sphere->targetLists[1] == &targets[1] &&
         labels->opaque40 == (void*)&targets[0] &&
         labels->opaque44 == (void*)&targets[1] &&
         labels->opaque990[23].limbName == text &&
         ((short*)sphere->wireEdges)[769] == 0x1234;
}

int Tests_AiFrameStages(void) {
  const Battle_Ai_FrameContext savedFrame = *Battle_Ai_FrameState();
  Battle_Actor* savedActor = Battle_actors[0];
  Battle_Ai_ActorAiState* savedState = Battle_Ai_actorAiStates[0];
  Battle_Ai_RoomAiState* savedRoom = Battle_Ai_roomAiState;
  u_short(*savedFlags)[32] = Battle_Ai_tileFlagMap;
  const int savedTarget = Battle_Ai_secondPerceptionTarget;
  const int savedMask = Battle_Ai_attributeActorMask;
  const int savedRandom = Battle_Ai_savedMovementRandomState;
  Battle_Actor actor = {0};
  Battle_ActorStats stats = {0};
  Actors_ActorRecord model = {0};
  Battle_Ai_ActorAiState state = {0};
  Battle_Ai_PerceptionUpdateView* view = (void*)&state;
  Battle_Ai_RoomAiState room = {0};
  u_short flags[32][32] = {{0}};
  actor.stats = &stats;
  view->actor = (void*)&actor;
  view->model = (void*)&model;
  Battle_actors[0] = &actor;
  Battle_Ai_actorAiStates[0] = &state;
  Battle_Ai_roomAiState = &room;
  Battle_Ai_tileFlagMap = NULL;
  Battle_Ai_secondPerceptionTarget = 0;
  Battle_Ai_attributeActorMask = 1 | (1 << 2) | (1 << 5);
  Battle_Ai_savedMovementRandomState = 0x12345678;
  Battle_Ai_RunActorAiUpdateStages();
  int passed =
      room.readinessLevel == 0 && Battle_Ai_secondPerceptionTarget == 0;
  Battle_Ai_tileFlagMap = flags;
  Battle_Ai_RunActorAiUpdateStages();
  passed = passed && room.readinessLevel == 1 &&
           Battle_Ai_secondPerceptionTarget == 2;
  Battle_Ai_RunActorAiUpdateStages();
  passed = passed && room.readinessLevel == 2 &&
           Battle_Ai_secondPerceptionTarget == 5;
  Battle_Ai_RunActorAiUpdateStages();
  passed = passed && room.readinessLevel == 3 &&
           Battle_Ai_secondPerceptionTarget == 0 &&
           Battle_Ai_savedMovementRandomState == 0x12345678 &&
           view->opaque14 == 1;
  *Battle_Ai_FrameState() = savedFrame;
  Battle_actors[0] = savedActor;
  Battle_Ai_actorAiStates[0] = savedState;
  Battle_Ai_roomAiState = savedRoom;
  Battle_Ai_tileFlagMap = savedFlags;
  Battle_Ai_secondPerceptionTarget = savedTarget;
  Battle_Ai_attributeActorMask = savedMask;
  Battle_Ai_savedMovementRandomState = savedRandom;
  return passed;
}

int Tests_AiFrameCache(void) {
  enum { kScratchSentinel = 0xA5 };
  const Battle_Ai_FrameContext savedFrame = *Battle_Ai_FrameState();
  Battle_Ai_ActorAiState* savedActors[16];
  memcpy(savedActors, Battle_Ai_actorAiStates, sizeof savedActors);
  Battle_Ai_RoomAiState* savedRoom = Battle_Ai_roomAiState;
  u_short(*savedFlags)[32] = Battle_Ai_tileFlagMap;
  u_short(*savedHeights)[32] = Battle_Ai_specialTerrainHeights;
  void* savedData = Battle_Ai_roomHasWaypoints;
  const int savedRandom = Battle_Ai_savedMovementRandomState;
  u_char savedScratch[132];
  u_char* scratch = (u_char*)Core_ScratchBase() + 0x37C;
  memcpy(savedScratch, scratch, sizeof savedScratch);
  for (size_t index = 0; index < sizeof savedScratch; ++index) {
    scratch[index] = kScratchSentinel;
  }
  Battle_Ai_ActorAiState actor = {0};
  Battle_Ai_RoomAiState room = {0};
  u_short flags[32][32] = {{0}};
  u_short heights[32][32] = {{0}};
  u_char auxiliary[0x898] = {0};
  const u_int bounds = 0x01021F1Eu;
  memcpy(room.reserved4, &bounds, sizeof bounds);
  memset(Battle_Ai_actorAiStates, 0, sizeof savedActors);
  Battle_Ai_actorAiStates[15] = &actor;
  Battle_Ai_roomAiState = &room;
  Battle_Ai_tileFlagMap = flags;
  Battle_Ai_specialTerrainHeights = heights;
  Battle_Ai_roomHasWaypoints = auxiliary;
  Battle_Ai_savedMovementRandomState = 0x12345678;
  const int returned = Battle_Ai_CacheActorAiWorkingState();
  const Battle_Ai_FrameContext* frame = Battle_Ai_FrameState();
  int passed = returned == (int)bounds && frame->actors[15] == &actor &&
               frame->actors[0] == NULL && frame->room == &room &&
               frame->tileFlags == flags && frame->heightMap == heights &&
               frame->roomData == auxiliary &&
               frame->terrainRegions == auxiliary + 0x498 &&
               frame->randomState == 0x12345678 &&
               memcmp(frame->directions, Battle_Ai_directionDeltas,
                      sizeof frame->directions) == 0;
  for (unsigned index = 0; index < sizeof savedScratch; ++index) {
    passed = passed && scratch[index] == kScratchSentinel;
  }
  *Battle_Ai_FrameState() = savedFrame;
  memcpy(Battle_Ai_actorAiStates, savedActors, sizeof savedActors);
  Battle_Ai_roomAiState = savedRoom;
  Battle_Ai_tileFlagMap = savedFlags;
  Battle_Ai_specialTerrainHeights = savedHeights;
  Battle_Ai_roomHasWaypoints = savedData;
  Battle_Ai_savedMovementRandomState = savedRandom;
  memcpy(scratch, savedScratch, sizeof savedScratch);
  return passed;
}

int Tests_DefaultEffectPackageLoaded(void) {
  if (sizeof Effects_emptyPackage != 0x5200 ||
      Effects_emptyPackage[0] != 0x00010A70 ||
      Effects_emptyPackage[0x51FC / sizeof(u_int)] != 0) {
    return 0;
  }
  Effects_EffectPackage package = {0};
  Effects_EffectPackage* saved = Effects_package;
  const uintptr_t base = (uintptr_t)Effects_emptyPackage;
  Effects_package = &package;
  Effects_BindEffectPackageBlocks((Effects_PackageFile*)Effects_emptyPackage);
  const int passed =
      package.block1TableCount == 1 && package.block2Count == 19 &&
      package.block3Data->count == 49 && package.curveBlock->count == 32 &&
      package.block5Data->reserved0 == 46 &&
      (uintptr_t)package.block8Data == base + 0x4B44 &&
      (uintptr_t)package.block9Data == base + 0x5078 &&
      (uintptr_t)package.block10Data == base + 0x50A0 &&
      (uintptr_t)package.block11Data == base + 0x51BC;
  Effects_package = saved;
  return passed;
}

int Tests_ActionDefinitionsLoaded(void) {
  const Battle_ActionDefinition* basic = &Battle_actionDefinitions[1];
  return basic->id == 1 && basic->effectId == 1 &&
         basic->type == Battle_ActionType_BaseAttack && basic->category == 14 &&
         basic->cost == 255 && basic->rangeX == 255 && basic->rangeY == 255 &&
         basic->rangeZ == 255 && basic->hits[0].effectId == 58 &&
         basic->hits[0].prerequisiteFunctionId == 2 &&
         basic->hits[0].hitFunctionId == 3 &&
         basic->hits[0].statCalculatorId == 25 &&
         Battle_categoryNeedsTargetSelection[basic->category] != 0 &&
         Battle_categoryTargetsBodyParts[basic->category] != 0;
}

int Tests_ActorConditionSnapshot(void) {
  Battle_Actor first = {0}, second = {0};
  Battle_ActorStats firstStats = {0}, secondStats = {0};
  Battle_Ai_ActorAiState firstAi = {0}, secondAi = {0};
  Battle_Actor* savedHead = Battle_actors[0];
  Battle_Ai_ActorAiState* savedFirst = Battle_Ai_actorAiStates[2];
  Battle_Ai_ActorAiState* savedSecond = Battle_Ai_actorAiStates[5];
  struct {
    unsigned before;
    Battle_Ai_ActionTiming rows[16];
    unsigned after;
  } output;
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
  const int passed =
      output.rows[2].opaque0Bit0 == 2 && output.rows[5].opaque0Bit0 == 5 &&
      output.rows[2].opaque4 == 20 && output.rows[2].opaque6 == 100 &&
      output.rows[2].opaque8 == 15 && output.rows[2].opaqueA == 60 &&
      output.rows[2].opaque10 == 10 && output.rows[2].opaque12 == 40 &&
      output.rows[2].opaque14 == 30 && output.rows[2].opaque16 == 60 &&
      Battle_Ai_ActorStatusConditionScore(&output.rows[2]) == -34 &&
      Battle_Ai_ActorStatusConditionScore(&output.rows[5]) == -314 &&
      output.before == 0xA5A5A5A5u && output.after == 0xA5A5A5A5u &&
      output.rows[1].opaqueC == 0xA5A5A5A5u;
  Battle_actors[0] = savedHead;
  Battle_Ai_actorAiStates[2] = savedFirst;
  Battle_Ai_actorAiStates[5] = savedSecond;
  return passed;
}

int Tests_GimmickViewsRoundTrip(void) {
  Field_GimmickInstance object = {0};
  Actors_ShpModel model = {0};
  Field_GimmickPhysicsView* physics = (void*)&object;
  Field_GimmickMotionView* motion = (void*)&object;
  object.model = &model;
  object.objectInfo.supportIndex = 13;
  object.objectInfo.material = 5;
  motion->y = -64;
  int passed = physics->nativePointer68 == &model &&
               motion->nativePointer68 == &model && physics->frame == 13 &&
               motion->frame == 13 && motion->material == 5 &&
               object.positionY == -64;
  physics->frame = 14;
  physics->carrier = 3;
  passed = passed && motion->frame == 14 &&
           object.objectInfo.supportIndex == 14 &&
           object.objectInfo.carrier == 3 && object.model == &model;
  return passed;
}

int Tests_AiViewsRoundTrip(void) {
  const uint64_t guard_pattern = UINT64_C(0xA5A5A5A5A5A5A5A5);
  enum { kCollisionHeight = 124 };
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
  if (guarded.before != guard_pattern || guarded.after != guard_pattern) {
    return 0;
  }
  for (i = 0; i < sizeof guarded.state; ++i) {
    if (((unsigned char*)&guarded.state)[i] != 0) return 0;
  }
  actor.id = 2;
  actor.kind = 5;
  actor.stats = &stats;
  actor.model = &model.instance;
  model.instance.position.vy = -300;
  model.collisionRadius = 61;
  model.collisionHeight = kCollisionHeight;
  if (actorView->actorId != 2 || actorView->aiControlFlags != 5 ||
      (void*)actorView->model != &model || (void*)actorView->stats != &stats ||
      modelView->position[1] != -300 || modelView->radius != 61 ||
      modelView->opaque63E != kCollisionHeight) {
    return 0;
  }
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
  return (void*)guarded.state.actor == &actor &&
         (void*)guarded.state.model == &model &&
         guarded.state.stats == &stats && guarded.state.actorId == 2 &&
         guarded.state.radius == 61 && settings->collisionRadius == 62 &&
         creation->farDistanceSquared == 0x240000 &&
         (void*)update->positionSource == creation->positionSource &&
         (void*)update->tileSource == creation->tileSource &&
         settings->decisionCallback(&guarded.state) == 2 &&
         guarded.state.candidateCount == 16 &&
         guarded.state.candidateOrder[15] == &guarded.state.candidates[15] &&
         guarded.before == guard_pattern && guarded.after == guard_pattern;
}

int Tests_LayoutAssertsLinked(void) { return 1; }
