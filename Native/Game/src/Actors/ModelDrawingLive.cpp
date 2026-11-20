// Hand-maintained (tools/port/owned.csv).
// Live binding for actor model drawing and bone projection (ModelDrawing.cpp):
// actor records, the model workspace, the GTE and the ordering table.
#include "Actors/ModelDrawingLive.hpp"

#include <stdint.h>
#include <string.h>

#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/ActorRecord.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/ModelDraw.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Shadow.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/TextureAnimationRecords.hpp"
#include "Actors/WeaponModel.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/GimmickLookup.hpp"
#include "Field/Gimmicks.hpp"
#include "Field/RoomLightingLive.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Camera.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/Palette.hpp"
#include "Render/Render.hpp"
#include "Render/RoomLighting.hpp"
#include "Render/Textures.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::MATRIX;
using VS::Core::Gte::RotTransPers;
using VS::Core::Gte::SetRotMatrix;
using VS::Core::Gte::SetTransMatrix;
using VS::Core::Gte::StoreLvnl;
using VS::Core::Gte::SVECTOR;
using VS::Core::Gte::VECTOR;

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

using PoseMatrices = decltype(VS::Actors::ActorRecord::bone_matrices);
constexpr unsigned kActorSlotCount = std::size(VS::Actors::actorRecords);
constexpr unsigned kPoseBoneCapacity = std::extent_v<PoseMatrices>;

// The model's header and bone table lie within its loaded data.
bool ModelHoldsBones(const VS::Actors::ActorRecord& actor, unsigned bones) {
  return actor.model_data_size >=
         sizeof(VS::Actors::ShpModel) + (bones * sizeof(VS::Actors::ShpBone));
}
}  // namespace

// Parent slots can be released before independently owned props in EVENT82.
// The original 44F14.c:1010 draw assumes the borrowed bone matrix is resident.
MATRIX* ResolveLiveAttachmentParentMatrix(
    const VS::Actors::ActorRecord* actor) {
  if (actor == nullptr) {
    VS::Core::Errorf("actors.attachment", "Cannot resolve a null child actor");
    return nullptr;
  }
  const unsigned parent_slot = actor->instance.attach_parent;
  const unsigned bone = actor->instance.attach_bone;
  if (parent_slot >= kActorSlotCount) {
    VS::Core::Errorf(
        "actors.attachment", "Invalid parent actor=%u parent=%u bone=%u",
        static_cast<unsigned>(actor->instance.slot), parent_slot, bone);
    return nullptr;
  }
  VS::Actors::ActorRecord* parent = VS::Actors::actorRecords[parent_slot];
  if (parent == nullptr || parent->instance.model == nullptr ||
      parent->instance.bone_count == 0) {
    VS::Core::Debugf(
        "actors.attachment",
        "Deferred draw actor=%u parent=%u bone=%u: parent unavailable",
        static_cast<unsigned>(actor->instance.slot), parent_slot, bone);
    return nullptr;
  }
  const unsigned bone_capacity =
      sizeof parent->bone_matrices / sizeof parent->bone_matrices[0];
  if (parent->instance.bone_count > bone_capacity ||
      bone >= parent->instance.bone_count) {
    VS::Core::Errorf("actors.attachment",
                     "Invalid bone actor=%u parent=%u bone=%u parent_bones=%u",
                     static_cast<unsigned>(actor->instance.slot), parent_slot,
                     bone, static_cast<unsigned>(parent->instance.bone_count));
    return nullptr;
  }
  return &parent->bone_matrices[bone];
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
    output->t[row] = static_cast<VS::Core::Gte::Word>(
        (value >> kFixedFractionBits) + parent->t[row]);
  }
}

static void AdjustBoneRotation(VS::Actors::ActorRecord* actor, unsigned tag,
                               SVECTOR* rotation) {
  enum {
    kAuxiliaryAngleShift = 3,
    kCameraPitchTag = 7,
    kFirstFrozenJointTag = 3,
    kLastFrozenJointTag = 6
  };
  const VS::Render::ModelScratchpad* workspace = VS::Render::ModelWorkspace();
  const unsigned yaw = workspace->angle_x;
  const unsigned pitch = workspace->angle_y;
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
             actor->instance.is_frozen) {
    uint16_t values[2];
    memcpy(values,
           actor->joint_slots +
               static_cast<size_t>(tag - kFirstFrozenJointTag) * sizeof values,
           sizeof values);
    values[1] += values[0];
    memcpy(actor->joint_slots +
               static_cast<size_t>(tag - kFirstFrozenJointTag) * sizeof values,
           values, sizeof values);
    rotation->vx = static_cast<int16_t>(values[1]);
  } else if (tag >= kFirstCameraFacingTag && tag <= kLastCameraFacingTag) {
    const VS::Render::Camera* camera = &VS::Render::FrameScratchpad().camera;
    const int delta_x =
        camera->position.vx / kActorFixedOne - workspace->root_offset.vx;
    const int delta_z =
        camera->position.vz / kActorFixedOne - workspace->root_offset.vz;
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
          camera->position.vy / kActorFixedOne - workspace->root_offset.vy,
          distance));
    }
  }
  rotation->vx &= kActorRotationMask;
  rotation->vy &= kActorRotationMask;
  rotation->vz &= kActorRotationMask;
}

// Unattached actors take their uniform scale, then every actor takes its
// per-axis instance scale (stored y, z, x).
static void ScaleActorRoot(const VS::Actors::ActorRecord& actor, MATRIX& root) {
  VECTOR scale = {actor.scale, actor.scale, actor.scale, 0};
  if (!actor.instance.is_attached) {
    VS::Core::Math::ApplyScaleMatrixColumns(root, scale);
  }
  scale.vx = actor.instance.scale_y;
  scale.vy = actor.instance.scale_z;
  scale.vz = actor.instance.scale_x;
  VS::Core::Math::ApplyScaleMatrixColumns(root, scale);
}

// decomp: BATTLE.PRG func_800B28A8, bone loop 0x800B2938..0x800B34CC
// (49268/func_800B28A8.s).
static int BuildActorModelPose(VS::Actors::ActorRecord* actor, MATRIX* camera) {
  enum { kPoseBoneCount = kPoseBoneCapacity, kSpatialDimensions = 3 };
  if (actor == nullptr || camera == nullptr ||
      actor->instance.model == nullptr || actor->instance.bone_count == 0 ||
      actor->instance.bone_count > kPoseBoneCount ||
      !ModelHoldsBones(*actor, actor->instance.bone_count)) {
    return 0;
  }
  VS::Actors::ShpModel* model = actor->instance.model;
  for (unsigned bone = 1; bone < actor->instance.bone_count; ++bone) {
    if (model->bones[bone].parent_index >= bone ||
        model->bones[bone].pose_lock_slot > 2) {
      return 0;
    }
  }
  const VS::Render::ModelScratchpad* workspace = VS::Render::ModelWorkspace();
  SVECTOR rotation = {
      static_cast<int16_t>((actor->blend_pose.bone_keys[0].vx >> 1) +
                           actor->instance.rotation_x),
      static_cast<int16_t>((actor->blend_pose.bone_keys[0].vy >> 1) +
                           actor->instance.facing),
      static_cast<int16_t>((actor->blend_pose.bone_keys[0].vz >> 1) +
                           actor->instance.rotation_z),
      0};
  if (actor->is_swaying) {
    rotation.vy = VS::Core::Math::WrapSigned16(
        static_cast<uint32_t>(rotation.vy) +
        static_cast<uint32_t>(actor->sway_angles[0] >> 1));
  }
  MATRIX root{};
  VS::Core::Math::ApplyBuildEulerRotationMatrixQuadrant(rotation, root);
  VS::Core::Math::ApplyPremultiplyRotationMatrix(*camera, root);
  TransformBonePosition(camera, &workspace->root_offset, &root);
  ScaleActorRoot(*actor, root);
  VECTOR scale{};
  int cumulative[kPoseBoneCount][kSpatialDimensions] = {};
  for (unsigned bone = 0; bone < actor->instance.bone_count; ++bone) {
    const VS::Actors::ShpBone* joint = &model->bones[bone];
    MATRIX matrix = root;
    if (bone != 0 && joint->pose_lock_slot != 0) {
      VS::Actors::ApplyLiveLockedBoneTransform(
          actor, camera, static_cast<int>(bone), joint->pose_lock_slot);
      cumulative[bone][0] = cumulative[bone][1] = cumulative[bone][2] =
          kPoseScaleOne;
      continue;
    }
    const unsigned parent = bone == 0 ? 0 : joint->parent_index;
    if (bone != 0) {
      const SVECTOR local = {static_cast<int16_t>(-model->bones[parent].length),
                             0, 0, 0};
      TransformBonePosition(&actor->bone_matrices[parent], &local, &matrix);
      rotation.vx =
          static_cast<int16_t>(actor->blend_pose.bone_keys[bone].vx >> 1);
      rotation.vy =
          static_cast<int16_t>(actor->blend_pose.bone_keys[bone].vy >> 1);
      rotation.vz =
          static_cast<int16_t>(actor->blend_pose.bone_keys[bone].vz >> 1);
      AdjustBoneRotation(actor, joint->auxiliary_tag, &rotation);
      if (joint->auxiliary_tag >= kFirstCameraFacingTag &&
          joint->auxiliary_tag <= kLastCameraFacingTag) {
        VS::Core::Math::ApplyBuildEulerRotationMatrix(rotation, matrix);
        VS::Core::Math::ApplyPremultiplyRotationMatrix(*camera, matrix);
        scale.vx = scale.vy = scale.vz = actor->scale;
        VS::Core::Math::ApplyScaleMatrixColumns(matrix, scale);
      } else {
        VS::Core::Math::ApplyBuildEulerRotationMatrixQuadrant(rotation, matrix);
        VS::Core::Math::ApplyPremultiplyRotationMatrix(
            skeletonPose.bone_matrices[parent], matrix);
      }
    }
    skeletonPose.bone_matrices[bone] = matrix;
    const SVECTOR* local_scale = &actor->blend_pose.scale_keys[bone];
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
    actor->bone_matrices[bone] = matrix;
  }
  return 1;
}

static void UpdateActorModelColors(VS::Actors::ActorRecord* actor,
                                   VS::Actors::ActorRecord* geometry,
                                   ModelDrawRole role,
                                   const SVECTOR* root_offset) {
  // The ground colour is the room's floor light at the actor's position,
  // whatever tile layer it stands on: the original called func_8008C458
  // (146C.c:16465), which samples layer 0 for every caller. The drawing
  // actor's own UV layout chooses the branch, and the Gouraud branch gates on
  // its own state (0x800B36C8, 0x800B385C).
  if (!actor->instance.uv_layout && role != ModelDrawRole::kCompanionOverlay) {
    if (!actor->custom_tint) {
      if (actor->floor_lit) {
        const unsigned packed =
            static_cast<uint16_t>(actor->instance.position.vx) |
            (static_cast<unsigned>(
                 static_cast<uint16_t>(actor->instance.position.vz))
             << 16);
        VS::Field::SampleLiveRoomLighting(
            packed, VS::Render::RoomLightLayer::kFloor, &actor->instance.tint);
      } else {
        actor->instance.tint.red = actor->instance.tint.green =
            actor->instance.tint.blue = kNeutralModelColor;
      }
    }
    if (geometry->instance.action_animation_state < kTintStateLimit) {
      geometry->instance.base_color.r =
          AdjustModelColorChannel(actor->instance.tint.red, geometry->fade_red);
      geometry->instance.base_color.g = AdjustModelColorChannel(
          actor->instance.tint.green, geometry->fade_green);
      geometry->instance.base_color.b = AdjustModelColorChannel(
          actor->instance.tint.blue, geometry->fade_blue);
      unsigned char* shadow_color =
          reinterpret_cast<unsigned char*>(&geometry->shadow_blend.color);
      shadow_color[0] = actor->instance.tint.red;
      shadow_color[1] = actor->instance.tint.green;
      shadow_color[2] = actor->instance.tint.blue;
    }
  } else if (actor->instance.uv_layout &&
             actor->instance.action_animation_state < kTintStateLimit) {
    const unsigned packed =
        static_cast<uint16_t>(root_offset->vx) |
        (static_cast<unsigned>(static_cast<uint16_t>(root_offset->vz)) << 16);
    VS::Render::ColorCode ground{};
    VS::Field::SampleLiveRoomLighting(
        packed, VS::Render::RoomLightLayer::kFloor, &ground);
    unsigned char* shadow_color =
        reinterpret_cast<unsigned char*>(&actor->shadow_blend.color);
    shadow_color[0] = GroundShadowChannel(ground.red);
    shadow_color[1] = GroundShadowChannel(ground.green);
    shadow_color[2] = GroundShadowChannel(ground.blue);
  }
}

namespace {
[[nodiscard]] ModelAppearance AppearanceOf(
    const VS::Actors::ActorRecord& actor) {
  return {.pulse_kind = actor.instance.palette_pulse.kind,
          .state =
              static_cast<std::uint8_t>(actor.instance.action_animation_state)};
}

class LiveActorModelDraw final : public ActorModelDrawHost {
 public:
  LiveActorModelDraw(VS::Actors::ActorRecord& actor, MATRIX& camera)
      : actor_(actor), camera_(camera) {}

  bool BuildPose() override {
    return BuildActorModelPose(&actor_, &camera_) != 0;
  }
  bool IsVisible() const override { return actor_.instance.is_visible != 0; }
  int Height() const override { return actor_.instance.position.vy; }
  bool IsLeader() const override { return actor_.instance.slot == 0; }
  bool IsEventPlaying() const override {
    return static_cast<int>(VS::Event::EventPlaybackState(
               VS::Event::LiveScriptVm())) == kEventPlaybackActive;
  }
  bool SelectGeometry(int slot) override {
    if (slot == 0) {
      geometry_ = &actor_;
      return true;
    }
    if (static_cast<unsigned>(slot) >= kActorSlotCount) return false;
    geometry_ = VS::Actors::actorRecords[slot];
    return geometry_ != nullptr;
  }
  bool BindGeometry() override {
    if (geometry_->texture_slot >=
            sizeof VS::Render::legacyActorTextureSlots /
                sizeof VS::Render::legacyActorTextureSlots[0] ||
        geometry_->instance.bone_count > actor_.instance.bone_count ||
        !VS::Render::ValidateLiveActorModel(*geometry_)) {
      VS::Core::Debugf("render", "Invalid actor model %u data or texture slot",
                       static_cast<unsigned>(geometry_->model_id));
      return false;
    }
    workspace_ = VS::Render::ModelWorkspace();
    workspace_->model = geometry_->instance.model;
    workspace_->texture_page =
        VS::Render::legacyActorTextureSlots[geometry_->texture_slot]
            .texture_page;
    workspace_->palette = static_cast<uint16_t>(geometry_->clut_a);
    workspace_->reserved38 = static_cast<uint16_t>(geometry_->clut_b);
    return true;
  }
  bool ProjectVertices() override {
    return VS::Render::ProjectLiveActorModel(*geometry_, actor_.bone_matrices);
  }
  void SetCulled(bool culled) override {
    actor_.instance.is_culled = culled ? 1 : 0;
  }
  void UpdateColors(ModelDrawRole role) override {
    UpdateActorModelColors(&actor_, geometry_, role, &workspace_->root_offset);
  }
  bool ActorHasUvLayout() const override {
    return actor_.instance.uv_layout != 0;
  }
  bool ModelOverridesForced() const override {
    return VS::Actors::forcedModelOverrides != 0;
  }
  ModelAppearance GeometryAppearance() const override {
    return AppearanceOf(*geometry_);
  }
  ModelAppearance ActorAppearance() const override {
    return AppearanceOf(actor_);
  }
  void DrawPolygons(ModelPolygons polygons,
                    const std::optional<ModelAppearance>& overrides) override {
    // 49EB8.c:362-407: the flat pass takes the geometry's base color, the
    // Gouraud pass the drawing actor's own.
    CVECTOR color = geometry_->instance.base_color;
    bool colored = false;
    switch (polygons) {
      case ModelPolygons::kFlat:
        break;
      case ModelPolygons::kGouraud:
        color = actor_.instance.base_color;
        colored = true;
        break;
      case ModelPolygons::kCompanionFlat:
        memcpy(&color, &actor_.companion_color, sizeof color);
        break;
    }
    if (overrides) {
      VS::Render::DrawLiveActorPolygonsWithOverrides(
          *geometry_, colored, color,
          {.pulse_mask = overrides->pulse_kind,
           .appearance_state = overrides->state});
    } else if (colored) {
      VS::Render::DrawLiveActorGouraudPolygons(*geometry_, color);
    } else {
      VS::Render::DrawLiveActorFlatPolygons(*geometry_, color);
    }
  }
  bool CastsShadow() const override {
    return actor_.instance.sound_enabled != 0;
  }
  void DrawShadow() override {
    VS::Render::DrawLiveActorGroundShadow(
        actor_, VS::Render::FrameScratchpad().view_matrix,
        VS::Render::ModelWorkspace()->root_offset);
  }

 private:
  VS::Actors::ActorRecord& actor_;
  MATRIX& camera_;
  VS::Actors::ActorRecord* geometry_ = nullptr;
  VS::Render::ModelScratchpad* workspace_ = nullptr;
};

class LiveBoneProjection final : public BoneProjectionHost {
 public:
  BoneSelection SelectBone(int slot, int bone) override {
    if (static_cast<unsigned>(slot) >= kActorSlotCount) return {};
    const VS::Actors::ActorRecord* actor = VS::Actors::actorRecords[slot];
    if (actor != nullptr) {
      // #263: the bone must lie within the pose matrices and the loaded bone
      // table, not only below the model's own bone count.
      if (bone < 0 || bone >= actor->instance.bone_count ||
          static_cast<unsigned>(bone) >= kPoseBoneCapacity ||
          (actor->instance.model != nullptr &&
           !ModelHoldsBones(*actor, static_cast<unsigned>(bone) + 1))) {
        return {BoneLookup::kInvalidBone, 0};
      }
      if (actor->instance.model == nullptr) return {};
      matrix_ = &actor->bone_matrices[bone];
      return {BoneLookup::kFound, actor->instance.model->bones[bone].length};
    }
    const VS::Field::GimmickInstance* object =
        VS::Field::FindGimmickForActorSlot(slot);
    if (object == nullptr) return {};
    if (bone < 0 || bone >= object->bone_count ||
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
    SetRotMatrix(matrix_);
    SetTransMatrix(matrix_);
    VS::Core::Gte::Word screen = 0;
    VS::Core::Gte::Word fog = 0;
    VS::Core::Gte::Word flags = 0;
    VECTOR transformed{};
    RotTransPers(&position, &screen, &fog, &flags);
    StoreLvnl(&transformed);
    return {static_cast<std::int32_t>(screen), transformed.vz};
  }

 private:
  const MATRIX* matrix_ = nullptr;
};
}  // namespace

bool BuildLiveActorModelPose(VS::Actors::ActorRecord& actor, MATRIX& camera) {
  return BuildActorModelPose(&actor, &camera) != 0;
}

ModelDrawResult DrawLiveActorModel(VS::Actors::ActorRecord& actor,
                                   MATRIX& camera, int companion) {
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
