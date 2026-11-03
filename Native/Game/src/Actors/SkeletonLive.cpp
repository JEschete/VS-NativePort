// Hand-maintained (tools/port/owned.csv).
// Live binding for the skeleton queries (Skeleton.hpp): the actor records, the
// field gimmicks that share their ids, the weapon models of each actor slot
// and the bone camera.
#include "Actors/SkeletonLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <span>
#include <type_traits>

#include "Actors/ActorTableLive.hpp"
#include "Actors/Appearance.hpp"
#include "Actors/ModelDrawing.hpp"
#include "Actors/ModelDrawingLive.hpp"
#include "Actors/ModelRenderingLive.hpp"
#include "Actors/Skeleton.hpp"
#include "Actors/SkeletonLiveState.h"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ModelGeometryLive.hpp"
#include "Render/SpritesLive.hpp"

extern "C" {
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Skeleton.h"
#include "Actors/WeaponModel.h"
#include "Core/Diagnostics.h"
#include "Core/Gte.h"
#include "Field/GimmickLookup.h"
#include "Field/Gimmicks.h"
#include "Legacy/Psx.h"
#include "Render/UiPrimitives.h"

// decomp: D_800F3C58 (D_800F3C58@BATTLE/BATTLE.PRG)
Actors_SkeletonPose Actors_skeletonPose;

// decomp: D_800F3C78 (D_800F3C78@BATTLE/BATTLE.PRG)
MATRIX Actors_skeletonRootMatrix;

// decomp: vs_battle_wepModels (vs_battle_wepModels@BATTLE/BATTLE.PRG)
Actors_WeaponModel* Actors_weaponModels[kActorsWeaponModelCount];

// decomp: D_800F49B8 (D_800F49B8@BATTLE/BATTLE.PRG)
MATRIX Actors_boneMatrixScratch;
}

namespace VS::Actors {
namespace {
using Core::Math::StoreTransform;
using Core::Math::StoreVector;
using Core::Math::Transform3;
using Core::Math::TransformOf;
using Core::Math::Vec3;
using Core::Math::VectorOf;

using PoseKeys = decltype(Actors_AnimationPose::boneKeys);
using WeaponBoneMatrices = decltype(Actors_WeaponModel::boneWorldMatrices);
// The root key is the last key of the blended pose.
constexpr std::size_t kRootKey = std::extent_v<PoseKeys> - 1;
constexpr std::size_t kWeaponTableSize = std::size(Actors_weaponModels);
constexpr std::size_t kWeaponSlots = kWeaponTableSize / kWeaponsPerActor;
constexpr int kWeaponBoneCapacity = std::extent_v<WeaponBoneMatrices>;
// SHP bone counts are bytes.
constexpr std::size_t kBoneLabelCapacity = 256;
// The head bone carries this auxiliary tag.
constexpr int kHeadAuxiliaryTag = 2;
// The size of one SHP bone record.
constexpr std::size_t kShpBoneBytes = 0x10;

static_assert(kWeaponTableSize % kWeaponsPerActor == 0);
static_assert(sizeof(Actors_ShpBone) == kShpBoneBytes);

// The skeleton an id names: the actor in its slot, else the gimmick in that
// slot.
struct LiveSkeleton {
  Actors_ActorRecord* actor = nullptr;
  Field_GimmickInstance* gimmick = nullptr;

  [[nodiscard]] bool Present() const {
    return actor != nullptr || gimmick != nullptr;
  }

  // The model bytes its loader recorded.
  [[nodiscard]] std::span<const std::byte> ModelBytes() const {
    if (actor != nullptr) {
      if (actor->instance.model == nullptr) return {};
      return {reinterpret_cast<const std::byte*>(actor->instance.model),
              actor->modelDataSize};
    }
    if (gimmick == nullptr) return {};
    return Render::LiveModelBytes(gimmick->model);
  }

  [[nodiscard]] std::size_t BoneCount() const {
    if (actor != nullptr) return actor->instance.boneCount;
    if (gimmick == nullptr) return 0;
    return static_cast<std::uint8_t>(gimmick->boneCount);
  }

  [[nodiscard]] std::span<const MATRIX> Matrices() const {
    if (actor != nullptr) return actor->boneMatrices;
    if (gimmick != nullptr) return gimmick->matrices;
    return {};
  }
};

[[nodiscard]] LiveSkeleton FindLiveSkeleton(int actor_id,
                                            const char* operation) {
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, operation);
  if (actor != nullptr) return {.actor = actor, .gimmick = nullptr};
  return {.actor = nullptr, .gimmick = Field_FindGimmickForActorSlot(actor_id)};
}

[[nodiscard]] std::optional<Actors_ShpBone> ReadBone(
    std::span<const std::byte> model, std::size_t bone) {
  const std::size_t offset =
      sizeof(Actors_ShpModel) + (bone * sizeof(Actors_ShpBone));
  if (model.size() < offset + sizeof(Actors_ShpBone)) return std::nullopt;
  Actors_ShpBone stored;
  std::memcpy(&stored, model.data() + offset, sizeof stored);
  return stored;
}

[[nodiscard]] BoneLabels LabelsOf(const Actors_ShpBone& bone) {
  return {.tag = bone.tag,
          .limb_ids = bone.limbIds,
          .auxiliary_tag = bone.auxiliaryTag};
}

struct BoneLabelTable {
  std::array<BoneLabels, kBoneLabelCapacity> labels{};
  std::size_t count = 0;

  [[nodiscard]] std::span<const BoneLabels> View() const {
    return std::span<const BoneLabels>(labels).first(count);
  }
};

// The labels of the skeleton's bones; a bone count past the loaded bone table
// is reported and only the loaded bones are searched.
[[nodiscard]] BoneLabelTable LiveBoneLabels(const LiveSkeleton& skeleton,
                                            const char* operation) {
  BoneLabelTable table;
  const std::span<const std::byte> model = skeleton.ModelBytes();
  const std::size_t bones = skeleton.BoneCount();
  for (; table.count < bones; ++table.count) {
    const std::optional<Actors_ShpBone> bone = ReadBone(model, table.count);
    if (!bone) {
      Core_Warnf("actors",
                 "%s refused: bone %u of %u lies past the loaded bone table",
                 operation, static_cast<unsigned>(table.count),
                 static_cast<unsigned>(bones));
      break;
    }
    table.labels[table.count] = LabelsOf(*bone);
  }
  return table;
}

[[nodiscard]] std::optional<std::int32_t> StoredBoneLength(
    std::span<const std::byte> model, int bone, const char* operation) {
  const std::optional<Actors_ShpBone> stored =
      bone >= 0 ? ReadBone(model, static_cast<std::size_t>(bone))
                : std::nullopt;
  if (!stored) {
    Core_Warnf("actors", "%s refused: bone %d lies past the loaded bone table",
               operation, bone);
    return std::nullopt;
  }
  return stored->length;
}

[[nodiscard]] const MATRIX* StoredBoneMatrix(const LiveSkeleton& skeleton,
                                             int bone, const char* operation) {
  const std::span<const MATRIX> matrices = skeleton.Matrices();
  if (bone < 0 || static_cast<std::size_t>(bone) >= matrices.size()) {
    Core_Warnf("actors", "%s refused: bone %d lies past the %u bone matrices",
               operation, bone, static_cast<unsigned>(matrices.size()));
    return nullptr;
  }
  return &matrices[static_cast<std::size_t>(bone)];
}

// The first weapon model of the actor's slot; none for an empty weapon slot
// or an actor slot past the weapon table (which the original read past).
[[nodiscard]] const Actors_WeaponModel* LiveFirstWeapon(int actor_id,
                                                        const char* operation) {
  if (actor_id < 0 || static_cast<std::size_t>(actor_id) >= kWeaponSlots) {
    Core_Warnf("actors", "%s refused: actor %d has no weapon slot", operation,
               actor_id);
    return nullptr;
  }
  return Actors_weaponModels[static_cast<std::size_t>(actor_id) *
                             kWeaponsPerActor];
}

[[nodiscard]] const MATRIX* WeaponBoneMatrix(const Actors_WeaponModel& weapon,
                                             int selector,
                                             const char* operation) {
  const int bone = WeaponBoneIndex(selector);
  if (bone < 0 || bone >= kWeaponBoneCapacity) {
    Core_Warnf("actors",
               "%s refused: weapon bone selector 0x%X lies past the %d weapon "
               "bone matrices",
               operation, static_cast<unsigned>(selector), kWeaponBoneCapacity);
    return nullptr;
  }
  return &weapon.boneWorldMatrices[bone];
}

[[nodiscard]] Core::Math::Angles3 ActorAngles(
    const Actors_ModelInstance& instance) {
  return {instance.rotationX, instance.facing, instance.rotationZ};
}

[[nodiscard]] Vec3 GimmickPosition(const Field_GimmickInstance& gimmick) {
  return {gimmick.positionX, gimmick.positionY, gimmick.positionZ};
}

// func_800A1DE8 over the live tables.
[[nodiscard]] std::optional<Transform3> SelectedBoneViewTransform(
    const LiveSkeleton& skeleton, int actor_id, int selector,
    const char* operation) {
  const Transform3 camera = TransformOf(Actors_boneMatrixScratch);
  const MATRIX* bone = nullptr;
  if (skeleton.actor == nullptr) {
    if (skeleton.gimmick == nullptr) return std::nullopt;
    bone = StoredBoneMatrix(skeleton, selector, operation);
  } else if (IsWeaponBoneSelector(selector)) {
    const Actors_WeaponModel* weapon = LiveFirstWeapon(actor_id, operation);
    if (weapon == nullptr) return std::nullopt;
    bone = WeaponBoneMatrix(*weapon, selector, operation);
  } else if (selector == kRootSelector) {
    const Actors_ActorRecord& actor = *skeleton.actor;
    return RootSelectorTransform(ActorAngles(actor.instance),
                                 VectorOf(actor.blendPose.boneKeys[kRootKey]),
                                 actor.scale,
                                 VectorOf(actor.instance.position));
  } else {
    bone = StoredBoneMatrix(skeleton, selector, operation);
  }
  if (bone == nullptr) return std::nullopt;
  return BoneViewTransform(camera, TransformOf(*bone));
}

// The length of the selected bone. The actor root has none: func_800A17BC
// gives 0, where func_800A1C10 read bone 255 past the bone table.
[[nodiscard]] std::optional<std::int32_t> SelectedBoneLength(
    const LiveSkeleton& skeleton, int actor_id, int selector,
    const char* operation) {
  if (skeleton.actor != nullptr) {
    if (selector == kRootSelector) return 0;
    if (IsWeaponBoneSelector(selector)) {
      const Actors_WeaponModel* weapon = LiveFirstWeapon(actor_id, operation);
      if (weapon == nullptr) return std::nullopt;
      return StoredBoneLength(Render::LiveModelBytes(weapon->sectionOffsets),
                              WeaponBoneIndex(selector), operation);
    }
  }
  return StoredBoneLength(skeleton.ModelBytes(), selector, operation);
}

// The perspective projection of a world point through the view matrix: the
// packed screen word and the depth RotTransPers returns.
struct ProjectedPoint {
  WordHalves screen;
  std::int32_t depth = 0;
};

[[nodiscard]] ProjectedPoint ProjectViewPoint(Vec3 point) {
  SVECTOR position{};
  StoreVector(position, point);
  Render::LoadLiveFrameViewMatrix();
  Core_GteWord screen = 0;
  Core_GteWord depth_cue = 0;
  Core_GteWord flags = 0;
  const Core_GteWord depth =
      Core_Gte_RotTransPers(&position, &screen, &depth_cue, &flags);
  return {.screen = SplitWord(static_cast<std::int32_t>(screen)),
          .depth = static_cast<std::int32_t>(depth)};
}

[[nodiscard]] HeadTracking LoadHeadTracking(const Actors_ActorRecord& actor) {
  return {.target = actor.headTrackTarget,
          .aim_x = actor.headTracking.vx,
          .aim_y = actor.headTracking.vy,
          .aim_z = actor.headTracking.vz,
          .speed = actor.headTracking.pad};
}

void StoreHeadTracking(const HeadTracking& tracking,
                       Actors_ActorRecord& actor) {
  actor.headTrackTarget = tracking.target;
  actor.headTracking.vx = tracking.aim_x;
  actor.headTracking.vy = tracking.aim_y;
  actor.headTracking.vz = tracking.aim_z;
  actor.headTracking.pad = tracking.speed;
}

[[nodiscard]] int LimbGroupBonePosition(int actor_id, int limb,
                                        BoneTagKind kind, SVECTOR* position_out,
                                        int endpoint) {
  const int bone = FindLiveActorBoneTag(actor_id, limb, static_cast<int>(kind));
  if (bone < 0) return bone;
  return LiveActorBoneEndpointPosition(actor_id, bone, position_out, endpoint);
}
}  // namespace

// decomp: BATTLE.PRG func_8009E228 (src/BATTLE/BATTLE.PRG/30DB0.c:2525)
int LiveActorPitchToPoint(const Actors_ActorRecord* actor,
                          const SVECTOR* target) {
  if (target == nullptr) return 0;
  if (actor == nullptr) {
    WarnActorRefused("ActorPitchToPoint", "no actor record");
    return 0;
  }
  // The original left the root position unset when the query failed.
  SVECTOR root{};
  LiveActorBonePosition(actor->instance.slot, 0, &root, 0);
  return PitchToPoint(VectorOf(actor->instance.position), VectorOf(*target),
                      root.vy);
}

// decomp: BATTLE.PRG func_8009FC60 (src/BATTLE/BATTLE.PRG/30DB0.c:3512)
int SetLiveActorHeadTracking(int actor_id, int target, const SVECTOR* aim,
                             int step) {
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, "SetActorHeadTracking");
  if (actor == nullptr) return kSkeletonNoOwner;
  if (FindLiveActorBoneTag(actor_id, kHeadAuxiliaryTag,
                           static_cast<int>(BoneTagKind::kAuxiliary)) < 0) {
    return kSkeletonNoHeadBone;
  }
  const HeadTrackingRequest request{
      .target = target,
      .aim =
          aim != nullptr ? std::optional<Vec3>(VectorOf(*aim)) : std::nullopt,
      .step = step};
  const ActorLook look{.pitch = actor->instance.lookPitch,
                       .yaw = actor->instance.lookYaw,
                       .collision_radius = actor->collisionRadius};
  StoreHeadTracking(StartHeadTracking(LoadHeadTracking(*actor), request, look),
                    *actor);
  return 0;
}

// decomp: BATTLE.PRG func_8009FD38 (src/BATTLE/BATTLE.PRG/30DB0.c:3540)
void ResetLiveActorHeadTrackingSpeed(Actors_ActorRecord* actor) {
  if (actor == nullptr) {
    WarnActorRefused("ResetActorHeadTrackingSpeed", "no actor record");
    return;
  }
  actor->headTracking.pad = HeadTrackingSpeed(actor->collisionRadius);
}

// decomp: BATTLE.PRG func_800A1178 (src/BATTLE/BATTLE.PRG/30DB0.c:4200)
int LiveActorTaggedBoneMatrix(int actor_id, int tag, MATRIX* matrix_out,
                              u_long* length_out) {
  const int bone =
      FindLiveActorBoneTag(actor_id, tag, static_cast<int>(BoneTagKind::kTag));
  if (bone < 0) return bone;
  LiveActorBoneMatrix(actor_id, bone, matrix_out, length_out);
  return 0;
}

// decomp: BATTLE.PRG func_800A11D8 (src/BATTLE/BATTLE.PRG/30DB0.c:4210)
void LiveActorBoneMatrix(int actor_id, int bone, MATRIX* matrix_out,
                         u_long* length_out) {
  constexpr const char* kOperation = "ActorBoneMatrix";
  const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
  if (!skeleton.Present()) return;
  if (matrix_out == nullptr || length_out == nullptr) {
    WarnActorRefused(kOperation, "no matrix or length output");
    return;
  }
  const MATRIX* matrix = StoredBoneMatrix(skeleton, bone, kOperation);
  if (matrix == nullptr) return;
  const std::optional<std::int32_t> length =
      StoredBoneLength(skeleton.ModelBytes(), bone, kOperation);
  if (!length) return;
  *matrix_out = *matrix;
  *length_out = static_cast<u_long>(*length);
}

// decomp: BATTLE.PRG func_800A1280 (src/BATTLE/BATTLE.PRG/30DB0.c:4224)
int ProjectLiveActorTaggedBone(int actor_id, int tag, SVECTOR* screen_out,
                               int endpoint) {
  constexpr const char* kOperation = "ProjectActorTaggedBone";
  if (screen_out == nullptr) {
    WarnActorRefused(kOperation, "no screen output");
    return kSkeletonNoOwner;
  }
  if (tag == kTargetPointSelector) {
    const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
    Vec3 point;
    if (skeleton.actor != nullptr) {
      if (skeleton.actor->instance.reactionState != 0) return kSkeletonNoOwner;
      point = TargetPoint(VectorOf(skeleton.actor->instance.position),
                          skeleton.actor->targetPointHeight);
    } else if (skeleton.gimmick != nullptr) {
      point =
          TargetPoint(GimmickPosition(*skeleton.gimmick), kGimmickTargetHeight);
    } else {
      return kSkeletonNoOwner;
    }
    const ProjectedPoint projected = ProjectViewPoint(point);
    screen_out->vx = projected.screen.low;
    screen_out->vy = projected.screen.high;
    screen_out->vz = static_cast<std::int16_t>(projected.depth);
    return 0;
  }
  const int bone =
      FindLiveActorBoneTag(actor_id, tag, static_cast<int>(BoneTagKind::kTag));
  if (bone < 0) return bone;
  std::array<int, 2> projected{};
  const int status =
      ProjectLiveActorBoneEndpoint(actor_id, bone, projected.data(), endpoint);
  if (status < 0) return status;
  // The original stored the packed screen word and the depth word over the
  // record's four halfwords.
  const WordHalves screen = SplitWord(projected[0]);
  const WordHalves depth = SplitWord(projected[1]);
  screen_out->vx = screen.low;
  screen_out->vy = screen.high;
  screen_out->vz = depth.low;
  screen_out->pad = depth.high;
  return 0;
}

// decomp: BATTLE.PRG func_800A13EC (src/BATTLE/BATTLE.PRG/30DB0.c:4280)
int ProjectLiveActorBone(int actor_id, int bone, int* screen_out,
                         int endpoint) {
  const int status =
      ProjectLiveActorBoneEndpoint(actor_id, bone, screen_out, endpoint);
  return status < 0 ? status : 0;
}

// decomp: BATTLE.PRG func_800A152C (src/BATTLE/BATTLE.PRG/38C1C.c:103)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int FindLiveActorBoneTag(int actor_id, int tag, int kind) {
  constexpr const char* kOperation = "FindActorBoneTag";
  const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
  if (!skeleton.Present()) return kSkeletonNoOwner;
  const std::optional<BoneTagKind> tag_kind = ClassifyBoneTagKind(kind);
  // Another kind matches no bone; the root selector is still found.
  const BoneLabelTable labels =
      tag_kind ? LiveBoneLabels(skeleton, kOperation) : BoneLabelTable{};
  const std::optional<std::size_t> bone =
      FindBoneTag(labels.View(), tag, tag_kind.value_or(BoneTagKind::kTag));
  return bone ? static_cast<int>(*bone) : kSkeletonTagNotFound;
}

// decomp: BATTLE.PRG func_800A1648 (src/BATTLE/BATTLE.PRG/38C1C.c:161)
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
int LiveActorBoneTag(int actor_id, int bone, int kind) {
  constexpr const char* kOperation = "ActorBoneTag";
  const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
  if (!skeleton.Present()) return kSkeletonNoOwner;
  const std::optional<BoneTagKind> tag_kind = ClassifyBoneTagKind(kind);
  if (!tag_kind) {
    Core_Warnf("actors", "%s refused: kind %d names no bone label", kOperation,
               kind);
    return kSkeletonNoOwner;
  }
  const std::optional<Actors_ShpBone> stored =
      bone >= 0
          ? ReadBone(skeleton.ModelBytes(), static_cast<std::size_t>(bone))
          : std::nullopt;
  if (!stored) {
    Core_Warnf("actors", "%s refused: bone %d lies past the loaded bone table",
               kOperation, bone);
    return kSkeletonNoOwner;
  }
  const std::optional<std::int32_t> value =
      BoneTagValue(LabelsOf(*stored), *tag_kind);
  return value ? *value : kSkeletonNoLimbGroup;
}

// decomp: BATTLE.PRG func_800A1720 (src/BATTLE/BATTLE.PRG/38C1C.c:195)
int LiveActorAnchorMatrix(int actor_id, int selector, MATRIX* matrix_out,
                          int* length_out) {
  if (selector != kGroundAnchorSelector) {
    return LiveActorBoneWorldMatrix(actor_id, selector, matrix_out, length_out);
  }
  constexpr const char* kOperation = "ActorAnchorMatrix";
  const Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr) {
    WarnActorRefused(kOperation, "the ground anchor needs an actor");
    return kSkeletonNoOwner;
  }
  const int status =
      LiveActorBoneWorldMatrix(actor_id, 0, matrix_out, length_out);
  if (status < 0) return status;
  matrix_out->t[1] = actor->instance.position.vy;
  return 0;
}

// decomp: BATTLE.PRG func_800A17BC (src/BATTLE/BATTLE.PRG/38C1C.c:216)
int LiveActorBoneWorldMatrix(int actor_id, int selector, MATRIX* matrix_out,
                             int* length_out) {
  constexpr const char* kOperation = "ActorBoneWorldMatrix";
  if (matrix_out == nullptr || length_out == nullptr) {
    WarnActorRefused(kOperation, "no matrix or length output");
    return kSkeletonNoOwner;
  }
  const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
  const std::optional<Transform3> transform =
      SelectedBoneViewTransform(skeleton, actor_id, selector, kOperation);
  if (!transform) return kSkeletonNoOwner;
  const std::optional<std::int32_t> length =
      SelectedBoneLength(skeleton, actor_id, selector, kOperation);
  if (!length) return kSkeletonNoOwner;
  StoreTransform(*matrix_out, *transform);
  *length_out = *length;
  return 0;
}

// decomp: BATTLE.PRG func_800A190C (src/BATTLE/BATTLE.PRG/38C1C.c:246)
int LiveActorTaggedBonePosition(int actor_id, int tag, SVECTOR* position_out,
                                int endpoint) {
  constexpr const char* kOperation = "ActorTaggedBonePosition";
  if (position_out == nullptr) {
    WarnActorRefused(kOperation, "no position output");
    return kSkeletonNoOwner;
  }
  if (tag == kTargetPointSelector) {
    const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
    if (skeleton.actor != nullptr) {
      const SVECTOR& position = skeleton.actor->instance.position;
      *position_out = position;
      StoreVector(
          *position_out,
          TargetPoint(VectorOf(position), skeleton.actor->targetPointHeight));
      return 0;
    }
    if (skeleton.gimmick == nullptr) return kSkeletonNoOwner;
    // The original copied the position record, its padding included.
    position_out->pad = skeleton.gimmick->reserved22;
    StoreVector(*position_out, TargetPoint(GimmickPosition(*skeleton.gimmick),
                                           kGimmickTargetHeight));
    return 0;
  }
  if (tag == kGroundAnchorSelector || tag == kAnchorHeightSelector) {
    const Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
    if (actor == nullptr) {
      WarnActorRefused(kOperation,
                       "the ground and anchor points need an actor");
      return kSkeletonNoOwner;
    }
    const SVECTOR& position = actor->instance.position;
    *position_out = position;
    if (tag == kAnchorHeightSelector) {
      StoreVector(
          *position_out,
          AnchorPoint(VectorOf(position), actor->anchorHeight, actor->scale));
    }
    return 0;
  }
  int bone = tag;
  if (!IsWeaponBoneSelector(tag)) {
    bone = FindLiveActorBoneTag(actor_id, tag,
                                static_cast<int>(BoneTagKind::kTag));
    if (bone < 0) return bone;
  }
  return LiveActorBoneEndpointPosition(actor_id, bone, position_out, endpoint);
}

// decomp: BATTLE.PRG func_800A1AF8 (src/BATTLE/BATTLE.PRG/38C1C.c:300)
int LiveActorBonePosition(int actor_id, int bone, SVECTOR* position_out,
                          int endpoint) {
  return LiveActorBoneEndpointPosition(actor_id, bone, position_out, endpoint);
}

// decomp: BATTLE.PRG func_800A1B28 (src/BATTLE/BATTLE.PRG/38C1C.c:311)
int LiveActorHighGroupBonePosition(int actor_id, int limb,
                                   SVECTOR* position_out, int endpoint) {
  return LimbGroupBonePosition(actor_id, limb, BoneTagKind::kHighLimb,
                               position_out, endpoint);
}

// decomp: BATTLE.PRG func_800A1B9C (src/BATTLE/BATTLE.PRG/38C1C.c:328)
int LiveActorLowGroupBonePosition(int actor_id, int limb, SVECTOR* position_out,
                                  int endpoint) {
  return LimbGroupBonePosition(actor_id, limb, BoneTagKind::kLowLimb,
                               position_out, endpoint);
}

// decomp: BATTLE.PRG func_800A1C10 (src/BATTLE/BATTLE.PRG/38C1C.c:345)
int LiveActorBoneEndpointPosition(int actor_id, int selector,
                                  SVECTOR* position_out, int endpoint) {
  constexpr const char* kOperation = "ActorBoneEndpointPosition";
  if (position_out == nullptr) {
    WarnActorRefused(kOperation, "no position output");
    return kSkeletonNoOwner;
  }
  const LiveSkeleton skeleton = FindLiveSkeleton(actor_id, kOperation);
  const std::optional<Transform3> transform =
      SelectedBoneViewTransform(skeleton, actor_id, selector, kOperation);
  if (!transform) return kSkeletonNoOwner;
  const BoneEndpoint point = ClassifyBoneEndpoint(endpoint);
  std::int32_t length = 0;
  if (point != BoneEndpoint::kBase) {
    const std::optional<std::int32_t> stored =
        SelectedBoneLength(skeleton, actor_id, selector, kOperation);
    if (!stored) return kSkeletonNoOwner;
    length = *stored;
  }
  StoreVector(*position_out, BoneEndpointPoint(*transform, length, point));
  return 0;
}

// decomp: BATTLE.PRG func_800A1DE8 (src/BATTLE/BATTLE.PRG/38C1C.c:400)
std::optional<Transform3> LiveActorBoneViewTransform(int actor_id,
                                                     int selector) {
  constexpr const char* kOperation = "ActorBoneWorldTransform";
  return SelectedBoneViewTransform(FindLiveSkeleton(actor_id, kOperation),
                                   actor_id, selector, kOperation);
}

// decomp: BATTLE.PRG func_800A4828 (src/BATTLE/BATTLE.PRG/3A1A0.c:1036)
void BuildLiveActorPoseAtMatrix(int actor_id, MATRIX* camera) {
  constexpr const char* kOperation = "BuildActorPoseAtMatrix";
  Actors_ActorRecord* actor = LiveActorRecord(actor_id, kOperation);
  if (actor == nullptr || camera == nullptr) {
    WarnActorRefused(kOperation, "no actor record or camera matrix");
    return;
  }
  const char visible = actor->instance.isVisible;
  actor->instance.isVisible = 0;
  actor->framesPerStep = 1;
  Render::PrepareLiveActorCameraMatrices(camera);
  PrepareLiveActorRootTransform(actor);
  // A hidden actor's draw builds its bone matrices without drawing it.
  DrawLiveActorModel(*actor, *camera, 0);
  actor->instance.isVisible = visible;
}
}  // namespace VS::Actors
