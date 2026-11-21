// Hand-maintained (tools/port/owned.csv).
// Live binding for the room model pass and the model transforms
// (ModelRendering.hpp): the actor, weapon and gimmick tables, the model
// workspace, the skeleton pose, the texture slot tables and the ordering table.
#include "Actors/ModelRenderingLive.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/ActorMotionLive.hpp"
#include "Actors/ActorRecord.hpp"
#include "Actors/ActorTableLive.hpp"
#include "Actors/Animation.hpp"
#include "Actors/AnimationPoseLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/AppearanceLiveState.hpp"
#include "Actors/AppearanceTransitionLive.hpp"
#include "Actors/ModelDraw.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelInstance.hpp"
#include "Actors/ModelRendering.hpp"
#include "Actors/ModelRenderingLiveState.hpp"
#include "Actors/Movement.hpp"
#include "Actors/ShpModel.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Actors/SkeletonLiveState.hpp"
#include "Actors/SoundTransitionBinding.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Actors/Textures.hpp"
#include "Actors/WeaponModel.hpp"
#include "Content/ActorAssets.hpp"
#include "Content/ActorAssetsLive.hpp"
#include "Core/Diagnostics.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/GimmickDynamicsLive.hpp"
#include "Field/Gimmicks.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ModelDraw.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/ModelWorkspace.hpp"
#include "Render/Render.hpp"
#include "Render/Textures.hpp"
#include "Render/UiPrimitives.hpp"
#include "State/Timers.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SVECTOR;
using VS::Core::Gte::VECTOR;

// decomp: D_800F2458 (D_800F2458@BATTLE/BATTLE.PRG)
VS::Actors::ModelRenderContext VS::Actors::modelRenderContext;

namespace VS::Actors {
namespace {
using Core::Math::AnglesOf;
using Core::Math::MatrixOf;
using Core::Math::StoreTransform;
using Core::Math::StoreVector;
using Core::Math::Transform3;
using Core::Math::TransformOf;
using Core::Math::Vec3;
using Core::Math::VectorOf;

constexpr const char* kRoomPass = "UpdateAndDrawRoomActors";
constexpr const char* kGimmickDraw = "DrawGimmickModel";
constexpr const char* kWeaponDraw = "DrawActorWeaponModel";

static_assert(std::size(VS::Actors::actorRecords) == kRoomActorSlots);
static_assert(std::size(weaponModels) == kRoomWeaponSlots);
static_assert(std::size(VS::Field::gimmickInstances) == kRoomGimmickSlots);
static_assert(std::size(VS::Render::legacyWeaponTextureSlots) ==
              kWeaponTextureSlots);
static_assert(std::size(VS::Render::legacyActorTextureSlots) >=
              kInitializedActorTextureSlots);
static_assert(
    std::extent_v<decltype(VS::Actors::WeaponModel::bone_world_matrices)> ==
    kWeaponBoneLimit);
static_assert(std::extent_v<decltype(VS::Actors::WeaponModel::bones)> ==
              kWeaponBoneLimit);
static_assert(std::size(skeletonPose.bone_matrices) >= kWeaponBoneLimit);

// The root key is the last key of the blended pose.
using PoseKeys = decltype(VS::Actors::AnimationPose::bone_keys);
constexpr std::size_t kRootKey = std::extent_v<PoseKeys> - 1;
constexpr std::size_t kPlacementMatrix = 4;
constexpr std::size_t kModelMatrix = 0;
constexpr std::size_t kRootMatrix = 1;
static_assert(
    std::tuple_size_v<decltype(VS::Field::GimmickInstance::matrices)> >
    kPlacementMatrix);
// The holder's two tagged anchor bones, read as bone endpoints.
constexpr int kFirstAnchorTag = 0xF0;
constexpr int kSecondAnchorTag = 0xF1;
constexpr int kAnchorEndpoint = 1;
// Tick speed 4 is the slowed frame, where a frozen actor steps twice as far.
constexpr int kSlowedTickSpeed = 4;
constexpr int kEventPlaybackActive = 4;
// A gimmick model's height is its bone 1 length.
constexpr std::size_t kGimmickHeightBone = 1;

[[nodiscard]] VS::Render::ModelScratchpad& Workspace() {
  return *VS::Render::ModelWorkspace();
}

[[nodiscard]] VS::Actors::ActorRecord* RoomActor(std::size_t slot) {
  return LiveActorRecord(static_cast<int>(slot), kRoomPass);
}

[[nodiscard]] Vec3 InstanceScale(const VS::Actors::ModelInstance& instance) {
  return Vec3{instance.scale_x, instance.scale_y, instance.scale_z};
}

[[nodiscard]] Core::Math::Angles3 ActorAngles(
    const VS::Actors::ModelInstance& instance) {
  return Core::Math::Angles3{instance.rotation_x, instance.facing,
                             instance.rotation_z};
}

[[nodiscard]] RoomActorState StateOf(const VS::Actors::ActorRecord& actor) {
  const VS::Actors::ModelInstance& instance = actor.instance;
  const VS::Actors::CompanionOffsetRecord& companion = actor.companion_state;
  return {.has_bones = instance.bone_count != 0,
          .skipped = instance.is_skipped != 0,
          .visible = instance.is_visible != 0,
          .culled = instance.is_culled != 0,
          .frozen = instance.is_frozen != 0,
          .attached = instance.is_attached != 0,
          .pulsing = instance.palette_pulse.kind != 0,
          .transitioning = instance.action_animation_state != 0,
          .attach_bone = instance.attach_bone,
          .companion_slot = actor.companion_slot,
          .model_id = actor.model_id,
          .collision_radius = actor.collision_radius,
          .blend_frame = static_cast<std::uint8_t>(actor.blend_frame),
          .companion_offset =
              Vec3{companion.offset_x, companion.offset_y, companion.offset_z},
          .depth_bias = companion.depth_bias};
}

// The model's height: its bone 1 length (the int at model + 0x50), or 0 for a
// model no loader recorded that far.
[[nodiscard]] std::int32_t GimmickModelHeight(
    const VS::Field::GimmickInstance& object) {
  const std::span<const std::byte> model = Render::LiveModelBytes(object.model);
  constexpr std::size_t kOffset =
      sizeof(VS::Actors::ShpModel) +
      (kGimmickHeightBone * sizeof(VS::Actors::ShpBone));
  std::int32_t height = 0;
  if (model.size() < kOffset + sizeof height) return 0;
  std::memcpy(&height, model.data() + kOffset, sizeof height);
  return height;
}

class LiveRoomModelPass final : public RoomModelPassHost {
 public:
  explicit LiveRoomModelPass(MATRIX& camera) : camera_(&camera) {}

  void PrepareCamera() override {
    Render::PrepareLiveActorCameraMatrices(camera_);
  }

  [[nodiscard]] std::optional<RoomActorState> Actor(
      std::size_t slot) const override {
    const VS::Actors::ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return std::nullopt;
    return StateOf(*actor);
  }

  void UpdateTextureAnimations(std::size_t slot) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorTextureAnimations(actor);
    }
  }

  void UpdatePalettePulse(std::size_t slot) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorPalettePulse(*actor);
    }
  }

  void UpdateAppearanceTransition(std::size_t slot) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorAppearanceTransition(*actor);
    }
  }

  void SetDrawn(std::size_t slot, bool drawn) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      actor->instance.was_drawn = drawn ? 1 : 0;
    }
  }

  void UpdateSoundTransition(std::size_t slot) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      UpdateActorSoundTransition(*actor);
    }
  }

  void UpdateTextureScrolling(std::size_t slot,
                              std::size_t carried_index) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorTextureScrolling(actor, static_cast<int>(carried_index));
    }
  }

  // decomp: 44F14.c:960-971. The route may release the slot; the original
  // kept writing through its pointer.
  void StepFrozenAnimation(std::size_t slot) override {
    VS::Actors::ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return;
    if (actor->route_phase != 0) {
      UpdateLiveActorRoute(static_cast<int>(slot), actor);
      actor = RoomActor(slot);
      if (actor == nullptr) return;
    }
    if (VS::State::tickSpeed == kSlowedTickSpeed) {
      actor->frames_per_step =
          static_cast<std::uint8_t>(actor->frames_per_step * 2);
    }
    UpdateLiveActorAnimationBlend(actor);
    actor->frames_per_step =
        static_cast<std::uint8_t>(actor->fast_animation + 1);
  }

  void PrepareRootTransform(std::size_t slot) override {
    PrepareLiveActorRootTransform(RoomActor(slot));
  }

  void DrawAttached(std::size_t slot) override {
    VS::Actors::ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return;
    if (MATRIX* parent = ResolveLiveAttachmentParentMatrix(actor)) {
      DrawLiveActorModel(*actor, *parent, 0);
    }
  }

  void DrawActor(std::size_t slot, std::uint8_t companion) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      DrawLiveActorModel(*actor, *camera_, companion);
    }
  }

  void MoveActor(std::size_t slot, Vec3 offset) override {
    VS::Actors::ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return;
    const Vec3 position = VectorOf(actor->instance.position);
    StoreVector(actor->instance.position,
                Vec3{position.x + offset.x, position.y + offset.y,
                     position.z + offset.z});
  }

  void OffsetOrderingTable(std::int32_t entries) override {
    if (VS::Render::FrameScratchpad().ordering_table == nullptr) return;
    VS::Render::FrameScratchpad().ordering_table =
        static_cast<std::uint32_t*>(
            VS::Render::FrameScratchpad().ordering_table) +
        entries;
  }

  void UpdateMotionAndLookAt(std::size_t slot,
                             std::uint8_t companion) override {
    if (VS::Actors::ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveMotionAndLookAt(actor, companion);
    }
  }

  [[nodiscard]] std::optional<RoomWeaponState> Weapon(
      std::size_t index) const override {
    const VS::Actors::WeaponModel* weapon = weaponModels[index];
    if (weapon == nullptr) return std::nullopt;
    return RoomWeaponState{.shown = weapon->shown != 0,
                           .owner = weapon->actor_id,
                           .attachment_bone = weapon->attachment_bone};
  }

  // decomp: 44F14.c:1056. The attachment bone indexes the owner's bone
  // matrices, which the original did not bound.
  void DrawWeapon(std::size_t index) override {
    VS::Actors::WeaponModel* weapon = weaponModels[index];
    if (weapon == nullptr) return;
    VS::Actors::ActorRecord* owner = RoomActor(weapon->actor_id);
    if (owner == nullptr) return;
    if (weapon->attachment_bone >= std::size(owner->bone_matrices)) {
      VS::Core::Warnf("actors", "%s refused: weapon %zu attaches to bone %u",
                      kRoomPass, index,
                      static_cast<unsigned>(weapon->attachment_bone));
      return;
    }
    DrawLiveActorWeaponModel(weapon,
                             &owner->bone_matrices[weapon->attachment_bone]);
  }

  [[nodiscard]] std::optional<RoomGimmickState> Gimmick(
      std::size_t index) const override {
    const VS::Field::GimmickInstance* object =
        VS::Field::gimmickInstances[index];
    if (object == nullptr) return std::nullopt;
    return RoomGimmickState{.has_bones = object->bone_count != 0,
                            .pulsing = object->palette_pulse.kind != 0,
                            .motion_state = object->motion_state,
                            .mode = object->mode};
  }

  [[nodiscard]] PushedGimmick PushedGimmickState() const override {
    return PushedGimmick{
        .index = VS::Actors::pushedGimmickIndex,
        .update_complete =
            static_cast<std::uint8_t>(VS::Actors::isActorUpdateComplete) != 0};
  }

  void UpdateGimmickMotion(std::size_t index) override {
    VS::Field::GimmickInstance* object = VS::Field::gimmickInstances[index];
    if (object == nullptr) return;
    static_cast<void>(VS::Field::UpdateLiveGimmickMotion(object));
  }

  // decomp: 44F14.c:1108-1113.
  void CenterGimmick(std::size_t index) override {
    const VS::Field::GimmickInstance* object =
        VS::Field::gimmickInstances[index];
    if (object == nullptr) return;
    StoreVector(Workspace().root_offset,
                GimmickCenter(Vec3{object->position_x, object->position_y,
                                   object->position_z},
                              GimmickModelHeight(*object)));
  }

  void UpdateGimmickPalettePulse(std::size_t index) override {
    if (VS::Field::GimmickInstance* object =
            VS::Field::gimmickInstances[index]) {
      UpdateLiveGimmickPalettePulse(*object);
    }
  }

  void UpdateGimmickAppearanceTransition(std::size_t index) override {
    if (VS::Field::GimmickInstance* object =
            VS::Field::gimmickInstances[index]) {
      UpdateLiveGimmickAppearanceTransition(*object);
    }
  }

  void DrawGimmick(std::size_t index) override {
    if (VS::Field::GimmickInstance* object =
            VS::Field::gimmickInstances[index]) {
      DrawLiveGimmickModel(object, camera_);
    }
  }

  void ClearGimmickPushUpdate() override {
    VS::Actors::isActorUpdateComplete = 0;
  }

 private:
  MATRIX* camera_;
};

// What DrawLiveGimmickModel reads of the gimmick, or none when a held
// gimmick's holder slot is empty.
[[nodiscard]] std::optional<GimmickPose> LiveGimmickPose(
    const VS::Field::GimmickInstance& object, GimmickPlacement placement) {
  GimmickPose pose;
  pose.rotation = Core::Math::Angles3{object.rotation_x, object.rotation_y,
                                      object.rotation_z};
  pose.rotation_offset = AnglesOf(object.rotation_offset);
  pose.placement = MatrixOf(object.matrices[kPlacementMatrix]);
  pose.scale = Vec3{object.scale_x, object.scale_y, object.scale_z};
  if (placement != GimmickPlacement::kHeld) return pose;
  const VS::Actors::ActorRecord* holder =
      LiveActorRecord(object.parent, kGimmickDraw);
  if (holder == nullptr) {
    VS::Core::Warnf("actors",
                    "%s refused: gimmick %u's holder slot %u is empty",
                    kGimmickDraw, static_cast<unsigned>(object.slot),
                    static_cast<unsigned>(object.parent));
    return std::nullopt;
  }
  // A tag the holder lacks leaves its anchor at the origin.
  std::array<SVECTOR, 2> anchors{};
  LiveActorTaggedBonePosition(object.parent, kFirstAnchorTag, &anchors[0],
                              kAnchorEndpoint);
  LiveActorTaggedBonePosition(object.parent, kSecondAnchorTag, &anchors[1],
                              kAnchorEndpoint);
  pose.anchors = {VectorOf(anchors[0]), VectorOf(anchors[1])};
  pose.holder_rotation = ActorAngles(holder->instance);
  return pose;
}

// The SHP bone table of a weapon model, bounded by the bytes its loader
// recorded; none when the table does not fit.
[[nodiscard]] std::optional<std::array<WeaponBone, kWeaponBoneLimit>>
LiveWeaponBones(const VS::Actors::WeaponModel& weapon, std::size_t bone_count) {
  std::array<WeaponBone, kWeaponBoneLimit> bones{};
  // The original read the bone table only for bones past the root.
  if (bone_count < 2) return bones;
  const std::span<const std::byte> model =
      Render::LiveModelBytes(weapon.section_offsets);
  if (model.size() < sizeof(VS::Actors::ShpModel) +
                         (bone_count * sizeof(VS::Actors::ShpBone))) {
    return std::nullopt;
  }
  for (std::size_t bone = 0; bone < bone_count; ++bone) {
    VS::Actors::ShpBone stored;
    std::memcpy(&stored,
                model.data() + sizeof(VS::Actors::ShpModel) +
                    (bone * sizeof(VS::Actors::ShpBone)),
                sizeof stored);
    bones[bone] = WeaponBone{.parent = stored.parent_index,
                             .length = stored.length,
                             .rotation = AnglesOf(weapon.bones[bone])};
  }
  return bones;
}

void StoreWeaponPose(VS::Actors::WeaponModel& weapon,
                     const WeaponFadeStep& fade,
                     std::span<const Transform3> chain,
                     std::span<const WeaponBone> bones) {
  VS::Render::ModelScratchpad& workspace = Workspace();
  weapon.blend_level = fade.fade.scale;
  weapon.blend_frames = fade.fade.frames;
  if (fade.hidden) weapon.shown = 0;
  if (const std::optional<Vec3> columns = ModelColumnScale(
          Vec3{fade.fade.scale, fade.fade.scale, fade.fade.scale})) {
    StoreVector(workspace.scale, *columns);
  }
  workspace.position.vx = 0;
  workspace.position.vy = 0;
  for (std::size_t bone = 0; bone < chain.size(); ++bone) {
    StoreTransform(skeletonPose.bone_matrices[bone], chain[bone]);
    StoreTransform(weapon.bone_world_matrices[bone], chain[bone]);
  }
  StoreTransform(workspace.matrix, chain.back());
  if (bones.size() < 2) return;
  // The workspace keeps the last bone's offset and angles.
  const WeaponBone& last = bones.back();
  StoreVector(
      workspace.position,
      Vec3{Core::Math::WrappingNegate(bones[last.parent].length), 0, 0});
  constexpr std::int32_t kAngleMask = 0xFFF;
  StoreVector(workspace.rotation,
              Vec3{last.rotation.x & kAngleMask, last.rotation.y & kAngleMask,
                   last.rotation.z & kAngleMask});
  workspace.rotation.pad = 0;
}

// decomp: 49268.c:203-245, after the bone chain.
void DrawWeaponForOwner(const VS::Actors::WeaponModel& weapon,
                        const VS::Actors::ActorRecord& owner) {
  const VS::Actors::ModelInstance& instance = owner.instance;
  const bool visible = instance.is_visible != 0;
  const std::int32_t height = instance.position.vy;
  const bool leader = instance.slot == 0;
  const bool event_playing =
      visible && height >= kHiddenActorHeight && !leader &&
      static_cast<std::int16_t>(VS::Event::EventPlaybackState(
          VS::Event::LiveScriptVm())) == kEventPlaybackActive;
  if (!OwnerShowsWeapon(visible, height, leader, event_playing)) return;
  if (!Render::ProjectLiveWeaponModel(weapon)) return;
  if (weapon.texture_slot >= kWeaponTextureSlots ||
      weapon.palette_slot_offset >= kWeaponTextureSlots) {
    VS::Core::Warnf(
        "actors",
        "%s refused: texture slot %u or palette slot %u is outside the "
        "weapon slot table",
        kWeaponDraw, static_cast<unsigned>(weapon.texture_slot),
        static_cast<unsigned>(weapon.palette_slot_offset));
    return;
  }
  VS::Render::ModelScratchpad& workspace = Workspace();
  workspace.texture_page = static_cast<std::uint16_t>(
      VS::Render::legacyWeaponTextureSlots[weapon.texture_slot].texture_page);
  workspace.palette = static_cast<std::uint16_t>(
      VS::Render::legacyWeaponTextureSlots[weapon.palette_slot_offset].palette);
  CVECTOR color = instance.base_color;
  if (weapon.brightness_override != 0) {
    color = CVECTOR{weapon.brightness, weapon.brightness, weapon.brightness, 0};
  }
  if (WeaponDrawsWithOverrides(instance.action_animation_state)) {
    Render::DrawLiveWeaponFlatPolygonsWithOverrides(weapon, color);
  } else {
    Render::DrawLiveWeaponFlatPolygons(weapon, color);
  }
}

[[nodiscard]] std::int16_t SlotOriginHalfword(std::size_t slot,
                                              std::size_t half) {
  constexpr std::size_t kSlotOriginBytes = 4;
  constexpr unsigned kByteBits = 8;
  const std::size_t byte = (slot * kSlotOriginBytes) + (half * 2);
  return static_cast<std::int16_t>(
      VS::Actors::textureSlotOrigins[byte] |
      (VS::Actors::textureSlotOrigins[byte + 1] << kByteBits));
}

void StoreSlotIds(VS::Render::TextureSlot& slot, const TextureSlotIds& ids) {
  slot.texture_page = static_cast<std::int16_t>(ids.texture_page);
  slot.palette = static_cast<std::int16_t>(ids.palette);
  slot.reference_count = 0;
}
}  // namespace

// decomp: BATTLE.PRG func_800AEAE8 (src/BATTLE/BATTLE.PRG/44F14.c:826)
void PrepareLiveActorRootTransform(VS::Actors::ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("PrepareActorRootTransform", "no actor record");
    return;
  }
  std::fill(std::begin(modelRenderContext.model_scale),
            std::end(modelRenderContext.model_scale), kUnitModelScale);
  VS::Render::ModelScratchpad& workspace = Workspace();
  StoreVector(workspace.root_offset,
              ActorRootOffset(VectorOf(actor->blend_pose.bone_keys[kRootKey]),
                              InstanceScale(actor->instance), actor->scale));
  TransformLiveActorRootOffset(actor);
  workspace.angle_x = actor->instance.look_yaw;
  workspace.angle_y = actor->instance.look_pitch;
}

// decomp: BATTLE.PRG func_800AEF94 (src/BATTLE/BATTLE.PRG/44F14.c:923)
void UpdateAndDrawLiveRoomActors(MATRIX* camera) {
  if (camera == nullptr) {
    WarnActorRefused(kRoomPass, "no camera matrix");
    return;
  }
  LiveRoomModelPass host(*camera);
  UpdateAndDrawRoomModels(host);
}

// decomp: BATTLE.PRG func_800B07DC (src/BATTLE/BATTLE.PRG/44F14.c:1695)
void TransformLiveActorRootOffset(VS::Actors::ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("TransformActorRootOffset", "no actor record");
    return;
  }
  VS::Render::ModelScratchpad& workspace = Workspace();
  const Core::Math::Angles3 angles = ActorAngles(actor->instance);
  const RootTransform root =
      TransformRootOffset(angles, VectorOf(workspace.root_offset),
                          VectorOf(actor->instance.position));
  StoreVector(workspace.rotation, Vec3{angles.x, angles.y, angles.z});
  StoreTransform(workspace.matrix, root.matrix);
  StoreVector(workspace.root_offset, root.root_offset);
}

// decomp: BATTLE.PRG func_800B1A68 (src/BATTLE/BATTLE.PRG/49268.c:48)
void DrawLiveGimmickModel(VS::Field::GimmickInstance* object, MATRIX* camera) {
  if (object == nullptr || camera == nullptr) {
    WarnActorRefused(kGimmickDraw, "no gimmick or camera matrix");
    return;
  }
  const GimmickPlacement placement = ClassifyGimmickPlacement(
      object->object_info.object_id, object->parent, object->mode);
  const std::optional<GimmickPose> pose = LiveGimmickPose(*object, placement);
  if (!pose) return;
  VS::Render::ModelScratchpad& workspace = Workspace();
  const GimmickTransform placed =
      PlaceGimmick(placement, *pose, TransformOf(*camera),
                   VectorOf(workspace.root_offset), VectorOf(roomActorScale));
  workspace.model = object->model;
  StoreVector(workspace.position, placed.position);
  if (placed.rotation) {
    StoreVector(workspace.rotation, *placed.rotation);
    // The free placement's masked word stores cleared the padding.
    if (placement == GimmickPlacement::kFree) workspace.rotation.pad = 0;
  }
  if (placed.scale) StoreVector(workspace.scale, *placed.scale);
  StoreTransform(workspace.matrix, placed.model);
  StoreTransform(skeletonPose.bone_matrices[0], placed.model);
  StoreTransform(object->matrices[kModelMatrix], placed.model);
  StoreTransform(skeletonRootMatrix, placed.model);
  StoreTransform(object->matrices[kRootMatrix], placed.model);
  if (object->is_visible == 0 || !Render::ProjectLiveGimmickModel(*object)) {
    return;
  }
  workspace.texture_page = GimmickTexturePage();
  workspace.palette = GimmickPalette(
      object->slot, object->palette_pulse.kind != 0, object->motion_state);
  if (GimmickDrawsWithOverrides(object->motion_state)) {
    Render::DrawLiveGimmickGouraudPolygonsWithOverrides(*object);
  } else {
    Render::DrawLiveGimmickGouraudPolygons(*object);
  }
}

// decomp: BATTLE.PRG func_800B217C (src/BATTLE/BATTLE.PRG/49268.c:155). The
// original also left the weapon's model and each parent's bone matrix in the
// model workspace; only this function read them.
void DrawLiveActorWeaponModel(VS::Actors::WeaponModel* weapon, MATRIX* source) {
  if (weapon == nullptr || source == nullptr) {
    WarnActorRefused(kWeaponDraw, "no weapon or attachment matrix");
    return;
  }
  const std::size_t bone_count = weapon->bone_count;
  if (bone_count > kWeaponBoneLimit) {
    VS::Core::Warnf("actors", "%s refused: the weapon has %zu bones",
                    kWeaponDraw, bone_count);
    return;
  }
  const std::optional<std::array<WeaponBone, kWeaponBoneLimit>> bones =
      LiveWeaponBones(*weapon, bone_count);
  if (!bones) {
    WarnActorRefused(kWeaponDraw, "the weapon model cannot hold its bones");
    return;
  }
  const std::span<const WeaponBone> used = std::span(*bones).first(bone_count);
  const WeaponFadeStep fade = StepWeaponFade(WeaponFade{
      weapon->blend_level, weapon->blend_target, weapon->blend_frames});
  const std::optional<std::array<Transform3, kWeaponBoneLimit>> chain =
      BuildWeaponBoneChain(
          WeaponRootTransform(TransformOf(*source), fade.fade.scale), used);
  if (!chain) {
    WarnActorRefused(kWeaponDraw, "a weapon bone's parent does not precede it");
    return;
  }
  StoreWeaponPose(*weapon, fade,
                  std::span(*chain).first(std::max<std::size_t>(bone_count, 1)),
                  used);
  const VS::Actors::ActorRecord* owner =
      LiveActorRecord(weapon->actor_id, kWeaponDraw);
  if (owner == nullptr) {
    WarnActorRefused(kWeaponDraw, "the weapon's owner slot is empty");
    return;
  }
  DrawWeaponForOwner(*weapon, *owner);
}

// decomp: INITBTL.PRG func_800FA7B0 (src/BATTLE/INITBTL.PRG/FB0.c:19)
void InitializeLiveActorModelResources() {
  VS::Actors::forcedModelOverrides = 0;
  VS::Content::ClearLiveActorAssetQueue();
  for (std::size_t slot = 0; slot < kInitializedActorTextureSlots; ++slot) {
    const TextureSlotIds ids = ActorTextureSlotIds(
        slot, SlotOriginHalfword(slot, 0), SlotOriginHalfword(slot, 1));
    VS::Render::TextureSlot& stored = VS::Render::legacyActorTextureSlots[slot];
    StoreSlotIds(stored, ids);
    stored.alternate_palette = static_cast<std::int16_t>(ids.alternate_palette);
  }
  std::fill(std::begin(VS::Actors::actorRecords),
            std::end(VS::Actors::actorRecords), nullptr);
  VS::Actors::primarySequenceBuffer =
      Core::Memory::AllocateLiveFromLowEnd(Content::kWeaponSequenceBytes);
  VS::Actors::sequenceWorkBuffer =
      Core::Memory::AllocateLiveFromLowEnd(Content::kSequenceWorkBufferBytes);
  for (std::size_t slot = 0; slot < kWeaponTextureSlots; ++slot) {
    StoreSlotIds(VS::Render::legacyWeaponTextureSlots[slot],
                 WeaponTextureSlotIds(slot));
  }
  std::fill(std::begin(weaponModels), std::end(weaponModels), nullptr);
  for (std::size_t buffer = 0; buffer < 2; ++buffer) {
    VS::Content::legacyWeaponModelBuffers[buffer] =
        static_cast<VS::Actors::WeaponModelData*>(
            Core::Memory::AllocateLiveFromLowEnd(
                sizeof(VS::Actors::WeaponModelData)));
  }
  std::fill(std::begin(VS::Field::gimmickInstances),
            std::end(VS::Field::gimmickInstances), nullptr);
  constexpr std::size_t kClearedBankPages = 3;
  constexpr std::size_t kClearedSequenceBanks = 3;
  std::fill_n(std::begin(VS::Actors::sharedSequenceBankTexturePages),
              kClearedBankPages, std::uint8_t{0});
  std::fill_n(std::begin(VS::Actors::sharedSequenceData), kClearedSequenceBanks,
              nullptr);
  VS::Content::legacySequenceParsePhase = 0;
  VS::Content::legacyEtmLoadPhase = 0;
  VS::Content::modelParseResetFlag = 0;
  VS::Content::objectDataLoadStep = 0;
  VS::Content::legacyModelLoadPhase = 0;
  VS::Actors::sharedSequenceBankIndex = 0;
  VECTOR unit_room_scale{};
  unit_room_scale.vx = kUnitActorScale;
  unit_room_scale.vy = kUnitActorScale;
  unit_room_scale.vz = kUnitActorScale;
  SetLiveRoomActorScale(&unit_room_scale);
  VS::Actors::pushedGimmickIndex = 0;
  VS::Actors::isActorUpdateComplete = 0;
}
}  // namespace VS::Actors
