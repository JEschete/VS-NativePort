// Hand-maintained (tools/port/owned.csv).
// Typed skeleton queries (Skeleton.hpp); SkeletonLive.cpp binds them to the
// actor, gimmick and weapon tables.
#include "Actors/Skeleton.hpp"  // NOLINT(build/include): native C++ interface.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ModelDrawing.hpp"
#include "Actors/ModelRendering.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
using Core::Math::Transform3;
using Core::Math::Vec3;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrappingSubtract;

constexpr std::int32_t kTagByteMask = 0xFF;
constexpr unsigned kHighLimbShift = 4;
constexpr std::uint8_t kLowLimbMask = 0x0F;
// Limb groups are stored one above their index, so 0 names no group.
constexpr std::int32_t kLimbGroupBias = 1;
constexpr std::int16_t kHeadTrackingStep = 30;
constexpr std::int16_t kWideHeadTrackingStep = 20;
constexpr std::uint16_t kWideActorRadius = 128;
constexpr std::uint32_t kHalfwordMask = 0xFFFFU;
constexpr unsigned kHalfwordBits = 16;

[[nodiscard]] std::int16_t Halfword(std::int32_t value) noexcept {
  return static_cast<std::int16_t>(value);
}

[[nodiscard]] Vec3 Halfwords(Vec3 value) noexcept {
  return {Halfword(value.x), Halfword(value.y), Halfword(value.z)};
}

// The square of a difference with the original's wrapping product, negated
// when the product wrapped negative.
[[nodiscard]] std::int32_t WrappedSquare(std::int32_t delta) noexcept {
  const std::int32_t square = WrappingMultiply(delta, delta);
  return square < 0 ? WrappingNegate(square) : square;
}

[[nodiscard]] std::int32_t HighLimb(const BoneLabels& bone) noexcept {
  return bone.limb_ids >> kHighLimbShift;
}

[[nodiscard]] std::int32_t LowLimb(const BoneLabels& bone) noexcept {
  return bone.limb_ids & kLowLimbMask;
}

[[nodiscard]] bool LabelMatches(const BoneLabels& bone, std::int32_t tag,
                                BoneTagKind kind) noexcept {
  switch (kind) {
    case BoneTagKind::kTag:
      return bone.tag == tag;
    case BoneTagKind::kHighLimb:
      return HighLimb(bone) == tag + kLimbGroupBias;
    case BoneTagKind::kLowLimb:
      return LowLimb(bone) == tag + kLimbGroupBias;
    case BoneTagKind::kAuxiliary:
      return bone.auxiliary_tag == tag;
  }
  return false;
}

[[nodiscard]] std::optional<std::int32_t> LimbGroup(
    std::int32_t nibble) noexcept {
  if (nibble == 0) return std::nullopt;
  return nibble;
}
}  // namespace

std::optional<BoneTagKind> ClassifyBoneTagKind(int kind) noexcept {
  if (kind < static_cast<int>(BoneTagKind::kTag) ||
      kind > static_cast<int>(BoneTagKind::kAuxiliary)) {
    return std::nullopt;
  }
  return static_cast<BoneTagKind>(kind);
}

// decomp: BATTLE.PRG func_800A152C (src/BATTLE/BATTLE.PRG/38C1C.c:103)
std::optional<std::size_t> FindBoneTag(std::span<const BoneLabels> bones,
                                       std::int32_t tag,
                                       BoneTagKind kind) noexcept {
  if (tag == kRootSelector) return 0;
  const std::int32_t wanted = tag & kTagByteMask;
  for (std::size_t bone = 0; bone < bones.size(); ++bone) {
    if (LabelMatches(bones[bone], wanted, kind)) return bone;
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG func_800A1648 (src/BATTLE/BATTLE.PRG/38C1C.c:161)
std::optional<std::int32_t> BoneTagValue(const BoneLabels& bone,
                                         BoneTagKind kind) noexcept {
  switch (kind) {
    case BoneTagKind::kTag:
      return bone.tag;
    case BoneTagKind::kHighLimb:
      return LimbGroup(HighLimb(bone));
    case BoneTagKind::kLowLimb:
      return LimbGroup(LowLimb(bone));
    case BoneTagKind::kAuxiliary:
      return bone.auxiliary_tag;
  }
  return std::nullopt;
}

// decomp: BATTLE.PRG func_8009E228 (src/BATTLE/BATTLE.PRG/30DB0.c:2525)
std::int32_t PitchToPoint(Vec3 origin, Vec3 target,
                          std::int32_t root_height) noexcept {
  const std::int32_t horizontal = Core::Math::SquareRootInteger(
      WrappingAdd(WrappedSquare(WrappingSubtract(target.x, origin.x)),
                  WrappedSquare(WrappingSubtract(target.z, origin.z))));
  return Core::Math::Arctangent2(WrappingSubtract(target.y, root_height),
                                 horizontal);
}

// decomp: BATTLE.PRG func_8009FD38 (src/BATTLE/BATTLE.PRG/30DB0.c:3540)
std::int16_t HeadTrackingSpeed(std::uint16_t collision_radius) noexcept {
  return collision_radius > kWideActorRadius ? kWideHeadTrackingStep
                                             : kHeadTrackingStep;
}

// decomp: BATTLE.PRG func_8009FC60 (src/BATTLE/BATTLE.PRG/30DB0.c:3512)
HeadTracking StartHeadTracking(const HeadTracking& current,
                               const HeadTrackingRequest& request,
                               const ActorLook& look) noexcept {
  HeadTracking tracking = current;
  tracking.target = static_cast<std::int8_t>(request.target);
  if (request.aim) {
    tracking.aim_x = Halfword(request.aim->x);
    tracking.aim_y = Halfword(request.aim->y);
    tracking.aim_z = Halfword(request.aim->z);
  } else {
    tracking.aim_x = static_cast<std::int16_t>(look.pitch);
    tracking.aim_y = static_cast<std::int16_t>(look.yaw);
  }
  tracking.speed = request.step == kHeadTrackingUsesRadiusStep
                       ? HeadTrackingSpeed(look.collision_radius)
                       : Halfword(request.step);
  return tracking;
}

// decomp: BATTLE.PRG func_800A1DE8 (src/BATTLE/BATTLE.PRG/38C1C.c:400): the
// saturating MulMatrix of the camera and the bone, then RotTrans of the
// bone's offset from the camera, reduced to halfwords, with no translation.
Transform3 BoneViewTransform(const Transform3& camera,
                             const Transform3& bone) noexcept {
  const Vec3 offset{WrappingSubtract(bone.translation.x, camera.translation.x),
                    WrappingSubtract(bone.translation.y, camera.translation.y),
                    WrappingSubtract(bone.translation.z, camera.translation.z)};
  return Transform3{
      Core::Math::MultiplyMatrixIntoRight(camera.rotation, bone.rotation),
      Core::Math::RotateTranslate(Transform3{camera.rotation, Vec3{}}, offset)};
}

// decomp: BATTLE.PRG func_800A1DE8 (src/BATTLE/BATTLE.PRG/38C1C.c:400), bone
// 0xFF: the root key halved and scaled by the actor scale in halfwords (the
// unit-scale root offset of func_800AEAE8), then func_800B07DC; the matrix
// takes the placed root offset as its translation.
Transform3 RootSelectorTransform(Core::Math::Angles3 angles, Vec3 root_key,
                                 std::int32_t actor_scale,
                                 Vec3 position) noexcept {
  const Vec3 unit_scale{kUnitActorScale, kUnitActorScale, kUnitActorScale};
  const RootTransform root = TransformRootOffset(
      angles, ActorRootOffset(root_key, unit_scale, actor_scale), position);
  return Transform3{root.matrix.rotation, root.root_offset};
}

// decomp: BATTLE.PRG func_800A1C10 (src/BATTLE/BATTLE.PRG/38C1C.c:345): the
// base copies the translation; another endpoint is RTV0TR of the bone-local
// offset, each stored to a halfword.
Vec3 BoneEndpointPoint(const Transform3& transform, std::int32_t length,
                       BoneEndpoint endpoint) noexcept {
  if (endpoint == BoneEndpoint::kBase) return Halfwords(transform.translation);
  const Vec3 local{BoneEndpointOffset(length, endpoint), 0, 0};
  return Halfwords(Core::Math::RotateTranslate(transform, local));
}

// decomp: BATTLE.PRG func_800A190C (src/BATTLE/BATTLE.PRG/38C1C.c:246) and
// func_800A1280 (30DB0.c:4224), selector 0xFB.
Vec3 TargetPoint(Vec3 position, std::int32_t height) noexcept {
  return Halfwords(
      Vec3{position.x, WrappingSubtract(position.y, height), position.z});
}

// decomp: BATTLE.PRG func_800A190C (src/BATTLE/BATTLE.PRG/38C1C.c:246),
// selector 0xFA.
Vec3 AnchorPoint(Vec3 position, std::int32_t anchor_height,
                 std::int32_t scale) noexcept {
  const std::int32_t raise =
      WrappingMultiply(anchor_height, scale) / kUnitActorScale;
  return Halfwords(
      Vec3{position.x, WrappingAdd(position.y, raise), position.z});
}

WordHalves SplitWord(std::int32_t word) noexcept {
  const auto bits = static_cast<std::uint32_t>(word);
  return {.low = static_cast<std::int16_t>(bits & kHalfwordMask),
          .high = static_cast<std::int16_t>(bits >> kHalfwordBits)};
}
}  // namespace VS::Actors
