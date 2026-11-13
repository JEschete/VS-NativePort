#include "Battle/SphereContainment.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle {
namespace {
namespace math = Core::Math;
constexpr std::size_t kExtentX = 0;
constexpr std::size_t kExtentY = 1;
constexpr std::size_t kExtentZ = 2;
constexpr std::size_t kPitch = 3;
constexpr int kPitchScale = 16;
constexpr int kExtentScale = 32;
constexpr int kFixedFractionBits = 12;
constexpr int kReciprocalNumerator = 32768;
constexpr std::int32_t kUnitRadius = 256;
constexpr std::uint32_t kUnitRadiusSquared = 65536;
constexpr int kConeSlopeScale = 8;
constexpr std::size_t kPrismCorners = 4;
constexpr std::uint8_t kReversePitch = 128;

enum class VolumeKind : std::uint8_t {
  kEllipsoid = 1,
  kCenteredCylinder = 2,
  kCylinder = 3,
  kCone = 4,
  kReversedCone = 5,
  kRectangle = 6,
  kDiamond = 7,
};

math::Mat3 Rotation(const SphereVolumeGeometry& volume) noexcept {
  return math::BuildRotationXyz(
      {volume.extents[kPitch] * kPitchScale,
       math::WrapSigned16(0U - static_cast<std::uint16_t>(volume.yaw)), 0});
}

std::int16_t SaturatedComponent(std::int32_t value) noexcept {
  return static_cast<std::int16_t>(std::clamp(
      value,
      static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
      static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
}

math::Position3 RotatePoint(math::Position3 origin, const math::Mat3& rotation,
                            math::Position3 point) noexcept {
  const math::Vec3 local{
      math::WrapSigned16(static_cast<std::uint32_t>(point.x - origin.x)),
      math::WrapSigned16(static_cast<std::uint32_t>(point.y - origin.y)),
      math::WrapSigned16(static_cast<std::uint32_t>(point.z - origin.z))};
  const auto rotated = math::RotateTranslate({rotation, {}}, local);
  return {SaturatedComponent(rotated.x), SaturatedComponent(rotated.y),
          SaturatedComponent(rotated.z)};
}

std::int32_t Square(std::int32_t value) noexcept {
  return math::WrappingMultiply(value, value);
}

std::int32_t NormalizedSquare(std::int16_t component,
                              std::int16_t reciprocal) noexcept {
  const auto scaled =
      math::WrappingMultiply(component, reciprocal) >> kFixedFractionBits;
  return Square(scaled);
}

std::int32_t HorizontalSquared(const SphereContainmentTransform& transform,
                               math::Position3 rotated) noexcept {
  return math::WrappingAdd(
      NormalizedSquare(rotated.x, transform.reciprocal_extents[kExtentX]),
      NormalizedSquare(rotated.z, transform.reciprocal_extents[kExtentZ]));
}

bool WithinHeight(std::int16_t point_y, std::uint8_t height) noexcept {
  return static_cast<std::uint16_t>(-static_cast<std::int32_t>(point_y)) <=
         height * kExtentScale;
}

struct HorizontalPoint {
  std::int16_t x{};
  std::int16_t z{};
};

std::int32_t EdgeWinding(HorizontalPoint point, HorizontalPoint current,
                         HorizontalPoint next) noexcept {
  const auto first_x = std::int64_t{current.x} - point.x;
  const auto first_z = std::int64_t{current.z} - point.z;
  const auto next_x = std::int64_t{next.x} - point.x;
  const auto next_z = std::int64_t{next.z} - point.z;
  const auto determinant = first_x * next_z - first_z * next_x;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(determinant));
}
}  // namespace

// ApplyMatrixSV (src/SLUS_010.40/libgte/MTX_06.s) stores saturated IR halves.
math::Position3 SphereContainmentTransform::LocalPoint(
    math::Position3 point) const noexcept {
  return RotatePoint(origin, rotation, point);
}

// decomp: func_800C0FA8 (src/BATTLE/BATTLE.PRG/58578.c:170).
SphereContainmentTransform BuildTargetVolumeTestTransform(
    const SphereVolumeGeometry& volume) noexcept {
  SphereContainmentTransform transform{.origin = volume.origin,
                                       .rotation = Rotation(volume)};
  for (std::size_t axis = 0; axis < transform.reciprocal_extents.size();
       ++axis) {
    transform.reciprocal_extents[axis] =
        math::WrapSigned16(static_cast<std::uint32_t>(
            math::R3000Quotient(kReciprocalNumerator, volume.extents[axis])));
  }
  return transform;
}

// decomp: func_800C1034 (src/BATTLE/BATTLE.PRG/58578.c:188).
int IsPointInTargetEllipsoid(const SphereVolumeGeometry& volume,
                             math::Position3 point) noexcept {
  const auto transform = BuildTargetVolumeTestTransform(volume);
  const auto rotated = transform.LocalPoint(point);
  const auto sum = math::WrappingAdd(
      HorizontalSquared(transform, rotated),
      NormalizedSquare(rotated.y, transform.reciprocal_extents[kExtentY]));
  return static_cast<std::uint32_t>(sum) < kUnitRadiusSquared ? 1 : 0;
}

// decomp: func_800C110C (src/BATTLE/BATTLE.PRG/58578.c:242).
int IsPointInTargetCylinder(const SphereVolumeGeometry& volume,
                            math::Position3 point,
                            int shift_height_origin) noexcept {
  const auto transform = BuildTargetVolumeTestTransform(volume);
  auto rotated = transform.LocalPoint(point);
  if (shift_height_origin != 0) {
    rotated.y = math::WrapSigned16(static_cast<std::uint32_t>(
        rotated.y - volume.extents[kExtentY] * kPitchScale));
  }
  if (!WithinHeight(rotated.y, volume.extents[kExtentY])) return 0;
  return static_cast<std::uint32_t>(HorizontalSquared(transform, rotated)) <
                 kUnitRadiusSquared
             ? 1
             : 0;
}

// decomp: func_800C123C (src/BATTLE/BATTLE.PRG/58578.c:299).
int IsPointInTargetCone(const SphereVolumeGeometry& volume,
                        math::Position3 point,
                        int shift_height_origin) noexcept {
  const auto transform = BuildTargetVolumeTestTransform(volume);
  auto rotated = transform.LocalPoint(point);
  if (shift_height_origin != 0) {
    rotated.y = math::WrapSigned16(static_cast<std::uint32_t>(
        rotated.y - volume.extents[kExtentY] * kExtentScale));
  }
  if (!WithinHeight(rotated.y, volume.extents[kExtentY])) return 0;
  const auto divisor = math::R3000Quotient(
      -static_cast<std::int32_t>(rotated.y) * kConeSlopeScale,
      volume.extents[kExtentY]);
  const auto radius = math::WrappingSubtract(kUnitRadius, divisor);
  return HorizontalSquared(transform, rotated) < Square(radius) ? 1 : 0;
}

// decomp: func_800C1384 (src/BATTLE/BATTLE.PRG/58578.c:348).
int IsPointInTargetPrism(const SphereVolumeGeometry& volume,
                         math::Position3 point, int rectangular) noexcept {
  const SphereContainmentTransform transform{.origin = volume.origin,
                                             .rotation = Rotation(volume)};
  const auto rotated = transform.LocalPoint(point);
  if (!WithinHeight(rotated.y, volume.extents[kExtentY])) return 0;
  const auto x =
      static_cast<std::int16_t>(volume.extents[kExtentX] * kExtentScale);
  const auto z =
      static_cast<std::int16_t>(volume.extents[kExtentZ] * kExtentScale);
  const std::int16_t offset_z = rectangular != 0 ? z : 0;
  const auto offset_x = static_cast<std::int16_t>(rectangular != 0 ? -x : 0);
  const std::array<HorizontalPoint, kPrismCorners> corners{{
      {static_cast<std::int16_t>(-x), static_cast<std::int16_t>(-offset_z)},
      {static_cast<std::int16_t>(-offset_x), static_cast<std::int16_t>(-z)},
      {x, offset_z},
      {offset_x, z},
  }};
  for (std::size_t edge = 0; edge < corners.size(); ++edge) {
    if (EdgeWinding({rotated.x, rotated.z}, corners[edge],
                    corners[(edge + 1) % corners.size()]) < 0) {
      return 0;
    }
  }
  return 1;
}

// decomp: func_800C1564 (src/BATTLE/BATTLE.PRG/58578.c:411).
int IsPointInTargetVolume(SphereContainmentQuery& query) {
  const auto kind = query.Kind();
  const auto saved = query.Extents();
  int result = 0;
  switch (static_cast<VolumeKind>(kind)) {
    case VolumeKind::kEllipsoid:
      result = query.Ellipsoid();
      break;
    case VolumeKind::kCenteredCylinder: {
      auto doubled = saved;
      doubled[kExtentY] = static_cast<std::uint8_t>(saved[kExtentY] * 2U);
      query.SetExtents(doubled);
      result = query.Cylinder(1);
      break;
    }
    case VolumeKind::kCylinder:
      result = query.Cylinder(0);
      break;
    case VolumeKind::kCone:
      result = query.Cone(0);
      break;
    case VolumeKind::kReversedCone: {
      auto reversed = saved;
      reversed[kPitch] =
          static_cast<std::uint8_t>(saved[kPitch] + kReversePitch);
      query.SetExtents(reversed);
      result = query.Cone(1);
      break;
    }
    case VolumeKind::kRectangle:
      result = query.Prism(1);
      break;
    case VolumeKind::kDiamond:
      result = query.Prism(0);
      break;
    default:
      break;
  }
  query.SetExtents(saved);
  return result;
}
}  // namespace VS::Battle
