#pragma once

#include <cstdint>

#include "Battle/SphereReleaseLive.hpp"
#include "Battle/TargetVolume.hpp"

namespace VS::Battle {
using SphereReleaseFunction = void (*)(const void*);

// The caller allocates and clears the legacy workspace before initialization.
void InitializeLiveSphere(VS::Battle::TargetSphereSetupView* sphere);

// A null sphere is already released. A nonnull sphere requires a release
// callback; the caller retains responsibility for resetting target pages.
void FreeLiveSphere(VS::Battle::TargetSphereSetupView*& sphere,
                    SphereReleaseFunction release);

void SetLiveSphereVolumes(VS::Battle::TargetSphereSetupView* sphere,
                          const VS::Battle::TargetSphereSpec* primary,
                          const VS::Battle::TargetSphereSpec* secondary,
                          std::uint8_t area_target_count,
                          VS::Battle::TargetPreview* target_previews,
                          std::uint8_t& reuse_volumes);
void ClearLivePrimarySphereVolume(VS::Battle::TargetSphereSetupView* sphere);
void SetLiveSecondarySphereVolume(
    VS::Battle::TargetSphereSetupView* sphere,
    const VS::Battle::TargetSphereSpec* secondary);
void ClearLiveSecondarySphereVolume(VS::Battle::TargetSphereSetupView* sphere);
void ClearLiveSphereTargets(VS::Battle::TargetSphereSetupView* sphere);
[[nodiscard]] int LiveSphereInteractionMode(
    const VS::Battle::TargetSphereSetupView* sphere, bool override_button);

void InitializeSphereEntry();
void SetSphereVolumesEntry(VS::Battle::TargetSphereSpec* primary,
                           VS::Battle::TargetSphereSpec* secondary,
                           std::uint8_t area_target_count,
                           VS::Battle::TargetPreview* target_previews);
void ClearPrimarySphereVolumeEntry();
void SetSecondarySphereVolumeEntry(VS::Battle::TargetSphereSpec* secondary);
void ClearSecondarySphereVolumeEntry();
void ClearSphereTargetsEntry();
[[nodiscard]] int SphereInteractionModeEntry();
}  // namespace VS::Battle
