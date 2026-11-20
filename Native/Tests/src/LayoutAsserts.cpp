// Compile-time checks of the x64 layout decisions the copy depends on
// (tools/port/copy_fixups.py TYPE_FIXES and FIXES). A failure here means a fix
// no longer holds after a ledger or decomp change.
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <type_traits>

#include "Actors/ActorAllocation.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/ShpModel.hpp"
#include "Audio/SoundBanksLive.hpp"
#include "Audio/SoundState.hpp"
#include "Battle/ActionTable.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/ConditionSnapshotBinding.hpp"
#include "Battle/Ai/DecisionsLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/Navigation.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/RelationsLive.hpp"
#include "Battle/Damage.hpp"
#include "Battle/TargetVolume.hpp"
#include "Battle/TargetingState.hpp"
#include "Content/DiscQueue.hpp"
#include "Content/DiscReaderLive.hpp"
#include "Core/Memory/WordCopyLive.hpp"
#include "Effects/EffectResourcesLive.hpp"
#include "Effects/Package.hpp"
#include "Effects/Runtime.hpp"
#include "Field/Gimmicks.hpp"
#include "Legacy/Psx.hpp"

using VS::Battle::ActionDefinition;
using VS::Battle::kActionTypeBaseAttack;
using VS::Battle::TargetSphereLabelView;

static_assert(
    sizeof(VS::Actors::ActorCore) - offsetof(VS::Actors::ActorCore, model) >=
        offsetof(VS::Field::GimmickInstance, reserved178),
    "The auxiliary actor allocation must own the native gimmick prefix");

static_assert(
    offsetof(VS::Battle::Ai::ActorAiState, engagement_frames) == 170,
    "Renamed engagement state must retain its verified native layout");
static_assert(offsetof(VS::Battle::Ai::ActorAiState,
                       engagement_cooldown_frames) == 180,
              "Engagement cooldown layout changed");
static_assert(offsetof(VS::Battle::Ai::ActorAiState,
                       reengagement_grace_frames) == 182,
              "Reengagement grace layout changed");
static_assert(offsetof(VS::Battle::Ai::ActorAiState,
                       awareness_frames_remaining) == 326,
              "Awareness duration layout changed");
static_assert(offsetof(VS::Battle::Ai::ActorAiState, awareness_marker) == 340,
              "Awareness marker layout changed");

int TestsRoomSfxBankBinding(void) {
  const VS::Audio::SoundState saved_state = VS::Audio::soundState;
  VS::Audio::soundState = VS::Audio::SoundState{};
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
  const VS::Content::DiscDrive saved = VS::Content::discDriveState;
  VS::Content::discDriveState = VS::Content::DiscDrive{};
  VS::Content::discDriveState.byte_count = 7;
  unsigned char destination[2049];
  int passed =
      !VS::Content::BeginLiveDiscRead(1, UINT32_MAX, destination) &&
      VS::Content::discDriveState.state == VS::Content::DiscDriveState::kIdle &&
      VS::Content::discDriveState.byte_count == 7 &&
      VS::Content::discDriveState.destination == nullptr;
  passed = passed &&
           VS::Content::BeginLiveDiscRead(1, sizeof destination, destination) &&
           VS::Content::discDriveState.state ==
               VS::Content::DiscDriveState::kReadInit &&
           VS::Content::discDriveState.byte_count == sizeof destination &&
           VS::Content::discDriveState.sector_count == 2 &&
           VS::Content::discDriveState.destination == destination;
  passed = passed && !VS::Content::BeginLiveDiscRead(2, 0, nullptr) &&
           VS::Content::discDriveState.start_sector == 1;
  VS::Content::discDriveState = saved;
  return passed;
}

static_assert(sizeof(ActionDefinition) == 52, "action definition disc stride");
static_assert(offsetof(ActionDefinition, cost) == 3, "action cost offset");
static_assert(offsetof(ActionDefinition, ai_flags) == 14,
              "action AI flags offset");
static_assert(offsetof(ActionDefinition, hits) == 20,
              "action hit definitions offset");
static_assert(offsetof(ActionDefinition, name) == 28, "action name offset");
static_assert(sizeof(VS::Battle::TargetSphereSetupView) >=
                  sizeof(VS::Battle::TargetSphereRecord),
              "sphere allocation extent");
static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_vertices) ==
                  offsetof(VS::Battle::TargetSphereRecord, wire_vertices),
              "sphere vertex view offset");
static_assert(offsetof(VS::Battle::TargetSphereSetupView, wire_edges) ==
                  offsetof(VS::Battle::TargetSphereRecord, wire_edges),
              "sphere edge view offset");
static_assert(offsetof(TargetSphereLabelView, opaque990) ==
                  offsetof(VS::Battle::TargetSphereRecord, labels),
              "sphere label view offset");

// An actor record placed at core.model (VS::Content::ParseActorModel) must
// end before the weapon model slots.
static_assert(sizeof(VS::Actors::ActorModelSlot) >=
                  sizeof(VS::Actors::WeaponModel),
              "equipment slot must fit a native weapon model");
static_assert(_Alignof(VS::Actors::ActorModelSlot) >=
                  _Alignof(VS::Actors::WeaponModel),
              "equipment slot must align a native weapon model");
static_assert(offsetof(VS::Actors::ActorAllocation, model_slots) %
                      _Alignof(VS::Actors::ActorRecord) ==
                  0,
              "secondary body record must be aligned");
static_assert(offsetof(VS::Actors::ActorAllocation, model_slots) -
                      offsetof(VS::Actors::ActorAllocation, core.model) >=
                  sizeof(VS::Actors::ActorRecord),
              "the actor record overlaps the weapon model slots");

// VS::Actors::LiveActorSpawnHost (ActorSpawningLive.cpp) addresses the stats
// block of an allocation as `actor + 1`.
static_assert(offsetof(VS::Actors::ActorAllocation, core.actor.stats) ==
                  sizeof(VS::Battle::Actor),
              "stats do not follow the actor");

// On-disc structs keep the file layout (ONDISC_OFFSETS): the SHP header is 64
// bytes, the bones follow.
static_assert(sizeof(VS::Actors::ShpModel) == 64, "SHP header layout");
static_assert(offsetof(VS::Actors::ShpModel, bones) == 64, "SHP bones offset");
static_assert(offsetof(VS::Actors::SeqAnimationBank, headers) == 8,
              "SEQ bank header layout");
static_assert(sizeof((static_cast<VS::Actors::ActorRecord*>(nullptr))
                         ->animation_header) == sizeof(void*),
              "SEQ bank cache must be pointer-sized");

// Runtime views must follow the native object, including pointers hidden in the
// original PS1 padding.
#define SAME_FIELD(View, member, Record, field)                    \
  static_assert(offsetof(View, member) == offsetof(Record, field), \
                "native view mismatch: " #View "." #member)
SAME_FIELD(VS::Battle::Ai::AiActorView, actor_id, VS::Battle::Actor, id);
SAME_FIELD(VS::Battle::Ai::AiActorView, ai_control_flags, VS::Battle::Actor,
           kind);
SAME_FIELD(VS::Battle::Ai::AiActorView, stats, VS::Battle::Actor, stats);
SAME_FIELD(VS::Battle::Ai::AiActorView, model, VS::Battle::Actor, model);
SAME_FIELD(VS::Battle::Ai::SettingsActorView, spawn_flags, VS::Battle::Actor,
           reserved34);
SAME_FIELD(VS::Battle::Ai::CreationModelView, radius, VS::Actors::ActorRecord,
           collision_radius);
SAME_FIELD(VS::Battle::Ai::CreationModelView, opaque_63e,
           VS::Actors::ActorRecord, collision_height);
SAME_FIELD(VS::Battle::Ai::AiCreationView, actor, VS::Battle::Ai::ActorAiState,
           actor);
SAME_FIELD(VS::Battle::Ai::AiCreationView, model, VS::Battle::Ai::ActorAiState,
           model);
SAME_FIELD(VS::Battle::Ai::AiCreationView, stats, VS::Battle::Ai::ActorAiState,
           stats);
SAME_FIELD(VS::Battle::Ai::AiCreationView, actor_id,
           VS::Battle::Ai::ActorAiState, actor_id);
SAME_FIELD(VS::Battle::Ai::AiCreationView, point_height,
           VS::Battle::Ai::ActorAiState, point_height);
SAME_FIELD(VS::Battle::Ai::SettingsView, decision_callback,
           VS::Battle::Ai::ActorAiState, native_pointer68);
SAME_FIELD(VS::Battle::Ai::PerceptionView, relations,
           VS::Battle::Ai::ActorAiState, relations);
SAME_FIELD(VS::Battle::Ai::CandidateReadinessView, candidate_order,
           VS::Battle::Ai::ActorAiState, candidate_order);
SAME_FIELD(VS::Battle::Ai::CandidateReadinessView, candidate_count,
           VS::Battle::Ai::ActorAiState, candidate_count);
SAME_FIELD(VS::Field::GimmickPhysicsView, native_pointer68,
           VS::Field::GimmickInstance, model);
SAME_FIELD(VS::Field::GimmickPhysicsView, frame, VS::Field::GimmickInstance,
           object_info.support_index);
SAME_FIELD(VS::Field::GimmickPhysicsView, carrier, VS::Field::GimmickInstance,
           object_info.carrier);
static_assert(sizeof(VS::Battle::Ai::AiCreationView) <=
                  sizeof(VS::Battle::Ai::ActorAiState),
              "creation view exceeds storage");
static_assert(std::tuple_size_v<decltype(VS::Battle::Ai::actorAiStates)> == 16,
              "AI table extent");
static_assert(
    std::is_same_v<decltype(VS::Battle::Ai::roomAuxiliaryData), void*>,
              "room auxiliary-data pointer width");
static_assert(sizeof(VS::Battle::Ai::ActorAiState) % 4 == 0,
              "AI clear requires whole words");
#undef SAME_FIELD

static int TestDecision(VS::Battle::Ai::ActorAiState* state) {
  return state->actor_id;
}

static int TestMovementDecision(VS::Battle::Ai::ActorAiState* state) {
  ++state->opaque44;
  return 1;
}

int Tests_AiMovementDispatch(void) {
  VS::Battle::Ai::ActorAiState state = {};
  VS::Battle::Ai::SettingsView* settings =
      reinterpret_cast<VS::Battle::Ai::SettingsView*>(&state);
  state.previous_tile = -1;
  settings->decision_callback = TestMovementDecision;
  VS::Battle::Ai::DispatchLiveMovementBehavior(
      reinterpret_cast<VS::Battle::Ai::DecisionDispatchView*>(&state));
  const int passed = state.opaque44 == 1 && state.detour_step == 0;
  VS::Battle::Ai::UpdateLiveActorLocomotionMode(
      reinterpret_cast<VS::Battle::Ai::DecisionDispatchView*>(&state));
  return passed && state.behavior_mode == 0;
}

int Tests_AiRelations(void) {
  VS::Battle::Actor actors[3] = {};
  VS::Battle::Ai::ActorAiState states[3] = {};
  const unsigned ids[3] = {0, 2, 5};
  VS::Battle::Actor* saved_head = VS::Battle::actors[0];
  VS::Battle::Ai::ActorAiState* saved_states[3];
  for (unsigned index = 0; index < 3; ++index) {
    saved_states[index] = VS::Battle::Ai::actorAiStates[ids[index]];
    actors[index].id = static_cast<std::int32_t>(ids[index]);
    actors[index].next = index + 1 < 3 ? &actors[index + 1] : nullptr;
    states[index].actor_id = ids[index];
    states[index].saved_relation_options = -1;
    VS::Battle::Ai::actorAiStates[ids[index]] = &states[index];
  }
  states[0].settings_bits21 = 3;
  states[1].settings_bits21 = 0;
  states[2].settings_bits21 = 2;
  states[1].relations[5].visible = 1;
  VS::Battle::actors[0] = actors;
  VS::Battle::Ai::InitializeLiveActorAiRelations(
      reinterpret_cast<VS::Battle::Ai::AiCreationView*>(&states[1]));
  int passed = states[1].relations[0].classification == 3 &&
               states[0].relations[2].classification == 3 &&
               states[1].relations[5].classification == 2 &&
               states[2].relations[2].classification == 2 &&
               states[1].relations[5].visible == 1;
  for (unsigned index = 0; index < 3; ++index) {
    passed = passed && states[index].relations[ids[index]].classification == 2;
  }
  states[1].saved_relation_options = 0x1234;
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
  VS::Battle::actors[0] = saved_head;
  for (unsigned index = 0; index < 3; ++index) {
    VS::Battle::Ai::actorAiStates[ids[index]] = saved_states[index];
  }
  return passed;
}

int Tests_AiMovementSetup(void) {
  VS::Battle::Ai::ActorAiState state = {};
  state.decision_delay = 2;
  state.min_action_delay = 1;
  VS::Battle::Ai::TickLiveActorDecisionTimers(
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&state));
  int passed = state.decision_delay == 1 && state.min_action_delay == 0;
  VS::Battle::Ai::TickLiveActorDecisionTimers(
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&state));
  VS::Battle::Ai::TickLiveActorDecisionTimers(
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&state));
  passed = passed && state.decision_delay == 0 && state.min_action_delay == 0;
  const std::uint16_t flags[3] = {0x1000, 0x800, 0x100};
  const std::uint8_t modes[3] = {1, 2, 0};
  for (size_t index = 0; index < sizeof flags / sizeof flags[0]; ++index) {
    state.locomotion_flags = flags[index];
    VS::Battle::Ai::InitializeLiveActorLocomotionMode(
        reinterpret_cast<VS::Battle::Ai::AiCreationView*>(&state));
    passed = passed && state.locomotion_mode == modes[index] &&
             state.movement_flags == (flags[index] | (flags[index] >> 8));
  }
  VS::Battle::Ai::ActorAiState* saved = VS::Battle::Ai::actorAiStates[2];
  VS::Battle::Ai::actorAiStates[2] = &state;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(2, 8, 12);
  passed = passed && state.walk_step == 8 && state.run_step == 12 &&
           state.max_range == 12;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(2, 16, 6);
  passed = passed && state.walk_step == 16 && state.run_step == 6 &&
           state.max_range == 16;
  VS::Battle::Ai::actorAiStates[2] = nullptr;
  VS::Battle::Ai::SetLiveActorVerticalMovementSteps(2, 4, 8);
  passed = passed && state.walk_step == 16 && state.run_step == 6;
  VS::Battle::Ai::actorAiStates[2] = saved;
  return passed;
}

int Tests_TargetSphereViews(void) {
  VS::Battle::TargetSphereSetupView setup = {};
  VS::Battle::TargetSphereRecord* sphere =
      reinterpret_cast<VS::Battle::TargetSphereRecord*>(&setup);
  TargetSphereLabelView* labels =
      reinterpret_cast<TargetSphereLabelView*>(&setup);
  VS::Battle::TargetPreview targets[2] = {};
  char text[] = "limb";
  setup.target_list0 = &targets[0];
  setup.target_list1 = &targets[1];
  sphere->labels[23].text = text;
  setup.wire_edges[769] = 0x1234;
  return sphere->target_lists[0] == &targets[0] &&
         sphere->target_lists[1] == &targets[1] &&
         labels->opaque40 == reinterpret_cast<void*>(&targets[0]) &&
         labels->opaque44 == reinterpret_cast<void*>(&targets[1]) &&
         labels->opaque990[23].limb_name == text &&
         (reinterpret_cast<int16_t*>(sphere->wire_edges))[769] == 0x1234;
}

int Tests_AiFrameStages(void) {
  const VS::Battle::Ai::FrameContext savedFrame =
      VS::Battle::Ai::LiveFrameContext();
  VS::Battle::Actor* saved_actor = VS::Battle::actors[0];
  VS::Battle::Ai::ActorAiState* saved_state = VS::Battle::Ai::actorAiStates[0];
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  std::uint16_t (*saved_flags)[32] = VS::Battle::Ai::tileFlagMap;
  const int saved_target = VS::Battle::Ai::secondPerceptionTarget;
  const int saved_mask = VS::Battle::Ai::attributeActorMask;
  const int saved_random = VS::Battle::Ai::savedMovementRandomState;
  VS::Battle::Actor actor = {};
  VS::Battle::ActorStats stats = {};
  VS::Actors::ActorRecord model = {};
  VS::Battle::Ai::ActorAiState state = {};
  VS::Battle::Ai::PerceptionUpdateView* view =
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&state);
  VS::Battle::Ai::RoomAiState room = {};
  std::uint16_t flags[32][32] = {{0}};
  actor.stats = &stats;
  view->actor = reinterpret_cast<VS::Battle::Actor*>(&actor);
  view->model = reinterpret_cast<VS::Battle::Ai::PerceivedObjectView*>(&model);
  VS::Battle::actors[0] = &actor;
  VS::Battle::Ai::actorAiStates[0] = &state;
  VS::Battle::Ai::roomAiState = &room;
  VS::Battle::Ai::tileFlagMap = nullptr;
  VS::Battle::Ai::secondPerceptionTarget = 0;
  VS::Battle::Ai::attributeActorMask = 1 | (1 << 2) | (1 << 5);
  VS::Battle::Ai::savedMovementRandomState = 0x12345678;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  int passed =
      room.readiness_level == 0 && VS::Battle::Ai::secondPerceptionTarget == 0;
  VS::Battle::Ai::tileFlagMap = flags;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  passed = passed && room.readiness_level == 1 &&
           VS::Battle::Ai::secondPerceptionTarget == 2;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  passed = passed && room.readiness_level == 2 &&
           VS::Battle::Ai::secondPerceptionTarget == 5;
  VS::Battle::Ai::RunLiveActorAiUpdateStages();
  passed = passed && room.readiness_level == 3 &&
           VS::Battle::Ai::secondPerceptionTarget == 0 &&
           VS::Battle::Ai::savedMovementRandomState == 0x12345678 &&
           view->opaque14 == 1;
  VS::Battle::Ai::LiveFrameContext() = savedFrame;
  VS::Battle::actors[0] = saved_actor;
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
  VS::Battle::Ai::RoomAiState* saved_room = VS::Battle::Ai::roomAiState;
  std::uint16_t (*saved_flags)[32] = VS::Battle::Ai::tileFlagMap;
  std::uint16_t (*saved_heights)[32] = VS::Battle::Ai::specialTerrainHeights;
  void* saved_data = VS::Battle::Ai::roomAuxiliaryData;
  const unsigned saved_aux_length = VS::Field::roomData.header.section6_length;
  const int saved_random = VS::Battle::Ai::savedMovementRandomState;
  std::uint8_t saved_scratch[132];
  std::uint8_t* scratch =
      reinterpret_cast<std::uint8_t*>(VS::Core::ScratchBase()) + 0x37C;
  memcpy(saved_scratch, scratch, sizeof saved_scratch);
  for (size_t index = 0; index < sizeof saved_scratch; ++index) {
    scratch[index] = kScratchSentinel;
  }
  VS::Battle::Ai::ActorAiState actor = {};
  VS::Battle::Ai::RoomAiState room = {};
  std::uint16_t flags[32][32] = {{0}};
  std::uint16_t heights[32][32] = {{0}};
  std::uint8_t auxiliary[0x898] = {0};
  const std::uint32_t bounds = 0x01021F1Eu;
  memcpy(room.reserved4.data(), &bounds, sizeof bounds);
  VS::Battle::Ai::actorAiStates.fill(nullptr);
  VS::Battle::Ai::actorAiStates[15] = &actor;
  VS::Battle::Ai::roomAiState = &room;
  VS::Battle::Ai::tileFlagMap = flags;
  VS::Battle::Ai::specialTerrainHeights = heights;
  VS::Battle::Ai::roomAuxiliaryData = auxiliary;
  VS::Field::roomData.header.section6_length = sizeof auxiliary;
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
  for (unsigned index = 0; index < sizeof saved_scratch; ++index) {
    passed = passed && scratch[index] == kScratchSentinel;
  }
  VS::Battle::Ai::LiveFrameContext() = savedFrame;
  VS::Battle::Ai::actorAiStates = saved_actors;
  VS::Battle::Ai::roomAiState = saved_room;
  VS::Battle::Ai::tileFlagMap = saved_flags;
  VS::Battle::Ai::specialTerrainHeights = saved_heights;
  VS::Battle::Ai::roomAuxiliaryData = saved_data;
  VS::Field::roomData.header.section6_length = saved_aux_length;
  VS::Battle::Ai::savedMovementRandomState = saved_random;
  memcpy(scratch, saved_scratch, sizeof saved_scratch);
  return passed;
}

int Tests_DefaultEffectPackageLoaded(void) {
  if (sizeof VS::Effects::emptyPackage != 0x5200 ||
      VS::Effects::emptyPackage[0] != 0x00010A70 ||
      VS::Effects::emptyPackage[0x51FC / sizeof(std::uint32_t)] != 0) {
    return 0;
  }
  VS::Effects::LiveEffectPackage package = {};
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
  const ActionDefinition* basic = &VS::Battle::actionDefinitions[1];
  return basic->id == 1 && basic->effect_id == 1 &&
         basic->type == kActionTypeBaseAttack && basic->category == 14 &&
         basic->cost == 255 && basic->range_x == 255 && basic->range_y == 255 &&
         basic->range_z == 255 && basic->hits[0].effect_id == 58 &&
         basic->hits[0].prerequisite_function_id == 2 &&
         basic->hits[0].hit_function_id == 3 &&
         basic->hits[0].stat_calculator_id == 25 &&
         VS::Battle::categoryNeedsTargetSelection[basic->category] != 0 &&
         VS::Battle::categoryTargetsBodyParts[basic->category] != 0;
}

int Tests_ActorConditionSnapshot(void) {
  VS::Battle::Actor first = {}, second = {};
  VS::Battle::ActorStats first_stats = {}, second_stats = {};
  VS::Battle::Ai::ActorAiState first_ai = {}, second_ai = {};
  VS::Battle::Actor* saved_head = VS::Battle::actors[0];
  VS::Battle::Ai::ActorAiState* saved_first = VS::Battle::Ai::actorAiStates[2];
  VS::Battle::Ai::ActorAiState* saved_second = VS::Battle::Ai::actorAiStates[5];
  struct {
    unsigned before;
    VS::Battle::Ai::ActionTiming rows[16];
    unsigned after;
  } output;
  memset(&output, 0xA5, sizeof output);
  first.id = 2;
  first.next = &second;
  first.stats = &first_stats;
  second.id = 5;
  second.stats = &second_stats;
  first_stats.current_hp = 20;
  first_stats.max_hp = 100;
  first_stats.current_mp = 15;
  first_stats.max_mp = 60;
  first_stats.statuses = (1 << 6) | (1 << 12);
  first_stats.weapon.current_dp = 10;
  first_stats.weapon.max_dp = 40;
  first_stats.shield.current_dp = 30;
  first_stats.shield.max_dp = 60;
  second_stats.max_hp = 120;
  second_stats.statuses = (1 << 0) | (1 << 12);
  second_ai.action_cost_enabled = 1;
  VS::Battle::actors[0] = &first;
  VS::Battle::Ai::actorAiStates[2] = &first_ai;
  VS::Battle::Ai::actorAiStates[5] = &second_ai;
  VS::Battle::Ai::CaptureBattleActorConditions(output.rows);
  const int passed =
      output.rows[2].actor_id == 2 && output.rows[5].actor_id == 5 &&
      output.rows[2].hp == 20 && output.rows[2].max_hp == 100 &&
      output.rows[2].mp == 15 && output.rows[2].max_mp == 60 &&
      output.rows[2].weapon_dp == 10 && output.rows[2].weapon_max_dp == 40 &&
      output.rows[2].shield_dp == 30 && output.rows[2].shield_max_dp == 60 &&
      VS::Battle::Ai::LiveActorStatusConditionScore(&output.rows[2]) == -34 &&
      VS::Battle::Ai::LiveActorStatusConditionScore(&output.rows[5]) == -314 &&
      output.before == 0xA5A5A5A5u && output.after == 0xA5A5A5A5u &&
      output.rows[1].statuses == 0xA5A5A5A5u;
  VS::Battle::actors[0] = saved_head;
  VS::Battle::Ai::actorAiStates[2] = saved_first;
  VS::Battle::Ai::actorAiStates[5] = saved_second;
  return passed;
}

int Tests_GimmickViewsRoundTrip(void) {
  VS::Field::GimmickInstance object = {};
  VS::Actors::ShpModel model = {};
  VS::Field::GimmickPhysicsView* physics =
      reinterpret_cast<VS::Field::GimmickPhysicsView*>(&object);
  object.model = &model;
  object.object_info.support_index = 13;
  physics->position.vy = -64;
  int passed = physics->native_pointer68 == &model && physics->frame == 13 &&
               object.position_y == -64;
  physics->frame = 14;
  physics->carrier = 3;
  passed = passed && object.object_info.support_index == 14 &&
           object.object_info.carrier == 3 && object.model == &model;
  return passed;
}

int Tests_AiViewsRoundTrip(void) {
  const uint64_t guard_pattern = UINT64_C(0xA5A5A5A5A5A5A5A5);
  enum { kCollisionHeight = 124 };
  struct {
    uint64_t before;
    VS::Battle::Ai::ActorAiState state;
    uint64_t after;
  } guarded;
  VS::Battle::Actor actor = {};
  VS::Actors::ActorRecord model = {};
  VS::Battle::ActorStats stats = {};
  VS::Battle::Ai::AiCreationView* creation =
      reinterpret_cast<VS::Battle::Ai::AiCreationView*>(&guarded.state);
  VS::Battle::Ai::SettingsView* settings =
      reinterpret_cast<VS::Battle::Ai::SettingsView*>(&guarded.state);
  VS::Battle::Ai::PerceptionUpdateView* update =
      reinterpret_cast<VS::Battle::Ai::PerceptionUpdateView*>(&guarded.state);
  VS::Battle::Ai::CandidateReadinessView* planning =
      reinterpret_cast<VS::Battle::Ai::CandidateReadinessView*>(&guarded.state);
  VS::Battle::Ai::AiActorView* actor_view =
      reinterpret_cast<VS::Battle::Ai::AiActorView*>(&actor);
  VS::Battle::Ai::CreationModelView* model_view =
      reinterpret_cast<VS::Battle::Ai::CreationModelView*>(&model);
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
  model.collision_radius = 61;
  model.collision_height = kCollisionHeight;
  if (actor_view->actor_id != 2 || actor_view->ai_control_flags != 5 ||
      reinterpret_cast<void*>(actor_view->model) != &model ||
      reinterpret_cast<void*>(actor_view->stats) != &stats ||
      model_view->position[1] != -300 || model_view->radius != 61 ||
      model_view->opaque_63e != kCollisionHeight) {
    return 0;
  }
  creation->actor = actor_view;
  creation->model = model_view;
  creation->stats =
      reinterpret_cast<VS::Battle::Ai::CreationStatsView*>(&stats);
  creation->actor_id = 2;
  creation->position_source = model_view->position.data();
  creation->tile_source = &model_view->tile.word;
  creation->radius = 61;
  creation->collision_radius = 62;
  settings->decision_callback = TestDecision;
  settings->far_distance_squared = 0x240000;
  planning->candidate_order[15] =
      reinterpret_cast<VS::Battle::Ai::CandidateEntryView*>(
          &guarded.state.candidates[15]);
  planning->candidate_count = 16;
  return reinterpret_cast<void*>(guarded.state.actor) == &actor &&
         reinterpret_cast<void*>(guarded.state.model) == &model &&
         guarded.state.stats == &stats && guarded.state.actor_id == 2 &&
         guarded.state.radius == 61 && settings->collision_radius == 62 &&
         creation->far_distance_squared == 0x240000 &&
         reinterpret_cast<void*>(update->position_source) ==
             creation->position_source &&
         reinterpret_cast<void*>(update->tile_source) ==
             creation->tile_source &&
         settings->decision_callback(&guarded.state) == 2 &&
         guarded.state.candidate_count == 16 &&
         guarded.state.candidate_order[15] == &guarded.state.candidates[15] &&
         guarded.before == guard_pattern && guarded.after == guard_pattern;
}

int Tests_LayoutAssertsLinked(void) { return 1; }
