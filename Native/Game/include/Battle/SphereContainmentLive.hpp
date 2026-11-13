#pragma once

#include <cstdint>
#include <span>

#include "Battle/SphereContainment.hpp"

struct Battle_TargetVolume;

namespace VS::Battle {
[[nodiscard]] SphereContainmentTransform BuildLiveTargetVolumeTestTransform(
    const Battle_TargetVolume* volume);
[[nodiscard]] int IsLivePointInTargetEllipsoid(
    const Battle_TargetVolume* volume, std::span<const std::uint16_t> point);
[[nodiscard]] int IsLivePointInTargetCylinder(
    const Battle_TargetVolume* volume, std::span<const std::uint16_t> point,
    int shift_height_origin);
[[nodiscard]] int IsLivePointInTargetCone(const Battle_TargetVolume* volume,
                                          std::span<const std::uint16_t> point,
                                          int shift_height_origin);
[[nodiscard]] int IsLivePointInTargetPrism(const Battle_TargetVolume* volume,
                                           std::span<const std::uint16_t> point,
                                           int rectangular);
[[nodiscard]] int IsLivePointInTargetVolume(
    Battle_TargetVolume* volume, std::span<const std::uint16_t> point);

// Resolves the current sphere on every call; no owner survives a room reset.
[[nodiscard]] int IsLivePointInCurrentSphereVolume(
    int volume_index, std::span<const std::uint16_t> point);
}  // namespace VS::Battle
