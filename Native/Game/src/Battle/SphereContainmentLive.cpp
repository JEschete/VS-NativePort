#include "Battle/SphereContainmentLive.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <span>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
extern "C" {
#include "Battle/BattleTypes.h"
#include "Battle/Sphere.h"
#include "Battle/TargetVolume.h"
}

namespace VS::Battle {
namespace {
namespace math = Core::Math;
constexpr std::size_t kPointComponents = math::kMatrixOrder;
constexpr std::size_t kSphereVolumes = 2;
static_assert(sizeof(Battle_TargetVolume{}.scale) ==
              sizeof(TargetVolumeExtents));

const Battle_TargetVolume& RequiredVolume(const Battle_TargetVolume* volume) {
  if (volume == nullptr) {
    VS_LOG_ERROR("battle.sphere", "Containment requires a target volume");
    std::abort();
  }
  return *volume;
}

TargetVolumeExtents ReadExtents(const Battle_TargetVolume& volume) {
  TargetVolumeExtents extents{};
  std::memcpy(extents.data(), &volume.scale, sizeof extents);
  return extents;
}

SphereVolumeGeometry Geometry(const Battle_TargetVolume& volume) {
  return {
      .extents = ReadExtents(volume),
      .yaw = volume.angle,
      .origin = {volume.position[0], volume.position[1], volume.position[2]}};
}

math::Position3 RequiredPoint(std::span<const std::uint16_t> point) {
  if (point.size() < kPointComponents) {
    VS_LOG_ERROR("battle.sphere",
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
  LiveContainmentQuery(Battle_TargetVolume& volume,
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
  Battle_TargetVolume& volume_;
  std::span<const std::uint16_t> point_;
};
}  // namespace

// decomp: func_800C0FA8 (src/BATTLE/BATTLE.PRG/58578.c:170).
SphereContainmentTransform BuildLiveTargetVolumeTestTransform(
    const Battle_TargetVolume* volume) {
  return BuildTargetVolumeTestTransform(Geometry(RequiredVolume(volume)));
}

// decomp: func_800C1034 (src/BATTLE/BATTLE.PRG/58578.c:188).
int IsLivePointInTargetEllipsoid(const Battle_TargetVolume* volume,
                                 std::span<const std::uint16_t> point) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetEllipsoid(geometry, RequiredPoint(point));
}

// decomp: func_800C110C (src/BATTLE/BATTLE.PRG/58578.c:242).
int IsLivePointInTargetCylinder(const Battle_TargetVolume* volume,
                                std::span<const std::uint16_t> point,
                                int shift_height_origin) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetCylinder(geometry, RequiredPoint(point),
                                 shift_height_origin);
}

// decomp: func_800C123C (src/BATTLE/BATTLE.PRG/58578.c:299).
int IsLivePointInTargetCone(const Battle_TargetVolume* volume,
                            std::span<const std::uint16_t> point,
                            int shift_height_origin) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetCone(geometry, RequiredPoint(point),
                             shift_height_origin);
}

// decomp: func_800C1384 (src/BATTLE/BATTLE.PRG/58578.c:348).
int IsLivePointInTargetPrism(const Battle_TargetVolume* volume,
                             std::span<const std::uint16_t> point,
                             int rectangular) {
  const auto geometry = Geometry(RequiredVolume(volume));
  return IsPointInTargetPrism(geometry, RequiredPoint(point), rectangular);
}

// decomp: func_800C1564 (src/BATTLE/BATTLE.PRG/58578.c:411).
int IsLivePointInTargetVolume(Battle_TargetVolume* volume,
                              std::span<const std::uint16_t> point) {
  static_cast<void>(RequiredVolume(volume));
  LiveContainmentQuery query(*volume, point);
  return IsPointInTargetVolume(query);
}

int IsLivePointInCurrentSphereVolume(int volume_index,
                                     std::span<const std::uint16_t> point) {
  if (volume_index < 0 ||
      static_cast<std::size_t>(volume_index) >= kSphereVolumes) {
    VS_LOG_ERROR("battle.sphere", "Containment volume index {} is invalid",
                 volume_index);
    std::abort();
  }
  auto* sphere = reinterpret_cast<Battle_TargetSphere*>(Battle_targetSphere);
  if (sphere == nullptr) {
    VS_LOG_ERROR("battle.sphere", "Containment requires the current sphere");
    std::abort();
  }
  static_assert(std::size(Battle_TargetSphere{}.volumes) == kSphereVolumes);
  return IsLivePointInTargetVolume(
      &sphere->volumes[static_cast<std::size_t>(volume_index)], point);
}
}  // namespace VS::Battle
