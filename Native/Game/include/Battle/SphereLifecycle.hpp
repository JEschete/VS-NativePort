#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Battle/Targeting.hpp"

namespace VS::Battle {
inline constexpr std::size_t kSphereWireEdgeCapacity = 0x302;
inline constexpr std::size_t kSphereWireEdgeCount = 720;

struct SphereLifecycleState {
  SphereVolume primary_volume{};
  SphereVolume secondary_volume{};
  std::int16_t wire_phase_a = 0;
  std::int16_t wire_phase_b = 0;
  std::int16_t wire_phase_c = 0;
  std::int16_t wire_shape = 0;
  std::int8_t target_count = 0;
  std::int8_t selected_target = 0;
  std::uint8_t sphere_mode = 0;
  std::uint32_t reserved_38 = 0;
  // The upper halfword is input_locked in the native sphere view.
  std::uint32_t reserved_3c_and_input_locked = 0;

  friend bool operator==(const SphereLifecycleState&,
                         const SphereLifecycleState&) = default;
};

enum class SphereVolumeSetResult {
  kReused,
  kSecondaryDisabled,
  kSecondarySet,
};

// F01279 initializes the typed lifecycle fields; the Live caller also resets
// the complete legacy workspace before mapping it here.
void InitializeSphereState(SphereLifecycleState& state) noexcept;

// Writes the four ordered terrain-grid edge lists into the first 720 words.
// Returns false without writing when the output span is too short.
[[nodiscard]] bool BuildSphereWireEdges(
    std::span<std::uint16_t> edges) noexcept;

// A null secondary volume is valid except when reusing the current volume.
// Invalid input is refused without modifying state or reuse.
[[nodiscard]] std::optional<SphereVolumeSetResult> SetSphereVolumes(
    SphereLifecycleState& state, const SphereVolume& primary,
    const SphereVolume* secondary, std::uint8_t sphere_mode,
    bool& reuse_volumes) noexcept;

void ClearPrimarySphereVolume(SphereLifecycleState& state) noexcept;
void SetSecondarySphereVolume(SphereLifecycleState& state,
                              const SphereVolume& secondary) noexcept;
void ClearSecondarySphereVolume(SphereLifecycleState& state) noexcept;
void ClearSphereTargetCount(SphereLifecycleState& state) noexcept;

[[nodiscard]] int SphereInteractionMode(const SphereLifecycleState* state,
                                        bool override_button) noexcept;
}  // namespace VS::Battle
