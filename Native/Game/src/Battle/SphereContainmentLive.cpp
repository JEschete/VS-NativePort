#include "Battle/SphereContainmentLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>

#include "Battle/BattleTypes.hpp"
#include "Battle/Sphere.hpp"
#include "Battle/TargetVolume.hpp"
#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

using VS::Battle::TargetVolume;

namespace VS::Battle {
namespace {
namespace math = Core::Math;
constexpr std::size_t kPointComponents = math::kMatrixOrder;
constexpr std::size_t kSphereVolumes = 2;
static_assert(sizeof(TargetVolume{}.scale) == sizeof(TargetVolumeExtents));

const TargetVolume& RequiredVolume(const TargetVolume* volume) {
  if (volume == nullptr) {
    VS::Core::LogError("battle.sphere", "Containment requires a target volume");
    std::abort();
  }
  return *volume;
}

TargetVolumeExtents ReadExtents(const TargetVolume& volume) {
  TargetVolumeExtents extents{};
  std::memcpy(extents.data(), &volume.scale, sizeof extents);
  return extents;
}

SphereVolumeGeometry Geometry(const TargetVolume& volume) {
  return {
      .extents = ReadExtents(volume),
      .yaw = volume.angle,
      .origin = {volume.position[0], volume.position[1], volume.position[2]}};
}

math::Position3 RequiredPoint(std::span<const std::uint16_t> point) {
  if (point.size() < kPointComponents) {
    VS::Core::LogError("battle.sphere",
                       "Containment point requires three components; got {}",
                       point.size());
    std::abort();
  }
  std::array<std::uint16_t, kPointComponents> components{};
  std::memcpy(components.data(), point.data(), sizeof components);
  return {math::WrapSigned16(components[0]), math::WrapSigned16(components[1]),
          math::WrapSigned16(components[2])};
}

class LiveContainmentQuery final : public SphereContainmentQuery {
 public:
  LiveContainmentQuery(TargetVolume& volume,
                       std::span<const std::uint16_t> point)
      : volume_(volume), point_(point) {}

  std::uint8_t Kind() const override { return volume_.kind; }
  TargetVolumeExtents Extents() const override { return ReadExtents(volume_); }
  void SetExtents(const TargetVolumeExtents& extents) override {
    std::memcpy(&volume_.scale, extents.data(), sizeof extents);
  }
  int Ellipsoid() override {
    return IsLivePointInTargetEllipsoid(&volume_, point_);
  }
  int Cylinder(int shift_height_origin) override {
    return IsLivePointInTargetCylinder(&volume_, point_, shift_height_origin);
  }
  int Cone(int shift_height_origin) override {
    return IsLivePointInTargetCone(&volume_, point_, shift_height_origin);
  }
  int Prism(int rectangular) override {
    return IsLivePointInTargetPrism(&volume_, point_, rectangular);
  }

 private:
  TargetVolume& volume_;
  std::span<const std::uint16_t> point_;
};
}  // namespace

// decomp: func_800C0FA8 (src/BATTLE/BATTLE.PRG/58578.c:119).
SphereContainmentTransform BuildLiveTargetVolumeTestTransform(
    const TargetVolume* volume) {
  return BuildTargetVolumeTestTransform(Geometry(RequiredVolume(volume)));
}

// decomp: func_800C1034 (src/BATTLE/BATTLE.PRG/58578.c:188).
int IsLivePointInTargetEllipsoid(const TargetVolume* volume,
                                 std::span<const std::uint16_t> point) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetEllipsoid(geometry, RequiredPoint(point));
}

// decomp: func_800C110C (src/BATTLE/BATTLE.PRG/58578.c:242).
int IsLivePointInTargetCylinder(const TargetVolume* volume,
                                std::span<const std::uint16_t> point,
                                int shift_height_origin) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetCylinder(geometry, RequiredPoint(point),
                                 shift_height_origin);
}

// decomp: func_800C123C (src/BATTLE/BATTLE.PRG/58578.c:248).
int IsLivePointInTargetCone(const TargetVolume* volume,
                            std::span<const std::uint16_t> point,
                            int shift_height_origin) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetCone(geometry, RequiredPoint(point),
                             shift_height_origin);
}

// decomp: func_800C1384 (src/BATTLE/BATTLE.PRG/58578.c:348).
int IsLivePointInTargetPrism(const TargetVolume* volume,
                             std::span<const std::uint16_t> point,
                             int rectangular) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetPrism(geometry, RequiredPoint(point), rectangular);
}

// decomp: func_800C1564 (src/BATTLE/BATTLE.PRG/58578.c:360).
int IsLivePointInTargetVolume(TargetVolume* volume,
                              std::span<const std::uint16_t> point) {
  static_cast<void>(RequiredVolume(volume));
  LiveContainmentQuery query(*volume, point);
  return IsPointInTargetVolume(query);
}

int IsLivePointInCurrentSphereVolume(int volume_index,
                                     std::span<const std::uint16_t> point) {
  if (volume_index < 0 ||
      static_cast<std::size_t>(volume_index) >= kSphereVolumes) {
    VS::Core::LogError("battle.sphere",
                       "Containment volume index {} is invalid", volume_index);
    std::abort();
  }
  auto* sphere = reinterpret_cast<VS::Battle::TargetSphereRecord*>(
      VS::Battle::targetSphere);
  if (sphere == nullptr) {
    VS::Core::LogError("battle.sphere",
                       "Containment requires the current sphere");
    std::abort();
  }
  static_assert(std::size(VS::Battle::TargetSphereRecord{}.volumes) ==
                kSphereVolumes);
  return IsLivePointInTargetVolume(
      &sphere->volumes[static_cast<std::size_t>(volume_index)], point);
}
}  // namespace VS::Battle
