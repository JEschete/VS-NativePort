#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Actors/ModelDrawing.hpp"
#include "Core/Math/Matrix.hpp"

// The bone queries of an actor's or gimmick's skeleton (BATTLE.PRG 30DB0.c,
// 38C1C.c and 3A1A0.c): tag lookup, bone-local points, the camera-relative
// bone transform, the root selector's transform, the head-tracking request and
// the pitch towards a point. Every function takes plain values;
// SkeletonLive.hpp binds them to the actor, gimmick and weapon tables.
namespace VS::Actors {
// Selectors the bone queries accept in place of a tag or bone index.
// The actor position raised by its scaled anchor height.
inline constexpr int kAnchorHeightSelector = 0xFA;
// The actor position less its target-point height (a gimmick's less 0xC0).
inline constexpr int kTargetPointSelector = 0xFB;
// Bone 0 lowered to the actor's ground height; for positions, the position.
inline constexpr int kGroundAnchorSelector = 0xFD;
// The model root; a tag search for it gives bone 0.
inline constexpr int kRootSelector = 0xFF;
// 0x40..0x4F name the bones of the actor's first weapon, from bone 1.
inline constexpr int kWeaponBoneSelector = 0x40;
inline constexpr int kWeaponBoneSelectorMask = 0xF0;
inline constexpr int kFirstWeaponBoneOffset = 0x3F;
// The height above a gimmick's position its target point sits at.
inline constexpr std::int32_t kGimmickTargetHeight = 0xC0;

// The statuses the bone queries return in place of a bone or 0.
inline constexpr int kSkeletonNoOwner = -1;
inline constexpr int kSkeletonNoHeadBone = -2;
inline constexpr int kSkeletonNoLimbGroup = -3;
inline constexpr int kSkeletonTagNotFound = -4;

// The bone label a tag search compares.
enum class BoneTagKind : std::uint8_t {
  kTag = 0,
  // The high nibble of the limb ids: the limb group plus one.
  kHighLimb = 1,
  // The low nibble of the limb ids: the limb group plus one.
  kLowLimb = 2,
  kAuxiliary = 3,
};

// The original kind argument; other values match no bone.
[[nodiscard]] std::optional<BoneTagKind> ClassifyBoneTagKind(int kind) noexcept;

// The labels of one SHP bone.
struct BoneLabels {
  std::uint8_t tag{};
  std::uint8_t limb_ids{};
  std::uint8_t auxiliary_tag{};
};

// The first bone whose label matches the low byte of `tag` (a limb group
// matches its nibble less one); the root selector gives bone 0 without a
// search (func_800A152C).
[[nodiscard]] std::optional<std::size_t> FindBoneTag(
    std::span<const BoneLabels> bones, std::int32_t tag,
    BoneTagKind kind) noexcept;

// The bone's label of `kind`: its tag, a limb nibble (none when 0) or its
// auxiliary tag (func_800A1648). The original left the auxiliary kind
// undefined; the native reads the label the tag search compares for it.
[[nodiscard]] std::optional<std::int32_t> BoneTagValue(
    const BoneLabels& bone, BoneTagKind kind) noexcept;

// Whether `selector` names a weapon bone, and the weapon bone it names.
[[nodiscard]] constexpr bool IsWeaponBoneSelector(int selector) noexcept {
  return (selector & kWeaponBoneSelectorMask) == kWeaponBoneSelector;
}
[[nodiscard]] constexpr int WeaponBoneIndex(int selector) noexcept {
  return selector - kFirstWeaponBoneOffset;
}

// The pitch from an actor at `origin` towards `target`, measured from the
// height of its root bone: the horizontal distance is the integer root of the
// wrapping sum of the squared x and z differences (func_8009E228).
[[nodiscard]] std::int32_t PitchToPoint(Core::Math::Vec3 origin,
                                        Core::Math::Vec3 target,
                                        std::int32_t root_height) noexcept;

// The step a head turns by each frame: 30, or 20 for an actor wider than 128
// (func_8009FD38).
inline constexpr int kHeadTrackingUsesRadiusStep = -1;
[[nodiscard]] std::int16_t HeadTrackingSpeed(
    std::uint16_t collision_radius) noexcept;

// An actor's head tracking: its target, the aim angles and the turn step.
struct HeadTracking {
  std::int8_t target{};
  std::int16_t aim_x{};
  std::int16_t aim_y{};
  std::int16_t aim_z{};
  std::int16_t speed{};
};

// The original arguments: the target, the aim (none to keep looking along the
// actor's look angles) and the step (kHeadTrackingUsesRadiusStep for the
// actor's default).
struct HeadTrackingRequest {
  std::int32_t target{};
  std::optional<Core::Math::Vec3> aim;
  std::int32_t step = kHeadTrackingUsesRadiusStep;
};

// The actor values a request without an aim or step reads.
struct ActorLook {
  std::uint16_t pitch{};
  std::uint16_t yaw{};
  std::uint16_t collision_radius{};
};

// The tracking `request` starts: the aim narrows to halfwords, a missing aim
// takes the look pitch and yaw and keeps the current z (func_8009FC60).
[[nodiscard]] HeadTracking StartHeadTracking(const HeadTracking& current,
                                             const HeadTrackingRequest& request,
                                             const ActorLook& look) noexcept;

// A bone's world transform seen from `camera`: the camera rotation times the
// bone's, and the bone's offset from the camera rotated into camera space
// (func_800A1DE8 after the bone is chosen).
[[nodiscard]] Core::Math::Transform3 BoneViewTransform(
    const Core::Math::Transform3& camera,
    const Core::Math::Transform3& bone) noexcept;

// The root selector's transform: the actor's Euler rotation and its halved,
// scaled root key rotated and placed at `position` (func_800A1DE8, bone
// 0xFF). The camera does not apply.
[[nodiscard]] Core::Math::Transform3 RootSelectorTransform(
    Core::Math::Angles3 angles, Core::Math::Vec3 root_key,
    std::int32_t actor_scale, Core::Math::Vec3 position) noexcept;

// The point at `endpoint` along a bone of `length` placed by `transform`:
// the base is its translation, the tip and midpoint the bone-local offset
// through it, each component truncated to a halfword (func_800A1C10).
[[nodiscard]] Core::Math::Vec3 BoneEndpointPoint(
    const Core::Math::Transform3& transform, std::int32_t length,
    BoneEndpoint endpoint) noexcept;

// `position` with y lowered by `height`, truncated to a halfword.
[[nodiscard]] Core::Math::Vec3 TargetPoint(Core::Math::Vec3 position,
                                           std::int32_t height) noexcept;

// `position` with y raised by the anchor height times the Q12 scale, with C
// division, truncated to a halfword.
[[nodiscard]] Core::Math::Vec3 AnchorPoint(Core::Math::Vec3 position,
                                           std::int32_t anchor_height,
                                           std::int32_t scale) noexcept;

// The signed halfwords of a packed word: screen x low, screen y high.
struct WordHalves {
  std::int16_t low{};
  std::int16_t high{};

  friend bool operator==(const WordHalves&, const WordHalves&) = default;
};
[[nodiscard]] WordHalves SplitWord(std::int32_t word) noexcept;
}  // namespace VS::Actors
