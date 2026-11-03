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

#include "Actors/ActorTableLive.hpp"
#include "Actors/AppearanceLive.hpp"
#include "Actors/AppearanceTransitionLive.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRendering.hpp"
#include "Actors/ModelRenderingLiveState.h"
#include "Actors/SoundTransitionBinding.hpp"
#include "Actors/TextureAnimationLive.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ModelGeometryLive.hpp"

extern "C" {
#include "ActorAssets.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/AppearanceLiveState.h"
#include "Actors/ModelDraw.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/Textures.h"
#include "Actors/WeaponModel.h"
#include "Core/Diagnostics.h"
#include "Event/ScriptVm.h"
#include "Field/Gimmicks.h"
#include "Menu/Inventory/Inventory.h"
#include "ModelLoading.h"
#include "Render/ModelDraw.h"
#include "Render/ModelWorkspace.h"
#include "Render/Render.h"
#include "Render/Textures.h"
#include "Render/UiPrimitives.h"
#include "State/Timers.h"

// decomp: D_800E9308 (D_800E9308@BATTLE/BATTLE.PRG)
int Actors_renderPassCounter;

// decomp: D_800F2458 (D_800F2458@BATTLE/BATTLE.PRG)
Actors_ModelRenderContext Actors_modelRenderContext;
}

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

static_assert(std::size(Actors_actorRecords) == kRoomActorSlots);
static_assert(std::size(Actors_weaponModels) == kRoomWeaponSlots);
static_assert(std::size(Field_gimmickInstances) == kRoomGimmickSlots);
static_assert(std::size(Render_legacyWeaponTextureSlots) ==
              kWeaponTextureSlots);
static_assert(std::size(Render_legacyActorTextureSlots) >=
              kInitializedActorTextureSlots);
static_assert(std::extent_v<decltype(Actors_WeaponModel::boneWorldMatrices)> ==
              kWeaponBoneLimit);
static_assert(std::extent_v<decltype(Actors_WeaponModel::bones)> ==
              kWeaponBoneLimit);
static_assert(std::size(Actors_skeletonPose.boneMatrices) >= kWeaponBoneLimit);

// The root key is the last key of the blended pose.
using PoseKeys = decltype(Actors_AnimationPose::boneKeys);
constexpr std::size_t kRootKey = std::extent_v<PoseKeys> - 1;
constexpr std::size_t kPlacementMatrix = 4;
constexpr std::size_t kModelMatrix = 0;
constexpr std::size_t kRootMatrix = 1;
static_assert(std::extent_v<decltype(Field_GimmickInstance::matrices)> >
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

static_assert(sizeof(u_long) == sizeof(std::uint32_t),
              "ordering-table entries are 4-byte words");

[[nodiscard]] Render_ModelScratchpad& Workspace() {
  return *Render_ModelWorkspace();
}

[[nodiscard]] Actors_ActorRecord* RoomActor(std::size_t slot) {
  return LiveActorRecord(static_cast<int>(slot), kRoomPass);
}

[[nodiscard]] Vec3 InstanceScale(const Actors_ModelInstance& instance) {
  return Vec3{instance.scaleX, instance.scaleY, instance.scaleZ};
}

[[nodiscard]] Core::Math::Angles3 ActorAngles(
    const Actors_ModelInstance& instance) {
  return Core::Math::Angles3{instance.rotationX, instance.facing,
                             instance.rotationZ};
}

[[nodiscard]] RoomActorState StateOf(const Actors_ActorRecord& actor) {
  const Actors_ModelInstance& instance = actor.instance;
  const Actors_CompanionOffset& companion = actor.companionState;
  return {.has_bones = instance.boneCount != 0,
          .skipped = instance.isSkipped != 0,
          .visible = instance.isVisible != 0,
          .culled = instance.isPreviewModel != 0,
          .frozen = instance.isFrozen != 0,
          .attached = instance.isAttached != 0,
          .pulsing = instance.palettePulse.kind != 0,
          .transitioning = instance.actionAnimationState != 0,
          .attach_bone = instance.attachBone,
          .companion_slot = actor.companionSlot,
          .model_id = actor.modelId,
          .collision_radius = actor.collisionRadius,
          .blend_frame = static_cast<std::uint8_t>(actor.blendFrame),
          .companion_offset =
              Vec3{companion.offsetX, companion.offsetY, companion.offsetZ},
          .depth_bias = companion.depthBias};
}

// The model's height: its bone 1 length (the int at model + 0x50), or 0 for a
// model no loader recorded that far.
[[nodiscard]] std::int32_t GimmickModelHeight(
    const Field_GimmickInstance& object) {
  const std::span<const std::byte> model = Render::LiveModelBytes(object.model);
  constexpr std::size_t kOffset =
      sizeof(Actors_ShpModel) + (kGimmickHeightBone * sizeof(Actors_ShpBone));
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
    const Actors_ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return std::nullopt;
    return StateOf(*actor);
  }

  void UpdateTextureAnimations(std::size_t slot) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorTextureAnimations(actor);
    }
  }

  void UpdatePalettePulse(std::size_t slot) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorPalettePulse(*actor);
    }
  }

  void UpdateAppearanceTransition(std::size_t slot) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorAppearanceTransition(*actor);
    }
  }

  void SetDrawn(std::size_t slot, bool drawn) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      actor->instance.wasDrawn = drawn ? 1 : 0;
    }
  }

  void UpdateSoundTransition(std::size_t slot) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      UpdateActorSoundTransition(*actor);
    }
  }

  void UpdateTextureScrolling(std::size_t slot,
                              std::size_t carried_index) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      UpdateLiveActorTextureScrolling(actor, static_cast<int>(carried_index));
    }
  }

  // decomp: 44F14.c:960-971. The route may release the slot; the original
  // kept writing through its pointer.
  void StepFrozenAnimation(std::size_t slot) override {
    Actors_ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return;
    if (actor->routePhase != 0) {
      Actors_UpdateActorRoute(static_cast<int>(slot), actor);
      actor = RoomActor(slot);
      if (actor == nullptr) return;
    }
    if (State_tickSpeed == kSlowedTickSpeed) {
      actor->framesPerStep = static_cast<u_char>(actor->framesPerStep * 2);
    }
    Actors_UpdateActorAnimationBlend(actor);
    actor->framesPerStep = static_cast<u_char>(actor->fastAnimation + 1);
  }

  void PrepareRootTransform(std::size_t slot) override {
    PrepareLiveActorRootTransform(RoomActor(slot));
  }

  void DrawAttached(std::size_t slot) override {
    Actors_ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return;
    if (MATRIX* parent = ResolveLiveAttachmentParentMatrix(actor)) {
      DrawLiveActorModel(*actor, *parent, 0);
    }
  }

  void DrawActor(std::size_t slot, std::uint8_t companion) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      DrawLiveActorModel(*actor, *camera_, companion);
    }
  }

  void MoveActor(std::size_t slot, Vec3 offset) override {
    Actors_ActorRecord* actor = RoomActor(slot);
    if (actor == nullptr) return;
    const Vec3 position = VectorOf(actor->instance.position);
    StoreVector(actor->instance.position,
                Vec3{position.x + offset.x, position.y + offset.y,
                     position.z + offset.z});
  }

  void OffsetOrderingTable(std::int32_t entries) override {
    if (Render_frameScratch.orderingTable == nullptr) return;
    Render_frameScratch.orderingTable =
        static_cast<u_long*>(Render_frameScratch.orderingTable) + entries;
  }

  void UpdateMotionAndLookAt(std::size_t slot,
                             std::uint8_t companion) override {
    if (Actors_ActorRecord* actor = RoomActor(slot)) {
      Actors_UpdateMotionAndLookAt(actor, companion);
    }
  }

  [[nodiscard]] std::optional<RoomWeaponState> Weapon(
      std::size_t index) const override {
    const Actors_WeaponModel* weapon = Actors_weaponModels[index];
    if (weapon == nullptr) return std::nullopt;
    return RoomWeaponState{.shown = weapon->reserved08Bit4 != 0,
                           .owner = weapon->actorId,
                           .attachment_bone = weapon->attachmentBone};
  }

  // decomp: 44F14.c:1056. The attachment bone indexes the owner's bone
  // matrices, which the original did not bound.
  void DrawWeapon(std::size_t index) override {
    Actors_WeaponModel* weapon = Actors_weaponModels[index];
    if (weapon == nullptr) return;
    Actors_ActorRecord* owner = RoomActor(weapon->actorId);
    if (owner == nullptr) return;
    if (weapon->attachmentBone >= std::size(owner->boneMatrices)) {
      Core_Warnf("actors", "%s refused: weapon %zu attaches to bone %u",
                 kRoomPass, index,
                 static_cast<unsigned>(weapon->attachmentBone));
      return;
    }
    DrawLiveActorWeaponModel(weapon,
                             &owner->boneMatrices[weapon->attachmentBone]);
  }

  [[nodiscard]] std::optional<RoomGimmickState> Gimmick(
      std::size_t index) const override {
    const Field_GimmickInstance* object = Field_gimmickInstances[index];
    if (object == nullptr) return std::nullopt;
    return RoomGimmickState{.has_bones = object->boneCount != 0,
                            .pulsing = object->palettePulse.kind != 0,
                            .motion_state = object->motionState,
                            .mode = object->mode};
  }

  [[nodiscard]] PushedGimmick PushedGimmickState() const override {
    return PushedGimmick{
        .index = Actors_pushedGimmickIndex,
        .update_complete =
            static_cast<std::uint8_t>(Actors_isActorUpdateComplete) != 0};
  }

  void UpdateGimmickMotion(std::size_t index) override {
    Field_GimmickInstance* object = Field_gimmickInstances[index];
    if (object == nullptr) return;
    // Field_UpdateGimmickMotion (unpromoted) still takes the decomp's
    // objectAnimationState view of the gimmick record; its own promotion
    // retires the view.
    Field_UpdateGimmickMotion(
        reinterpret_cast<Field_GimmickPhysicsView*>(object),
        State_tickSpeed / 2);
  }

  // decomp: 44F14.c:1108-1113.
  void CenterGimmick(std::size_t index) override {
    const Field_GimmickInstance* object = Field_gimmickInstances[index];
    if (object == nullptr) return;
    StoreVector(Workspace().rootOffset,
                GimmickCenter(Vec3{object->positionX, object->positionY,
                                   object->positionZ},
                              GimmickModelHeight(*object)));
  }

  void UpdateGimmickPalettePulse(std::size_t index) override {
    if (Field_GimmickInstance* object = Field_gimmickInstances[index]) {
      UpdateLiveGimmickPalettePulse(*object);
    }
  }

  void UpdateGimmickAppearanceTransition(std::size_t index) override {
    if (Field_GimmickInstance* object = Field_gimmickInstances[index]) {
      UpdateLiveGimmickAppearanceTransition(*object);
    }
  }

  void DrawGimmick(std::size_t index) override {
    if (Field_GimmickInstance* object = Field_gimmickInstances[index]) {
      DrawLiveGimmickModel(object, camera_);
    }
  }

  void ClearGimmickPushUpdate() override { Actors_isActorUpdateComplete = 0; }

  void CountPass() override { ++Actors_renderPassCounter; }

 private:
  MATRIX* camera_;
};

// What DrawLiveGimmickModel reads of the gimmick, or none when a held
// gimmick's holder slot is empty.
[[nodiscard]] std::optional<GimmickPose> LiveGimmickPose(
    const Field_GimmickInstance& object, GimmickPlacement placement) {
  GimmickPose pose;
  pose.rotation =
      Core::Math::Angles3{object.rotationX, object.rotationY, object.rotationZ};
  pose.rotation_offset = AnglesOf(object.rotationOffset);
  pose.placement = MatrixOf(object.matrices[kPlacementMatrix]);
  pose.scale = Vec3{object.scaleX, object.scaleY, object.scaleZ};
  if (placement != GimmickPlacement::kHeld) return pose;
  const Actors_ActorRecord* holder =
      LiveActorRecord(object.parent, kGimmickDraw);
  if (holder == nullptr) {
    Core_Warnf("actors", "%s refused: gimmick %u's holder slot %u is empty",
               kGimmickDraw, static_cast<unsigned>(object.slot),
               static_cast<unsigned>(object.parent));
    return std::nullopt;
  }
  // A tag the holder lacks leaves its anchor at the origin.
  std::array<SVECTOR, 2> anchors{};
  Actors_ActorTaggedBonePosition(object.parent, kFirstAnchorTag, &anchors[0],
                                 kAnchorEndpoint);
  Actors_ActorTaggedBonePosition(object.parent, kSecondAnchorTag, &anchors[1],
                                 kAnchorEndpoint);
  pose.anchors = {VectorOf(anchors[0]), VectorOf(anchors[1])};
  pose.holder_rotation = ActorAngles(holder->instance);
  return pose;
}

// The SHP bone table of a weapon model, bounded by the bytes its loader
// recorded; none when the table does not fit.
[[nodiscard]] std::optional<std::array<WeaponBone, kWeaponBoneLimit>>
LiveWeaponBones(const Actors_WeaponModel& weapon, std::size_t bone_count) {
  std::array<WeaponBone, kWeaponBoneLimit> bones{};
  // The original read the bone table only for bones past the root.
  if (bone_count < 2) return bones;
  const std::span<const std::byte> model =
      Render::LiveModelBytes(weapon.sectionOffsets);
  if (model.size() <
      sizeof(Actors_ShpModel) + (bone_count * sizeof(Actors_ShpBone))) {
    return std::nullopt;
  }
  for (std::size_t bone = 0; bone < bone_count; ++bone) {
    Actors_ShpBone stored;
    std::memcpy(&stored,
                model.data() + sizeof(Actors_ShpModel) +
                    (bone * sizeof(Actors_ShpBone)),
                sizeof stored);
    bones[bone] = WeaponBone{.parent = stored.parentIndex,
                             .length = stored.length,
                             .rotation = AnglesOf(weapon.bones[bone])};
  }
  return bones;
}

void StoreWeaponPose(Actors_WeaponModel& weapon, const WeaponFadeStep& fade,
                     std::span<const Transform3> chain,
                     std::span<const WeaponBone> bones) {
  Render_ModelScratchpad& workspace = Workspace();
  weapon.reserved11 = fade.fade.scale;
  weapon.reserved13 = fade.fade.frames;
  if (fade.hidden) weapon.reserved08Bit4 = 0;
  if (const std::optional<Vec3> columns = ModelColumnScale(
          Vec3{fade.fade.scale, fade.fade.scale, fade.fade.scale})) {
    StoreVector(workspace.scale, *columns);
  }
  workspace.position.vx = 0;
  workspace.position.vy = 0;
  for (std::size_t bone = 0; bone < chain.size(); ++bone) {
    StoreTransform(Actors_skeletonPose.boneMatrices[bone], chain[bone]);
    StoreTransform(weapon.boneWorldMatrices[bone], chain[bone]);
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
void DrawWeaponForOwner(const Actors_WeaponModel& weapon,
                        const Actors_ActorRecord& owner) {
  const Actors_ModelInstance& instance = owner.instance;
  const bool visible = instance.isVisible != 0;
  const std::int32_t height = instance.position.vy;
  const bool leader = instance.slot == 0;
  const bool event_playing =
      visible && height >= kHiddenActorHeight && !leader &&
      static_cast<std::int16_t>(Event_EventPlaybackState()) ==
          kEventPlaybackActive;
  if (!OwnerShowsWeapon(visible, height, leader, event_playing)) return;
  if (!Render::ProjectLiveWeaponModel(weapon)) return;
  if (weapon.textureSlot >= kWeaponTextureSlots ||
      weapon.paletteSlotOffset >= kWeaponTextureSlots) {
    Core_Warnf("actors",
               "%s refused: texture slot %u or palette slot %u is outside the "
               "weapon slot table",
               kWeaponDraw, static_cast<unsigned>(weapon.textureSlot),
               static_cast<unsigned>(weapon.paletteSlotOffset));
    return;
  }
  Render_ModelScratchpad& workspace = Workspace();
  workspace.texturePage = static_cast<u_short>(
      Render_legacyWeaponTextureSlots[weapon.textureSlot].texturePage);
  workspace.palette = static_cast<u_short>(
      Render_legacyWeaponTextureSlots[weapon.paletteSlotOffset].palette);
  CVECTOR color = instance.baseColor;
  if (weapon.reserved08Bit6 != 0) {
    color =
        CVECTOR{weapon.reserved5C0, weapon.reserved5C0, weapon.reserved5C0, 0};
  }
  if (WeaponDrawsWithOverrides(instance.actionAnimationState)) {
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
      Actors_textureSlotOrigins[byte] |
      (Actors_textureSlotOrigins[byte + 1] << kByteBits));
}

void StoreSlotIds(Render_TextureSlot& slot, const TextureSlotIds& ids) {
  slot.texturePage = static_cast<std::int16_t>(ids.texture_page);
  slot.palette = static_cast<std::int16_t>(ids.palette);
  slot.referenceCount = 0;
}
}  // namespace

// decomp: BATTLE.PRG func_800AEAE8 (src/BATTLE/BATTLE.PRG/44F14.c:826)
void PrepareLiveActorRootTransform(Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("PrepareActorRootTransform", "no actor record");
    return;
  }
  std::fill(std::begin(Actors_modelRenderContext.modelScale),
            std::end(Actors_modelRenderContext.modelScale), kUnitModelScale);
  Render_ModelScratchpad& workspace = Workspace();
  StoreVector(workspace.rootOffset,
              ActorRootOffset(VectorOf(actor->blendPose.boneKeys[kRootKey]),
                              InstanceScale(actor->instance), actor->scale));
  TransformLiveActorRootOffset(actor);
  workspace.angleX = actor->instance.lookYaw;
  workspace.angleY = actor->instance.lookPitch;
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
void TransformLiveActorRootOffset(Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("TransformActorRootOffset", "no actor record");
    return;
  }
  Render_ModelScratchpad& workspace = Workspace();
  const Core::Math::Angles3 angles = ActorAngles(actor->instance);
  const RootTransform root =
      TransformRootOffset(angles, VectorOf(workspace.rootOffset),
                          VectorOf(actor->instance.position));
  StoreVector(workspace.rotation, Vec3{angles.x, angles.y, angles.z});
  StoreTransform(workspace.matrix, root.matrix);
  StoreVector(workspace.rootOffset, root.root_offset);
}

// decomp: BATTLE.PRG func_800B1A68 (src/BATTLE/BATTLE.PRG/49268.c:48)
void DrawLiveGimmickModel(Field_GimmickInstance* object, MATRIX* camera) {
  if (object == nullptr || camera == nullptr) {
    WarnActorRefused(kGimmickDraw, "no gimmick or camera matrix");
    return;
  }
  const GimmickPlacement placement = ClassifyGimmickPlacement(
      object->objectInfo.objectId, object->parent, object->mode);
  const std::optional<GimmickPose> pose = LiveGimmickPose(*object, placement);
  if (!pose) return;
  Render_ModelScratchpad& workspace = Workspace();
  const GimmickTransform placed = PlaceGimmick(
      placement, *pose, TransformOf(*camera), VectorOf(workspace.rootOffset),
      VectorOf(Actors_roomActorScale));
  workspace.model = object->model;
  StoreVector(workspace.position, placed.position);
  if (placed.rotation) {
    StoreVector(workspace.rotation, *placed.rotation);
    // The free placement's masked word stores cleared the padding.
    if (placement == GimmickPlacement::kFree) workspace.rotation.pad = 0;
  }
  if (placed.scale) StoreVector(workspace.scale, *placed.scale);
  StoreTransform(workspace.matrix, placed.model);
  StoreTransform(Actors_skeletonPose.boneMatrices[0], placed.model);
  StoreTransform(object->matrices[kModelMatrix], placed.model);
  StoreTransform(Actors_skeletonRootMatrix, placed.model);
  StoreTransform(object->matrices[kRootMatrix], placed.model);
  if (object->isVisible == 0 || !Render::ProjectLiveGimmickModel(*object)) {
    return;
  }
  workspace.texturePage = GimmickTexturePage();
  workspace.palette = GimmickPalette(
      object->slot, object->palettePulse.kind != 0, object->motionState);
  if (GimmickDrawsWithOverrides(object->motionState)) {
    Render::DrawLiveGimmickGouraudPolygonsWithOverrides(*object);
  } else {
    Render::DrawLiveGimmickGouraudPolygons(*object);
  }
}

// decomp: BATTLE.PRG func_800B217C (src/BATTLE/BATTLE.PRG/49268.c:155). The
// original also left the weapon's model and each parent's bone matrix in the
// model workspace; only this function read them.
void DrawLiveActorWeaponModel(Actors_WeaponModel* weapon, MATRIX* source) {
  if (weapon == nullptr || source == nullptr) {
    WarnActorRefused(kWeaponDraw, "no weapon or attachment matrix");
    return;
  }
  const std::size_t bone_count = weapon->boneCount;
  if (bone_count > kWeaponBoneLimit) {
    Core_Warnf("actors", "%s refused: the weapon has %zu bones", kWeaponDraw,
               bone_count);
    return;
  }
  const std::optional<std::array<WeaponBone, kWeaponBoneLimit>> bones =
      LiveWeaponBones(*weapon, bone_count);
  if (!bones) {
    WarnActorRefused(kWeaponDraw, "the weapon model cannot hold its bones");
    return;
  }
  const std::span<const WeaponBone> used = std::span(*bones).first(bone_count);
  const WeaponFadeStep fade = StepWeaponFade(
      WeaponFade{weapon->reserved11, weapon->reserved12, weapon->reserved13});
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
  const Actors_ActorRecord* owner =
      LiveActorRecord(weapon->actorId, kWeaponDraw);
  if (owner == nullptr) {
    WarnActorRefused(kWeaponDraw, "the weapon's owner slot is empty");
    return;
  }
  DrawWeaponForOwner(*weapon, *owner);
}

// decomp: INITBTL.PRG func_800FA7B0 (src/BATTLE/INITBTL.PRG/FB0.c:19)
void InitializeLiveActorModelResources() {
  Actors_reservedControlByte = 0;
  for (Content_ActorAssetRequest& request : Content_legacyActorAssetRequests) {
    request.assetKind = 0;
  }
  for (std::size_t slot = 0; slot < kInitializedActorTextureSlots; ++slot) {
    const TextureSlotIds ids = ActorTextureSlotIds(
        slot, SlotOriginHalfword(slot, 0), SlotOriginHalfword(slot, 1));
    Render_TextureSlot& stored = Render_legacyActorTextureSlots[slot];
    StoreSlotIds(stored, ids);
    stored.alternatePalette = static_cast<std::int16_t>(ids.alternate_palette);
  }
  std::fill(std::begin(Actors_actorRecords), std::end(Actors_actorRecords),
            nullptr);
  constexpr std::size_t kPrimarySequenceBytes = 0x7800;
  constexpr std::size_t kSequenceWorkBytes = 0x4800;
  Actors_primarySequenceBuffer =
      Core::Memory::AllocateLiveFromLowEnd(kPrimarySequenceBytes);
  Actors_sequenceWorkBuffer =
      Core::Memory::AllocateLiveFromLowEnd(kSequenceWorkBytes);
  for (std::size_t slot = 0; slot < kWeaponTextureSlots; ++slot) {
    StoreSlotIds(Render_legacyWeaponTextureSlots[slot],
                 WeaponTextureSlotIds(slot));
  }
  std::fill(std::begin(Actors_weaponModels), std::end(Actors_weaponModels),
            nullptr);
  for (std::size_t buffer = 0; buffer < 2; ++buffer) {
    Content_legacyWeaponModelBuffers[buffer] =
        static_cast<Actors_WeaponModelData*>(
            Core::Memory::AllocateLiveFromLowEnd(
                sizeof(Actors_WeaponModelData)));
  }
  std::fill(std::begin(Field_gimmickInstances),
            std::end(Field_gimmickInstances), nullptr);
  constexpr std::size_t kClearedBankPages = 3;
  constexpr std::size_t kClearedSequenceBanks = 3;
  std::fill_n(std::begin(Actors_sharedSequenceBankTexturePages),
              kClearedBankPages, u_char{0});
  std::fill_n(std::begin(Actors_sharedSequenceData), kClearedSequenceBanks,
              nullptr);
  Content_legacySequenceParsePhase = 0;
  Content_legacyEtmLoadPhase = 0;
  Content_modelParseResetFlag = 0;
  Menu_Inventory_equipmentModelLoadStep = 0;
  Content_legacyModelLoadPhase = 0;
  Actors_sharedSequenceBankIndex = 0;
  VECTOR unit_room_scale{};
  unit_room_scale.vx = kUnitActorScale;
  unit_room_scale.vy = kUnitActorScale;
  unit_room_scale.vz = kUnitActorScale;
  SetLiveRoomActorScale(&unit_room_scale);
  Actors_pushedGimmickIndex = 0;
  Actors_isActorUpdateComplete = 0;
}
}  // namespace VS::Actors
