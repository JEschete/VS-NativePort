#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "Battle/BattleTypes.h"
#include "Battle/Sphere.hpp"
#include "Battle/SphereContainmentLive.hpp"
#include "Battle/TargetVolume.hpp"

namespace VS::test_support::sphere_containment {
using Point = std::array<std::uint16_t, 3>;
using VolumeBytes = std::array<std::uint8_t, 16>;
inline constexpr Battle::TargetVolumeExtents kDefaultExtents{8, 4, 8, 0};

inline Battle_TargetVolume Volume(
    std::uint8_t kind, Battle::TargetVolumeExtents extents = kDefaultExtents,
    Core::Math::Position3 origin = {}) {
  Battle_TargetVolume volume{};
  volume.kind = kind;
  std::memcpy(&volume.scale, extents.data(), sizeof extents);
  volume.position[0] = origin.x;
  volume.position[1] = origin.y;
  volume.position[2] = origin.z;
  return volume;
}

inline VolumeBytes Bytes(const Battle_TargetVolume& volume) {
  return std::bit_cast<VolumeBytes>(volume);
}

inline std::span<const std::uint16_t> FlagsPoint(
    const Battle_TargetVolume& volume) {
  return {reinterpret_cast<const std::uint16_t*>(&volume.scale), 3};
}

class State {
 public:
  State() : saved_(VS::Battle::targetSphere) { Select(0); }
  State(const State&) = delete;
  State& operator=(const State&) = delete;
  State(State&&) = delete;
  State& operator=(State&&) = delete;
  ~State() { VS::Battle::targetSphere = saved_; }

  void Select(std::size_t owner) {
    VS::Battle::targetSphere =
        reinterpret_cast<VS::Battle::TargetSphereSetupView*>(
            &spheres.at(owner));
  }

  std::array<VS::Battle::TargetSphereRecord, 2> spheres{};

 private:
  VS::Battle::TargetSphereSetupView* saved_;
};
}  // namespace VS::test_support::sphere_containment
