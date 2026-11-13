#pragma once

#include <array>
#include <cstdint>

#include "Battle/TargetingDraw.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"

namespace VS::Battle {
struct SphereVolumeGeometry {
  TargetVolumeExtents extents{};
  std::int16_t yaw{};
  Core::Math::Position3 origin{};
};

struct SphereContainmentTransform {
  Core::Math::Position3 origin{};
  std::array<std::int16_t, Core::Math::kMatrixOrder> reciprocal_extents{};
  Core::Math::Mat3 rotation{};

  [[nodiscard]] Core::Math::Position3 LocalPoint(
      Core::Math::Position3 point) const noexcept;
};

// The dispatcher changes the live extents before the selected query reads
// its point. Keeping that read in the query preserves shape/point aliases.
class SphereContainmentQuery {
 public:
  SphereContainmentQuery() = default;
  SphereContainmentQuery(const SphereContainmentQuery&) = delete;
  SphereContainmentQuery& operator=(const SphereContainmentQuery&) = delete;
  SphereContainmentQuery(SphereContainmentQuery&&) = delete;
  SphereContainmentQuery& operator=(SphereContainmentQuery&&) = delete;
  virtual ~SphereContainmentQuery() = default;

  [[nodiscard]] virtual std::uint8_t Kind() const = 0;
  [[nodiscard]] virtual TargetVolumeExtents Extents() const = 0;
  virtual void SetExtents(const TargetVolumeExtents& extents) = 0;
  [[nodiscard]] virtual int Ellipsoid() = 0;
  [[nodiscard]] virtual int Cylinder(int shift_height_origin) = 0;
  [[nodiscard]] virtual int Cone(int shift_height_origin) = 0;
  [[nodiscard]] virtual int Prism(int rectangular) = 0;
};

[[nodiscard]] SphereContainmentTransform BuildTargetVolumeTestTransform(
    const SphereVolumeGeometry& volume) noexcept;
[[nodiscard]] int IsPointInTargetEllipsoid(
    const SphereVolumeGeometry& volume, Core::Math::Position3 point) noexcept;
[[nodiscard]] int IsPointInTargetCylinder(const SphereVolumeGeometry& volume,
                                          Core::Math::Position3 point,
                                          int shift_height_origin) noexcept;
[[nodiscard]] int IsPointInTargetCone(const SphereVolumeGeometry& volume,
                                      Core::Math::Position3 point,
                                      int shift_height_origin) noexcept;
[[nodiscard]] int IsPointInTargetPrism(const SphereVolumeGeometry& volume,
                                       Core::Math::Position3 point,
                                       int rectangular) noexcept;
[[nodiscard]] int IsPointInTargetVolume(SphereContainmentQuery& query);
}  // namespace VS::Battle
