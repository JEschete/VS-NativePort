#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <cstddef>
#include <span>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRecord.h"
#include "Actors/ActorStateLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationControlLive.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/Location.hpp"
#include "Actors/ModelDrawing.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.h"
#include "Actors/MovementCommand.hpp"
#include "Actors/SequencePlaybackLive.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Actors/Textures.hpp"
#include "Battle/ActionAnimation.h"
#include "Battle/ActionAnimationsLive.hpp"
#include "Battle/ActionExecution.hpp"
#include "Battle/ActionFlow.h"
#include "Battle/ActionSequenceLive.hpp"
#include "Battle/Damage.h"
#include "Battle/Sphere.h"
#include "Battle/SphereTargetLabelsLive.hpp"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingDrawLive.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Content/DiscQueue.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/Runtime.hpp"
#include "Event/ScriptActorOpcodes.hpp"
#include "Event/ScriptActors.hpp"
#include "Event/ScriptEffectOpcodes.hpp"
#include "Event/ScriptEffects.hpp"
#include "Field/CameraLiveState.hpp"
#include "Field/GimmickPlacementLive.hpp"
#include "Field/Gimmicks.h"
#include "Field/RoomLoadingLive.hpp"
#include "Field/RoomLoadingLiveState.hpp"
#include "Field/Zone.hpp"
#include "Legacy/Psx.h"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/Display.hpp"
#include "Render/Frame.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/Render.hpp"
#include "Render/TexturePagesLive.hpp"
#include "Render/Textures.hpp"

enum {
  kSpatialDimensions = 3,
  kFixedPointOne = 4096,
  kFacialImageHeaderWords = 2,
  kFacialImageWidth = 64,
  kFacialImageHeight = 2,
  kFacialUploadWidth = 4,
  kFacialFirstPixel = 0x100
};

int TestBattleAnimationHeaderOutputs(int consumer, int mode, int available) {
  enum { kActorId = 2, kMapOffset = 32, kMapEntries = 256 };
  u_short file[256] = {0};
  VS::Actors::SeqAnimationBank* bank =
      reinterpret_cast<VS::Actors::SeqAnimationBank*>(file);
  Actors_ActorRecord actor = {0};
  Actors_ActorRecord* saved_actor = Actors_actorRecords[kActorId];
  const char saved_index = VS::Actors::lastResolvedSequenceIndex;
  const char saved_pitch = Battle_specialActionPitch;
  bank->animation_map = kMapOffset;
  memset(reinterpret_cast<u_char*>(file) + kMapOffset, available ? 0 : 0xFF,
         kMapEntries);
  actor.instance.slot = kActorId;
  actor.animationLocked = 1;
  actor.sequenceModelId = 1;
  actor.sequenceFlags = 100;
  actor.commonAnimationCount = 128;
  actor.battleAnimationCount = 64;
  actor.commonSequence = bank;
  actor.battleSequence = bank;
  actor.sharedSequences[3] = bank;
  actor.animationId = 0xC6;
  Actors_actorRecords[kActorId] = &actor;
  Battle_specialActionPitch = 2;
  VS::Actors::RegisterSequenceBank(bank, sizeof file);
  if (consumer == 0) {
    VS::Battle::PlayLiveSpecialActionAnimation(kActorId, nullptr, mode);
  } else if (consumer == 1) {
    VS::Battle::PlayLiveBreakArtAnimation(kActorId, nullptr, mode);
  } else if (consumer == 2) {
    VS::Battle::PlayLiveSpellCastAnimation(kActorId, nullptr, mode);
  } else {
    VS::Battle::SetLiveEnemyCombatPose(kActorId, 3);
  }
  int passed = actor.animationLocked == 1 && actor.animationId == 0xC6;
  if (consumer < 2) {
    const int executing = mode == 1 && available;
    passed &= Battle_specialActionPitch == (executing ? 1 : 2);
    passed &= actor.headTracking.pad == (executing ? 0 : 30);
    passed &= actor.instance.turnFrames == (executing ? 0 : 10);
    passed &= actor.queuedAnimation ==
              (consumer == 1 && mode == 1 && available ? 0xFE : 0);
  } else if (consumer == 2) {
    passed &= actor.headTracking.pad == 30;
    passed &= actor.instance.turnFrames == (mode == 0 ? 10 : 0);
  }
  Actors_actorRecords[kActorId] = saved_actor;
  VS::Actors::lastResolvedSequenceIndex = saved_index;
  Battle_specialActionPitch = saved_pitch;
  VS::Actors::ForgetSequenceBanks(bank, sizeof file);
  return passed;
}

struct MorphAnimationFixture {
  Actors_ActorRecord actor;
  unsigned model[64];
  unsigned sequence[128];
};

static void InitializeMorphAnimation(MorphAnimationFixture* fixture,
                                     int animation) {
  memset(fixture, 0, sizeof *fixture);
  VS::Actors::ShpModel* model =
      reinterpret_cast<VS::Actors::ShpModel*>(fixture->model);
  VS::Actors::SeqAnimationBank* bank =
      reinterpret_cast<VS::Actors::SeqAnimationBank*>(fixture->sequence);
  const u_char events[] = {0, 36, 2, 0, 0xFF, 0xFF};
  bank->animation_map = 32;
  bank->animation_data = 128;
  memset(reinterpret_cast<u_char*>(bank) + bank->animation_map, 0xFF, 64);
  (reinterpret_cast<u_char*>(
      bank))[bank->animation_map +
             (animation >= 220 ? animation - 220 : animation)] = 0;
  bank->headers[0] = 16;
  bank->headers[2] = 1;
  memcpy(reinterpret_cast<u_char*>(bank) + bank->animation_data + 1, events,
         sizeof events);
  model->group_section = 64;
  model->vertex_section = 128;
  model->polygon_section = 136;
  SVECTOR* vertex = VS_ONDISC(model, vertex_section, SVECTOR*);
  *vertex = (SVECTOR){11, 22, 33, 44};
  fixture->actor.instance.model = model;
  fixture->actor.modelDataSize = sizeof fixture->model;
  fixture->actor.instance.slot = 3;
  fixture->actor.poseOverrideActive = 1;
  fixture->actor.animationId = animation;
  fixture->actor.animationLength = 16;
  fixture->actor.animationHeader = reinterpret_cast<intptr_t>(bank);
  fixture->actor.sequenceFlags = 64;
  fixture->actor.commonSequence = bank;
  fixture->actor.sharedSequences[2] = bank;
  fixture->actor.pose.event_script =
      reinterpret_cast<u_char*>(bank) + bank->animation_data + 2;
  fixture->actor.blendPose.event_script = fixture->actor.pose.event_script;
  fixture->actor.pose.bone_keys[0] = (SVECTOR){101, 202, 303, 0};
  VS::Actors::RegisterSequenceBank(bank, sizeof fixture->sequence);
}

static void ForgetMorphAnimation(MorphAnimationFixture* fixture) {
  VS::Actors::ForgetSequenceBanks(fixture->sequence, sizeof fixture->sequence);
}

struct MorphBindingScope {
  Actors_ActorRecord* actors[17];
  int bank;
  u_char length;
  u_char page;
  void* data;
  char resolved;
  char skip;
};

static void BeginMorphBindings(MorphBindingScope* scope) {
  scope->bank = VS::Actors::sharedSequenceBankIndex;
  scope->length = VS::Actors::animationEventLengths[36];
  scope->page = VS::Actors::sharedSequenceBankTexturePages[2];
  scope->data = VS::Actors::sharedSequenceData[2];
  scope->resolved = VS::Actors::lastResolvedSequenceIndex;
  scope->skip = VS::Event::skipState;
  for (int i = 0; i < 17; ++i) {
    scope->actors[i] = Actors_actorRecords[i];
    Actors_actorRecords[i] = nullptr;
  }
  VS::Actors::sharedSequenceBankIndex = 2;
  VS::Actors::animationEventLengths[36] = 3;
  VS::Actors::sharedSequenceBankTexturePages[2] = 0;
  VS::Actors::sharedSequenceData[2] = nullptr;
  VS::Event::skipState = 0;
}

static void EndMorphBindings(const MorphBindingScope* scope) {
  for (int i = 0; i < 17; ++i) {
    Actors_actorRecords[i] = scope->actors[i];
  }
  VS::Actors::sharedSequenceBankIndex = scope->bank;
  VS::Actors::animationEventLengths[36] = scope->length;
  VS::Actors::sharedSequenceBankTexturePages[2] = scope->page;
  VS::Actors::sharedSequenceData[2] = scope->data;
  VS::Actors::lastResolvedSequenceIndex = scope->resolved;
  VS::Event::skipState = scope->skip;
}

static int MorphIsRetired(const MorphAnimationFixture* fixture) {
  const SVECTOR* vertex =
      VS_ONDISC(fixture->actor.instance.model, vertex_section, SVECTOR*);
  return vertex->vx == 10 && vertex->vy == 22 && vertex->vz == 33 &&
         vertex->pad == 44 && !fixture->actor.poseOverrideActive &&
         fixture->actor.animationHeader == 0 &&
         fixture->actor.sharedSequences[2] == nullptr &&
         fixture->actor.pose.event_script == nullptr &&
         fixture->actor.blendPose.event_script == nullptr &&
         fixture->actor.pose.next_event_frame == 0xFFFF &&
         fixture->actor.blendPose.next_event_frame == 0xFFFF &&
         fixture->actor.pose.bone_keys[0].vx == 101 &&
         fixture->actor.pose.bone_keys[0].vy == 202 &&
         fixture->actor.pose.bone_keys[0].vz == 303;
}

int TestSharedSequenceRetirement(void) {
  enum { kBank = 2 };
  MorphAnimationFixture fixture;
  InitializeMorphAnimation(&fixture, 222);
  MorphBindingScope scope;
  BeginMorphBindings(&scope);
  Actors_actorRecords[3] = &fixture.actor;
  VS::Actors::UnloadLiveSharedSequence(kBank + 1);
  const int passed = MorphIsRetired(&fixture) &&
                     VS::Actors::LiveAnimationBattleWindowFrame(3) == 0;
  EndMorphBindings(&scope);
  ForgetMorphAnimation(&fixture);
  return passed;
}

int TestSharedSequenceRetirementControl(int scenario) {
  MorphAnimationFixture first;
  MorphAnimationFixture second;
  MorphBindingScope scope;
  InitializeMorphAnimation(&first, 222);
  InitializeMorphAnimation(&second, 222);
  second.actor.instance.slot = 4;
  BeginMorphBindings(&scope);
  Actors_actorRecords[3] = &first.actor;
  Actors_actorRecords[4] = &second.actor;
  if (scenario == 1) {
    first.actor.animationHeader = reinterpret_cast<intptr_t>(second.sequence);
    first.actor.pose.event_script = second.actor.pose.event_script;
    first.actor.blendPose.event_script = second.actor.blendPose.event_script;
    Actors_actorRecords[4] = nullptr;
  }
  if (scenario == 4 || scenario == 5) {
    MorphAnimationFixture* invalid = scenario == 4 ? &first : &second;
    VS::Actors::SeqAnimationBank* bank =
        reinterpret_cast<VS::Actors::SeqAnimationBank*>(invalid->sequence);
    (reinterpret_cast<u_char*>(bank))[bank->animation_map + 2] = 0xFF;
  }
  const intptr_t expected_header = first.actor.animationHeader;
  if (scenario == 6) VS::Actors::SelectLiveSharedSequenceBank(2);
  int status;
  if (scenario == 4) {
    u_char instruction[] = {0x82, 3};
    status = static_cast<int>(
        VS::Event::ScriptReleaseObjectModelOpcode(instruction, 0));
  } else {
    status = VS::Actors::UnloadLiveSharedSequence(scenario == 3 ? 0 : 3);
  }
  const SVECTOR* vertex =
      VS_ONDISC(first.actor.instance.model, vertex_section, SVECTOR*);
  int passed;
  if (scenario >= 3 && scenario <= 5) {
    passed = status == (scenario == 4 ? 1 : -1) && vertex->vx == 11 &&
             first.actor.poseOverrideActive &&
             first.actor.animationHeader == expected_header &&
             first.actor.sharedSequences[2] != nullptr &&
             first.actor.pose.event_script != nullptr;
  } else if (scenario == 1) {
    passed = status == 0 && vertex->vx == 11 &&
             first.actor.poseOverrideActive &&
             first.actor.animationHeader == expected_header &&
             first.actor.pose.event_script == second.actor.pose.event_script &&
             first.actor.sharedSequences[2] == nullptr;
  } else {
    passed = status == 0 && MorphIsRetired(&first) && MorphIsRetired(&second);
  }
  EndMorphBindings(&scope);
  ForgetMorphAnimation(&first);
  ForgetMorphAnimation(&second);
  return passed;
}

static void ArrangeUnwindScenario(MorphAnimationFixture* fixture, int scenario,
                                  unsigned* wide_sequence) {
  VS::Actors::SeqAnimationBank* bank =
      reinterpret_cast<VS::Actors::SeqAnimationBank*>(fixture->sequence);
  if (scenario == 10) {
    memcpy(wide_sequence, fixture->sequence, sizeof fixture->sequence);
    VS::Actors::SeqAnimationBank* wide =
        reinterpret_cast<VS::Actors::SeqAnimationBank*>(wide_sequence);
    (reinterpret_cast<u_char*>(wide))[wide->animation_map + 2] = 200;
    wide->headers[static_cast<size_t>(5) * 200] = 16;
    wide->headers[static_cast<size_t>(5) * 200 + 2] = 1;
    fixture->actor.animationHeader = reinterpret_cast<intptr_t>(wide);
    fixture->actor.animationSet = 200;
    fixture->actor.sharedSequences[2] = wide;
    VS::Actors::RegisterSequenceBank(wide, sizeof fixture->sequence * 8);
  }
  if (scenario == 2 || (scenario >= 6 && scenario <= 10)) {
    fixture->actor.commonSequence = nullptr;
  }
  // Finished steps, falls and reactions leave this "reinitialize on next play"
  // sentinel; it says nothing about which clip's pose is still bound (#252).
  if (scenario >= 11 && scenario <= 13) fixture->actor.animationSet = 0xFF;
  if (scenario == 14) fixture->actor.animationSet = 5;
  if (scenario == 3) {
    (reinterpret_cast<u_char*>(bank))[bank->animation_map + 1] = 0xFF;
  }
  if (scenario == 4 || scenario == 5) bank->headers[2] = 0;
  if (scenario == 4) fixture->actor.poseOverrideActive = 0;
}

static int UnwindScenarioPassed(const MorphAnimationFixture& fixture,
                                int scenario, int result,
                                const u_char* expected_script,
                                int unchanged_frame) {
  if (scenario == 13) {
    return result != -1 && fixture.actor.animationSet == 0;
  }
  const SVECTOR* vertex =
      VS_ONDISC(fixture.actor.instance.model, vertex_section, SVECTOR*);
  const int successful = scenario < 2 || scenario == 8 || scenario == 9 ||
                         scenario == 10 || scenario == 11 || scenario == 12;
  int passed = vertex->vx == (successful ? 10 : 11);
  if (successful) {
    passed &= result == 0 && fixture.actor.blendPose.next_event_frame == 510;
    if (scenario == 8 || scenario == 9 || scenario == 10) {
      passed &= VS::Actors::sharedSequenceBankIndex == 1;
    }
    if (scenario == 10) {
      passed &=
          static_cast<u_char>(VS::Actors::lastResolvedSequenceIndex) == 200;
    }
  } else {
    passed &= result == (scenario == 4 || scenario == 7 ? 0 : -1) &&
              fixture.actor.poseOverrideActive == (scenario != 4) &&
              fixture.actor.blendPose.next_event_frame == unchanged_frame &&
              fixture.actor.blendPose.event_script == expected_script &&
              fixture.actor.pose.frame == 9;
  }
  return passed;
}

int TestAnimationEventUnwind(int scenario) {
  enum { kMorphOpcode = 36, kSlot = 3, kUnchangedFrame = 77 };
  MorphAnimationFixture fixture;
  MorphAnimationFixture selected;
  unsigned wide_sequence[1024] = {0};
  InitializeMorphAnimation(&fixture, scenario == 1 || scenario == 8 ||
                                             scenario == 9 || scenario == 10 ||
                                             scenario == 12
                                         ? 222
                                         : 1);
  if (scenario == 9) {
    InitializeMorphAnimation(&selected, 222);
    (reinterpret_cast<VS::Actors::SeqAnimationBank*>(selected.sequence))
        ->headers[2] = 0;
    fixture.actor.sharedSequences[1] = selected.sequence;
  }
  Actors_ActorRecord* saved_actor = Actors_actorRecords[kSlot];
  const int saved_bank = VS::Actors::sharedSequenceBankIndex;
  const u_char saved_length = VS::Actors::animationEventLengths[kMorphOpcode];
  const char saved_resolved = VS::Actors::lastResolvedSequenceIndex;
  Actors_actorRecords[kSlot] = &fixture.actor;
  VS::Actors::sharedSequenceBankIndex = 2;
  if (scenario == 8 || scenario == 9 || scenario == 10) {
    VS::Actors::SelectLiveSharedSequenceBank(2);
  }
  VS::Actors::animationEventLengths[kMorphOpcode] = 3;
  fixture.actor.blendPose.next_event_frame = kUnchangedFrame;
  fixture.actor.pose.frame = 9;
  u_char* expected_script = fixture.actor.blendPose.event_script;
  ArrangeUnwindScenario(&fixture, scenario, wide_sequence);
  int result = 0;
  if (scenario == 6 || scenario == 13) {
    result = VS::Actors::PlayLiveActorAnimation(kSlot, 1, 0, 0);
  } else if (scenario == 7) {
    VS::Actors::InitializeLiveActorAnimationPose(&fixture.actor,
                                                 &fixture.actor.pose);
  } else {
    result = VS::Actors::RestartLiveAnimationEvents(fixture.actor);
  }
  const int passed = UnwindScenarioPassed(fixture, scenario, result,
                                          expected_script, kUnchangedFrame);
  Actors_actorRecords[kSlot] = saved_actor;
  VS::Actors::sharedSequenceBankIndex = saved_bank;
  VS::Actors::animationEventLengths[kMorphOpcode] = saved_length;
  VS::Actors::lastResolvedSequenceIndex = saved_resolved;
  ForgetMorphAnimation(&fixture);
  if (scenario == 9) {
    ForgetMorphAnimation(&selected);
  }
  VS::Actors::ForgetSequenceBanks(wide_sequence, sizeof wide_sequence);
  return passed;
}

int Tests_ActionCameraRecovery(void) {
  Battle_ActionState state = {0};
  Battle_ActionState* saved_action = Battle_actionState;
  const VS::Field::SphericalCamera saved_camera = VS::Field::sphericalCamera;
  const int saved_projection = VS::Render::projectionDistance;
  const int saved_ease = VS::Battle::cameraEaseMode;
  const int saved_distance = VS::Battle::settledCameraDistance;
  state.preferredCameraDistance = 2304;
  Battle_actionState = &state;
  VS::Field::sphericalCamera.motion.zoom_mode = 1;
  VS::Field::sphericalCamera.values.distance = 1536;
  VS::Render::projectionDistance = 640;
  VS::Battle::cameraEaseMode = 0;
  VS::Battle::settledCameraDistance = 0;
  VS::Battle::FinishLiveActionSequence();
  const int passed = VS::Render::projectionDistance == 576 &&
                     VS::Field::sphericalCamera.values.distance == 1728;
  Battle_actionState = saved_action;
  VS::Field::sphericalCamera = saved_camera;
  VS::Render::projectionDistance = saved_projection;
  VS::Battle::cameraEaseMode = saved_ease;
  VS::Battle::settledCameraDistance = saved_distance;
  Core_Gte_Reset();
  return passed;
}

int Tests_SparseIdlePoseReset(void) {
  Actors_ActorRecord* saved_actor = Actors_actorRecords[4];
  Field_GimmickInstance* saved_object = Field_gimmickInstances[4];
  Field_GimmickInstance object = {0};
  Actors_ActorRecord actor = {0};
  Actors_actorRecords[4] = nullptr;
  Field_gimmickInstances[4] = nullptr;
  VS::Actors::RestoreLiveActorIdlePose(4);
  object.mode = 0xF9;
  Field_gimmickInstances[4] = &object;
  VS::Actors::RestoreLiveActorIdlePose(4);
  int passed = object.mode == 0;
  actor.animationId = 0x63;
  Actors_actorRecords[4] = &actor;
  VS::Actors::RestoreLiveActorIdlePose(4);
  passed = passed && actor.animationId == 0x63;
  actor.animationId = 0xC7;
  VS::Actors::RestoreLiveActorIdlePose(4);
  passed = passed && actor.animationId == 0xC7;
  Actors_actorRecords[4] = saved_actor;
  Field_gimmickInstances[4] = saved_object;
  return passed;
}

int Tests_TargetLabelStride(void) {
  Battle_TargetSphereSetupView* saved_sphere = Battle_targetSphere;
  Battle_TargetSphere sphere = {0};
  char text[] = "Target";
  for (unsigned index = 0; index < 24; ++index) {
    sphere.labels[index].state = 3;
    sphere.labels[index].screenX = 3000;
    sphere.labels[index].text = text;
    sphere.labels[index].worldPosition = (SVECTOR){10, 20, 30, 0};
  }
  Battle_targetSphere =
      reinterpret_cast<Battle_TargetSphereSetupView*>(&sphere);
  VS::Battle::UpdateSphereTargetLabelsEntry();
  int passed = VS::Battle::AreTargetLabelsSettledEntry() == 1;
  for (unsigned index = 0; index < 24; ++index) {
    passed = passed && sphere.labels[index].screenX == 3032 &&
             sphere.labels[index].state == 0 &&
             sphere.labels[index].text == text &&
             sphere.labels[index].worldPosition.vy == 20;
  }
  Battle_targetSphere = saved_sphere;
  return passed;
}

int Tests_TargetSphereWireframe(void) {
  VS::Render::FrameScratch* frame =
      reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
  const VS::Render::FrameScratch saved_frame = *frame;
  void* saved_primitives = VS::Render::legacyPrimitiveBuffers[0];
  const int saved_frame_buffer = VS::Render::frameBuf;
  Battle_TargetSphereSetupView* saved_sphere = Battle_targetSphere;
  Battle_TargetSphere sphere = {0};
  Battle_TargetVolume volume = {0};
  u_long ordering[0x822];
  u_long packets[12];
  memset(packets, 0xA5, sizeof packets);
  // The Live wire line reserves its packets inside the frame's primitive
  // buffer.
  VS::Render::frameBuf = 0;
  VS::Render::legacyPrimitiveBuffers[0] = packets + 1;
  Core_Gte_Reset();
  Core_Gte_SetGeomOffset(160, 120);
  Core_Gte_SetGeomScreen(256);
  *frame = (VS::Render::FrameScratch){nullptr};
  for (unsigned axis = 0; axis < kSpatialDimensions; ++axis) {
    frame->view_matrix.m[axis][axis] = kFixedPointOne;
  }
  frame->view_matrix.t[2] = 512;
  ClearOTagR(ordering, 0x822);
  frame->ordering_table = ordering;
  frame->primitive_cursor = packets + 1;
  Battle_targetSphere =
      reinterpret_cast<Battle_TargetSphereSetupView*>(&sphere);
  sphere.depth = 128;
  sphere.vertexCount = 2;
  sphere.wireVertices[1].xyz[0] = 4096;
  sphere.wireVertices[1].opaque6 = 1;
  volume.kind = 1;
  volume.scale.extents[0] = volume.scale.extents[1] = volume.scale.extents[2] =
      4;
  VS::Battle::DrawLiveSphereVolumeWireframe(&volume, 0x010401);
  int passed = frame->primitive_cursor == packets + 6 &&
               (ordering[128] & 0xFFFFFF) == LegacyGpu_Handle(packets + 1) &&
               (packets[1] >> 24) == 4 && packets[3] == 0x42208020u &&
               packets[5] == 0x007800A0u && packets[0] == 0xA5A5A5A5u &&
               packets[6] == 0xA5A5A5A5u;
  sphere.wireVertices[1].opaque6 = 2;
  VS::Battle::DrawLiveSphereVolumeWireframe(&volume, 0x010401);
  passed = passed && frame->primitive_cursor == packets + 11 &&
           packets[8] == 0x40208020u && packets[11] == 0xA5A5A5A5u;
  sphere.vertexCount = 0;
  VS::Battle::DrawLiveSphereVolumeWireframe(&volume, 0x010401);
  passed = passed && frame->primitive_cursor == packets + 11;
  Battle_targetSphere = saved_sphere;
  VS::Render::legacyPrimitiveBuffers[0] = saved_primitives;
  VS::Render::frameBuf = saved_frame_buffer;
  *frame = saved_frame;
  Core_Gte_Reset();
  return passed;
}

int Tests_DirectMovementCommand(void) {
  Actors_ActorRecord actor;
  std::ranges::fill(std::as_writable_bytes(std::span(&actor, 1)),
                    std::byte{0xA5});
  Actors_ActorRecord* saved = Actors_actorRecords[3];
  Actors_actorRecords[3] = &actor;
  VS::Actors::ActorMovementRequests& requests =
      VS::Actors::ActorRecordMovementRequests();
  VS::Actors::SetDirectMovement(requests, 3, 0xC00, 1536);
  int passed =
      actor.turnState.mode == 0 && actor.turnState.reserved1 == 0 &&
      actor.turnState.reserved2 == 0 && actor.turnState.reserved3 == 0 &&
      actor.turnState.direction == 0xC00 && actor.turnState.facing == 0xC00 &&
      actor.turnState.distance == 1536 &&
      static_cast<unsigned>(actor.turnState.reservedC) == 0xA5A5A5A5u;
  VS::Actors::SetDirectMovement(requests, 3, 0, 0);
  passed = passed && actor.turnState.mode == 0 &&
           actor.turnState.distance == 0 && actor.turnState.direction == 0 &&
           actor.turnState.facing == 0;
  VS::Actors::QueueNavigationTileTarget(requests, 3, 0x300, 1024, 0x800);
  passed = passed && actor.turnState.mode == 1 &&
           actor.turnState.direction == 0x300 &&
           actor.turnState.facing == 0x800 &&
           actor.turnState.distance == 1024 &&
           static_cast<unsigned>(actor.turnState.reservedC) == 0xA5A5A5A5u;
  Actors_actorRecords[3] = saved;
  return passed;
}

int Tests_GimmickRoomScale(void) {
  enum { kGimmickCameraMatrix = 4 };
  const VS::Render::ModelScratchpad saved_workspace =
      *VS::Render::ModelWorkspace();
  const VS::Actors::SkeletonPose saved_pose = VS::Actors::skeletonPose;
  const MATRIX saved_root = VS::Actors::skeletonRootMatrix;
  const VECTOR saved_scale = VS::Actors::roomActorScale;
  Field_GimmickInstance object = {0};
  VS::Actors::ShpModel model = {0};
  MATRIX camera = {0};
  for (unsigned axis = 0; axis < kSpatialDimensions; ++axis) {
    camera.m[axis][axis] = kFixedPointOne;
    object.matrices[kGimmickCameraMatrix].m[axis][axis] = kFixedPointOne;
  }
  object.model = &model;
  object.scaleX = object.scaleY = object.scaleZ = 64;
  VS::Render::ModelWorkspace()->root_offset = (SVECTOR){10, 20, 30, 0};
  VS::Actors::skeletonPose.scale = (VECTOR){0};
  VS::Actors::roomActorScale = (VECTOR){2048, 4096, 4096, 0};
  VS::Actors::DrawLiveGimmickModel(&object, &camera);
  const int passed =
      object.matrices[0].m[0][0] == 2048 &&
      object.matrices[0].m[1][1] == 4096 &&
      object.matrices[0].m[2][2] == 4096 && object.matrices[0].t[0] == 10 &&
      object.matrices[0].t[1] == 36 && object.matrices[0].t[2] == 30;
  *VS::Render::ModelWorkspace() = saved_workspace;
  VS::Actors::skeletonPose = saved_pose;
  VS::Actors::skeletonRootMatrix = saved_root;
  VS::Actors::roomActorScale = saved_scale;
  Core_Gte_Reset();
  return passed;
}

static int FacialTexturePixelsMatch(RECT* destination, const uint16_t* image,
                                    unsigned source_column) {
  uint16_t pixels[kFacialUploadWidth * kFacialImageHeight];
  StoreImage(destination, reinterpret_cast<u_long*>(pixels));
  for (unsigned row = 0; row < kFacialImageHeight; ++row) {
    for (unsigned column = 0; column < kFacialUploadWidth; ++column) {
      if (pixels[row * kFacialUploadWidth + column] !=
          image[kFacialImageHeaderWords + row * kFacialImageWidth +
                source_column + column]) {
        return 0;
      }
    }
  }
  return 1;
}

int Tests_FacialTextureBankHandoff(void) {
  enum { kFirstFrameColumn = 8, kSharedFrameColumn = 4 };
  uint16_t image[kFacialImageHeaderWords +
                 kFacialImageWidth * kFacialImageHeight] = {kFacialImageWidth,
                                                            kFacialImageHeight};
  for (unsigned index = 0; index < kFacialImageWidth * kFacialImageHeight;
       ++index) {
    image[index + kFacialImageHeaderWords] =
        static_cast<uint16_t>(kFacialFirstPixel + index);
  }
  RECT source = {640, 256, 64, 2}, destination = {264, 84, 4, 2};
  uint16_t saved_source[kFacialImageWidth * kFacialImageHeight];
  uint16_t saved_destination[kFacialUploadWidth * kFacialImageHeight];
  StoreImage(&source, reinterpret_cast<u_long*>(saved_source));
  StoreImage(&destination, reinterpret_cast<u_long*>(saved_destination));
  const VS::Render::FrameScratch saved_frame =
      *reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
  void* saved_ordering = VS::Render::legacyCurrentOrderingTable;
  const unsigned char saved_phase = VS::Content::legacyEtmLoadPhase;
  uint16_t* saved_image = VS::Content::legacyEtmFileBuffer;
  VS::Content::DiscQueueSlot* saved_request = VS::Content::legacyEtmDiscRequest;
  const unsigned char saved_page =
      VS::Actors::sharedSequenceBankTexturePages[0];
  const unsigned char saved_legacy_page =
      VS::Actors::sequenceTexturePagesOneBased[1];
  const VS::Render::TexturePage saved_reservation =
      VS::Render::LiveTexturePages()[26];
  void* saved_primitives =
      VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1];
  const unsigned char saved_bank = VS::Actors::sharedSequenceBankIndex;
  const unsigned char saved_row = VS::Actors::textureSlotRowOffsets[0];
  Actors_ActorRecord* saved_actors[17];
  std::ranges::copy(Actors_actorRecords, saved_actors);
  unsigned char saved_origins[4];
  memcpy(saved_origins, VS::Actors::textureSlotOrigins.data(),
         sizeof saved_origins);
  const VS::Actors::TextureAnimationKeyRecord saved_key =
      VS::Actors::secondarySlotSequences[1][0];
  // The loaded read is a queue slot, as the loader's own reads are.
  VS::Content::DiscQueueSlot* const request =
      &VS::Content::discRequestSlots[31];
  const VS::Content::DiscQueueSlot saved_slot = *request;
  const auto saved_requests = VS::Content::legacyActorAssetRequests;
  request->state = VS::Content::DiscRequestState::kLoaded;
  VS::Content::LegacyActorAssetRequest upload{};
  upload.asset_kind = 9;
  upload.target_slot = 1;
  upload.texture_slot = 26;
  VS::Content::legacyActorAssetRequests[0] = upload;
  VS::Content::legacyEtmLoadPhase = 1;
  VS::Content::legacyEtmFileBuffer = image;
  VS::Content::legacyEtmDiscRequest = request;
  VS::Actors::sharedSequenceBankTexturePages[0] = 0;
  VS::Actors::sequenceTexturePagesOneBased[1] = 0;
  VS::Render::LiveTexturePages()[26] = VS::Render::TexturePage{};
  // The upload leaves the request at the head, pending its second frame.
  int passed = VS::Content::UpdateLiveActorAssetQueue() == 1 &&
               VS::Content::legacyActorAssetRequests[0].asset_kind == 9 &&
               VS::Actors::sharedSequenceBankTexturePages[0] == 26;
  Actors_ActorRecord actor = {0};
  actor.textureHeaderBit = 1;
  actor.textureOverlays[6] = VS::Actors::TextureOverlayRect{16, 20, 8, 2};
  actor.textureAnimationFrames[1] =
      VS::Actors::TextureAnimationFrame{7, 8, 0, 2};
  actor.textureAnimationSlots[1].timeline = 1;
  actor.textureAnimationSlots[1].mode = 1;
  VS::Actors::secondarySlotSequences[1][0] =
      VS::Actors::TextureAnimationKeyRecord{2, 5};
  VS::Actors::sharedSequenceBankIndex = 0;
  const int16_t origin[2] = {4, 0};
  memcpy(VS::Actors::textureSlotOrigins.data(), origin, sizeof origin);
  VS::Actors::textureSlotRowOffsets[0] = 1;
  u_long ordering[0x822];
  DR_MOVE packet = {0};
  // The texture move is bounded to the frame's primitive buffer.
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] = &packet;
  ClearOTagR(ordering, 0x822);
  VS::Render::legacyCurrentOrderingTable = ordering;
  (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
      ->primitive_cursor = &packet;
  VS::Actors::UpdateLiveActorTextureAnimations(&actor);
  passed =
      passed &&
      (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
              ->primitive_cursor == &packet + 1;
  LegacyGpu_WalkList(&ordering[0x821]);
  passed = FacialTexturePixelsMatch(&destination, image, kFirstFrameColumn) &&
           passed;
  std::ranges::fill(Actors_actorRecords, nullptr);
  Actors_actorRecords[1] = &actor;
  actor.textureRowOffsets[1] = 0xFF;
  actor.momentumDistance = 0x12FF;
  passed = passed && actor.textureRowOffsets[2] == 0xFF &&
           offsetof(Actors_ActorRecord, momentumDistance) ==
               offsetof(Actors_ActorRecord, textureRowOffsets) + 2;
  for (unsigned selected = 2; selected <= 3; ++selected) {
    VS::Actors::sharedSequenceBankIndex = 0;
    ClearOTagR(ordering, 0x822);
    (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
        ->primitive_cursor = &packet;
    const int switched = VS::Actors::SelectLiveSharedSequenceBank(selected);
    passed =
        passed && switched == 0 &&
        VS::Actors::sharedSequenceBankIndex == selected - 1 &&
        (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
                ->primitive_cursor == &packet + 1;
    LegacyGpu_WalkList(&ordering[0x821]);
    passed =
        FacialTexturePixelsMatch(&destination, image, kSharedFrameColumn) &&
        passed;
  }
  VS::Actors::sharedSequenceBankIndex = 0;
  ClearOTagR(ordering, 0x822);
  (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
      ->primitive_cursor = &packet;
  const int released = VS::Content::ReleaseLiveSequenceEffectTexture(1);
  passed =
      passed && released == 0 &&
      VS::Actors::sharedSequenceBankTexturePages[0] == 0 &&
      VS::Render::LiveTexturePages()[26].state ==
          VS::Render::TexturePageState::kRetiring &&
      (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
              ->primitive_cursor == &packet + 1;
  LegacyGpu_WalkList(&ordering[0x821]);
  passed = FacialTexturePixelsMatch(&destination, image, kSharedFrameColumn) &&
           passed;
  passed = passed && VS::Content::ReleaseLiveSequenceEffectTexture(0) == -1 &&
           VS::Content::ReleaseLiveSequenceEffectTexture(4) == -1;
  LoadImage(&source, reinterpret_cast<u_long*>(saved_source));
  LoadImage(&destination, reinterpret_cast<u_long*>(saved_destination));
  *reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()) =
      saved_frame;
  VS::Render::legacyCurrentOrderingTable = saved_ordering;
  VS::Content::legacyEtmLoadPhase = saved_phase;
  VS::Content::legacyEtmFileBuffer = saved_image;
  VS::Content::legacyEtmDiscRequest = saved_request;
  *request = saved_slot;
  VS::Content::legacyActorAssetRequests = saved_requests;
  VS::Actors::sharedSequenceBankTexturePages[0] = saved_page;
  VS::Actors::sequenceTexturePagesOneBased[1] = saved_legacy_page;
  VS::Render::LiveTexturePages()[26] = saved_reservation;
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] =
      saved_primitives;
  VS::Actors::sharedSequenceBankIndex = saved_bank;
  VS::Actors::textureSlotRowOffsets[0] = saved_row;
  std::ranges::copy(saved_actors, Actors_actorRecords);
  memcpy(VS::Actors::textureSlotOrigins.data(), saved_origins,
         sizeof saved_origins);
  VS::Actors::secondarySlotSequences[1][0] = saved_key;
  return passed;
}

int Tests_ActorTextureFrameRectangles(int wide, int fallback,
                                      unsigned* command) {
  Actors_ActorRecord actor = {0};
  const VS::Render::FrameScratch saved_frame =
      *reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
  void* saved_ordering = VS::Render::legacyCurrentOrderingTable;
  void* saved_primitives =
      VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1];
  unsigned char saved_origins[4];
  memcpy(saved_origins, VS::Actors::textureSlotOrigins.data(),
         sizeof saved_origins);
  const unsigned char saved_row = VS::Actors::textureSlotRowOffsets[0];
  const unsigned char saved_bank = VS::Actors::sharedSequenceBankIndex;
  const unsigned char saved_page =
      VS::Actors::sharedSequenceBankTexturePages[0];
  struct {
    unsigned before;
    DR_MOVE packet;
    unsigned after;
  } guarded;
  memset(&guarded, 0xA5, sizeof guarded);
  u_long ordering[0x822];
  ClearOTagR(ordering, 0x822);
  VS::Render::legacyCurrentOrderingTable = ordering;
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] =
      &guarded.packet;
  (reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()))
      ->primitive_cursor = &guarded.packet;
  VS::Actors::sharedSequenceBankIndex = 0;
  VS::Actors::sharedSequenceBankTexturePages[0] = 0x1A;
  const int16_t origin[2] = {4, 0};
  memcpy(VS::Actors::textureSlotOrigins.data(), origin, sizeof origin);
  VS::Actors::textureSlotRowOffsets[0] = 1;
  actor.textureHeaderBit = wide;
  actor.textureSlot = 0;
  actor.textureRowOffsets[0] = 7;
  const VS::Actors::TextureOverlayRect rectangle = {16, 20, 32, 12};
  actor.textureOverlays[0] = rectangle;
  actor.textureOverlays[6] = rectangle;
  actor.textureAnimationFrames[0] =
      VS::Actors::TextureAnimationFrame{1, 32, 5, 2};
  actor.textureCellOrigins[1] =
      VS::Actors::TextureCellOriginRecord{32, 5, 2, 0};
  VS::Actors::UploadLiveActorTextureFrame(&actor, fallback ? 1 : 0, 3);
  const unsigned divisor = wide ? 2 : 4;
  const unsigned expectedSource =
      (280u << 16) | (640 + 32 / divisor + 32 / divisor);
  const unsigned expectedDestination = (84u << 16) | (256 + 16 / divisor);
  const unsigned expectedSize = (12u << 16) | (32 / divisor);
  int passed = guarded.before == 0xA5A5A5A5 && guarded.after == 0xA5A5A5A5 &&
               guarded.packet.code[1] == 0x80000000 &&
               guarded.packet.code[2] == expectedSource &&
               guarded.packet.code[3] == expectedDestination &&
               guarded.packet.code[4] == expectedSize &&
               nextPrim(&ordering[0x821]) == &guarded.packet;
  if (command != nullptr) {
    for (size_t index = 0;
         index < sizeof guarded.packet.code / sizeof guarded.packet.code[0];
         ++index) {
      command[index] = guarded.packet.code[index];
    }
  }
  *reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase()) =
      saved_frame;
  VS::Render::legacyCurrentOrderingTable = saved_ordering;
  VS::Render::legacyPrimitiveBuffers[VS::Render::frameBuf & 1] =
      saved_primitives;
  memcpy(VS::Actors::textureSlotOrigins.data(), saved_origins,
         sizeof saved_origins);
  VS::Actors::textureSlotRowOffsets[0] = saved_row;
  VS::Actors::sharedSequenceBankIndex = saved_bank;
  VS::Actors::sharedSequenceBankTexturePages[0] = saved_page;
  return passed;
}

int Tests_RoomSpawnCountGuard(void) {
  const VS::Field::ZoneContext saved = VS::Field::zoneContext;
  int sentinel = 0;
  VS::Field::zoneContext = {};
  VS::Field::zoneContext.zud_data = &sentinel;
  VS::Field::LiveSpawnRoomActors();
  const int passed = VS::Field::zoneContext.zud_data == &sentinel;
  VS::Field::zoneContext = saved;
  return passed;
}

int TestGimmickStackTop(int scenario) {
  enum { kActorSlots = 17, kGimmickSlots = 16, kRoot = 3, kMember = 4 };
  Actors_ActorRecord* saved_actors[kActorSlots];
  Field_GimmickInstance* saved_objects[kGimmickSlots];
  Actors_ActorRecord actor = {0};
  Field_GimmickInstance root = {0};
  Field_GimmickInstance member = {0};
  const int skipped = scenario == 0;
  const int has_actor = scenario != 0;
  const int has_member = scenario == 2;
  for (int i = 0; i < kActorSlots; ++i) {
    saved_actors[i] = Actors_actorRecords[i];
    Actors_actorRecords[i] = nullptr;
  }
  for (int i = 0; i < kGimmickSlots; ++i) {
    saved_objects[i] = Field_gimmickInstances[i];
    Field_gimmickInstances[i] = nullptr;
  }
  root.tileX = root.tileZ = member.tileX = member.tileZ = 2;
  root.parent = root.objectInfo.carrier = 0xFF;
  member.parent = member.objectInfo.carrier = 0xFF;
  root.isSkipped = skipped;
  root.slot = kRoot;
  member.slot = kMember;
  member.positionY = -128;
  member.velocityY = 77;
  Field_gimmickInstances[kRoot] = &root;
  if (has_member) Field_gimmickInstances[kMember] = &member;
  if (has_actor) {
    actor.instance.slot = 2;
    actor.instance.currentTileX = actor.instance.currentTileZ = 2;
    actor.instance.position = (SVECTOR){320, -128, 320, 0};
    actor.instance.rootMotion = (SVECTOR){1, 2, 3, 4};
    Actors_actorRecords[2] = &actor;
  }
  VS::Field::DropUnsupportedGimmickStackEntry(kRoot);
  int passed = root.objectInfo.isFalling == 1;
  if (has_actor) {
    passed &= actor.instance.movementMode == 3 &&
              actor.instance.jumpCountdown == 4 && actor.destinationMode == 1 &&
              actor.destinationX == 2 && actor.destinationZ == 2 &&
              actor.instance.rootMotion.vx == 0 &&
              actor.instance.rootMotion.vy == 0 &&
              actor.instance.rootMotion.vz == 0 && actor.supportGimmick == 0;
  }
  if (has_member) passed &= member.mode == 0xFC && member.velocityY == 0;
  for (int i = 0; i < kActorSlots; ++i) {
    Actors_actorRecords[i] = saved_actors[i];
  }
  for (int i = 0; i < kGimmickSlots; ++i) {
    Field_gimmickInstances[i] = saved_objects[i];
  }
  return passed;
}

int TestOptionalMovementWrapper(int operation, int rate) {
  enum { kSlot = 4 };
  Actors_ActorRecord* saved_actor = Actors_actorRecords[kSlot];
  SVECTOR target = {117, -384, 370, 0};
  VS::Actors::TileTarget tile = {0};
  int valid = 1;
  Actors_actorRecords[kSlot] = nullptr;
  switch (operation) {
    case 0:
      VS::Actors::MoveLiveActorToPositionAtRate(kSlot, &target, rate, rate);
      break;
    case 1:
      VS::Actors::MoveLiveActorToTileAtRate(kSlot, &tile, rate, rate);
      break;
    case 2:
      VS::Actors::TranslateLiveActorToTileAtRate(kSlot, &tile, rate);
      break;
    case 3:
      VS::Actors::TurnLiveActorTowardTileAtRate(kSlot, &tile, rate);
      break;
    case 4:
      VS::Actors::TranslateLiveActorByAtRate(kSlot, &target, rate);
      break;
    case 5:
      VS::Actors::TranslateLiveActorToAtRate(kSlot, &target, rate);
      break;
    case 6:
      VS::Actors::TurnLiveActorByAtRate(kSlot, 0, rate);
      break;
    case 7:
      VS::Actors::TurnLiveActorToAtRate(kSlot, 0, rate);
      break;
    default:
      valid = 0;
      break;
  }
  const int passed = valid && Actors_actorRecords[kSlot] == nullptr;
  Actors_actorRecords[kSlot] = saved_actor;
  return passed;
}

int TestOptionalActorScriptSlide(int present, int rate, int skip) {
  enum { kSlot = 4, kDefaultRate = 3 };
  Actors_ActorRecord actor = {0};
  Actors_ActorRecord* saved_actor = Actors_actorRecords[kSlot];
  const char saved_control = VS::Event::scriptControlledActors[kSlot];
  const char saved_skip = VS::Event::skipState;
  u_char instruction[] = {0x28, kSlot, 0x10, 234, 0, 128, 254, 114, 1, 0};
  instruction[9] = static_cast<u_char>(rate);
  Actors_actorRecords[kSlot] = present ? &actor : nullptr;
  actor.moveRate = kDefaultRate;
  actor.instance.position = (SVECTOR){100, 200, 300, 0};
  VS::Event::skipState = skip ? 2 : 0;
  VS::Event::scriptControlledActors[kSlot] = 0;
  const intptr_t status =
      VS::Event::ScriptSlideActorToWorldPosOpcode(instruction, 0);
  int passed = status == 0 && VS::Event::scriptControlledActors[kSlot] == 1;
  if (present) {
    passed &= actor.moveRate == kDefaultRate;
    passed &= actor.instance.rootMotion.vx == 17 &&
              actor.instance.rootMotion.vy == -584 &&
              actor.instance.rootMotion.vz == 70;
    if (rate == 0 || skip) {
      passed &= actor.instance.position.vx == 117 &&
                actor.instance.position.vy == -384 &&
                actor.instance.position.vz == 370;
    } else {
      passed &= actor.instance.position.vx == 100 &&
                actor.instance.position.vy == 200 &&
                actor.instance.position.vz == 300 &&
                actor.instance.currentAnimation > 0;
    }
  }
  Actors_actorRecords[kSlot] = saved_actor;
  VS::Event::scriptControlledActors[kSlot] = saved_control;
  VS::Event::skipState = saved_skip;
  return passed;
}

enum { kScriptedEffectSlot = 3, kScriptedEffectModel = 53 };

static u_short ScriptedEffectSelector(int scenario) {
  switch (scenario) {
    case 0:
    case 3:
    case 6:
    case 7:
      return kScriptedEffectModel;
    case 5:
      return 0x101F;
    case 10:
      return 0x1002;
    case 8:
      return 0x2000;
    case 9:
      return 0x3000;
    default:
      return 0x1000 | kScriptedEffectSlot;
  }
}

int TestScriptedEffectTargetBinding(int scenario) {
  enum { kEffect = 222, kSlots = 16 };
  Actors_ActorRecord actor = {0};
  Field_GimmickInstance object = {0};
  Battle_Actor battle_actor = {nullptr};
  Battle_Actor* saved_battle[kSlots];
  Actors_ActorRecord* saved_actor[kSlots];
  Field_GimmickInstance* saved_object[kSlots];
  const VS::Effects::LiveActionEffectRequest saved_request =
      VS::Effects::actionRequest;
  const VS::Effects::LiveEffectCamera saved_camera = VS::Effects::effectCamera;
  const VS::Effects::LiveEffectCamera saved_action_camera =
      VS::Effects::savedActionCamera;
  VS::Effects::LiveEffectPackage* saved_package = VS::Effects::package;
  const u_int saved_phase = VS::Effects::primaryPhase;
  const char saved_token = VS::Effects::primaryToken;
  int saved_bitmaps[4];
  u_char request_before[sizeof VS::Effects::actionRequest];
  u_char request_after[sizeof VS::Effects::actionRequest];
  memcpy(request_before, &VS::Effects::actionRequest, sizeof request_before);
  for (int i = 0; i < kSlots; ++i) {
    saved_battle[i] = Battle_actors[i];
    saved_actor[i] = Actors_actorRecords[i];
    saved_object[i] = Field_gimmickInstances[i];
    Battle_actors[i] = nullptr;
    Actors_actorRecords[i] = nullptr;
    Field_gimmickInstances[i] = nullptr;
  }
  memcpy(saved_bitmaps, VS::Effects::tokenBitmaps, sizeof saved_bitmaps);
  memset(VS::Effects::tokenBitmaps, 0, sizeof saved_bitmaps);
  const int present = scenario < 3 || scenario == 7;
  const int slot = scenario == 10 ? 2
                   : scenario == 3 || scenario == 6 || scenario == 7
                       ? 1
                       : kScriptedEffectSlot;
  if (present || scenario == 6) {
    Battle_actors[slot] = &battle_actor;
    battle_actor.modelId =
        scenario == 6 ? kScriptedEffectModel + 1 : kScriptedEffectModel;
    if (scenario == 2) {
      Field_gimmickInstances[slot] = &object;
    } else {
      Actors_actorRecords[slot] = &actor;
    }
  }
  const u_short selector = ScriptedEffectSelector(scenario);
  const int effect = scenario == 10 ? 298 : kEffect;
  u_char instruction[] = {
      0xA0, static_cast<u_char>(effect), static_cast<u_char>(effect >> 8),
      static_cast<u_char>(selector), static_cast<u_char>(selector >> 8)};
  VS::Effects::primaryPhase = 3;
  const intptr_t result =
      VS::Event::CommandPlayActorEffectOpcode(instruction, 0);
  memcpy(request_after, &VS::Effects::actionRequest, sizeof request_after);
  // Absent model (3) and direct slot references to an empty slot (4, 10) start
  // like the original: func_800BFE50 returns type-1 slots unchecked.
  const int empty = scenario == 3 || scenario == 4 || scenario == 10;
  const int queued = present || empty;
  int passed = result == (queued ? 0 : 1);
  if (queued) {
    passed &= VS::Effects::primaryPhase == 6 &&
              VS::Effects::actionRequest.effect_id == effect &&
              VS::Effects::actionRequest.count == 1 &&
              VS::Effects::actionRequest.targets[0].type == 4 &&
              VS::Effects::actionRequest.targets[0].Actor().actor == slot;
    if (empty) {
      passed &= Battle_actors[slot] == nullptr &&
                Actors_actorRecords[slot] == nullptr &&
                Field_gimmickInstances[slot] == nullptr;
      // The started effect's emit_to readers must leave defaults untouched.
      SVECTOR bone = {11, 22, 33, 44};
      VS::Actors::ActorLocation placement{};
      placement.packed_tile.word = 55;
      placement.position = {66, 77, 88, 99};
      passed &= VS::Actors::LiveActorBonePosition(slot, 0, &bone, 0) == -1 &&
                bone.vx == 11 && bone.vy == 22 && bone.vz == 33 &&
                VS::Actors::LiveActorPlacement(slot, &placement) == -1 &&
                placement.packed_tile.word == 55 &&
                placement.position.vx == 66 && placement.position.vy == 77 &&
                placement.position.vz == 88 && placement.position.pad == 99 &&
                VS::Actors::LiveActorStateFlags(slot) == 0;
    }
  } else {
    passed &=
        VS::Effects::primaryPhase == 3 &&
        VS::Effects::primaryToken == saved_token &&
        memcmp(request_before, request_after, sizeof request_before) == 0 &&
        VS::Effects::package == saved_package;
    for (int i = 0; i < 4; ++i) {
      passed &= VS::Effects::tokenBitmaps[i] == 0;
    }
  }
  for (int i = 0; i < kSlots; ++i) {
    Battle_actors[i] = saved_battle[i];
    Actors_actorRecords[i] = saved_actor[i];
    Field_gimmickInstances[i] = saved_object[i];
  }
  memcpy(VS::Effects::tokenBitmaps, saved_bitmaps, sizeof saved_bitmaps);
  VS::Effects::actionRequest = saved_request;
  VS::Effects::effectCamera = saved_camera;
  VS::Effects::savedActionCamera = saved_action_camera;
  VS::Effects::package = saved_package;
  VS::Effects::primaryPhase = saved_phase;
  VS::Effects::primaryToken = saved_token;
  return passed;
}

int TestBoneEndpointOwner(int owner, int endpoint) {
  enum { kModelWords = 20, kBoneLength = 128 };
  const int slot = owner >= 4 ? 16 : 3;
  const int has_gimmick_slot =
      slot < sizeof Field_gimmickInstances / sizeof Field_gimmickInstances[0];
  Actors_ActorRecord actor = {0};
  Field_GimmickInstance object = {0};
  unsigned model_bytes[kModelWords] = {0};
  VS::Actors::ShpModel* model =
      reinterpret_cast<VS::Actors::ShpModel*>(model_bytes);
  Actors_ActorRecord* saved_actor = Actors_actorRecords[slot];
  Field_GimmickInstance* saved_object =
      has_gimmick_slot ? Field_gimmickInstances[slot] : nullptr;
  const MATRIX saved_matrix = VS::Actors::boneMatrixScratch;
  const VS::Render::ModelScratchpad saved_workspace =
      *VS::Render::ModelWorkspace();
  Actors_actorRecords[slot] =
      owner == 0 || owner == 2 || owner == 5 ? &actor : nullptr;
  if (has_gimmick_slot) {
    Field_gimmickInstances[slot] = owner == 1 || owner == 2 ? &object : nullptr;
  }
  actor.instance.model = object.model = model;
  actor.instance.boneCount = object.boneCount = 1;
  actor.modelDataSize = sizeof model_bytes;
  VS::Render::RecordLiveModelExtent(model, sizeof model_bytes);
  model->bones[0].length = kBoneLength;
  VS::Actors::boneMatrixScratch = (MATRIX){0};
  for (int axis = 0; axis < kSpatialDimensions; ++axis) {
    VS::Actors::boneMatrixScratch.m[axis][axis] = kFixedPointOne;
    VS::Actors::boneMatrixScratch.t[axis] = (axis + 1) * 10;
    actor.boneMatrices[0].m[axis][axis] = kFixedPointOne;
    object.matrices[0].m[axis][axis] = kFixedPointOne;
    actor.boneMatrices[0].t[axis] = (axis + 1) * 110;
    object.matrices[0].t[axis] = actor.boneMatrices[0].t[axis] + 100;
  }
  Core_Gte_Reset();
  SVECTOR result = {1, 2, 3, 77};
  const int status =
      VS::Actors::LiveActorBoneEndpointPosition(slot, 0, &result, endpoint);
  const int offset = owner == 1 ? 100 : 0;
  const int length = endpoint == 0   ? 0
                     : endpoint == 1 ? kBoneLength
                                     : kBoneLength / 2;
  int passed =
      owner == 3 || owner == 4
          ? status == -1 && result.vx == 1 && result.vy == 2 && result.vz == 3
          : status == 0 && result.vx == 100 + offset - length &&
                result.vy == 200 + offset && result.vz == 300 + offset;
  if (owner == 3 || owner == 4) {
    MATRIX untouched;
    MATRIX original;
    int bone_length = 777;
    memset(&untouched, 0x6B, sizeof untouched);
    original = untouched;
    passed &= !VS::Actors::LiveActorBoneViewTransform(slot, 0);
    passed &= VS::Actors::LiveActorBoneWorldMatrix(slot, 0, &untouched,
                                                   &bone_length) == -1;
    passed &= memcmp(untouched.m, original.m, sizeof untouched.m) == 0 &&
              memcmp(untouched.t, original.t, sizeof untouched.t) == 0 &&
              bone_length == 777;
  }
  Actors_actorRecords[slot] = saved_actor;
  if (has_gimmick_slot) Field_gimmickInstances[slot] = saved_object;
  VS::Render::ForgetLiveModelExtent(model);
  VS::Actors::boneMatrixScratch = saved_matrix;
  *VS::Render::ModelWorkspace() = saved_workspace;
  Core_Gte_Reset();
  return passed && result.pad == 77;
}

int Tests_ProjectedBoneEndpoints(void) {
  Actors_ActorRecord actor = {0};
  unsigned modelBytes[24] = {0};
  VS::Actors::ShpModel* model =
      reinterpret_cast<VS::Actors::ShpModel*>(modelBytes);
  Actors_ActorRecord* saved = Actors_actorRecords[0];
  Actors_actorRecords[0] = &actor;
  actor.instance.boneCount = 1;
  actor.instance.model = model;
  actor.modelDataSize = sizeof modelBytes;
  model->bones[0].length = 128;
  for (int axis = 0; axis < kSpatialDimensions; ++axis) {
    actor.boneMatrices[0].m[axis][axis] = kFixedPointOne;
  }
  actor.boneMatrices[0].t[2] = 1024;
  Core_Gte_Reset();
  Core_Gte_SetGeomOffset(160, 120);
  Core_Gte_SetGeomScreen(256);
  int output[2] = {0};
  int passed = VS::Actors::ProjectLiveActorBoneEndpoint(0, 0, output, 0) == 0 &&
               output[0] == ((120 << 16) | 160) && output[1] == 1024;
  passed = passed &&
           VS::Actors::ProjectLiveActorBoneEndpoint(0, 0, output, 1) == 0 &&
           static_cast<int16_t>(output[0]) == 128 && output[1] == 1024;
  passed = passed &&
           VS::Actors::ProjectLiveActorBoneEndpoint(0, 0, output, 2) == 0 &&
           static_cast<int16_t>(output[0]) == 144;
  passed = passed &&
           VS::Actors::ProjectLiveActorBoneEndpoint(0, 1, output, 0) == -4 &&
           VS::Actors::ProjectLiveActorBoneEndpoint(-1, 0, output, 0) == -1;
  Actors_actorRecords[0] = saved;
  Core_Gte_Reset();
  return passed;
}

int Tests_ActorPoseHierarchy(void) {
  unsigned file[32] = {0};
  Actors_ActorRecord actor = {0};
  VS::Actors::ShpModel* model = reinterpret_cast<VS::Actors::ShpModel*>(file);
  const VS::Actors::SkeletonPose saved_pose = VS::Actors::skeletonPose;
  const VS::Actors::ModelRenderContext saved_context =
      VS::Actors::modelRenderContext;
  const VS::Render::ModelScratchpad saved_workspace =
      *VS::Render::ModelWorkspace();
  actor.instance.model = model;
  actor.modelDataSize = sizeof file;
  actor.instance.boneCount = 2;
  actor.instance.scaleX = actor.instance.scaleY = actor.instance.scaleZ =
      actor.scale = 4096;
  model->bones[0].length = 128;
  model->bones[1].parent_index = 0;
  for (int bone = 0; bone < 2; ++bone) {
    actor.blendPose.scale_keys[bone].vx = actor.blendPose.scale_keys[bone].vy =
        actor.blendPose.scale_keys[bone].vz = 64;
  }
  actor.blendPose.scale_keys[1].vx = 32;
  MATRIX camera = {0};
  for (int axis = 0; axis < 3; ++axis) camera.m[axis][axis] = 4096;
  setVector(&VS::Render::ModelWorkspace()->root_offset, 100, 200, 300);
  int passed = VS::Actors::BuildLiveActorModelPose(actor, camera) == 1 &&
               actor.boneMatrices[0].t[0] == 100 &&
               actor.boneMatrices[1].t[0] == -28 &&
               actor.boneMatrices[1].t[1] == 200 &&
               actor.boneMatrices[1].m[0][0] == 2048 &&
               VS::Actors::skeletonPose.bone_matrices[1].m[0][0] == 4096;
  actor.instance.isVisible = 1;
  actor.instance.wasDrawn = 1;
  actor.customTint = 1;
  actor.instance.tint = (VS::Render::ColorCode){100, 70, 20, 0};
  actor.fadeRed = 255;
  actor.fadeGreen = 50;
  actor.fadeBlue = 246;
  actor.instance.vertexGroupCount = 1;
  model->group_section = 96;
  model->vertex_section = 100;
  model->polygon_section = 108;
  uint16_t* group =
      reinterpret_cast<uint16_t*>(reinterpret_cast<unsigned char*>(file) + 96);
  group[0] = 0;
  group[1] = 1;
  setVector(&VS::Render::ModelWorkspace()->root_offset, 0, 0, 1024);
  Core_Gte_Reset();
  Core_Gte_SetGeomScreen(256);
  VS::Actors::DrawLiveActorModel(actor, camera, 0);
  passed = passed && actor.instance.wasDrawn &&
           VS::Actors::modelRenderContext.projected_vertex_count == 1;
  passed = passed && actor.instance.baseColor.r == 96 &&
           actor.instance.baseColor.g == 255 &&
           actor.instance.baseColor.b == 0 &&
           (actor.shadowBlend.color & 0xFFFFFF) == 0x144664;
  actor.customTint = 0;
  VS::Actors::DrawLiveActorModel(actor, camera, 0);
  passed = passed && actor.instance.baseColor.r == 124 &&
           actor.instance.baseColor.g == 255 &&
           actor.instance.baseColor.b == 88;
  model->bones[1].parent_index = 1;
  passed = passed && VS::Actors::BuildLiveActorModelPose(actor, camera) == 0;
  model->bones[1].parent_index = 0;
  actor.modelDataSize = sizeof(VS::Actors::ShpModel);
  passed = passed && VS::Actors::BuildLiveActorModelPose(actor, camera) == 0;
  VS::Actors::skeletonPose = saved_pose;
  VS::Actors::modelRenderContext = saved_context;
  *VS::Render::ModelWorkspace() = saved_workspace;
  Core_Gte_Reset();
  return passed;
}

struct ModelPacketOptions {
  int colored;
  int split;
  int limb;
  unsigned pulse_mask;
};

static void BuildModelPacketFixture(VS::Actors::ShpModel* model,
                                    Actors_ModelInstance* instance,
                                    const ModelPacketOptions* options) {
  instance->triangleCount = 1;
  instance->uvLayout = options->colored != 0;
  instance->palettePulse.kind = static_cast<u_char>(options->pulse_mask);
  if (static_cast<unsigned>(options->limb) < 8) {
    const u_short count = 1;
    const size_t group =
        (options->split != 0 ? 32 : 0) + static_cast<size_t>(options->limb) * 2;
    memcpy(model->header + group, &count, sizeof count);
  }
  model->polygon_section = 64;
  unsigned char* polygon = reinterpret_cast<unsigned char*>(model) + 64;
  const uint16_t indices[3] = {0, 4, 8};
  memcpy(polygon + (options->colored ? 0 : 4), indices, sizeof indices);
  if (options->colored) {
    polygon[11] = 0x34;
    polygon[8] = 32;
    polygon[12] = 64;
    polygon[16] = 96;
    polygon[6] = 50;
    polygon[7] = 60;
    polygon[20] = 10;
    polygon[21] = 20;
    polygon[22] = 30;
    polygon[23] = 40;
  } else {
    polygon[0] = 0x24;
    polygon[1] = 16;
    polygon[10] = 50;
    polygon[11] = 60;
    polygon[12] = 10;
    polygon[13] = 20;
    polygon[14] = 30;
    polygon[15] = 40;
  }
  if (options->split != 0) {
    VS::Actors::modelRenderContext.projected_vertex_count = 4;
    VS::Actors::modelRenderContext.projected_vertices[3] = (160 << 16) | 160;
    VS::Actors::modelRenderContext.vertex_depths[3] =
        1024 + options->split * 128;
    instance->triangleCount = 0;
    instance->otherPolygonCount = 1;
    polygon[0] = 0x2C;
    polygon[1] = 20;
    polygon[2] = 3;
    const uint16_t quad_indices[4] = {0, 4, 8, 12};
    memcpy(polygon + 4, quad_indices, sizeof quad_indices);
    const unsigned char coordinates[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    memcpy(polygon + 12, coordinates, sizeof coordinates);
  }
}

static int ModelPacketColorsMatch(const u_long* packet, int colored,
                                  unsigned blend, const CVECTOR* tint) {
  enum { kVertexPacketWords = 3, kTriangleCorners = 3 };
  if (!colored) return 1;
  if (blend == 0) {
    return (packet[1] & UINT8_MAX) == 32 &&
           (packet[1 + kVertexPacketWords] & UINT8_MAX) == 64;
  }
  const unsigned uniform = tint->r | static_cast<unsigned>(tint->g) << 8 |
                           static_cast<unsigned>(tint->b) << 16;
  for (int corner = 0; corner < kTriangleCorners; ++corner) {
    if ((packet[1 + corner * kVertexPacketWords] & 0xFFFFFF) != uniform) {
      return 0;
    }
  }
  return 1;
}

static void DrawGimmickOverrideFixture(const Actors_ModelInstance* instance,
                                       const CVECTOR* tint,
                                       VS::Actors::ShpModel* model,
                                       unsigned appearance,
                                       unsigned pulse_mask) {
  Field_GimmickInstance object = {0};
  enum { kSharedRendererHeaderBytes = 8 };
  memcpy(&object, instance, kSharedRendererHeaderBytes);
  object.motionState = appearance;
  object.palettePulse.kind = pulse_mask;
  object.model = model;
  memcpy(&object.baseColor, tint, sizeof object.baseColor);
  VS::Render::DrawLiveGimmickGouraudPolygonsWithOverrides(object);
}

struct SourceWeaponMatrixLayout {
  unsigned char header[0x1C];
  uint32_t offsets;
  MATRIX matrices[4];
};
static_assert(offsetof(SourceWeaponMatrixLayout, matrices) == 0x20,
              "Original 3A1A0.h weapon matrix offset");
static_assert(0x20 + 2 * sizeof(MATRIX) + 2 * sizeof(int16_t) == 0x64,
              "Original mask byte is matrix2 m02");
static_assert(offsetof(VS::Actors::WeaponModel, bone_world_matrices) != 0x20,
              "Native weapon pointers must not use the guest matrix offset");

static void DrawWeaponOverrideFixture(const Actors_ModelInstance* instance,
                                      const CVECTOR* tint,
                                      VS::Actors::ShpModel* model,
                                      unsigned appearance,
                                      unsigned pulse_mask) {
  VS::Actors::WeaponModel weapon = {0};
  enum { kSharedRendererHeaderBytes = 8 };
  memcpy(&weapon, instance, kSharedRendererHeaderBytes);
  weapon.owner_motion_state = appearance;
  const int mask = static_cast<int>(pulse_mask & UINT8_MAX);
  weapon.bone_world_matrices[2].m[0][2] = static_cast<int16_t>(0x5A00 | mask);
  weapon.bone_world_matrices[1].t[2] = mask ^ UINT8_MAX;
  weapon.section_offsets =
      reinterpret_cast<VS::Actors::WeaponModelData*>(model);
  VS::Render::DrawLiveWeaponFlatPolygonsWithOverrides(weapon, *tint);
}

// An actor drawing its own geometry: the override drawers when its pulse or
// appearance selects them (func_800B28A8 49EB8.c:292-304), else the plain pass.
static void DrawActorPolygonsFixture(const Actors_ActorRecord& actor,
                                     bool colored, const CVECTOR& tint,
                                     unsigned pulse_mask, unsigned appearance) {
  if (VS::Actors::UsesModelOverrides(
          false, {.pulse_kind = static_cast<uint8_t>(pulse_mask),
                  .state = static_cast<uint8_t>(appearance)})) {
    VS::Render::DrawLiveActorPolygonsWithOverrides(
        actor, colored, tint,
        {.pulse_mask = pulse_mask, .appearance_state = appearance});
  } else if (colored) {
    VS::Render::DrawLiveActorGouraudPolygons(actor, tint);
  } else {
    VS::Render::DrawLiveActorFlatPolygons(actor, tint);
  }
}

static int ModelPolygonPackets(int colored, int split, int limb,
                               unsigned pulse_mask, unsigned appearance,
                               int override_kind) {
  enum { kTriangleCorners = 3, kVertexDepth = 1024 };
  unsigned modelWords[32] = {0};
  VS::Actors::ShpModel* model =
      reinterpret_cast<VS::Actors::ShpModel*>(modelWords);
  Actors_ModelInstance instance = {0};
  const ModelPacketOptions options = {colored, split, limb, pulse_mask};
  const VS::Actors::ModelRenderContext saved_context =
      VS::Actors::modelRenderContext;
  VS::Render::FrameScratch* frame =
      reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
  const VS::Render::FrameScratch saved_frame = *frame;
  const int saved_frame_index = VS::Render::frameBuf;
  void* saved_buffer = VS::Render::legacyPrimitiveBuffers[0];
  const VS::Render::ModelScratchpad saved_workspace =
      *VS::Render::ModelWorkspace();
  unsigned char* memory = static_cast<unsigned char*>(malloc(0x20010));
  if (memory == nullptr) return 0;
  memset(memory, 0xA5, 0x20010);
  u_long ordering[2048];
  ClearOTagR(ordering, 2048);
  memset(frame, 0, sizeof *frame);
  frame->primitive_cursor = memory + 8;
  frame->ordering_table = ordering;
  VS::Render::frameBuf = 0;
  VS::Render::legacyPrimitiveBuffers[0] = memory + 8;
  VS::Render::ModelWorkspace()->palette = 0x1234;
  VS::Render::ModelWorkspace()->texture_page = 0x15;
  memset(&VS::Actors::modelRenderContext, 0,
         sizeof VS::Actors::modelRenderContext);
  VS::Actors::modelRenderContext.projected_vertex_count = 3;
  VS::Actors::modelRenderContext.projected_vertices[0] = (80 << 16) | 80;
  VS::Actors::modelRenderContext.projected_vertices[1] = (80 << 16) | 160;
  VS::Actors::modelRenderContext.projected_vertices[2] = (160 << 16) | 80;
  for (int index = 0; index < kTriangleCorners; ++index) {
    VS::Actors::modelRenderContext.vertex_depths[index] = kVertexDepth;
  }
  BuildModelPacketFixture(model, &instance, &options);
  instance.actionAnimationState = appearance;
  CVECTOR tint = {128, 96, 64, 0};
  static Actors_ActorRecord actor;
  actor = {};
  actor.instance = instance;
  actor.instance.model = model;
  actor.modelDataSize = sizeof modelWords;
  VS::Render::RecordLiveModelExtent(model, sizeof modelWords);
  if (override_kind == 1) {
    DrawGimmickOverrideFixture(&instance, &tint, model, appearance, pulse_mask);
  } else if (override_kind == 2) {
    DrawWeaponOverrideFixture(&instance, &tint, model, appearance, pulse_mask);
  } else {
    DrawActorPolygonsFixture(actor, colored != 0, tint, pulse_mask, appearance);
  }
  VS::Render::ForgetLiveModelExtent(model);
  // The static record must not keep the stack model past this frame.
  actor = {};
  const u_long* packet = reinterpret_cast<const u_long*>(memory + 8);
  const unsigned palette = 0x1234 + (static_cast<unsigned>(limb) < 8 &&
                                             (pulse_mask & (0x80u >> limb)) != 0
                                         ? 0x480
                                         : 0);
  const unsigned first_uv = (palette << 16) | 0x140A;
  const int appearance_override =
      override_kind != 0 || pulse_mask != 0 ||
      (appearance >= 4 && appearance < 9 && appearance != 6) ||
      appearance == 15;
  const unsigned blend = appearance_override && appearance != 0
                             ? (appearance < 7 ? 0x40 : 0x20)
                             : 0;
  const unsigned command_bit = blend != 0 ? 2 : 0;
  const unsigned page_uv = ((0x15 | blend) << 16) | 0x281E;
  int passed = frame->primitive_cursor == memory + 8 + (colored ? 40 : 32) &&
               (packet[1] >> 24) == ((colored ? 0x34u : 0x24u) | command_bit) &&
               packet[3] == first_uv && packet[colored ? 6 : 5] == page_uv &&
               packet[colored ? 9 : 7] == 0x3C32u &&
               nextPrim(&ordering[254]) == packet;
  passed = passed && ModelPacketColorsMatch(packet, colored, blend, &tint);
  if (split != 0) {
    const u_long* second = packet + 8;
    const auto& projected = VS::Actors::modelRenderContext.projected_vertices;
    passed = frame->primitive_cursor == memory + 72 &&
             (packet[1] >> 24) == (0x24 | command_bit) &&
             (second[1] >> 24) == (0x24 | command_bit) &&
             packet[3] == first_uv && second[3] >> 16 == palette &&
             packet[5] == page_uv &&
             packet[7] == (split > 0 ? 0x3C32u : 0x5046u) &&
             packet[6] == static_cast<u_long>(projected[split > 0 ? 2 : 3]) &&
             second[2] == static_cast<u_long>(projected[split > 0 ? 1 : 0]) &&
             second[4] == static_cast<u_long>(projected[3]) &&
             second[6] == static_cast<u_long>(projected[2]);
  }
  passed = passed && memory[0] == 0xA5 && memory[0x2000F] == 0xA5;
  *frame = saved_frame;
  VS::Actors::modelRenderContext = saved_context;
  *VS::Render::ModelWorkspace() = saved_workspace;
  VS::Render::frameBuf = saved_frame_index;
  VS::Render::legacyPrimitiveBuffers[0] = saved_buffer;
  free(memory);
  return passed;
}

int Tests_ModelPolygonPackets(int colored, int split) {
  return ModelPolygonPackets(colored, split, 8, 0, 0, 0);
}

int TestsModelLimbPalettePackets(int colored, int split, int limb,
                                 unsigned pulse_mask) {
  return ModelPolygonPackets(colored, split, limb, pulse_mask, 0, 0);
}

int TestsModelAppearancePackets(int colored, int split, unsigned state,
                                unsigned pulse_mask) {
  return ModelPolygonPackets(colored, split, 8, pulse_mask, state, 0);
}

int TestsModelGimmickOverridePackets(unsigned state, unsigned pulse_mask,
                                     int limb) {
  return ModelPolygonPackets(1, 0, limb, pulse_mask, state, 1);
}

int TestsModelWeaponOverridePackets(unsigned state, unsigned pulse_mask,
                                    int limb) {
  return ModelPolygonPackets(0, 0, limb, pulse_mask, state, 2);
}

int Tests_ModelProjectionGroups(void) {
  unsigned file[64] = {0};
  VS::Actors::ShpModel* model = reinterpret_cast<VS::Actors::ShpModel*>(file);
  Actors_ModelInstance instance = {0};
  MATRIX matrices[2] = {0};
  const VS::Actors::ModelRenderContext saved = VS::Actors::modelRenderContext;
  model->group_section = 96;
  model->vertex_section = 104;
  model->polygon_section = 128;
  uint16_t* groups =
      reinterpret_cast<uint16_t*>(reinterpret_cast<unsigned char*>(file) + 96);
  groups[0] = 0;
  groups[1] = 1;
  groups[2] = 1;
  groups[3] = 3;
  SVECTOR* vertices =
      reinterpret_cast<SVECTOR*>(reinterpret_cast<unsigned char*>(file) + 104);
  vertices[0].vx = -64;
  vertices[2].vx = 64;
  instance.boneCount = 2;
  instance.vertexGroupCount = 2;
  for (int bone = 0; bone < 2; ++bone) {
    for (int axis = 0; axis < 3; ++axis) matrices[bone].m[axis][axis] = 4096;
    matrices[bone].t[2] = 1024;
  }
  matrices[1].t[0] = 128;
  Core_Gte_Reset();
  Core_Gte_SetGeomOffset(160, 120);
  Core_Gte_SetGeomScreen(256);
  static Actors_ActorRecord actor;
  actor = {};
  actor.instance = instance;
  actor.instance.model = model;
  actor.modelDataSize = sizeof file;
  int passed =
      VS::Render::ProjectLiveActorModel(actor, matrices) &&
      VS::Actors::modelRenderContext.projected_vertex_count == 3 &&
      static_cast<int16_t>(
          VS::Actors::modelRenderContext.projected_vertices[0]) == 144 &&
      static_cast<int16_t>(
          VS::Actors::modelRenderContext.projected_vertices[1]) == 192 &&
      static_cast<int16_t>(
          VS::Actors::modelRenderContext.projected_vertices[2]) == 208 &&
      VS::Actors::modelRenderContext.vertex_depths[2] == 1024;
  groups[2] = 2;
  passed = passed && !VS::Render::ProjectLiveActorModel(actor, matrices) &&
           VS::Actors::modelRenderContext.projected_vertex_count == 0;
  groups[2] = 1;
  groups[3] = 769;
  passed = passed && !VS::Render::ProjectLiveActorModel(actor, matrices);
  VS::Actors::modelRenderContext = saved;
  Core_Gte_Reset();
  return passed;
}

int Tests_ModelWorkspaceIsolation(void) {
  unsigned char saved_scratch[1024];
  unsigned char sentinel[1024];
  VS::Render::ModelScratchpad* workspace = VS::Render::ModelWorkspace();
  const VS::Render::ModelScratchpad saved_workspace = *workspace;
  const MATRIX saved_basis = VS::Render::cameraBasisMatrix;
  const MATRIX saved_inverse = VS::Actors::boneMatrixScratch;
  Actors_ActorRecord actor = {0};
  memcpy(saved_scratch, VS::Core::ScratchBase(), sizeof saved_scratch);
  memset(sentinel, 0xA5, sizeof sentinel);
  memcpy(VS::Core::ScratchBase(), sentinel, sizeof sentinel);
  memset(workspace, 0, sizeof *workspace);
  actor.instance.scaleX = actor.instance.scaleY = actor.instance.scaleZ =
      actor.scale = 4096;
  actor.instance.position.vx = 100;
  actor.instance.position.vy = 200;
  actor.instance.position.vz = 300;
  actor.instance.facing = 1024;
  actor.blendPose.bone_keys[41].vx = 8;
  actor.blendPose.bone_keys[41].vy = -4;
  actor.blendPose.bone_keys[41].vz = 12;
  VS::Actors::PrepareLiveActorRootTransform(&actor);
  int passed = workspace->root_offset.vx == 106 &&
               workspace->root_offset.vy == 198 &&
               workspace->root_offset.vz == 296 &&
               memcmp(VS::Core::ScratchBase(), sentinel, sizeof sentinel) == 0;
  VS::Render::FrameScratch* frame =
      reinterpret_cast<VS::Render::FrameScratch*>(VS::Core::ScratchBase());
  memset(frame, 0, sizeof *frame);
  frame->camera.far_clip = 4096;
  MATRIX camera = {0};
  for (int axis = 0; axis < kSpatialDimensions; ++axis) {
    camera.m[axis][axis] = kFixedPointOne;
    camera.t[axis] = 1000 + axis;
  }
  VS::Render::cameraBasisMatrix = camera;
  VS::Render::PrepareLiveActorCameraMatrices(&camera);
  // The bone scratch keeps the source translation (44F14.c:880-883).
  passed = passed && VS::Actors::boneMatrixScratch.t[0] == camera.t[0] &&
           VS::Actors::boneMatrixScratch.t[1] == camera.t[1] &&
           VS::Actors::boneMatrixScratch.t[2] == camera.t[2];
  memcpy(VS::Core::ScratchBase(), saved_scratch, sizeof saved_scratch);
  *workspace = saved_workspace;
  VS::Render::cameraBasisMatrix = saved_basis;
  VS::Actors::boneMatrixScratch = saved_inverse;
  Core_Gte_Reset();
  return passed;
}

// _loadSeq, BATTLE/30DB0.c:1748-1779: adjacent four-byte offsets must not
// become one native pointer.
int Tests_SharedSequenceOffsets(void) {
  u_int source[128] = {2, 101, 102, 5, 6, 32, 144};
  u_char* payload = reinterpret_cast<u_char*>(source) + 7 * sizeof(u_int);
  VS::Actors::SeqAnimationBank* first =
      reinterpret_cast<VS::Actors::SeqAnimationBank*>((payload + 32 + 8));
  VS::Actors::SeqAnimationBank* second =
      reinterpret_cast<VS::Actors::SeqAnimationBank*>((payload + 144 + 8));
  Actors_ActorRecord actor = {0};
  VS::Content::LegacyActorAssetRequest request{};
  const auto saved_requests = VS::Content::legacyActorAssetRequests;
  Actors_ActorRecord* saved_actors[17];
  void* saved_data = VS::Actors::sharedSequenceData[0];
  int16_t saved_ids[16];
  int16_t saved_params[16];
  char* saved_entries[16];
  u_char saved_page = VS::Actors::sharedSequenceBankTexturePages[0];
  u_char saved_bank = VS::Actors::sharedSequenceBankIndex;
  char saved_resolved = VS::Actors::lastResolvedSequenceIndex;
  u_char saved_phase = VS::Content::legacySequenceParsePhase;
  u_char* saved_parse_data = VS::Content::legacySequenceParseData;
  u_short* header = nullptr;
  int passed;

  std::ranges::copy(Actors_actorRecords, saved_actors);
  memcpy(saved_ids, VS::Actors::sharedSequenceAnimationIds[0].data(),
         sizeof saved_ids);
  memcpy(saved_params, VS::Actors::sharedSequenceEntryParams[0].data(),
         sizeof saved_params);
  std::ranges::copy(VS::Actors::sharedSequenceAnimationData[0], saved_entries);
  std::ranges::fill(Actors_actorRecords, nullptr);
  Actors_actorRecords[0] = &actor;
  VS::Actors::sharedSequenceData[0] = nullptr;
  VS::Actors::sharedSequenceBankIndex = 0;
  VS::Content::legacySequenceParsePhase = 0;

  actor.modelId = 101;
  actor.instance.boneCount = 2;
  first->animation_map = second->animation_map = 64;
  // Each embedded file's size word bounds the bank the parse registers.
  const u_int first_bytes = 144 - 32 - 8;
  const u_int second_bytes =
      static_cast<u_int>(sizeof source - (7 * sizeof(u_int) + 144 + 8));
  memcpy(payload + 32 + 4, &first_bytes, sizeof first_bytes);
  memcpy(payload + 144 + 4, &second_bytes, sizeof second_bytes);
  (reinterpret_cast<u_char*>(first))[64] = 1;
  (reinterpret_cast<u_char*>(second))[64] = 0;
  request.asset_kind = 8;
  request.target_slot = 1;
  request.sub_kind = 1;
  request.data_address = reinterpret_cast<uintptr_t>(source);
  VS::Content::legacyActorAssetRequests[0] = request;

  // A finished parse returns 1 and shifts the queue.
  passed = VS::Content::UpdateLiveActorAssetQueue() == 1 &&
           VS::Actors::sharedSequenceAnimationData[0][0] ==
               reinterpret_cast<char*>(payload) + 32 &&
           VS::Actors::sharedSequenceAnimationData[0][1] ==
               reinterpret_cast<char*>(payload) + 144 &&
           VS::Actors::sharedSequenceAnimationIds[0][2] == -1 &&
           actor.sharedSequences[0] == first && actor.textureRowOffsets[0] == 5;
  if (passed) {
    passed = VS::Actors::FindLiveActorAnimationHeader(&actor, 220, &header) ==
                 reinterpret_cast<intptr_t>(first) &&
             header == first->headers + 9 &&
             VS::Actors::lastResolvedSequenceIndex == 1;
  }
  // Reparsing the bank rebinds every actor (func_8009CAEC).
  actor.modelId = 102;
  VS::Actors::sharedSequenceData[0] = nullptr;
  VS::Content::legacyActorAssetRequests[0] = request;
  passed = passed && VS::Content::UpdateLiveActorAssetQueue() == 1;
  passed = passed && actor.sharedSequences[0] == second &&
           actor.textureRowOffsets[0] == 6;
  if (passed) {
    passed = VS::Actors::FindLiveActorAnimationHeader(&actor, 220, &header) ==
                 reinterpret_cast<intptr_t>(second) &&
             header == second->headers &&
             VS::Actors::lastResolvedSequenceIndex == 0;
  }

  std::ranges::copy(saved_actors, Actors_actorRecords);
  memcpy(VS::Actors::sharedSequenceAnimationIds[0].data(), saved_ids,
         sizeof saved_ids);
  memcpy(VS::Actors::sharedSequenceEntryParams[0].data(), saved_params,
         sizeof saved_params);
  std::ranges::copy(saved_entries,
                    VS::Actors::sharedSequenceAnimationData[0].begin());
  VS::Actors::sharedSequenceData[0] = saved_data;
  VS::Actors::sharedSequenceBankTexturePages[0] = saved_page;
  VS::Actors::sharedSequenceBankIndex = saved_bank;
  VS::Actors::lastResolvedSequenceIndex = saved_resolved;
  VS::Content::legacySequenceParsePhase = saved_phase;
  VS::Content::legacySequenceParseData = saved_parse_data;
  VS::Content::legacyActorAssetRequests = saved_requests;
  VS::Actors::ForgetSequenceBanks(source, sizeof source);
  return passed;
}

int Tests_AdvanceSequenceWithoutScratch(void) {
  u_int file[64] = {0};
  VS::Actors::SeqAnimationBank* bank =
      reinterpret_cast<VS::Actors::SeqAnimationBank*>(file);
  Actors_ActorRecord actor = {0};
  unsigned char scratch[1024];
  const unsigned char tracks[] = {0xE0, 1, 2, 3, 0xE0, 1, 1, 1};
  char saved_resolved = VS::Actors::lastResolvedSequenceIndex;
  int passed;

  bank->animation_data = 64;
  bank->animation_map = 48;
  bank->headers[3] = 4;
  bank->headers[5] = 8;
  memcpy(reinterpret_cast<u_char*>(file) + 68, tracks, sizeof tracks);
  VS::Actors::RegisterSequenceBank(bank, sizeof file);
  actor.ownedSequence = bank;
  actor.instance.boneCount = 1;
  actor.framesPerStep = 1;
  actor.animationLength = 10;
  actor.pose.frame = 1;
  actor.pose.bone_keys[0].vx = 8191;
  memcpy(scratch, VS::Core::ScratchBase(), sizeof scratch);

  VS::Actors::AdvanceLiveSkeletalAnimation(actor, 0);
  VS::Actors::ForgetSequenceBanks(bank, sizeof file);
  passed = actor.pose.frame == 2 && actor.pose.bone_keys[41].vx == 1 &&
           actor.pose.bone_keys[41].vy == 2 &&
           actor.pose.bone_keys[41].vz == 3 &&
           actor.pose.bone_keys[0].vx == 0 && actor.pose.bone_keys[0].vy == 1 &&
           actor.pose.bone_keys[0].vz == 1 &&
           actor.pose.rotation_deltas[0].vx == 1 &&
           actor.pose.rotation_deltas[41].vy == 2 &&
           memcmp(scratch, VS::Core::ScratchBase(), sizeof scratch) == 0;
  memcpy(VS::Core::ScratchBase(), scratch, sizeof scratch);
  VS::Actors::lastResolvedSequenceIndex = saved_resolved;
  return passed;
}

int Tests_BlendSequencePose(int mode) {
  VS::Actors::AnimationPose dst = {0};
  VS::Actors::AnimationPose src = {0};
  unsigned char events[2] = {0};
  dst.bone_keys[0].vx = 8190;
  src.bone_keys[0].vx = 2;
  dst.bone_keys[41].vy = 10;
  src.bone_keys[41].vy = -3;
  dst.scale_keys[0].vx = 64;
  src.scale_keys[0].vx = 128;
  src.event_script = events;
  src.frame = 19;
  src.reached_end = 1;
  if (mode == 0) {
    VS::Actors::BlendLiveActorPose(dst, src, 1, 1, 4);
    return dst.bone_keys[0].vx == 8191 && dst.bone_keys[41].vy == 6 &&
           dst.scale_keys[0].vx == 80 && dst.event_script == nullptr;
  }
  if (mode == 1) {
    VS::Actors::BlendLiveActorPose(dst, src, 1, 2, 4);
    return dst.bone_keys[0].vx == 8192 && dst.bone_keys[41].vy == 3 &&
           dst.scale_keys[0].vx == 96 && dst.event_script == nullptr;
  }
  VS::Actors::BlendLiveActorPose(dst, src, 1, 4, 4);
  // BlendLiveActorPose copies a full-weight pose bytewise, padding included.
  // NOLINTNEXTLINE(bugprone-suspicious-memory-comparison)
  return memcmp(&dst, &src, sizeof src) == 0 && dst.event_script == events;
}
