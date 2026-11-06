#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>

#include "Core/Math/ScalarMath.hpp"
#include "Effects/Curves.hpp"

// The effect camera (BATTLE.PRG 5BF94.c:6474-6760, 8308-8335): a pose the
// effect timeline moves between two captured ends, plus four shake channels
// that offset it along the view axes. The cores work on an EffectCameraState
// value and reach the field camera, the curves and the random generator
// through an EffectCameraHost; CameraLive.hpp binds them to the battle's
// globals.
namespace VS::Effects {
inline constexpr int kCameraShakeChannels = 4;

// D_800F54D8_t (5BF94.h:386): a camera pose with its shake offsets.
struct EffectCameraPose {
  Core::Math::Vec3 position{};
  Core::Math::Vec3 look_at{};
  std::int32_t shake_lateral{};
  std::int32_t shake_vertical{};
  std::int32_t shake_depth{};
  std::int32_t shake_roll{};
  std::int32_t roll{};
  std::int32_t near_clip{};
  std::int32_t projection_distance{};
  std::int32_t far_clip{};

  friend bool operator==(const EffectCameraPose&,
                         const EffectCameraPose&) = default;
};

// D_800F54B8_t (5BF94.c:243): one shake channel. `shape_curve` names a
// segmented curve (five bits); the step and random step are either curve ids
// sampled at the phase or literal values.
struct CameraShakeChannel {
  std::uint8_t phase{};
  std::uint8_t shape_curve{};
  bool curve_step{};
  bool random_step{};
  std::uint8_t step_curve{};
  std::uint8_t random_step_curve{};
  std::uint8_t amplitude{};
  std::uint8_t angle{};

  friend bool operator==(const CameraShakeChannel&,
                         const CameraShakeChannel&) = default;
};

// The transition modes UpdateEffectCameraMotion runs; other values only keep
// the camera applied.
enum class CameraTransition : std::uint32_t {
  kNone = 0,
  kLinear = 1,
  kCosine = 2,
  kHold = 3,
};

// The effect camera globals (D_800F54B8-D_800F55E8).
struct EffectCameraState {
  std::array<CameraShakeChannel, kCameraShakeChannels> channels{};
  std::int16_t motion_frame{};
  EffectCameraPose camera{};
  std::uint8_t enabled_channels{};
  std::array<EffectCameraPose, 2> motion_ends{};
  std::int16_t motion_duration{};
  EffectCameraPose saved_action_camera{};
  std::uint32_t transition_mode{};

  friend bool operator==(const EffectCameraState&,
                         const EffectCameraState&) = default;
};

// The field camera as the effect camera reads it back.
struct FieldCameraSnapshot {
  Core::Math::Vec3 position{};
  Core::Math::Vec3 look_at{};
  std::int32_t roll{};
  std::int32_t near_clip{};
  std::int32_t projection_distance{};
  std::int32_t far_clip{};
};

// The camera ApplyEffectCamera hands to the field and the renderer.
// `texture_lod_hint` is the final depth offset the original leaves in the far
// clip call's second argument register (#279).
struct AppliedEffectCamera {
  Core::Math::Vec3 position{};
  Core::Math::Vec3 look_at{};
  std::int32_t roll{};
  std::int32_t near_clip{};
  std::int32_t projection_distance{};
  std::int32_t far_clip{};
  std::int32_t texture_lod_hint{};

  friend bool operator==(const AppliedEffectCamera&,
                         const AppliedEffectCamera&) = default;
};

// What the effect camera needs from the rest of the game.
class EffectCameraHost {
 public:
  virtual ~EffectCameraHost() = default;
  [[nodiscard]] virtual FieldCameraSnapshot CurrentCamera() = 0;
  virtual void ApplyCamera(const AppliedEffectCamera& camera) = 0;
  // SampleEffectCurve over the current package.
  [[nodiscard]] virtual CurveSample SampleCurve(std::int32_t curve_id,
                                                std::int32_t distance) = 0;
  // SampleSegmentedEffectCurve over the current package.
  [[nodiscard]] virtual CurveSample SampleSegmentedCurve(
      std::int32_t curve_index, std::int32_t distance) = 0;
  // The BIOS rand() draw (0..0x7FFF).
  [[nodiscard]] virtual std::int32_t Random() = 0;

 protected:
  EffectCameraHost() = default;
  EffectCameraHost(const EffectCameraHost&) = default;
  EffectCameraHost& operator=(const EffectCameraHost&) = default;
  EffectCameraHost(EffectCameraHost&&) = default;
  EffectCameraHost& operator=(EffectCameraHost&&) = default;
};

// The settings StartCameraShakeChannels gives each started channel, as the
// original's arguments (truncated to the channel's fields).
struct CameraShakeSettings {
  std::int32_t shape_curve{};
  std::int32_t amplitude{};
  std::int32_t step_uses_curve{};
  std::int32_t step_curve_or_value{};
  std::int32_t random_step_uses_curve{};
  std::int32_t random_step_curve_or_value{};
};

// func_800D1930 (5BF94.c:6474): stops every channel and transition.
void ResetEffectCameraMotion(EffectCameraState& state) noexcept;
// func_800D1B18 (5BF94.c:6516): reads the field camera into `pose`, leaving
// its shake offsets; the roll is stored half a turn round.
void CaptureEffectCamera(EffectCameraHost& host, EffectCameraPose& pose);
// func_800D1B80 (5BF94.c:6526): offsets `pose` by its shake along the view's
// right, up and depth axes and applies it.
void ApplyEffectCamera(const EffectCameraPose& pose, EffectCameraHost& host);
// func_800D1DE4 (5BF94.c:6564).
void EnableCameraShakeChannels(EffectCameraState& state,
                               std::int32_t channel_mask) noexcept;
// func_800D1DFC (5BF94.c:6566): a zero mask disables every channel.
void DisableCameraShakeChannels(EffectCameraState& state,
                                std::int32_t channel_mask) noexcept;
// func_800D1E20 (5BF94.c:6575).
void SetEffectCameraTransitionMode(EffectCameraState& state,
                                   std::int32_t transition_mode) noexcept;
// func_800D1E2C (5BF94.c:6577): the fixed per-frame angle step.
[[nodiscard]] std::uint8_t CameraShakePhaseStep(
    const CameraShakeChannel& channel, EffectCameraHost& host);
// func_800D1E78 (5BF94.c:6589): a random angle step below the channel's
// limit, or 0 for a zero limit.
[[nodiscard]] std::uint8_t RandomCameraShakePhaseStep(
    const CameraShakeChannel& channel, EffectCameraHost& host);
// func_800D1EF0 (5BF94.c:6606): starts every channel in the low four bits of
// `channel_mask`.
void StartCameraShakeChannels(EffectCameraState& state, EffectCameraHost& host,
                              std::int32_t channel_mask,
                              const CameraShakeSettings& settings);
// func_800D1FEC (5BF94.c:6626).
void StopCameraShakeChannels(EffectCameraState& state,
                             std::int32_t channel_mask) noexcept;
// func_800D206C (5BF94.c:6638): advances every enabled channel along its
// shape curve, stopping it at the curve's end.
void UpdateCameraShakeChannels(EffectCameraState& state,
                               EffectCameraHost& host);
// func_800D21C0 (5BF94.c:6681).
void InterpolateEffectCameraLinear(EffectCameraState& state) noexcept;
// func_800D236C (5BF94.c:6708).
void InterpolateEffectCameraCosine(EffectCameraState& state) noexcept;
// func_800D2518 (5BF94.c:6735).
void HoldEffectCameraStartPose(EffectCameraState& state) noexcept;
// func_800D2560 (5BF94.c:6737): one frame of the transition and the shake.
void UpdateEffectCameraMotion(EffectCameraState& state, EffectCameraHost& host);
// func_800D66FC (5BF94.c:8308): eases the look-at towards a transform's
// world translation over `duration_frames` (at least one).
void FocusCameraOnEffectTransform(EffectCameraState& state,
                                  EffectCameraHost& host,
                                  Core::Math::Vec3 transform_translation,
                                  std::uint8_t duration_frames);
// func_800D67C4 (5BF94.c:8323): eases back to the saved action camera.
void RestoreActionEffectCamera(EffectCameraState& state, EffectCameraHost& host,
                               std::uint8_t duration_frames);
}  // namespace VS::Effects
