#include <stdlib.h>
#include <string.h>

#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDrawing.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/MovementCommands.h"
#include "Actors/SequencePlayback.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Battle/ActionExecution.h"
#include "Battle/ActionFlow.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"
#include "Battle/TargetingDraw.h"
#include "CdDrive.h"
#include "Field/Camera.h"
#include "Field/RoomLoading.h"
#include "Field/Zone.h"
#include "Legacy/Psx.h"
#include "ModelLoading.h"
#include "Render/Camera.h"
#include "Render/Display.h"
#include "Render/Frame.h"
#include "Render/Model.h"
#include "Render/ModelDraw.h"
#include "Render/ModelGeometry.h"
#include "Render/ModelWorkspace.h"
#include "Render/Render.h"
#include "Render/Textures.h"

enum {
  kSpatialDimensions = 3,
  kFixedPointOne = 4096,
  kFacialImageHeaderWords = 2,
  kFacialImageWidth = 64,
  kFacialImageHeight = 2,
  kFacialUploadWidth = 4,
  kFacialFirstPixel = 0x100
};

int Tests_ActionCameraRecovery(void) {
  Battle_ActionState state = {0};
  Battle_ActionState* savedAction = Battle_actionState;
  const Field_SphericalCamera savedCamera = Field_sphericalCamera;
  const int savedProjection = Render_projectionDistance;
  const int savedEase = Battle_cameraEaseMode;
  const int savedDistance = Battle_settledCameraDistance;
  state.preferredCameraDistance = 2304;
  Battle_actionState = &state;
  Field_sphericalCamera.delta.mode = 1;
  Field_sphericalCamera.values.distance = 1536;
  Render_projectionDistance = 640;
  Battle_cameraEaseMode = 0;
  Battle_settledCameraDistance = 0;
  Battle_FinishActionSequence();
  const int passed = Render_projectionDistance == 576 &&
                     Field_sphericalCamera.values.distance == 1728;
  Battle_actionState = savedAction;
  Field_sphericalCamera = savedCamera;
  Render_projectionDistance = savedProjection;
  Battle_cameraEaseMode = savedEase;
  Battle_settledCameraDistance = savedDistance;
  Core_Gte_Reset();
  return passed;
}

int Tests_SparseIdlePoseReset(void) {
  Actors_ActorRecord* savedActor = Actors_actorRecords[4];
  Field_GimmickInstance* savedObject = Field_gimmickInstances[4];
  Field_GimmickInstance object = {0};
  Actors_ActorRecord actor = {0};
  Actors_actorRecords[4] = NULL;
  Field_gimmickInstances[4] = NULL;
  Actors_RestoreActorIdlePose(4);
  object.mode = 0xF9;
  Field_gimmickInstances[4] = &object;
  Actors_RestoreActorIdlePose(4);
  int passed = object.mode == 0;
  actor.animationId = 0x63;
  Actors_actorRecords[4] = &actor;
  Actors_RestoreActorIdlePose(4);
  passed = passed && actor.animationId == 0x63;
  actor.animationId = 0xC7;
  Actors_RestoreActorIdlePose(4);
  passed = passed && actor.animationId == 0xC7;
  Actors_actorRecords[4] = savedActor;
  Field_gimmickInstances[4] = savedObject;
  return passed;
}

int Tests_TargetLabelStride(void) {
  Battle_TargetSphereSetupView* savedSphere = Battle_targetSphere;
  Battle_TargetSphere sphere = {0};
  char text[] = "Target";
  for (unsigned index = 0; index < 24; ++index) {
    sphere.labels[index].state = 3;
    sphere.labels[index].screenX = 3000;
    sphere.labels[index].text = text;
    sphere.labels[index].worldPosition = (SVECTOR){10, 20, 30, 0};
  }
  Battle_targetSphere = (void*)&sphere;
  Battle_UpdateSphereTargetLabels();
  int passed = Battle_AreTargetLabelsSettled() == 1;
  for (unsigned index = 0; index < 24; ++index) {
    passed = passed && sphere.labels[index].screenX == 3032 &&
             sphere.labels[index].state == 0 &&
             sphere.labels[index].text == text &&
             sphere.labels[index].worldPosition.vy == 20;
  }
  Battle_targetSphere = savedSphere;
  return passed;
}

int Tests_TargetSphereWireframe(void) {
  Render_FrameScratch* frame = (Render_FrameScratch*)Core_ScratchBase();
  const Render_FrameScratch savedFrame = *frame;
  Battle_TargetSphereSetupView* savedSphere = Battle_targetSphere;
  Battle_TargetSphere sphere = {0};
  Battle_TargetVolume volume = {0};
  u_long ordering[0x822];
  u_long packets[12];
  memset(packets, 0xA5, sizeof packets);
  Core_Gte_Reset();
  Core_Gte_SetGeomOffset(160, 120);
  Core_Gte_SetGeomScreen(256);
  *frame = (Render_FrameScratch){0};
  for (unsigned axis = 0; axis < kSpatialDimensions; ++axis) {
    frame->viewMatrix.m[axis][axis] = kFixedPointOne;
  }
  frame->viewMatrix.t[2] = 512;
  ClearOTagR(ordering, 0x822);
  frame->orderingTable = ordering;
  frame->primitiveCursor = packets + 1;
  Battle_targetSphere = (void*)&sphere;
  sphere.depth = 128;
  sphere.vertexCount = 2;
  sphere.wireVertices[1].xyz[0] = 4096;
  sphere.wireVertices[1].opaque6 = 1;
  volume.kind = 1;
  volume.scale.extents[0] = volume.scale.extents[1] = volume.scale.extents[2] =
      4;
  Battle_DrawSphereVolumeWireframe(&volume, 0x010401);
  int passed = frame->primitiveCursor == packets + 6 &&
               (ordering[128] & 0xFFFFFF) == LegacyGpu_Handle(packets + 1) &&
               (packets[1] >> 24) == 4 && packets[3] == 0x42208020u &&
               packets[5] == 0x007800A0u && packets[0] == 0xA5A5A5A5u &&
               packets[6] == 0xA5A5A5A5u;
  sphere.wireVertices[1].opaque6 = 2;
  Battle_DrawSphereVolumeWireframe(&volume, 0x010401);
  passed = passed && frame->primitiveCursor == packets + 11 &&
           packets[8] == 0x40208020u && packets[11] == 0xA5A5A5A5u;
  sphere.vertexCount = 0;
  Battle_DrawSphereVolumeWireframe(&volume, 0x010401);
  passed = passed && frame->primitiveCursor == packets + 11;
  Battle_targetSphere = savedSphere;
  *frame = savedFrame;
  Core_Gte_Reset();
  return passed;
}

int Tests_DirectMovementCommand(void) {
  Actors_ActorRecord actor;
  memset(&actor, 0xA5, sizeof actor);
  Actors_ActorRecord* saved = Actors_actorRecords[3];
  Actors_actorRecords[3] = &actor;
  Actors_SetDirectMovement(3, 0xC00, 1536);
  int passed =
      actor.turnState.mode == 0 && actor.turnState.reserved1 == 0 &&
      actor.turnState.reserved2 == 0 && actor.turnState.reserved3 == 0 &&
      actor.turnState.direction == 0xC00 && actor.turnState.facing == 0xC00 &&
      actor.turnState.distance == 1536 &&
      (unsigned)actor.turnState.reservedC == 0xA5A5A5A5u;
  Actors_SetDirectMovement(3, 0, 0);
  passed = passed && actor.turnState.mode == 0 &&
           actor.turnState.distance == 0 && actor.turnState.direction == 0 &&
           actor.turnState.facing == 0;
  Actors_QueueNavigationTileTarget(3, 0x300, 1024, 0x800);
  passed = passed && actor.turnState.mode == 1 &&
           actor.turnState.direction == 0x300 &&
           actor.turnState.facing == 0x800 &&
           actor.turnState.distance == 1024 &&
           (unsigned)actor.turnState.reservedC == 0xA5A5A5A5u;
  Actors_actorRecords[3] = saved;
  return passed;
}

int Tests_GimmickRoomScale(void) {
  enum { kGimmickCameraMatrix = 4 };
  const Render_ModelScratchpad savedWorkspace = *Render_ModelWorkspace();
  const Actors_SkeletonPose savedPose = Actors_skeletonPose;
  const MATRIX savedRoot = Actors_skeletonRootMatrix;
  const VECTOR savedScale = Actors_roomActorScale;
  Field_GimmickInstance object = {0};
  Actors_ShpModel model = {0};
  MATRIX camera = {0};
  for (unsigned axis = 0; axis < kSpatialDimensions; ++axis) {
    camera.m[axis][axis] = kFixedPointOne;
    object.matrices[kGimmickCameraMatrix].m[axis][axis] = kFixedPointOne;
  }
  object.model = &model;
  object.scaleX = object.scaleY = object.scaleZ = 64;
  Render_ModelWorkspace()->rootOffset = (SVECTOR){10, 20, 30, 0};
  Actors_skeletonPose.scale = (VECTOR){0};
  Actors_roomActorScale = (VECTOR){2048, 4096, 4096, 0};
  Actors_DrawGimmickModel(&object, &camera);
  const int passed =
      object.matrices[0].m[0][0] == 2048 &&
      object.matrices[0].m[1][1] == 4096 &&
      object.matrices[0].m[2][2] == 4096 && object.matrices[0].t[0] == 10 &&
      object.matrices[0].t[1] == 36 && object.matrices[0].t[2] == 30;
  *Render_ModelWorkspace() = savedWorkspace;
  Actors_skeletonPose = savedPose;
  Actors_skeletonRootMatrix = savedRoot;
  Actors_roomActorScale = savedScale;
  Core_Gte_Reset();
  return passed;
}

static int FacialTexturePixelsMatch(RECT* destination, const uint16_t* image,
                                    unsigned source_column) {
  uint16_t pixels[kFacialUploadWidth * kFacialImageHeight];
  StoreImage(destination, (u_long*)pixels);
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
        (uint16_t)(kFacialFirstPixel + index);
  }
  RECT source = {640, 256, 64, 2}, destination = {264, 84, 4, 2};
  uint16_t saved_source[kFacialImageWidth * kFacialImageHeight];
  uint16_t saved_destination[kFacialUploadWidth * kFacialImageHeight];
  StoreImage(&source, (u_long*)saved_source);
  StoreImage(&destination, (u_long*)saved_destination);
  const Render_FrameScratch savedFrame =
      *(Render_FrameScratch*)Core_ScratchBase();
  void* savedOrdering = Render_legacyCurrentOrderingTable;
  const unsigned char savedPhase = Content_legacyEtmLoadPhase;
  unsigned short* savedImage = Content_legacyEtmFileBuffer;
  Content_DiscQueueSlot* savedRequest = Content_legacyEtmDiscRequest;
  const unsigned char savedPage = Actors_sharedSequenceBankTexturePages[0];
  const unsigned char savedLegacyPage = Actors_sequenceTexturePagesOneBased[1];
  const unsigned char savedReservation =
      Render_legacyTexturePageReservations[26];
  const unsigned char savedBank = Actors_sharedSequenceBankIndex;
  const unsigned char savedRow = Actors_textureSlotRowOffsets[0];
  Actors_ActorRecord* savedActors[17];
  memcpy(savedActors, Actors_actorRecords, sizeof savedActors);
  unsigned char savedOrigins[4];
  memcpy(savedOrigins, Actors_textureSlotOrigins, sizeof savedOrigins);
  const Actors_TextureAnimationKey savedKey =
      Actors_secondarySlotSequences[1][0];
  Content_DiscQueueSlot request = {0};
  request.state = Content_DiscRequestState_Loaded;
  Content_ActorAssetRequest upload = {0};
  upload.targetSlot = 1;
  upload.textureSlot = 26;
  Content_legacyEtmLoadPhase = 1;
  Content_legacyEtmFileBuffer = image;
  Content_legacyEtmDiscRequest = &request;
  Actors_sharedSequenceBankTexturePages[0] = 0;
  Actors_sequenceTexturePagesOneBased[1] = 0;
  Render_legacyTexturePageReservations[26] = 0;
  int passed = Content_UpdateSequenceEffectTextureLoad(&upload) == -1 &&
               Actors_sharedSequenceBankTexturePages[0] == 26;
  Actors_ActorRecord actor = {0};
  actor.textureHeaderBit = 1;
  actor.textureOverlays[6] = (Actors_TextureOverlayRect){16, 20, 8, 2};
  actor.textureAnimationFrames[1] = (Actors_TextureAnimationFrame){7, 8, 0, 2};
  actor.textureAnimationSlots[1].timeline = 1;
  actor.textureAnimationSlots[1].mode = 1;
  Actors_secondarySlotSequences[1][0] = (Actors_TextureAnimationKey){2, 5};
  Actors_sharedSequenceBankIndex = 0;
  const short origin[2] = {4, 0};
  memcpy(Actors_textureSlotOrigins, origin, sizeof origin);
  Actors_textureSlotRowOffsets[0] = 1;
  u_long ordering[0x822];
  DR_MOVE packet = {0};
  ClearOTagR(ordering, 0x822);
  Render_legacyCurrentOrderingTable = ordering;
  ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor = &packet;
  Actors_UpdateActorTextureAnimations(&actor);
  passed =
      passed && ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor ==
                    &packet + 1;
  LegacyGpu_WalkList(&ordering[0x821]);
  passed = FacialTexturePixelsMatch(&destination, image, kFirstFrameColumn) &&
           passed;
  memset(Actors_actorRecords, 0, sizeof savedActors);
  Actors_actorRecords[1] = &actor;
  actor.textureRowOffsets[1] = 0xFF;
  actor.momentumDistance = 0x12FF;
  passed = passed && actor.textureRowOffsets[2] == 0xFF &&
           offsetof(Actors_ActorRecord, momentumDistance) ==
               offsetof(Actors_ActorRecord, textureRowOffsets) + 2;
  for (unsigned selected = 2; selected <= 3; ++selected) {
    Actors_sharedSequenceBankIndex = 0;
    ClearOTagR(ordering, 0x822);
    ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor = &packet;
    const int switched = Actors_SelectSharedSequenceBank(selected);
    passed = passed && switched == 0 &&
             Actors_sharedSequenceBankIndex == selected - 1 &&
             ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor ==
                 &packet + 1;
    LegacyGpu_WalkList(&ordering[0x821]);
    passed =
        FacialTexturePixelsMatch(&destination, image, kSharedFrameColumn) &&
        passed;
  }
  Actors_sharedSequenceBankIndex = 0;
  ClearOTagR(ordering, 0x822);
  ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor = &packet;
  const int released = Content_ReleaseSequenceEffectTexture(1);
  passed = passed && released == 0 &&
           Actors_sharedSequenceBankTexturePages[0] == 0 &&
           Render_legacyTexturePageReservations[26] == 0xFD &&
           ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor ==
               &packet + 1;
  LegacyGpu_WalkList(&ordering[0x821]);
  passed = FacialTexturePixelsMatch(&destination, image, kSharedFrameColumn) &&
           passed;
  passed = passed && Content_ReleaseSequenceEffectTexture(0) == -1 &&
           Content_ReleaseSequenceEffectTexture(4) == -1;
  LoadImage(&source, (u_long*)saved_source);
  LoadImage(&destination, (u_long*)saved_destination);
  *(Render_FrameScratch*)Core_ScratchBase() = savedFrame;
  Render_legacyCurrentOrderingTable = savedOrdering;
  Content_legacyEtmLoadPhase = savedPhase;
  Content_legacyEtmFileBuffer = savedImage;
  Content_legacyEtmDiscRequest = savedRequest;
  Actors_sharedSequenceBankTexturePages[0] = savedPage;
  Actors_sequenceTexturePagesOneBased[1] = savedLegacyPage;
  Render_legacyTexturePageReservations[26] = savedReservation;
  Actors_sharedSequenceBankIndex = savedBank;
  Actors_textureSlotRowOffsets[0] = savedRow;
  memcpy(Actors_actorRecords, savedActors, sizeof savedActors);
  memcpy(Actors_textureSlotOrigins, savedOrigins, sizeof savedOrigins);
  Actors_secondarySlotSequences[1][0] = savedKey;
  return passed;
}

int Tests_ActorTextureFrameRectangles(int wide, int fallback,
                                      unsigned* command) {
  Actors_ActorRecord actor = {0};
  const Render_FrameScratch savedFrame =
      *(Render_FrameScratch*)Core_ScratchBase();
  void* savedOrdering = Render_legacyCurrentOrderingTable;
  unsigned char savedOrigins[4];
  memcpy(savedOrigins, Actors_textureSlotOrigins, sizeof savedOrigins);
  const unsigned char savedRow = Actors_textureSlotRowOffsets[0];
  const unsigned char savedBank = Actors_sharedSequenceBankIndex;
  const unsigned char savedPage = Actors_sharedSequenceBankTexturePages[0];
  struct {
    unsigned before;
    DR_MOVE packet;
    unsigned after;
  } guarded;
  memset(&guarded, 0xA5, sizeof guarded);
  u_long ordering[0x822];
  ClearOTagR(ordering, 0x822);
  Render_legacyCurrentOrderingTable = ordering;
  ((Render_FrameScratch*)Core_ScratchBase())->primitiveCursor = &guarded.packet;
  Actors_sharedSequenceBankIndex = 0;
  Actors_sharedSequenceBankTexturePages[0] = 0x1A;
  const short origin[2] = {4, 0};
  memcpy(Actors_textureSlotOrigins, origin, sizeof origin);
  Actors_textureSlotRowOffsets[0] = 1;
  actor.textureHeaderBit = wide;
  actor.textureSlot = 0;
  actor.textureRowOffsets[0] = 7;
  const Actors_TextureOverlayRect rectangle = {16, 20, 32, 12};
  actor.textureOverlays[0] = rectangle;
  actor.textureOverlays[6] = rectangle;
  actor.textureAnimationFrames[0] = (Actors_TextureAnimationFrame){1, 32, 5, 2};
  actor.textureCellOrigins[1] = (Actors_TextureCellOrigin){32, 5, 2, 0};
  Actors_UploadActorTextureFrame(&actor, fallback ? 1 : 0, 3);
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
  if (command != NULL) {
    for (size_t index = 0;
         index < sizeof guarded.packet.code / sizeof guarded.packet.code[0];
         ++index) {
      command[index] = guarded.packet.code[index];
    }
  }
  *(Render_FrameScratch*)Core_ScratchBase() = savedFrame;
  Render_legacyCurrentOrderingTable = savedOrdering;
  memcpy(Actors_textureSlotOrigins, savedOrigins, sizeof savedOrigins);
  Actors_textureSlotRowOffsets[0] = savedRow;
  Actors_sharedSequenceBankIndex = savedBank;
  Actors_sharedSequenceBankTexturePages[0] = savedPage;
  return passed;
}

int Tests_RoomSpawnCountGuard(void) {
  const Field_ZoneContext saved = Field_zoneContext;
  const int savedAlias = Field_isRoomActorSpawnEnabled;
  int sentinel = 0;
  memset(&Field_zoneContext, 0, sizeof Field_zoneContext);
  Field_isRoomActorSpawnEnabled = 1;
  Field_zoneContext.zudData = &sentinel;
  Field_SpawnRoomActors();
  const int passed = Field_zoneContext.zudData == &sentinel;
  Field_zoneContext = saved;
  Field_isRoomActorSpawnEnabled = savedAlias;
  return passed;
}

int Tests_ProjectedBoneEndpoints(void) {
  Actors_ActorRecord actor = {0};
  unsigned modelBytes[24] = {0};
  Actors_ShpModel* model = (void*)modelBytes;
  Actors_ActorRecord* saved = Actors_actorRecords[0];
  Actors_actorRecords[0] = &actor;
  actor.instance.boneCount = 1;
  actor.instance.model = model;
  model->bones[0].length = 128;
  for (int axis = 0; axis < kSpatialDimensions; ++axis) {
    actor.boneMatrices[0].m[axis][axis] = kFixedPointOne;
  }
  actor.boneMatrices[0].t[2] = 1024;
  Core_Gte_Reset();
  Core_Gte_SetGeomOffset(160, 120);
  Core_Gte_SetGeomScreen(256);
  int output[2] = {0};
  int passed = Actors_ProjectActorBoneEndpoint(0, 0, output, 0) == 0 &&
               output[0] == ((120 << 16) | 160) && output[1] == 1024;
  passed = passed && Actors_ProjectActorBoneEndpoint(0, 0, output, 1) == 0 &&
           (short)output[0] == 128 && output[1] == 1024;
  passed = passed && Actors_ProjectActorBoneEndpoint(0, 0, output, 2) == 0 &&
           (short)output[0] == 144;
  passed = passed && Actors_ProjectActorBoneEndpoint(0, 1, output, 0) == -4 &&
           Actors_ProjectActorBoneEndpoint(-1, 0, output, 0) == -1;
  Actors_actorRecords[0] = saved;
  Core_Gte_Reset();
  return passed;
}

int Tests_ActorPoseHierarchy(void) {
  unsigned file[32] = {0};
  Actors_ActorRecord actor = {0};
  Actors_ShpModel* model = (void*)file;
  const Actors_SkeletonPose savedPose = Actors_skeletonPose;
  const Actors_ModelRenderContext savedContext = Actors_modelRenderContext;
  const Render_ModelScratchpad savedWorkspace = *Render_ModelWorkspace();
  actor.instance.model = model;
  actor.modelDataSize = sizeof file;
  actor.instance.boneCount = 2;
  actor.instance.scaleX = actor.instance.scaleY = actor.instance.scaleZ =
      actor.scale = 4096;
  model->bones[0].length = 128;
  model->bones[1].parentIndex = 0;
  for (int bone = 0; bone < 2; ++bone) {
    actor.blendPose.scaleKeys[bone].vx = actor.blendPose.scaleKeys[bone].vy =
        actor.blendPose.scaleKeys[bone].vz = 64;
  }
  actor.blendPose.scaleKeys[1].vx = 32;
  MATRIX camera = {0};
  for (int axis = 0; axis < 3; ++axis) camera.m[axis][axis] = 4096;
  setVector(&Render_ModelWorkspace()->rootOffset, 100, 200, 300);
  int passed = Actors_BuildActorModelPose(&actor, &camera) == 1 &&
               actor.boneMatrices[0].t[0] == 100 &&
               actor.boneMatrices[1].t[0] == -28 &&
               actor.boneMatrices[1].t[1] == 200 &&
               actor.boneMatrices[1].m[0][0] == 2048 &&
               Actors_skeletonPose.boneMatrices[1].m[0][0] == 4096;
  actor.instance.isVisible = 1;
  actor.instance.wasDrawn = 1;
  actor.customTint = 1;
  actor.instance.tint = (Render_ColorCode){100, 70, 20, 0};
  actor.fadeRed = 255;
  actor.fadeGreen = 50;
  actor.fadeBlue = 246;
  actor.instance.vertexGroupCount = 1;
  model->groupSection = 96;
  model->vertexSection = 100;
  model->polygonSection = 108;
  unsigned short* group = (void*)((unsigned char*)file + 96);
  group[0] = 0;
  group[1] = 1;
  setVector(&Render_ModelWorkspace()->rootOffset, 0, 0, 1024);
  Core_Gte_Reset();
  Core_Gte_SetGeomScreen(256);
  Actors_DrawActorModel(&actor, &camera, 0);
  passed = passed && actor.instance.wasDrawn &&
           Actors_modelRenderContext.projectedVertexCount == 1;
  passed = passed && actor.instance.baseColor.r == 96 &&
           actor.instance.baseColor.g == 255 &&
           actor.instance.baseColor.b == 0 &&
           (actor.shadowBlend.reserved0 & 0xFFFFFF) == 0x144664;
  actor.customTint = 0;
  Actors_DrawActorModel(&actor, &camera, 0);
  passed = passed && actor.instance.baseColor.r == 124 &&
           actor.instance.baseColor.g == 255 &&
           actor.instance.baseColor.b == 88;
  model->bones[1].parentIndex = 1;
  passed = passed && Actors_BuildActorModelPose(&actor, &camera) == 0;
  model->bones[1].parentIndex = 0;
  actor.modelDataSize = sizeof(Actors_ShpModel);
  passed = passed && Actors_BuildActorModelPose(&actor, &camera) == 0;
  Actors_skeletonPose = savedPose;
  Actors_modelRenderContext = savedContext;
  *Render_ModelWorkspace() = savedWorkspace;
  Core_Gte_Reset();
  return passed;
}

int Tests_ModelPolygonPackets(int colored, int split) {
  enum {
    kTriangleCorners = 3,
    kVertexPacketWords = 3,
    kVertexDepth = 1024,
    kFirstRed = 32,
    kSecondRed = 64
  };
  unsigned modelWords[32] = {0};
  Actors_ShpModel* model = (void*)modelWords;
  Actors_ModelInstance instance = {0};
  const Actors_ModelRenderContext savedContext = Actors_modelRenderContext;
  Render_FrameScratch* frame = Core_ScratchBase();
  const Render_FrameScratch savedFrame = *frame;
  const int savedFrameIndex = Render_frameBuf;
  void* savedBuffer = Render_legacyPrimitiveBuffers[0];
  const Render_ModelScratchpad savedWorkspace = *Render_ModelWorkspace();
  unsigned char* memory = malloc(0x20010);
  if (memory == NULL) return 0;
  memset(memory, 0xA5, 0x20010);
  u_long ordering[2048];
  ClearOTagR(ordering, 2048);
  memset(frame, 0, sizeof *frame);
  frame->primitiveCursor = memory + 8;
  frame->orderingTable = ordering;
  Render_frameBuf = 0;
  Render_legacyPrimitiveBuffers[0] = memory + 8;
  Render_ModelWorkspace()->palette = 0x1234;
  Render_ModelWorkspace()->texturePage = 0x15;
  memset(&Actors_modelRenderContext, 0, sizeof Actors_modelRenderContext);
  Actors_modelRenderContext.projectedVertexCount = 3;
  Actors_modelRenderContext.projectedVertices[0] = (80 << 16) | 80;
  Actors_modelRenderContext.projectedVertices[1] = (80 << 16) | 160;
  Actors_modelRenderContext.projectedVertices[2] = (160 << 16) | 80;
  for (int index = 0; index < kTriangleCorners; ++index) {
    Actors_modelRenderContext.vertexDepths[index] = kVertexDepth;
  }
  instance.triangleCount = 1;
  model->polygonSection = 64;
  unsigned char* polygon = (void*)(modelWords + 16);
  const unsigned short indices[3] = {0, 4, 8};
  memcpy(polygon + (colored ? 0 : 4), indices, sizeof indices);
  if (colored) {
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
  if (split != 0) {
    Actors_modelRenderContext.projectedVertexCount = 4;
    Actors_modelRenderContext.projectedVertices[3] = (160 << 16) | 160;
    Actors_modelRenderContext.vertexDepths[3] = 1024 + split * 128;
    instance.triangleCount = 0;
    instance.otherPolygonCount = 1;
    polygon[0] = 0x2C;
    polygon[1] = 20;
    polygon[2] = 3;
    const unsigned short quadIndices[4] = {0, 4, 8, 12};
    memcpy(polygon + 4, quadIndices, sizeof quadIndices);
    const unsigned char coordinates[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    memcpy(polygon + 12, coordinates, sizeof coordinates);
  }
  CVECTOR tint = {128, 96, 64, 0};
  if (colored) {
    Render_DrawModelGouraudPolygons(&Actors_modelRenderContext, &instance,
                                    &tint, model);
  } else {
    Render_DrawModelFlatPolygons(&Actors_modelRenderContext, &instance, &tint,
                                 model);
  }
  const u_long* packet = (void*)(memory + 8);
  int passed =
      frame->primitiveCursor == memory + 8 + (colored ? 40 : 32) &&
      (packet[1] >> 24) == (colored ? 0x34u : 0x24u) &&
      packet[3] == 0x1234140Au && packet[colored ? 6 : 5] == 0x0015281Eu &&
      packet[colored ? 9 : 7] == 0x3C32u && nextPrim(&ordering[254]) == packet;
  if (colored) {
    passed = passed && (packet[1] & UINT8_MAX) == kFirstRed &&
             (packet[1 + kVertexPacketWords] & UINT8_MAX) == kSecondRed;
  }
  if (split != 0) {
    const u_long* second = packet + 8;
    passed =
        frame->primitiveCursor == memory + 72 && (packet[1] >> 24) == 0x24 &&
        (second[1] >> 24) == 0x24 && packet[3] == 0x1234140Au &&
        packet[5] == 0x0015281Eu &&
        packet[7] == (split > 0 ? 0x3C32u : 0x5046u) &&
        packet[6] == (u_long)Actors_modelRenderContext
                         .projectedVertices[split > 0 ? 2 : 3] &&
        second[2] == (u_long)Actors_modelRenderContext
                         .projectedVertices[split > 0 ? 1 : 0] &&
        second[4] == (u_long)Actors_modelRenderContext.projectedVertices[3] &&
        second[6] == (u_long)Actors_modelRenderContext.projectedVertices[2];
  }
  passed = passed && memory[0] == 0xA5 && memory[0x2000F] == 0xA5;
  *frame = savedFrame;
  Actors_modelRenderContext = savedContext;
  *Render_ModelWorkspace() = savedWorkspace;
  Render_frameBuf = savedFrameIndex;
  Render_legacyPrimitiveBuffers[0] = savedBuffer;
  free(memory);
  return passed;
}

int Tests_ModelProjectionGroups(void) {
  unsigned file[64] = {0};
  Actors_ShpModel* model = (void*)file;
  Actors_ModelInstance instance = {0};
  MATRIX matrices[2] = {0};
  const Actors_ModelRenderContext saved = Actors_modelRenderContext;
  model->groupSection = 96;
  model->vertexSection = 104;
  model->polygonSection = 128;
  unsigned short* groups = (void*)((unsigned char*)file + 96);
  groups[0] = 0;
  groups[1] = 1;
  groups[2] = 1;
  groups[3] = 3;
  SVECTOR* vertices = (void*)((unsigned char*)file + 104);
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
  int passed =
      Render_ProjectModelVertices(&instance, matrices, -32, model) == 1024 &&
      Actors_modelRenderContext.projectedVertexCount == 3 &&
      (short)Actors_modelRenderContext.projectedVertices[0] == 144 &&
      (short)Actors_modelRenderContext.projectedVertices[1] == 192 &&
      (short)Actors_modelRenderContext.projectedVertices[2] == 208 &&
      Actors_modelRenderContext.vertexDepths[2] == 1024;
  groups[2] = 2;
  passed = passed &&
           Render_ProjectModelVertices(&instance, matrices, -32, model) == 0 &&
           Actors_modelRenderContext.projectedVertexCount == 0;
  groups[2] = 1;
  groups[3] = 769;
  passed = passed &&
           Render_ProjectModelVertices(&instance, matrices, -32, model) == 0;
  Actors_modelRenderContext = saved;
  Core_Gte_Reset();
  return passed;
}

int Tests_ModelWorkspaceIsolation(void) {
  unsigned char savedScratch[1024];
  unsigned char sentinel[1024];
  Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  const Render_ModelScratchpad savedWorkspace = *workspace;
  const MATRIX savedCamera = Render_actorCameraMatrix;
  const MATRIX savedBasis = Render_cameraBasisMatrix;
  const MATRIX savedInverse = Actors_boneMatrixScratch;
  Actors_ActorRecord actor = {0};
  memcpy(savedScratch, Core_ScratchBase(), sizeof savedScratch);
  memset(sentinel, 0xA5, sizeof sentinel);
  memcpy(Core_ScratchBase(), sentinel, sizeof sentinel);
  memset(workspace, 0, sizeof *workspace);
  actor.instance.scaleX = actor.instance.scaleY = actor.instance.scaleZ =
      actor.scale = 4096;
  actor.instance.position.vx = 100;
  actor.instance.position.vy = 200;
  actor.instance.position.vz = 300;
  actor.instance.facing = 1024;
  actor.blendPose.boneKeys[41].vx = 8;
  actor.blendPose.boneKeys[41].vy = -4;
  actor.blendPose.boneKeys[41].vz = 12;
  Actors_PrepareActorRootTransform(&actor);
  int passed = workspace->rootOffset.vx == 106 &&
               workspace->rootOffset.vy == 198 &&
               workspace->rootOffset.vz == 296 &&
               memcmp(Core_ScratchBase(), sentinel, sizeof sentinel) == 0;
  Render_FrameScratch* frame = Core_ScratchBase();
  memset(frame, 0, sizeof *frame);
  frame->camera.farClip = 4096;
  MATRIX camera = {0};
  for (int axis = 0; axis < kSpatialDimensions; ++axis) {
    camera.m[axis][axis] = kFixedPointOne;
    camera.t[axis] = 1000 + axis;
  }
  Render_cameraBasisMatrix = camera;
  Render_PrepareActorCameraMatrices(&camera);
  passed =
      passed && memcmp(&Render_actorCameraMatrix, &camera, sizeof camera) == 0;
  memcpy(Core_ScratchBase(), savedScratch, sizeof savedScratch);
  *workspace = savedWorkspace;
  Render_actorCameraMatrix = savedCamera;
  Render_cameraBasisMatrix = savedBasis;
  Actors_boneMatrixScratch = savedInverse;
  Core_Gte_Reset();
  return passed;
}

// _loadSeq, BATTLE/30DB0.c:1748-1779: adjacent four-byte offsets must not
// become one native pointer.
int Tests_SharedSequenceOffsets(void) {
  u_int source[128] = {2, 101, 102, 5, 6, 32, 144};
  u_char* payload = (u_char*)source + 7 * sizeof(u_int);
  Actors_SeqAnimationBank* first = (void*)(payload + 32 + 8);
  Actors_SeqAnimationBank* second = (void*)(payload + 144 + 8);
  Actors_ActorRecord actor = {0};
  Content_ActorAssetRequest request = {0};
  Actors_ActorRecord* savedActors[17];
  void* savedData = Actors_sharedSequenceData[0];
  short savedIds[16];
  short savedParams[16];
  char* savedEntries[16];
  u_char savedPage = Actors_sharedSequenceBankTexturePages[0];
  u_char savedBank = Actors_sharedSequenceBankIndex;
  char savedResolved = Actors_lastResolvedSequenceIndex;
  u_char savedPhase = Content_legacySequenceParsePhase;
  u_char* savedParseData = Content_legacySequenceParseData;
  u_short* header = NULL;
  int passed;

  memcpy(savedActors, Actors_actorRecords, sizeof savedActors);
  memcpy(savedIds, Actors_sharedSequenceAnimationIds[0], sizeof savedIds);
  memcpy(savedParams, Actors_sharedSequenceEntryParams[0], sizeof savedParams);
  memcpy(savedEntries, Actors_sharedSequenceAnimationData[0],
         sizeof savedEntries);
  memset(Actors_actorRecords, 0, sizeof savedActors);
  Actors_actorRecords[0] = &actor;
  Actors_sharedSequenceData[0] = NULL;
  Actors_sharedSequenceBankIndex = 0;
  Content_legacySequenceParsePhase = 0;

  actor.modelId = 101;
  actor.instance.boneCount = 2;
  first->animationMap = second->animationMap = 64;
  ((u_char*)first)[64] = 1;
  ((u_char*)second)[64] = 0;
  request.targetSlot = 1;
  request.subKind = 1;
  request.dataAddress = (uintptr_t)source;

  passed = Content_ParseActorSequence(&request) == 0 &&
           Actors_sharedSequenceAnimationData[0][0] == (char*)payload + 32 &&
           Actors_sharedSequenceAnimationData[0][1] == (char*)payload + 144 &&
           Actors_sharedSequenceAnimationIds[0][2] == -1 &&
           actor.sharedSequences[0] == first && actor.textureRowOffsets[0] == 5;
  if (passed) {
    passed = Actors_FindActorAnimationHeader(&actor, 220, &header) ==
                 (intptr_t)first &&
             header == first->headers + 9 &&
             Actors_lastResolvedSequenceIndex == 1;
  }
  actor.modelId = 102;
  Content_BindSharedActorSequence(&actor, 0);
  passed = passed && actor.sharedSequences[0] == second &&
           actor.textureRowOffsets[0] == 6;
  if (passed) {
    passed = Actors_FindActorAnimationHeader(&actor, 220, &header) ==
                 (intptr_t)second &&
             header == second->headers && Actors_lastResolvedSequenceIndex == 0;
  }

  memcpy(Actors_actorRecords, savedActors, sizeof savedActors);
  memcpy(Actors_sharedSequenceAnimationIds[0], savedIds, sizeof savedIds);
  memcpy(Actors_sharedSequenceEntryParams[0], savedParams, sizeof savedParams);
  memcpy(Actors_sharedSequenceAnimationData[0], savedEntries,
         sizeof savedEntries);
  Actors_sharedSequenceData[0] = savedData;
  Actors_sharedSequenceBankTexturePages[0] = savedPage;
  Actors_sharedSequenceBankIndex = savedBank;
  Actors_lastResolvedSequenceIndex = savedResolved;
  Content_legacySequenceParsePhase = savedPhase;
  Content_legacySequenceParseData = savedParseData;
  return passed;
}

int Tests_AdvanceSequenceWithoutScratch(void) {
  u_int file[64] = {0};
  Actors_SeqAnimationBank* bank = (void*)file;
  Actors_ActorRecord actor = {0};
  unsigned char scratch[1024];
  const unsigned char tracks[] = {0xE0, 1, 2, 3, 0xE0, 1, 1, 1};
  char savedResolved = Actors_lastResolvedSequenceIndex;
  int passed;

  bank->animationData = 64;
  bank->animationMap = 48;
  bank->headers[3] = 4;
  bank->headers[5] = 8;
  memcpy((u_char*)file + 68, tracks, sizeof tracks);
  actor.ownedSequence = bank;
  actor.instance.boneCount = 1;
  actor.framesPerStep = 1;
  actor.animationLength = 10;
  actor.pose.frame = 1;
  actor.pose.boneKeys[0].vx = 8191;
  memcpy(scratch, Core_ScratchBase(), sizeof scratch);

  Actors_AdvanceSkeletalAnimation(&actor, &actor.pose, 0);
  passed = actor.pose.frame == 2 && actor.pose.boneKeys[41].vx == 1 &&
           actor.pose.boneKeys[41].vy == 2 && actor.pose.boneKeys[41].vz == 3 &&
           actor.pose.boneKeys[0].vx == 0 && actor.pose.boneKeys[0].vy == 1 &&
           actor.pose.boneKeys[0].vz == 1 &&
           actor.pose.reserved2A0[0].vx == 1 &&
           actor.pose.reserved2A0[41].vy == 2 &&
           memcmp(scratch, Core_ScratchBase(), sizeof scratch) == 0;
  memcpy(Core_ScratchBase(), scratch, sizeof scratch);
  Actors_lastResolvedSequenceIndex = savedResolved;
  return passed;
}

int Tests_BlendSequencePose(int mode) {
  Actors_AnimationPose dst = {0};
  Actors_AnimationPose src = {0};
  unsigned char events[2] = {0};
  dst.boneKeys[0].vx = 8190;
  src.boneKeys[0].vx = 2;
  dst.boneKeys[41].vy = 10;
  src.boneKeys[41].vy = -3;
  dst.scaleKeys[0].vx = 64;
  src.scaleKeys[0].vx = 128;
  src.eventScript = events;
  src.frame = 19;
  src.reachedEnd = 1;
  if (mode == 0) {
    Actors_BlendActorPose(&dst, &src, 1, 1, 4);
    return dst.boneKeys[0].vx == 8191 && dst.boneKeys[41].vy == 6 &&
           dst.scaleKeys[0].vx == 80 && dst.eventScript == NULL;
  }
  if (mode == 1) {
    Actors_BlendActorPose(&dst, &src, 1, 2, 4);
    return dst.boneKeys[0].vx == 8192 && dst.boneKeys[41].vy == 3 &&
           dst.scaleKeys[0].vx == 96 && dst.eventScript == NULL;
  }
  Actors_BlendActorPose(&dst, &src, 1, 4, 4);
  return memcmp(&dst, &src, sizeof src) == 0 && dst.eventScript == events;
}
