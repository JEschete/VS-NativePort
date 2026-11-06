#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>

#include "Effects/Camera.hpp"

struct Effects_EffectCamera;
struct Effects_EffectInstance;

// Live binding for the effect camera (Camera.hpp) over the copied effect
// camera globals (CameraLiveState.h): each entry loads them into an
// EffectCameraState, runs the typed core over the field camera, the
// renderer's camera, the current package's curves and the BIOS rand(), and
// stores them back. The entries keep the original signatures for the copied
// effect runtime, timeline and PLG128 callers; a null camera or instance and
// a transform index past the instance's sixteen are refused with a
// diagnostic.
namespace VS::Effects {
// The effect camera globals as a typed value, and back. Storing keeps the
// VECTOR pads and each channel's unused top bit.
[[nodiscard]] EffectCameraState LoadLiveEffectCameraState();
void StoreLiveEffectCameraState(const EffectCameraState& state);

// The host the entries run the core over.
[[nodiscard]] EffectCameraHost& LiveEffectCameraHost();

// func_800D1930 (5BF94.c:6474).
void LiveResetEffectCameraMotion();
// func_800D1B18 (5BF94.c:6516).
void LiveCaptureEffectCamera(Effects_EffectCamera* camera);
// func_800D1E20 (5BF94.c:6575).
void LiveSetEffectCameraTransitionMode(int transition_mode);
// func_800D1EF0 (5BF94.c:6606).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void LiveStartCameraShakeChannels(int channel_mask, int shape_curve,
                                  int amplitude, int step_uses_curve,
                                  int step_curve_or_value,
                                  int random_step_uses_curve,
                                  int random_step_curve_or_value);
// func_800D2560 (5BF94.c:6737).
void LiveUpdateEffectCameraMotion();
// func_800D66FC (5BF94.c:8308).
void LiveFocusCameraOnEffectTransform(Effects_EffectInstance* instance,
                                      std::uint8_t transform_index,
                                      std::uint8_t duration_frames);
// func_800D67C4 (5BF94.c:8323).
void LiveRestoreActionEffectCamera(Effects_EffectInstance* instance,
                                   std::uint8_t duration_frames);
}  // namespace VS::Effects
