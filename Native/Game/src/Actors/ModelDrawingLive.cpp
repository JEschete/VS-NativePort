// Hand-maintained (tools/port/owned.csv).
// Live binding for actor model drawing and bone projection (ModelDrawing.cpp):
// actor records, the model workspace, the GTE and the ordering table.
#include "Actors/ModelDrawingLive.hpp"

#include <stdint.h>
#include <string.h>

#include <array>
#include <iterator>
#include <span>
#include <type_traits>

#include "Core/Math/ScalarMath.hpp"
#include "Render/ModelGeometryLive.hpp"
extern "C" {
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
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
#include "Field/Actors.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "Field/RoomRendering.h"
#include "Legacy/Psx.h"
#include "Menu/Inventory/Inventory.h"
#include "ModelLoading.h"
#include "Render/Camera.h"
#include "Render/Model.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Palette.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"
}

namespace VS::Actors {
namespace {
enum {
  kActorFixedFractionBits = 12,
  kActorFixedOne = 1 << kActorFixedFractionBits,
  kActorRotationMask = kActorFixedOne - 1,
  kPoseScaleFractionBits = 6,
  kPoseScaleOne = 1 << kPoseScaleFractionBits,
  kFirstCameraFacingTag = 7,
  kCameraHeightTag = 8,
  kLastCameraFacingTag = 9,
  kTintStateLimit = 4,
  kEventPlaybackActive = 4
};

using PoseMatrices = decltype(Actors_ActorRecord::boneMatrices);
constexpr unsigned kActorSlotCount = std::size(Actors_actorRecords);
constexpr unsigned kPoseBoneCapacity = std::extent_v<PoseMatrices>;

// The model's header and bone table lie within its loaded data.
bool ModelHoldsBones(const Actors_ActorRecord& actor, unsigned bones) {
  return actor.modelDataSize >=
         sizeof(Actors_ShpModel) + (bones * sizeof(Actors_ShpBone));
}
}  // namespace

// Parent slots can be released before independently owned props in EVENT82.
// The original 44F14.c:1010 draw assumes the borrowed bone matrix is resident.
MATRIX* ResolveLiveAttachmentParentMatrix(const Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    Core_Errorf("actors.attachment", "Cannot resolve a null child actor");
    return nullptr;
  }
  const unsigned parent_slot = actor->instance.attachParent;
  const unsigned bone = actor->instance.attachBone;
  if (parent_slot >= kActorSlotCount) {
    Core_Errorf("actors.attachment",
                "Invalid parent actor=%u parent=%u bone=%u",
                static_cast<unsigned>(actor->instance.slot), parent_slot, bone);
    return nullptr;
  }
  Actors_ActorRecord* parent = Actors_actorRecords[parent_slot];
  if (parent == nullptr || parent->instance.model == nullptr ||
      parent->instance.boneCount == 0) {
    Core_Debugf("actors.attachment",
                "Deferred draw actor=%u parent=%u bone=%u: parent unavailable",
                static_cast<unsigned>(actor->instance.slot), parent_slot, bone);
    return nullptr;
  }
  const unsigned bone_capacity =
      sizeof parent->boneMatrices / sizeof parent->boneMatrices[0];
  if (parent->instance.boneCount > bone_capacity ||
      bone >= parent->instance.boneCount) {
    Core_Errorf("actors.attachment",
                "Invalid bone actor=%u parent=%u bone=%u parent_bones=%u",
                static_cast<unsigned>(actor->instance.slot), parent_slot, bone,
                static_cast<unsigned>(parent->instance.boneCount));
    return nullptr;
  }
  return &parent->boneMatrices[bone];
}

static void TransformBonePosition(const MATRIX* parent, const SVECTOR* local,
                                  MATRIX* output) {
  enum { kFixedFractionBits = 12, kSpatialDimensions = 3 };
  const int16_t coordinates[kSpatialDimensions] = {local->vx, local->vy,
                                                   local->vz};
  for (int row = 0; row < kSpatialDimensions; ++row) {
    int64_t value = 0;
    for (int axis = 0; axis < kSpatialDimensions; ++axis) {
      value += static_cast<int64_t>(parent->m[row][axis]) * coordinates[axis];
    }
    output->t[row] = static_cast<Core_GteWord>((value >> kFixedFractionBits) +
                                               parent->t[row]);
  }
}

static void AdjustBoneRotation(Actors_ActorRecord* actor, unsigned tag,
                               SVECTOR* rotation) {
  enum {
    kAuxiliaryAngleShift = 3,
    kCameraPitchTag = 7,
    kFirstFrozenJointTag = 3,
    kLastFrozenJointTag = 6
  };
  const Render_ModelScratchpad* workspace = Render_ModelWorkspace();
  const unsigned yaw = workspace->angleX;
  const unsigned pitch = workspace->angleY;
  if (tag == 1 || tag == 2) {
    rotation->vx = VS::Core::Math::WrapSigned16(
        static_cast<uint32_t>(rotation->vx) - (yaw >> 1));
    rotation->vz = VS::Core::Math::WrapSigned16(
        static_cast<uint32_t>(rotation->vz) + (yaw >> kAuxiliaryAngleShift));
    if (tag == 2) {
      rotation->vy = VS::Core::Math::WrapSigned16(
          static_cast<uint32_t>(rotation->vy) - (yaw >> 1));
      rotation->vy = VS::Core::Math::WrapSigned16(
          static_cast<uint32_t>(rotation->vy) +
          (yaw != 0 ? pitch >> kAuxiliaryAngleShift
                    : 0u - (pitch >> kAuxiliaryAngleShift)));
      rotation->vz = VS::Core::Math::WrapSigned16(
          static_cast<uint32_t>(rotation->vz) + (pitch >> 2));
    }
  } else if (tag >= kFirstFrozenJointTag && tag <= kLastFrozenJointTag &&
             actor->instance.isFrozen) {
    uint16_t values[2];
    memcpy(values,
           actor->jointSlots +
               static_cast<size_t>(tag - kFirstFrozenJointTag) * sizeof values,
           sizeof values);
    values[1] += values[0];
    memcpy(actor->jointSlots +
               static_cast<size_t>(tag - kFirstFrozenJointTag) * sizeof values,
           values, sizeof values);
    rotation->vx = static_cast<int16_t>(values[1]);
  } else if (tag >= kFirstCameraFacingTag && tag <= kLastCameraFacingTag) {
    const Render_Camera* camera = &Render_frameScratch.camera;
    const int delta_x =
        camera->position.vx / kActorFixedOne - workspace->rootOffset.vx;
    const int delta_z =
        camera->position.vz / kActorFixedOne - workspace->rootOffset.vz;
    if (tag == kCameraPitchTag) {
      rotation->vx = static_cast<int16_t>(camera->angles.vx);
    }
    rotation->vy =
        static_cast<int16_t>(-VS::Core::Math::Arctangent2(delta_z, delta_x));
    rotation->vz = 0;
    if (tag == kCameraHeightTag) {
      const int64_t squared = static_cast<int64_t>(delta_x) * delta_x +
                              static_cast<int64_t>(delta_z) * delta_z;
      const int distance = VS::Core::Math::SquareRootInteger(
          squared > INT32_MAX ? INT32_MAX : static_cast<int>(squared));
      rotation->vz = static_cast<int16_t>(VS::Core::Math::Arctangent2(
          camera->position.vy / kActorFixedOne - workspace->rootOffset.vy,
          distance));
    }
  }
  rotation->vx &= kActorRotationMask;
  rotation->vy &= kActorRotationMask;
  rotation->vz &= kActorRotationMask;
}

// Unattached actors take their uniform scale, then every actor takes its
// per-axis instance scale (stored y, z, x).
static void ScaleActorRoot(const Actors_ActorRecord& actor, MATRIX& root) {
  VECTOR scale = {actor.scale, actor.scale, actor.scale, 0};
  if (!actor.instance.isAttached) {
    VS::Core::Math::ApplyScaleMatrixColumns(root, scale);
  }
  scale.vx = actor.instance.scaleY;
  scale.vy = actor.instance.scaleZ;
  scale.vz = actor.instance.scaleX;
  VS::Core::Math::ApplyScaleMatrixColumns(root, scale);
}

// decomp: BATTLE.PRG func_800B28A8, bone loop 0x800B2938..0x800B34CC
// (49268/func_800B28A8.s).
static int BuildActorModelPose(Actors_ActorRecord* actor, MATRIX* camera) {
  enum { kPoseBoneCount = kPoseBoneCapacity, kSpatialDimensions = 3 };
  if (actor == nullptr || camera == nullptr ||
      actor->instance.model == nullptr || actor->instance.boneCount == 0 ||
      actor->instance.boneCount > kPoseBoneCount ||
      !ModelHoldsBones(*actor, actor->instance.boneCount)) {
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
  SVECTOR rotation = {
      static_cast<int16_t>((actor->blendPose.boneKeys[0].vx >> 1) +
                           actor->instance.rotationX),
      static_cast<int16_t>((actor->blendPose.boneKeys[0].vy >> 1) +
                           actor->instance.facing),
      static_cast<int16_t>((actor->blendPose.boneKeys[0].vz >> 1) +
                           actor->instance.rotationZ),
      0};
  if (actor->paletteFadeActive) {
    rotation.vy = VS::Core::Math::WrapSigned16(
        static_cast<uint32_t>(rotation.vy) +
        static_cast<uint32_t>(actor->paletteFadeState[0] >> 1));
  }
  MATRIX root{};
  VS::Core::Math::ApplyBuildEulerRotationMatrixQuadrant(rotation, root);
  VS::Core::Math::ApplyPremultiplyRotationMatrix(*camera, root);
  TransformBonePosition(camera, &workspace->rootOffset, &root);
  ScaleActorRoot(*actor, root);
  VECTOR scale{};
  int cumulative[kPoseBoneCount][kSpatialDimensions] = {};
  for (unsigned bone = 0; bone < actor->instance.boneCount; ++bone) {
    const Actors_ShpBone* joint = &model->bones[bone];
    MATRIX matrix = root;
    if (bone != 0 && joint->poseLockSlot != 0) {
      Actors_ApplyLockedBoneTransform(actor, camera, static_cast<int>(bone),
                                      joint->poseLockSlot);
      cumulative[bone][0] = cumulative[bone][1] = cumulative[bone][2] =
          kPoseScaleOne;
      continue;
    }
    const unsigned parent = bone == 0 ? 0 : joint->parentIndex;
    if (bone != 0) {
      const SVECTOR local = {static_cast<int16_t>(-model->bones[parent].length),
                             0, 0, 0};
      TransformBonePosition(&actor->boneMatrices[parent], &local, &matrix);
      rotation.vx =
          static_cast<int16_t>(actor->blendPose.boneKeys[bone].vx >> 1);
      rotation.vy =
          static_cast<int16_t>(actor->blendPose.boneKeys[bone].vy >> 1);
      rotation.vz =
          static_cast<int16_t>(actor->blendPose.boneKeys[bone].vz >> 1);
      AdjustBoneRotation(actor, joint->auxiliaryTag, &rotation);
      if (joint->auxiliaryTag >= kFirstCameraFacingTag &&
          joint->auxiliaryTag <= kLastCameraFacingTag) {
        VS::Core::Math::ApplyBuildEulerRotationMatrix(rotation, matrix);
        VS::Core::Math::ApplyPremultiplyRotationMatrix(*camera, matrix);
        scale.vx = scale.vy = scale.vz = actor->scale;
        VS::Core::Math::ApplyScaleMatrixColumns(matrix, scale);
      } else {
        VS::Core::Math::ApplyBuildEulerRotationMatrixQuadrant(rotation, matrix);
        VS::Core::Math::ApplyPremultiplyRotationMatrix(
            Actors_skeletonPose.boneMatrices[parent], matrix);
      }
    }
    Actors_skeletonPose.boneMatrices[bone] = matrix;
    const SVECTOR* local_scale = &actor->blendPose.scaleKeys[bone];
    const int components[3] = {local_scale->vx, local_scale->vy,
                               local_scale->vz};
    for (size_t axis = 0; axis < sizeof components / sizeof components[0];
         ++axis) {
      cumulative[bone][axis] =
          bone == 0 ? components[axis]
                    : static_cast<int>(
                          (static_cast<int64_t>(cumulative[parent][axis]) *
                           components[axis]) >>
                          kPoseScaleFractionBits);
    }
    scale.vx = cumulative[bone][0] * kPoseScaleOne;
    scale.vy = cumulative[bone][1] * kPoseScaleOne;
    scale.vz = cumulative[bone][2] * kPoseScaleOne;
    VS::Core::Math::ApplyScaleMatrixColumns(matrix, scale);
    actor->boneMatrices[bone] = matrix;
  }
  return 1;
}

static void UpdateActorModelColors(Actors_ActorRecord* actor,
                                   Actors_ActorRecord* geometry,
                                   ModelDrawRole role,
                                   const SVECTOR* root_offset) {
  if (!geometry->instance.uvLayout &&
      role != ModelDrawRole::kCompanionOverlay) {
    if (!actor->customTint) {
      if (actor->previewFlag) {
        const unsigned packed =
            static_cast<uint16_t>(actor->instance.position.vx) |
            (static_cast<unsigned>(
                 static_cast<uint16_t>(actor->instance.position.vz))
             << 16);
        Field_SampleRoomGroundColor(static_cast<int>(packed),
                                    actor->instance.currentTileLayer,
                                    &actor->instance.tint);
      } else {
        actor->instance.tint.red = actor->instance.tint.green =
            actor->instance.tint.blue = kNeutralModelColor;
      }
    }
    if (geometry->instance.actionAnimationState < kTintStateLimit) {
      geometry->instance.baseColor.r =
          AdjustModelColorChannel(actor->instance.tint.red, geometry->fadeRed);
      geometry->instance.baseColor.g = AdjustModelColorChannel(
          actor->instance.tint.green, geometry->fadeGreen);
      geometry->instance.baseColor.b = AdjustModelColorChannel(
          actor->instance.tint.blue, geometry->fadeBlue);
      unsigned char* shadow_color =
          reinterpret_cast<unsigned char*>(&geometry->shadowBlend.reserved0);
      shadow_color[0] = actor->instance.tint.red;
      shadow_color[1] = actor->instance.tint.green;
      shadow_color[2] = actor->instance.tint.blue;
    }
  } else if (geometry->instance.uvLayout &&
             geometry->instance.actionAnimationState < kTintStateLimit) {
    const unsigned packed =
        static_cast<uint16_t>(root_offset->vx) |
        (static_cast<unsigned>(static_cast<uint16_t>(root_offset->vz)) << 16);
    Render_ColorCode ground;
    Field_SampleRoomGroundColor(static_cast<int>(packed),
                                actor->instance.currentTileLayer, &ground);
    unsigned char* shadow_color =
        reinterpret_cast<unsigned char*>(&actor->shadowBlend.reserved0);
    shadow_color[0] = GroundShadowChannel(ground.red);
    shadow_color[1] = GroundShadowChannel(ground.green);
    shadow_color[2] = GroundShadowChannel(ground.blue);
  }
}

namespace {
class LiveActorModelDraw final : public ActorModelDrawHost {
 public:
  LiveActorModelDraw(Actors_ActorRecord& actor, MATRIX& camera)
      : actor_(actor), camera_(camera) {}

  bool BuildPose() override {
    return BuildActorModelPose(&actor_, &camera_) != 0;
  }
  bool IsVisible() const override { return actor_.instance.isVisible != 0; }
  int Height() const override { return actor_.instance.position.vy; }
  bool IsLeader() const override { return actor_.instance.slot == 0; }
  bool IsEventPlaying() const override {
    return Event_EventPlaybackState() == kEventPlaybackActive;
  }
  bool SelectGeometry(int slot) override {
    if (slot == 0) {
      geometry_ = &actor_;
      return true;
    }
    if (static_cast<unsigned>(slot) >= kActorSlotCount) return false;
    geometry_ = Actors_actorRecords[slot];
    return geometry_ != nullptr;
  }
  bool BindGeometry() override {
    if (geometry_->textureSlot >=
            sizeof Render_legacyActorTextureSlots /
                sizeof Render_legacyActorTextureSlots[0] ||
        geometry_->instance.boneCount > actor_.instance.boneCount ||
        !VS::Render::ValidateLiveActorModel(*geometry_)) {
      Core_Debugf("render", "Invalid actor model %u data or texture slot",
                  static_cast<unsigned>(geometry_->modelId));
      return false;
    }
    workspace_ = Render_ModelWorkspace();
    workspace_->model = geometry_->instance.model;
    workspace_->texturePage =
        Render_legacyActorTextureSlots[geometry_->textureSlot].texturePage;
    workspace_->palette = static_cast<uint16_t>(geometry_->clutA);
    workspace_->reserved38 = static_cast<uint16_t>(geometry_->clutB);
    return true;
  }
  bool ProjectVertices() override {
    return VS::Render::ProjectLiveActorModel(*geometry_, actor_.boneMatrices);
  }
  void SetCulled(bool culled) override {
    actor_.instance.isPreviewModel = culled ? 1 : 0;
  }
  void UpdateColors(ModelDrawRole role) override {
    UpdateActorModelColors(&actor_, geometry_, role, &workspace_->rootOffset);
  }
  bool GeometryHasUvLayout() const override {
    return geometry_->instance.uvLayout != 0;
  }
  void DrawPolygons(ModelPolygons polygons) override {
    CVECTOR color = geometry_->instance.baseColor;
    switch (polygons) {
      case ModelPolygons::kTextured:
        VS::Render::DrawLiveActorPolygons(*geometry_, color);
        return;
      case ModelPolygons::kCompanionGouraud:
        memcpy(&color, &actor_.companionColor, sizeof color);
        VS::Render::DrawLiveActorGouraudPolygons(*geometry_, color);
        return;
      case ModelPolygons::kCompanionFlat:
        memcpy(&color, &actor_.companionColor, sizeof color);
        VS::Render::DrawLiveActorFlatPolygons(*geometry_, color);
        return;
    }
  }
  bool CastsShadow() const override {
    return actor_.instance.soundEnabled != 0;
  }
  void DrawShadow() override {
    VS::Render::DrawLiveActorGroundShadow(actor_,
                                          Render_frameScratch.viewMatrix,
                                          Render_ModelWorkspace()->rootOffset);
  }

 private:
  Actors_ActorRecord& actor_;
  MATRIX& camera_;
  Actors_ActorRecord* geometry_ = nullptr;
  Render_ModelScratchpad* workspace_ = nullptr;
};

class LiveBoneProjection final : public BoneProjectionHost {
 public:
  BoneSelection SelectBone(int slot, int bone) override {
    if (static_cast<unsigned>(slot) >= kActorSlotCount) return {};
    const Actors_ActorRecord* actor = Actors_actorRecords[slot];
    if (actor != nullptr) {
      // #263: the bone must lie within the pose matrices and the loaded bone
      // table, not only below the model's own bone count.
      if (bone < 0 || bone >= actor->instance.boneCount ||
          static_cast<unsigned>(bone) >= kPoseBoneCapacity ||
          (actor->instance.model != nullptr &&
           !ModelHoldsBones(*actor, static_cast<unsigned>(bone) + 1))) {
        return {BoneLookup::kInvalidBone, 0};
      }
      if (actor->instance.model == nullptr) return {};
      matrix_ = &actor->boneMatrices[bone];
      return {BoneLookup::kFound, actor->instance.model->bones[bone].length};
    }
    const Field_GimmickInstance* object = Field_FindGimmickForActorSlot(slot);
    if (object == nullptr) return {};
    if (bone < 0 || bone >= object->boneCount ||
        static_cast<size_t>(bone) >=
            sizeof object->matrices / sizeof object->matrices[0]) {
      return {BoneLookup::kInvalidBone, 0};
    }
    if (object->model == nullptr) return {};
    matrix_ = &object->matrices[bone];
    return {BoneLookup::kFound, object->model->bones[bone].length};
  }
  ProjectedBonePoint Project(std::int16_t offset_x) override {
    SVECTOR position = {offset_x, 0, 0, 0};
    Core_Gte_SetRotMatrix(matrix_);
    Core_Gte_SetTransMatrix(matrix_);
    Core_GteWord screen = 0;
    Core_GteWord fog = 0;
    Core_GteWord flags = 0;
    VECTOR transformed{};
    Core_Gte_RotTransPers(&position, &screen, &fog, &flags);
    Core_Gte_StoreLvnl(&transformed);
    return {static_cast<std::int32_t>(screen), transformed.vz};
  }

 private:
  const MATRIX* matrix_ = nullptr;
};
}  // namespace

bool BuildLiveActorModelPose(Actors_ActorRecord& actor, MATRIX& camera) {
  return BuildActorModelPose(&actor, &camera) != 0;
}

ModelDrawResult DrawLiveActorModel(Actors_ActorRecord& actor, MATRIX& camera,
                                   int companion) {
  LiveActorModelDraw host(actor, camera);
  return DrawActorModel(host, companion);
}

int ProjectLiveActorBoneEndpoint(int slot, int bone, int* output,
                                 int endpoint) {
  if (output == nullptr) return kBoneProjectionMissing;
  LiveBoneProjection host;
  std::array<std::int32_t, 2> projected{};
  const int result =
      ProjectActorBoneEndpoint(host, slot, bone, endpoint, projected);
  if (result == 0) {
    output[0] = projected[0];
    output[1] = projected[1];
  }
  return result;
}
}  // namespace VS::Actors
