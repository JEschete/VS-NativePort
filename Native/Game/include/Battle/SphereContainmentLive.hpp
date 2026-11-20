#pragma once

#include <cstdint>
#include <span>

#include "Battle/SphereContainment.hpp"

namespace VS::Battle {

struct TargetVolume;
[[nodiscard]] SphereContainmentTransform BuildLiveTargetVolumeTestTransform(
    const VS::Battle::TargetVolume* volume);
[[nodiscard]] int IsLivePointInTargetEllipsoid(
    const VS::Battle::TargetVolume* volume,
    std::span<const std::uint16_t> point);
[[nodiscard]] int IsLivePointInTargetCylinder(
    const VS::Battle::TargetVolume* volume,
    std::span<const std::uint16_t> point, int shift_height_origin);
[[nodiscard]] int IsLivePointInTargetCone(
    const VS::Battle::TargetVolume* volume,
    std::span<const std::uint16_t> point, int shift_height_origin);
[[nodiscard]] int IsLivePointInTargetPrism(
    const VS::Battle::TargetVolume* volume,
    std::span<const std::uint16_t> point, int rectangular);
[[nodiscard]] int IsLivePointInTargetVolume(
    VS::Battle::TargetVolume* volume, std::span<const std::uint16_t> point);

// Resolves the current sphere on every call; no owner survives a room reset.
[[nodiscard]] int IsLivePointInCurrentSphereVolume(
    int volume_index, std::span<const std::uint16_t> point);
}  // namespace VS::Battle
