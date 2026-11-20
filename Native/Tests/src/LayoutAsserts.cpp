// Compile-time checks of the x64 layout decisions the copy depends on
// (tools/port/copy_fixups.py TYPE_FIXES and FIXES). A failure here means a fix
// no longer holds after a ledger or decomp change.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <type_traits>

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundState.hpp"
#include "Battle/ActionTable.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/ConditionSnapshotBinding.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Battle/Damage.h"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingState.h"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "DiscQueue.h"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/Runtime.hpp"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"

static_assert(
    sizeof(Actors_ActorCore) - offsetof(Actors_ActorCore, model) >=
        offsetof(Field_GimmickInstance, reserved178),
    "The auxiliary actor allocation must own the native gimmick prefix");

static_assert(
    offsetof(Battle_Ai_ActorAiState, engagementFrames) == 170,
    "Renamed engagement state must retain its verified native layout");
static_assert(offsetof(Battle_Ai_ActorAiState, engagementCooldownFrames) == 180,
              "Engagement cooldown layout changed");
static_assert(offsetof(Battle_Ai_ActorAiState, reengagementGraceFrames) == 182,
              "Reengagement grace layout changed");
static_assert(offsetof(Battle_Ai_ActorAiState, awarenessFramesRemaining) == 326,
              "Awareness duration layout changed");
static_assert(offsetof(Battle_Ai_ActorAiState, awarenessMarker) == 340,
              "Awareness marker layout changed");

int TestsRoomSfxBankBinding(void) {
  const VS::Audio::SoundState saved_state = VS::Audio::soundState;
  VS::Audio::soundState = (VS::Audio::SoundState){0};
  unsigned char bank = 0;
  unsigned char replacement = 0;
  int passed =
      VS::Audio::LiveBindRoomSfxBank(reinterpret_cast<uintptr_t>(&bank)) == 1 &&
      VS::Audio::soundState.sfx_bank_data[2] == &bank;
  passed = passed &&
           VS::Audio::LiveBindRoomSfxBank(
               reinterpret_cast<uintptr_t>(&replacement)) == 0 &&
           VS::Audio::soundState.sfx_bank_data[2] == &bank &&
           VS::Audio::soundState.sfx_bank_data[0] == nullptr &&
           VS::Audio::soundState.sfx_bank_data[1] == nullptr;
  for (size_t slot = 0;
       slot < sizeof VS::Audio::soundState.music_handles /
                  sizeof VS::Audio::soundState.music_handles[0];
       ++slot) {
    passed = passed && VS::Audio::soundState.music_handles[slot] == 0 &&
             VS::Audio::soundState.music_data[slot] == nullptr &&
             VS::Audio::soundState.music_requests[slot] == nullptr;
  }
  VS::Audio::soundState = saved_state;
  return passed;
}

int TestsDiscReadSizeContract(void) {
  const Content_DiscDrive saved = Content_discDriveState;
  Content_discDriveState = (Content_DiscDrive){0};
  Content_discDriveState.byteCount = 7;
  unsigned char destination[2049];
  int passed = !VS::Content::BeginLiveDiscRead(1, UINT32_MAX, destination) &&
               Content_discDriveState.state == Content_DiscDriveState_Idle &&
               Content_discDriveState.byteCount == 7 &&
               Content_discDriveState.destination == nullptr;
  passed = passed &&
           VS::Content::BeginLiveDiscRead(1, sizeof destination, destination) &&
           Content_discDriveState.state == Content_DiscDriveState_ReadInit &&
           Content_discDriveState.byteCount == sizeof destination &&
           Content_discDriveState.sectorCount == 2 &&
           Content_discDriveState.destination == destination;
  passed = passed && !VS::Content::BeginLiveDiscRead(2, 0, nullptr) &&
           Content_discDriveState.startSector == 1;
  Content_discDriveState = saved;
  return passed;
}

static_assert(sizeof(Battle_ActionDefinition) == 52,
              "action definition disc stride");
static_assert(offsetof(Battle_ActionDefinition, cost) == 3,
              "action cost offset");
static_assert(offsetof(Battle_ActionDefinition, aiFlags) == 14,
              "action AI flags offset");
static_assert(offsetof(Battle_ActionDefinition, hits) == 20,
              "action hit definitions offset");
static_assert(offsetof(Battle_ActionDefinition, name) == 28,
              "action name offset");
static_assert(sizeof(Battle_TargetSphereSetupView) >=
                  sizeof(Battle_TargetSphere),
              "sphere allocation extent");
static_assert(offsetof(Battle_TargetSphereSetupView, wireVertices) ==
                  offsetof(Battle_TargetSphere, wireVertices),
              "sphere vertex view offset");
static_assert(offsetof(Battle_TargetSphereSetupView, wireEdges) ==
                  offsetof(Battle_TargetSphere, wireEdges),
              "sphere edge view offset");
static_assert(offsetof(Battle_TargetSphereLabelView, opaque990) ==
                  offsetof(Battle_TargetSphere, labels),
              "sphere label view offset");

// An actor record placed at core.model (VS::Content::ParseActorModel) must
// end before the weapon model slots.
static_assert(sizeof(VS::Actors::ActorModelSlot) >= sizeof(Actors_WeaponModel),
              "equipment slot must fit a native weapon model");
static_assert(_Alignof(VS::Actors::ActorModelSlot) >=
                  _Alignof(Actors_WeaponModel),
              "equipment slot must align a native weapon model");
static_assert(offsetof(Actors_ActorAllocation, modelSlots) %
                      _Alignof(Actors_ActorRecord) ==
                  0,
              "secondary body record must be aligned");
static_assert(offsetof(Actors_ActorAllocation, modelSlots) -
                      offsetof(Actors_ActorAllocation, core.model) >=
                  sizeof(Actors_ActorRecord),
              "the actor record overlaps the weapon model slots");

// VS::Actors::LiveActorSpawnHost (ActorSpawningLive.cpp) addresses the stats
// block of an allocation as `actor + 1`.
static_assert(offsetof(Actors_ActorAllocation, core.actor.stats) ==
                  sizeof(Battle_Actor),
              "stats do not follow the Battle_Actor");

// On-disc structs keep the file layout (ONDISC_OFFSETS): the SHP header is 64
// bytes, the bones follow.
static_assert(sizeof(Actors_ShpModel) == 64, "SHP header layout");
static_assert(offsetof(Actors_ShpModel, bones) == 64, "SHP bones offset");
static_assert(offsetof(Actors_SeqAnimationBank, headers) == 8,
              "SEQ bank header layout");
static_assert(
    sizeof((static_cast<Actors_ActorRecord*>(nullptr))->animationHeader) ==
        sizeof(void*),
    "SEQ bank cache must be pointer-sized");

// Runtime views must follow the native object, including pointers hidden in the
// original PS1 padding.
#define SAME_FIELD(View, member, Record, field)                    \
  static_assert(offsetof(View, member) == offsetof(Record, field), \
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
static_assert(sizeof(Battle_Ai_AiCreationView) <=
                  sizeof(Battle_Ai_ActorAiState),
              "creation view exceeds storage");
static_assert(std::tuple_size_v<decltype(VS::Battle::Ai::actorAiStates)> == 16,
              "AI table extent");
static_assert(std::is_same_v<decltype(VS::Battle::Ai::roomHasWaypoints), void*>,
              "room auxiliary-data pointer width");
static_assert(sizeof(Battle_Ai_ActorAiState) % 4 == 0,
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
  Battle_Ai_SettingsView* settings =
      reinterpret_cast<Battle_Ai_SettingsView*>(&state);
  state.previousTile = -1;
  settings->decisionCallback = TestMovementDecision;
  VS::Battle::Ai::DispatchLiveMovementBehavior(
      reinterpret_cast<Battle_Ai_DecisionDispatchView*>(&state));
  const int passed = state.opaque44 == 1 && state.detourStep == 0;
  VS::Battle::Ai::UpdateLiveActorLocomotionMode(
      reinterpret_cast<Battle_Ai_DecisionDispatchView*>(&state));
  return passed && state.behaviorMode == 0;
}

int Tests_AiRelations(void) {
  Battle_Actor actors[3] = {{nullptr}};
  Battle_Ai_ActorAiState states[3] = {{0}};
  const unsigned ids[3] = {0, 2, 5};
  Battle_Actor* savedHead = Battle_actors[0];
  Battle_Ai_ActorAiState* savedStates[3];
  for (unsigned index = 0; index < 3; ++index) {
    savedStates[index] = VS::Battle::Ai::actorAiStates[ids[index]];
    actors[index].id = ids[index];
    actors[index].next = index + 1 < 3 ? &actors[index + 1] : nullptr;
    states[index].actorId = ids[index];
    states[index].savedRelationOptions = -1;
    VS::Battle::Ai::actorAiStates[ids[index]] = &states[index];
  }
  states[0].settingsBits21 = 3;
  states[1].settingsBits21 = 0;
  states[2].settingsBits21 = 2;
  states[1].relations[5].visible = 1;
  Battle_actors[0] = actors;
  VS::Battle::Ai::InitializeLiveActorAiRelations(
      reinterpret_cast<Battle_Ai_AiCreationView*>(&states[1]));
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
  VS::Battle::Ai::ApplyLiveSavedActorAiRelations();
  passed =
      passed && states[1].relations[0].classification == ((0x1234 >> 1) & 3) &&
      states[1].relations[0].previous_classification == ((0x1234 >> 3) & 3) &&
      states[1].relations[0].classification_timer == ((0x1234 >> 6) & 0x7F) &&
      states[1].relations[0].active == 0 &&
      states[1].relations[0].visible == 1 &&
      states[1].relations[0].counter == 37 &&
      states[0].relations[2].classification == 3;
  Battle_actors[0] = savedHead;
  for (unsigned index = 0; index < 3; ++index) {
    VS::Battle::Ai::actorAiStates[ids[index]] = savedStates[index];
  }
  return passed;
}

int Tests_AiMovementSetup(void) {
  Battle_Ai_ActorAiState state = {0};
  state.decisionDelay = 2;
  state.minActionDelay = 1;
  VS::Battle::Ai::TickLiveActorDecisionTimers(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&state));
  int passed = state.decisionDelay == 1 && state.minActionDelay == 0;
  VS::Battle::Ai::TickLiveActorDecisionTimers(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&state));
  VS::Battle::Ai::TickLiveActorDecisionTimers(
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&state));
  passed = passed && state.decisionDelay == 0 && state.minActionDelay == 0;
  const u_short flags[3] = {0x1000, 0x800, 0x100};
  const u_char modes[3] = {1, 2, 0};
  for (size_t index = 0; index < sizeof flags / sizeof flags[0]; ++index) {
    state.locomotionFlags = flags[index];
    VS::Battle::Ai::InitializeLiveActorLocomotionMode(
        reinterpret_cast<Battle_Ai_AiCreationView*>(&state));
    passed = passed && state.locomotionMode == modes[index] &&
             state.movementFlags == (flags[index] | (flags[index] >> 8));
  }
  Battle_Ai_ActorAiState* saved = VS::Battle::Ai::actorAiStates[2];
  VS::Battle::Ai::actorAiStates[2] = &state;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(2, 8, 12);
  passed = passed && state.walkStep == 8 && state.runStep == 12 &&
           state.maxRange == 12;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(2, 16, 6);
  passed = passed && state.walkStep == 16 && state.runStep == 6 &&
           state.maxRange == 16;
  VS::Battle::Ai::actorAiStates[2] = nullptr;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(2, 4, 8);
  passed = passed && state.walkStep == 16 && state.runStep == 6;
  VS::Battle::Ai::actorAiStates[2] = saved;
  return passed;
}

int Tests_TargetSphereViews(void) {
  Battle_TargetSphereSetupView setup = {0};
  Battle_TargetSphere* sphere = reinterpret_cast<Battle_TargetSphere*>(&setup);
  Battle_TargetSphereLabelView* labels =
      reinterpret_cast<Battle_TargetSphereLabelView*>(&setup);
  Battle_TargetPreview targets[2] = {{0}};
  char text[] = "limb";
  setup.targetList0 = &targets[0];
  setup.targetList1 = &targets[1];
  sphere->labels[23].text = text;
  setup.wireEdges[769] = 0x1234;
  return sphere->targetLists[0] == &targets[0] &&
         sphere->targetLists[1] == &targets[1] &&
         labels->opaque40 == reinterpret_cast<void*>(&targets[0]) &&
         labels->opaque44 == reinterpret_cast<void*>(&targets[1]) &&
         labels->opaque990[23].limbName == text &&
         (reinterpret_cast<int16_t*>(sphere->wireEdges))[769] == 0x1234;
}

int Tests_AiFrameStages(void) {
  const VS::Battle::Ai::FrameContext savedFrame =
      VS::Battle::Ai::LiveFrameContext();
  Battle_Actor* savedActor = Battle_actors[0];
  Battle_Ai_ActorAiState* saved_state = VS::Battle::Ai::actorAiStates[0];
  Battle_Ai_RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  u_short(*saved_flags)[32] = VS::Battle::Ai::tileFlagMap;
  const int saved_target = VS::Battle::Ai::secondPerceptionTarget;
  const int saved_mask = VS::Battle::Ai::attributeActorMask;
  const int saved_random = VS::Battle::Ai::savedMovementRandomState;
  Battle_Actor actor = {nullptr};
  Battle_ActorStats stats = {0};
  Actors_ActorRecord model = {0};
  Battle_Ai_ActorAiState state = {0};
  Battle_Ai_PerceptionUpdateView* view =
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&state);
  Battle_Ai_RoomAiState room = {0};
  u_short flags[32][32] = {{0}};
  actor.stats = &stats;
  view->actor = reinterpret_cast<Battle_Actor*>(&actor);
  view->model = reinterpret_cast<Battle_Ai_PerceivedObjectView*>(&model);
  Battle_actors[0] = &actor;
  VS::Battle::Ai::actorAiStates[0] = &state;
  VS::Battle::Ai::roomAiState = &room;
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::secondPerceptionTarget = 0;
  VS::Battle::Ai::attributeActorMask = 1 | (1 << 2) | (1 << 5);
  VS::Battle::Ai::savedMovementRandomState = 0x12345678;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  int passed =
      room.readinessLevel == 0 && VS::Battle::Ai::secondPerceptionTarget == 0;
  VS::Battle::Ai::tileFlagMap = flags;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  passed = passed && room.readinessLevel == 1 &&
           VS::Battle::Ai::secondPerceptionTarget == 2;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  passed = passed && room.readinessLevel == 2 &&
           VS::Battle::Ai::secondPerceptionTarget == 5;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  passed = passed && room.readinessLevel == 3 &&
           VS::Battle::Ai::secondPerceptionTarget == 0 &&
           VS::Battle::Ai::savedMovementRandomState == 0x12345678 &&
           view->opaque14 == 1;
  VS::Battle::Ai::LiveFrameContext() = savedFrame;
  Battle_actors[0] = savedActor;
  VS::Battle::Ai::actorAiStates[0] = saved_state;
  VS::Battle::Ai::roomAiState = saved_room;
  VS::Battle::Ai::tileFlagMap = saved_flags;
  VS::Battle::Ai::secondPerceptionTarget = saved_target;
  VS::Battle::Ai::attributeActorMask = saved_mask;
  VS::Battle::Ai::savedMovementRandomState = saved_random;
  return passed;
}

int Tests_AiFrameCache(void) {
  enum { kScratchSentinel = 0xA5 };
  const VS::Battle::Ai::FrameContext savedFrame =
      VS::Battle::Ai::LiveFrameContext();
  const auto saved_actors = VS::Battle::Ai::actorAiStates;
  Battle_Ai_RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  u_short(*saved_flags)[32] = VS::Battle::Ai::tileFlagMap;
  u_short(*saved_heights)[32] = VS::Battle::Ai::specialTerrainHeights;
  void* saved_data = VS::Battle::Ai::roomHasWaypoints;
  const unsigned saved_aux_length = Field_roomData.header.section6Length;
  const int saved_random = VS::Battle::Ai::savedMovementRandomState;
  u_char savedScratch[132];
  u_char* scratch = reinterpret_cast<u_char*>(VS::Core::ScratchBase()) + 0x37C;
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
  VS::Battle::Ai::actorAiStates.fill(nullptr);
  VS::Battle::Ai::actorAiStates[15] = &actor;
  VS::Battle::Ai::roomAiState = &room;
  VS::Battle::Ai::tileFlagMap = flags;
  VS::Battle::Ai::specialTerrainHeights = heights;
  VS::Battle::Ai::roomHasWaypoints = auxiliary;
  Field_roomData.header.section6Length = sizeof auxiliary;
  VS::Battle::Ai::savedMovementRandomState = 0x12345678;
  const int returned = VS::Battle::Ai::CacheLiveActorAiWorkingState();
  const VS::Battle::Ai::FrameContext* frame =
      &VS::Battle::Ai::LiveFrameContext();
  int passed = returned == static_cast<int>(bounds) &&
               frame->actors[15] == &actor && frame->actors[0] == nullptr &&
               frame->room == &room && frame->tileFlags == flags &&
               frame->heightMap == heights && frame->roomData == auxiliary &&
               frame->terrainRegions == auxiliary + 0x498 &&
               frame->terrain_region_count == sizeof auxiliary - 0x498 &&
               frame->randomState == 0x12345678 &&
               memcmp(frame->directions, VS::Battle::Ai::directionDeltas.data(),
                      sizeof frame->directions) == 0;
  for (unsigned index = 0; index < sizeof savedScratch; ++index) {
    passed = passed && scratch[index] == kScratchSentinel;
  }
  VS::Battle::Ai::LiveFrameContext() = savedFrame;
  VS::Battle::Ai::actorAiStates = saved_actors;
  VS::Battle::Ai::roomAiState = saved_room;
  VS::Battle::Ai::tileFlagMap = saved_flags;
  VS::Battle::Ai::specialTerrainHeights = saved_heights;
  VS::Battle::Ai::roomHasWaypoints = saved_data;
  Field_roomData.header.section6Length = saved_aux_length;
  VS::Battle::Ai::savedMovementRandomState = saved_random;
  memcpy(scratch, savedScratch, sizeof savedScratch);
  return passed;
}

int Tests_DefaultEffectPackageLoaded(void) {
  if (sizeof VS::Effects::emptyPackage != 0x5200 ||
      VS::Effects::emptyPackage[0] != 0x00010A70 ||
      VS::Effects::emptyPackage[0x51FC / sizeof(u_int)] != 0) {
    return 0;
  }
  VS::Effects::LiveEffectPackage package = {0};
  VS::Effects::LiveEffectPackage* saved = VS::Effects::package;
  const uintptr_t base = reinterpret_cast<uintptr_t>(VS::Effects::emptyPackage);
  VS::Effects::package = &package;
  VS::Effects::LiveBindEffectPackageBlocks(VS::Effects::emptyPackage);
  const int passed =
      package.texture_table_count == 1 && package.animation_count == 19 &&
      package.timelines->count == 49 && package.curve_block->count == 32 &&
      package.particle_definitions->count == 46 &&
      reinterpret_cast<uintptr_t>(package.programs) == base + 0x4B44 &&
      reinterpret_cast<uintptr_t>(package.parameters) == base + 0x5078 &&
      reinterpret_cast<uintptr_t>(package.transform_definitions) ==
          base + 0x50A0 &&
      reinterpret_cast<uintptr_t>(package.sound_sequences) == base + 0x51BC;
  VS::Effects::package = saved;
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
  Battle_Actor first = {nullptr}, second = {nullptr};
  Battle_ActorStats firstStats = {0}, secondStats = {0};
  Battle_Ai_ActorAiState firstAi = {0}, secondAi = {0};
  Battle_Actor* savedHead = Battle_actors[0];
  Battle_Ai_ActorAiState* saved_first = VS::Battle::Ai::actorAiStates[2];
  Battle_Ai_ActorAiState* saved_second = VS::Battle::Ai::actorAiStates[5];
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
  firstStats.weapon.current_dp = 10;
  firstStats.weapon.max_dp = 40;
  firstStats.shield.current_dp = 30;
  firstStats.shield.max_dp = 60;
  secondStats.maxHp = 120;
  secondStats.statuses = (1 << 0) | (1 << 12);
  secondAi.actionCostEnabled = 1;
  Battle_actors[0] = &first;
  VS::Battle::Ai::actorAiStates[2] = &firstAi;
  VS::Battle::Ai::actorAiStates[5] = &secondAi;
  VS::Battle::Ai::CaptureBattleActorConditions(output.rows);
  const int passed =
      output.rows[2].opaque0Bit0 == 2 && output.rows[5].opaque0Bit0 == 5 &&
      output.rows[2].opaque4 == 20 && output.rows[2].opaque6 == 100 &&
      output.rows[2].opaque8 == 15 && output.rows[2].opaqueA == 60 &&
      output.rows[2].opaque10 == 10 && output.rows[2].opaque12 == 40 &&
      output.rows[2].opaque14 == 30 && output.rows[2].opaque16 == 60 &&
      VS::Battle::Ai::LiveActorStatusConditionScore(&output.rows[2]) == -34 &&
      VS::Battle::Ai::LiveActorStatusConditionScore(&output.rows[5]) == -314 &&
      output.before == 0xA5A5A5A5u && output.after == 0xA5A5A5A5u &&
      output.rows[1].opaqueC == 0xA5A5A5A5u;
  Battle_actors[0] = savedHead;
  VS::Battle::Ai::actorAiStates[2] = saved_first;
  VS::Battle::Ai::actorAiStates[5] = saved_second;
  return passed;
}

int Tests_GimmickViewsRoundTrip(void) {
  Field_GimmickInstance object = {0};
  Actors_ShpModel model = {0};
  Field_GimmickPhysicsView* physics =
      reinterpret_cast<Field_GimmickPhysicsView*>(&object);
  object.model = &model;
  object.objectInfo.supportIndex = 13;
  physics->position.vy = -64;
  int passed = physics->nativePointer68 == &model && physics->frame == 13 &&
               object.positionY == -64;
  physics->frame = 14;
  physics->carrier = 3;
  passed = passed && object.objectInfo.supportIndex == 14 &&
           object.objectInfo.carrier == 3 && object.model == &model;
  return passed;
}

int Tests_AiViewsRoundTrip(void) {
  const uint64_t guard_pattern = UINT64_C(0xA5A5A5A5A5A5A5A5);
  enum { kCollisionHeight = 124 };
  struct {
    uint64_t before;
    Battle_Ai_ActorAiState state;
    uint64_t after;
  } guarded;
  Battle_Actor actor = {nullptr};
  Actors_ActorRecord model = {0};
  Battle_ActorStats stats = {0};
  Battle_Ai_AiCreationView* creation =
      reinterpret_cast<Battle_Ai_AiCreationView*>(&guarded.state);
  Battle_Ai_SettingsView* settings =
      reinterpret_cast<Battle_Ai_SettingsView*>(&guarded.state);
  Battle_Ai_PerceptionUpdateView* update =
      reinterpret_cast<Battle_Ai_PerceptionUpdateView*>(&guarded.state);
  Battle_Ai_CandidateReadinessView* planning =
      reinterpret_cast<Battle_Ai_CandidateReadinessView*>(&guarded.state);
  Battle_Ai_AiActorView* actor_view =
      reinterpret_cast<Battle_Ai_AiActorView*>(&actor);
  Battle_Ai_CreationModelView* model_view =
      reinterpret_cast<Battle_Ai_CreationModelView*>(&model);
  unsigned int i;
  memset(&guarded, 0xA5, sizeof guarded);
  VS::Core::Memory::ClearLiveAlignedWords(&guarded.state, sizeof guarded.state);
  if (guarded.before != guard_pattern || guarded.after != guard_pattern) {
    return 0;
  }
  for (i = 0; i < sizeof guarded.state; ++i) {
    if (reinterpret_cast<unsigned char*>(&guarded.state)[i] != 0) return 0;
  }
  actor.id = 2;
  actor.kind = 5;
  actor.stats = &stats;
  actor.model = &model.instance;
  model.instance.position.vy = -300;
  model.collisionRadius = 61;
  model.collisionHeight = kCollisionHeight;
  if (actor_view->actorId != 2 || actor_view->aiControlFlags != 5 ||
      reinterpret_cast<void*>(actor_view->model) != &model ||
      reinterpret_cast<void*>(actor_view->stats) != &stats ||
      model_view->position[1] != -300 || model_view->radius != 61 ||
      model_view->opaque63E != kCollisionHeight) {
    return 0;
  }
  creation->actor = actor_view;
  creation->model = model_view;
  creation->stats = reinterpret_cast<Battle_Ai_CreationStatsView*>(&stats);
  creation->actorId = 2;
  creation->positionSource = model_view->position;
  creation->tileSource = &model_view->tile.word;
  creation->radius = 61;
  creation->collisionRadius = 62;
  settings->decisionCallback = TestDecision;
  settings->farDistanceSquared = 0x240000;
  planning->candidateOrder[15] =
      reinterpret_cast<Battle_Ai_CandidateEntryView*>(
          &guarded.state.candidates[15]);
  planning->candidateCount = 16;
  return reinterpret_cast<void*>(guarded.state.actor) == &actor &&
         reinterpret_cast<void*>(guarded.state.model) == &model &&
         guarded.state.stats == &stats && guarded.state.actorId == 2 &&
         guarded.state.radius == 61 && settings->collisionRadius == 62 &&
         creation->farDistanceSquared == 0x240000 &&
         reinterpret_cast<void*>(update->positionSource) ==
             creation->positionSource &&
         reinterpret_cast<void*>(update->tileSource) == creation->tileSource &&
         settings->decisionCallback(&guarded.state) == 2 &&
         guarded.state.candidateCount == 16 &&
         guarded.state.candidateOrder[15] == &guarded.state.candidates[15] &&
         guarded.before == guard_pattern && guarded.after == guard_pattern;
}

int Tests_LayoutAssertsLinked(void) { return 1; }
