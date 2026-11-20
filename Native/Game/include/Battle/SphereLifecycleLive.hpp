#pragma once

#include <cstdint>

#include "Battle/SphereReleaseLive.hpp"
#include "Battle/TargetVolume.h"

namespace VS::Battle {
using SphereReleaseFunction = void (*)(const void*);

// The caller allocates and clears the legacy workspace before initialization.
void InitializeLiveSphere(Battle_TargetSphereSetupView* sphere);

// A null sphere is already released. A nonnull sphere requires a release
// callback; the caller retains responsibility for resetting target pages.
void FreeLiveSphere(Battle_TargetSphereSetupView*& sphere,
                    SphereReleaseFunction release);

void SetLiveSphereVolumes(Battle_TargetSphereSetupView* sphere,
                          const Battle_TargetSphereSpec* primary,
                          const Battle_TargetSphereSpec* secondary,
                          std::uint8_t sphere_mode,
                          VS::Battle::TargetPreview* target_previews,
                          std::uint8_t& reuse_volumes);
void ClearLivePrimarySphereVolume(Battle_TargetSphereSetupView* sphere);
void SetLiveSecondarySphereVolume(Battle_TargetSphereSetupView* sphere,
                                  const Battle_TargetSphereSpec* secondary);
void ClearLiveSecondarySphereVolume(Battle_TargetSphereSetupView* sphere);
void ClearLiveSphereTargets(Battle_TargetSphereSetupView* sphere);
[[nodiscard]] int LiveSphereInteractionMode(
    const Battle_TargetSphereSetupView* sphere, bool override_button);

void InitializeSphereEntry();
void SetSphereVolumesEntry(Battle_TargetSphereSpec* primary,
                           Battle_TargetSphereSpec* secondary,
                           u_char sphere_mode,
                           VS::Battle::TargetPreview* target_previews);
void ClearPrimarySphereVolumeEntry();
void SetSecondarySphereVolumeEntry(Battle_TargetSphereSpec* secondary);
void ClearSecondarySphereVolumeEntry();
void ClearSphereTargetsEntry();
[[nodiscard]] int SphereInteractionModeEntry();
}  // namespace VS::Battle
