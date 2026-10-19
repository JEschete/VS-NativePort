// Hand-maintained (tools/port/owned.csv).
#include "Actors/ModelDrawing.h"

#include <stdint.h>
#include <string.h>

#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/Appearance.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/ModelRendering.h"
#include "Actors/Movement.h"
#include "Actors/Palette.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Memory/Heap.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/Gimmicks.h"
#include "Field/RoomRendering.h"
#include "Legacy/Psx.h"
#include "Menu/Inventory/Inventory.h"
#include "ModelLoading.h"
#include "Render/Camera.h"
#include "Render/Model.h"
#include "Render/ModelDraw.h"
#include "Render/ModelGeometry.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

static unsigned char ModelColorChannel(unsigned char base,
                                       unsigned char adjustment) {
  const int value = base + (signed char)adjustment * 4;
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (unsigned char)value;
}

static void TransformBonePosition(const MATRIX* parent, const SVECTOR* local,
                                  MATRIX* output) {
  const int16_t coordinates[3] = {local->vx, local->vy, local->vz};
  for (int row = 0; row < 3; ++row) {
    int64_t value = 0;
    for (int axis = 0; axis < 3; ++axis) {
      value += (int64_t)parent->m[row][axis] * coordinates[axis];
    }
    output->t[row] = (long)((value >> 12) + parent->t[row]);
  }
}

static void AdjustBoneRotation(Actors_ActorRecord* actor, unsigned tag,
                               SVECTOR* rotation) {
  const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  const unsigned yaw = workspace->angleX;
  const unsigned pitch = workspace->angleY;
  if (tag == 1 || tag == 2) {
    rotation->vx = Core_Math_WrapSigned16((uint32_t)rotation->vx - (yaw >> 1));
    rotation->vz = Core_Math_WrapSigned16((uint32_t)rotation->vz + (yaw >> 3));
    if (tag == 2) {
      rotation->vy =
          Core_Math_WrapSigned16((uint32_t)rotation->vy - (yaw >> 1));
      rotation->vy = Core_Math_WrapSigned16(
          (uint32_t)rotation->vy + (yaw != 0 ? pitch >> 3 : 0u - (pitch >> 3)));
      rotation->vz =
          Core_Math_WrapSigned16((uint32_t)rotation->vz + (pitch >> 2));
    }
  } else if (tag >= 3 && tag <= 6 && actor->instance.isFrozen) {
    uint16_t values[2];
    memcpy(values, actor->jointSlots + (tag - 3) * 4, sizeof values);
    values[1] += values[0];
    memcpy(actor->jointSlots + (tag - 3) * 4, values, sizeof values);
    rotation->vx = (int16_t)values[1];
  } else if (tag >= 7 && tag <= 9) {
    const Render_Camera* camera = &Render_frameScratch.camera;
    const int delta_x = camera->position.vx / 4096 - workspace->rootOffset.vx;
    const int delta_z = camera->position.vz / 4096 - workspace->rootOffset.vz;
    if (tag == 7) rotation->vx = (int16_t)camera->angles.vx;
    rotation->vy = (int16_t)-Core_Gte_Atan2(delta_z, delta_x);
    rotation->vz = 0;
    if (tag == 8) {
      const int64_t squared =
          (int64_t)delta_x * delta_x + (int64_t)delta_z * delta_z;
      const int distance = Core_Math_SquareRootInteger(
          squared > INT32_MAX ? INT32_MAX : (int)squared);
      rotation->vz = (int16_t)Core_Gte_Atan2(
          camera->position.vy / 4096 - workspace->rootOffset.vy, distance);
    }
  }
  rotation->vx &= 0xFFF;
  rotation->vy &= 0xFFF;
  rotation->vz &= 0xFFF;
}

// decomp: BATTLE.PRG func_800B28A8, bone loop 0x800B2938..0x800B34CC
// (49268/func_800B28A8.s).
int Actors_BuildActorModelPose(Actors_ActorRecord* actor, MATRIX* camera) {
  enum {
    kPoseBoneCount = sizeof actor->boneMatrices / sizeof actor->boneMatrices[0],
    kSpatialDimensions = 3
  };
  if (actor == NULL || camera == NULL || actor->instance.model == NULL ||
      actor->instance.boneCount == 0 ||
      actor->instance.boneCount > kPoseBoneCount ||
      actor->modelDataSize <
          sizeof(Actors_ShpModel) +
              actor->instance.boneCount * sizeof(Actors_ShpBone)) {
    return 0;
  }
  Actors_ShpModel* model = actor->instance.model;
  for (unsigned bone = 1; bone < actor->instance.boneCount; ++bone) {
    if (model->bones[bone].parentIndex >= bone ||
        model->bones[bone].poseLockSlot > 2) {
      return 0;
    }
  }
  const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  SVECTOR rotation = {(int16_t)((actor->blendPose.boneKeys[0].vx >> 1) +
                                actor->instance.rotationX),
                      (int16_t)((actor->blendPose.boneKeys[0].vy >> 1) +
                                actor->instance.facing),
                      (int16_t)((actor->blendPose.boneKeys[0].vz >> 1) +
                                actor->instance.rotationZ),
                      0};
  if (actor->paletteFadeActive) {
    rotation.vy = Core_Math_WrapSigned16(
        (uint32_t)rotation.vy + (uint32_t)(actor->paletteFadeState[0] >> 1));
  }
  MATRIX root = {0};
  Core_Math_BuildEulerRotationMatrixQuadrant(&rotation, &root);
  Core_Math_PremultiplyRotationMatrix(camera, &root);
  TransformBonePosition(camera, &workspace->rootOffset, &root);
  VECTOR scale = {actor->scale, actor->scale, actor->scale, 0};
  if (!actor->instance.isAttached) Core_Math_ScaleMatrixColumns(&root, &scale);
  scale.vx = actor->instance.scaleY;
  scale.vy = actor->instance.scaleZ;
  scale.vz = actor->instance.scaleX;
  Core_Math_ScaleMatrixColumns(&root, &scale);
  int cumulative[kPoseBoneCount][kSpatialDimensions] = {0};
  for (unsigned bone = 0; bone < actor->instance.boneCount; ++bone) {
    const Actors_ShpBone* joint = &model->bones[bone];
    MATRIX matrix = root;
    if (bone != 0 && joint->poseLockSlot != 0) {
      Actors_ApplyLockedBoneTransform(actor, camera, (int)bone,
                                      joint->poseLockSlot);
      cumulative[bone][0] = cumulative[bone][1] = cumulative[bone][2] = 64;
      continue;
    }
    const unsigned parent = bone == 0 ? 0 : joint->parentIndex;
    if (bone != 0) {
      const SVECTOR local = {(int16_t)-model->bones[parent].length, 0, 0, 0};
      TransformBonePosition(&actor->boneMatrices[parent], &local, &matrix);
      rotation.vx = (int16_t)(actor->blendPose.boneKeys[bone].vx >> 1);
      rotation.vy = (int16_t)(actor->blendPose.boneKeys[bone].vy >> 1);
      rotation.vz = (int16_t)(actor->blendPose.boneKeys[bone].vz >> 1);
      AdjustBoneRotation(actor, joint->auxiliaryTag, &rotation);
      if (joint->auxiliaryTag >= 7 && joint->auxiliaryTag <= 9) {
        Core_Gte_RotMatrixYXZ(&rotation, &matrix);
        Core_Math_PremultiplyRotationMatrix(camera, &matrix);
        scale.vx = scale.vy = scale.vz = actor->scale;
        Core_Math_ScaleMatrixColumns(&matrix, &scale);
      } else {
        Core_Math_BuildEulerRotationMatrixQuadrant(&rotation, &matrix);
        Core_Math_PremultiplyRotationMatrix(
            &Actors_skeletonPose.boneMatrices[parent], &matrix);
      }
    }
    Actors_skeletonPose.boneMatrices[bone] = matrix;
    const SVECTOR* local_scale = &actor->blendPose.scaleKeys[bone];
    const int components[3] = {local_scale->vx, local_scale->vy,
                               local_scale->vz};
    for (size_t axis = 0; axis < sizeof components / sizeof components[0];
         ++axis) {
      cumulative[bone][axis] =
          bone == 0
              ? components[axis]
              : (int)(((int64_t)cumulative[parent][axis] * components[axis]) >>
                      6);
    }
    scale.vx = cumulative[bone][0] * 64;
    scale.vy = cumulative[bone][1] * 64;
    scale.vz = cumulative[bone][2] * 64;
    Core_Math_ScaleMatrixColumns(&matrix, &scale);
    actor->boneMatrices[bone] = matrix;
  }
  return 1;
}

// decomp: BATTLE.PRG func_800B28A8, material dispatch at 0x800B34CC
// (49268/func_800B28A8.s).
void Actors_DrawActorModel(void* actor_data, MATRIX* camera, char companion) {
  enum { kEventPlaybackActive = 4 };
  Actors_ActorRecord* actor = actor_data;
  if (!Actors_BuildActorModelPose(actor, camera) ||
      !actor->instance.isVisible) {
    return;
  }
  if (actor->instance.position.vy >= 256 &&
      (actor->instance.slot == 0 ||
       Event_EventPlaybackState() != kEventPlaybackActive)) {
    return;
  }
  Actors_ActorRecord* geometry = actor;
  if ((unsigned char)companion >= 2) {
    if ((unsigned char)companion >=
            sizeof Actors_actorRecords / sizeof Actors_actorRecords[0] ||
        Actors_actorRecords[(unsigned char)companion] == NULL) {
      return;
    }
    geometry = Actors_actorRecords[(unsigned char)companion];
  }
  if (geometry->textureSlot >= 24 ||
      geometry->instance.boneCount > actor->instance.boneCount ||
      !Render_ValidateModelData(&geometry->instance, geometry->instance.model,
                                geometry->modelDataSize)) {
    Core_Debugf("render", "Invalid actor model %u data or texture slot",
                (unsigned)geometry->modelId);
    return;
  }
  Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  workspace->model = geometry->instance.model;
  workspace->texturePage =
      Render_legacyActorTextureSlots[geometry->textureSlot].texturePage;
  workspace->palette = (uint16_t)geometry->clutA;
  workspace->reserved38 = (uint16_t)geometry->clutB;
  if (!Render_ProjectModelVertices(&geometry->instance, actor->boneMatrices,
                                   -Render_nearClip * 4, workspace->model)) {
    actor->instance.isPreviewModel = 1;
    return;
  }
  actor->instance.isPreviewModel = 0;
  if (!geometry->instance.uvLayout && companion != 1) {
    if (!actor->customTint) {
      if (actor->previewFlag) {
        const unsigned packed =
            (uint16_t)actor->instance.position.vx |
            ((unsigned)(uint16_t)actor->instance.position.vz << 16);
        Field_SampleRoomGroundColor((int)packed,
                                    actor->instance.currentTileLayer,
                                    &actor->instance.tint);
      } else {
        actor->instance.tint.red = actor->instance.tint.green =
            actor->instance.tint.blue = 128;
      }
    }
    if (geometry->instance.actionAnimationState < 4) {
      geometry->instance.baseColor.r =
          ModelColorChannel(actor->instance.tint.red, geometry->fadeRed);
      geometry->instance.baseColor.g =
          ModelColorChannel(actor->instance.tint.green, geometry->fadeGreen);
      geometry->instance.baseColor.b =
          ModelColorChannel(actor->instance.tint.blue, geometry->fadeBlue);
      unsigned char* shadow_color = (void*)&geometry->shadowBlend.reserved0;
      shadow_color[0] = actor->instance.tint.red;
      shadow_color[1] = actor->instance.tint.green;
      shadow_color[2] = actor->instance.tint.blue;
    }
  }
  CVECTOR color = geometry->instance.baseColor;
  if (companion == 1) memcpy(&color, &actor->companionColor, sizeof color);
  if (geometry->instance.uvLayout) {
    Render_DrawModelGouraudPolygons(&Actors_modelRenderContext,
                                    &geometry->instance, &color,
                                    workspace->model);
  } else {
    Render_DrawModelFlatPolygons(&Actors_modelRenderContext,
                                 &geometry->instance, &color, workspace->model);
  }
}

// decomp: BATTLE.PRG func_800A141C (src/BATTLE/BATTLE.PRG/38C1C.c:39) [C: not
// yet ported]
int Actors_ProjectActorBoneEndpoint(int actor_id, int bone, int* output,
                                    int endpoint) {
  if (actor_id < 0 || actor_id >= 17 || output == NULL) return -1;
  const Actors_ShpModel* model;
  const MATRIX* matrix;
  const Actors_ActorRecord* actor = Actors_actorRecords[actor_id];
  if (actor != NULL) {
    if (bone < 0 || bone >= actor->instance.boneCount) return -4;
    model = actor->instance.model;
    matrix = &actor->boneMatrices[bone];
  } else {
    const Field_GimmickInstance* object =
        actor_id < 16 ? Field_gimmickInstances[actor_id] : NULL;
    if (object == NULL) return -1;
    if (bone < 0 || bone >= object->boneCount || bone >= 5) return -4;
    model = object->model;
    matrix = &object->matrices[bone];
  }
  if (model == NULL) return -1;
  SVECTOR position = {0};
  if (endpoint == 1) {
    position.vx = (int16_t)-(uint16_t)model->bones[bone].length;
  } else if (endpoint != 0) {
    position.vx = (int16_t)(-model->bones[bone].length / 2);
  }
  Core_Gte_SetRotMatrix(matrix);
  Core_Gte_SetTransMatrix(matrix);
  long screen, fog, flags;
  VECTOR transformed;
  Core_Gte_RotTransPers(&position, &screen, &fog, &flags);
  Core_Gte_StoreLvnl(&transformed);
  output[0] = (int)screen;
  output[1] = transformed.vz < 0       ? 0
              : transformed.vz > 65535 ? 65535
                                       : transformed.vz;
  return 0;
}
