// Hand-maintained (tools/port/owned.csv).
#include "Effects/Camera.hpp"  // NOLINT(build/include): typed API

#include <cstddef>
#include <cstdint>

#include "Core/Math/Interpolation.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Curves.hpp"

namespace VS::Effects {
namespace {
namespace Math = VS::Core::Math;

constexpr int kFractionBits = 12;
// The captured roll is kept half a turn round (ONE * 8, 5BF94.c:6519).
constexpr std::int32_t kRollBias = 0x8000;
constexpr std::uint8_t kStoppedPhase = 0xFF;
constexpr std::uint8_t kShapeCurveMask = 0x1F;
// The shake angle steps in 1/128 turns (angle * 32 of 4096, 5BF94.c:6652).
constexpr std::int32_t kShakeAngleScale = 32;
constexpr int kShakeAmplitudeBits = 8;
constexpr int kLateralChannel = 0;
constexpr int kVerticalChannel = 1;
constexpr int kDepthChannel = 2;
constexpr int kRollChannel = 3;

[[nodiscard]] constexpr std::int32_t Whole(std::int32_t fixed) noexcept {
  return fixed >> kFractionBits;
}

[[nodiscard]] constexpr std::int32_t Fixed(std::int32_t whole) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(whole)
                                   << kFractionBits);
}

[[nodiscard]] constexpr Math::Vec3 WholeVector(Math::Vec3 fixed) noexcept {
  return {.x = Whole(fixed.x), .y = Whole(fixed.y), .z = Whole(fixed.z)};
}

[[nodiscard]] constexpr Math::Vec3 FixedVector(Math::Vec3 whole) noexcept {
  return {.x = Fixed(whole.x), .y = Fixed(whole.y), .z = Fixed(whole.z)};
}

[[nodiscard]] constexpr Math::Vec3 Add(Math::Vec3 left,
                                       Math::Vec3 right) noexcept {
  return {.x = Math::WrappingAdd(left.x, right.x),
          .y = Math::WrappingAdd(left.y, right.y),
          .z = Math::WrappingAdd(left.z, right.z)};
}

[[nodiscard]] constexpr bool ChannelBit(std::uint32_t mask,
                                        int channel) noexcept {
  return ((mask >> channel) & 1U) != 0;
}

[[nodiscard]] constexpr std::int32_t ChannelMask(int channel) noexcept {
  return std::int32_t{1} << channel;
}

// The fields both transitions ease between the two motion ends.
template <typename Vector, typename Scalar>
void InterpolatePose(EffectCameraState& state, Vector vector, Scalar scalar) {
  const EffectCameraPose& from = state.motion_ends[0];
  const EffectCameraPose& to = state.motion_ends[1];
  const Math::StepProgress progress{.current = state.motion_frame,
                                    .total = state.motion_duration};
  EffectCameraPose& camera = state.camera;
  camera.position = FixedVector(
      vector(WholeVector(from.position), WholeVector(to.position), progress));
  camera.look_at = FixedVector(
      vector(WholeVector(from.look_at), WholeVector(to.look_at), progress));
  camera.roll = scalar({.start = from.roll, .end = to.roll}, progress);
  camera.far_clip =
      scalar({.start = from.far_clip, .end = to.far_clip}, progress);
  camera.near_clip =
      scalar({.start = from.near_clip, .end = to.near_clip}, progress);
}

void StoreShake(EffectCameraPose& camera, int channel, std::int32_t offset) {
  switch (channel) {
    case kLateralChannel:
      camera.shake_lateral = offset;
      break;
    case kVerticalChannel:
      camera.shake_vertical = offset;
      break;
    case kDepthChannel:
      camera.shake_depth = offset;
      break;
    case kRollChannel:
      camera.shake_roll = offset >> kFractionBits;
      break;
    default:
      break;
  }
}

void BeginEasedTransition(EffectCameraState& state,
                          std::uint8_t duration_frames) noexcept {
  state.motion_frame = 0;
  state.motion_duration =
      static_cast<std::int16_t>(duration_frames == 0 ? 1 : duration_frames);
  SetEffectCameraTransitionMode(
      state, static_cast<std::int32_t>(CameraTransition::kCosine));
}
}  // namespace

// decomp: BATTLE.PRG func_800D1930 (src/BATTLE/BATTLE.PRG/5BF94.c:6474)
void ResetEffectCameraMotion(EffectCameraState& state) noexcept {
  state.enabled_channels = 0;
  state.transition_mode = 0;
  state.motion_frame = 0;
  state.motion_duration = 0;
  for (CameraShakeChannel& channel : state.channels) {
    channel.phase = kStoppedPhase;
  }
}

// decomp: BATTLE.PRG func_800D1B18 (src/BATTLE/BATTLE.PRG/5BF94.c:6516)
void CaptureEffectCamera(EffectCameraHost& host, EffectCameraPose& pose) {
  const FieldCameraSnapshot field = host.CurrentCamera();
  pose.position = field.position;
  pose.look_at = field.look_at;
  pose.roll = Math::WrappingAdd(field.roll, kRollBias);
  pose.near_clip = field.near_clip;
  pose.projection_distance = field.projection_distance;
  pose.far_clip = field.far_clip;
}

// decomp: BATTLE.PRG func_800D1B80 (src/BATTLE/BATTLE.PRG/5BF94.c:6526)
void ApplyEffectCamera(const EffectCameraPose& pose, EffectCameraHost& host) {
  const std::int32_t dx =
      Whole(Math::WrappingSubtract(pose.position.x, pose.look_at.x));
  const std::int32_t dz =
      Whole(Math::WrappingSubtract(pose.position.z, pose.look_at.z));
  const std::int32_t yaw = Math::Arctangent2(dx, dz);
  const std::int32_t lateral = Whole(pose.shake_lateral);
  Math::Vec3 offset{.x = Math::WrappingMultiply(Math::Cosine(yaw), lateral),
                    .y = 0,
                    .z = Math::WrappingMultiply(-Math::Sine(yaw), lateral)};
  const std::int32_t pitch = Math::Arctangent2(
      Whole(Math::WrappingSubtract(pose.position.y, pose.look_at.y)),
      Math::SquareRootInteger(Math::WrappingAdd(
          Math::WrappingMultiply(dx, dx), Math::WrappingMultiply(dz, dz))));
  const std::int32_t depth = Whole(pose.shake_depth);
  const std::int32_t vertical = Whole(pose.shake_vertical);
  offset.y =
      Math::WrappingAdd(Math::WrappingMultiply(-Math::Sine(pitch), depth),
                        Math::WrappingMultiply(Math::Cosine(pitch), vertical));
  const std::int32_t horizontal = Whole(
      Math::WrappingAdd(Math::WrappingMultiply(-Math::Cosine(pitch), depth),
                        Math::WrappingMultiply(-Math::Sine(pitch), vertical)));
  offset.x = Math::WrappingAdd(
      offset.x, Math::WrappingMultiply(Math::Sine(yaw), horizontal));
  offset.z = Math::WrappingAdd(
      offset.z, Math::WrappingMultiply(Math::Cosine(yaw), horizontal));
  host.ApplyCamera({
      .position = Add(pose.position, offset),
      .look_at = Add(pose.look_at, offset),
      .roll = Math::WrappingAdd(
          pose.roll, Math::WrappingSubtract(pose.shake_roll, kRollBias)),
      .near_clip = pose.near_clip,
      .projection_distance = pose.projection_distance,
      .far_clip = pose.far_clip,
      .texture_lod_hint = offset.z,
  });
}

// decomp: BATTLE.PRG func_800D1DE4 (src/BATTLE/BATTLE.PRG/5BF94.c:6564)
void EnableCameraShakeChannels(EffectCameraState& state,
                               std::int32_t channel_mask) noexcept {
  state.enabled_channels = static_cast<std::uint8_t>(
      state.enabled_channels | static_cast<std::uint32_t>(channel_mask));
}

// decomp: BATTLE.PRG func_800D1DFC (src/BATTLE/BATTLE.PRG/5BF94.c:6566)
void DisableCameraShakeChannels(EffectCameraState& state,
                                std::int32_t channel_mask) noexcept {
  if (channel_mask != 0) {
    state.enabled_channels = static_cast<std::uint8_t>(
        state.enabled_channels & ~static_cast<std::uint32_t>(channel_mask));
    return;
  }
  state.enabled_channels = 0;
}

// decomp: BATTLE.PRG func_800D1E20 (src/BATTLE/BATTLE.PRG/5BF94.c:6575)
void SetEffectCameraTransitionMode(EffectCameraState& state,
                                   std::int32_t transition_mode) noexcept {
  state.transition_mode = static_cast<std::uint32_t>(transition_mode);
}

// decomp: BATTLE.PRG func_800D1E2C (src/BATTLE/BATTLE.PRG/5BF94.c:6577)
std::uint8_t CameraShakePhaseStep(const CameraShakeChannel& channel,
                                  EffectCameraHost& host) {
  if (!channel.curve_step) {
    return channel.step_curve;
  }
  return static_cast<std::uint8_t>(
      host.SampleCurve(channel.step_curve, channel.phase).value);
}

// decomp: BATTLE.PRG func_800D1E78 (src/BATTLE/BATTLE.PRG/5BF94.c:6589)
std::uint8_t RandomCameraShakePhaseStep(const CameraShakeChannel& channel,
                                        EffectCameraHost& host) {
  const std::int32_t limit =
      channel.random_step
          ? host.SampleCurve(channel.random_step_curve, channel.phase).value
          : channel.random_step_curve;
  if (limit == 0) {
    return 0;
  }
  return static_cast<std::uint8_t>(host.Random() % limit);
}

// decomp: BATTLE.PRG func_800D1EF0 (src/BATTLE/BATTLE.PRG/5BF94.c:6606)
void StartCameraShakeChannels(EffectCameraState& state, EffectCameraHost& host,
                              std::int32_t channel_mask,
                              const CameraShakeSettings& settings) {
  for (int i = 0; i < kCameraShakeChannels; ++i) {
    if (!ChannelBit(static_cast<std::uint32_t>(channel_mask), i)) {
      continue;
    }
    CameraShakeChannel& channel = state.channels[static_cast<std::size_t>(i)];
    channel.shape_curve =
        static_cast<std::uint8_t>(settings.shape_curve & kShapeCurveMask);
    channel.amplitude = static_cast<std::uint8_t>(settings.amplitude);
    channel.curve_step = (settings.step_uses_curve & 1) != 0;
    channel.step_curve =
        static_cast<std::uint8_t>(settings.step_curve_or_value);
    channel.random_step = (settings.random_step_uses_curve & 1) != 0;
    channel.random_step_curve =
        static_cast<std::uint8_t>(settings.random_step_curve_or_value);
    channel.phase = 0;
    channel.angle = RandomCameraShakePhaseStep(channel, host);
    EnableCameraShakeChannels(state, ChannelMask(i));
  }
}

// decomp: BATTLE.PRG func_800D1FEC (src/BATTLE/BATTLE.PRG/5BF94.c:6626)
void StopCameraShakeChannels(EffectCameraState& state,
                             std::int32_t channel_mask) noexcept {
  for (int i = 0; i < kCameraShakeChannels; ++i) {
    if (ChannelBit(static_cast<std::uint32_t>(channel_mask), i)) {
      state.channels[static_cast<std::size_t>(i)].phase = kStoppedPhase;
      DisableCameraShakeChannels(state, ChannelMask(i));
    }
  }
}

// decomp: BATTLE.PRG func_800D206C (src/BATTLE/BATTLE.PRG/5BF94.c:6638)
void UpdateCameraShakeChannels(EffectCameraState& state,
                               EffectCameraHost& host) {
  for (int i = 0; i < kCameraShakeChannels; ++i) {
    if (!ChannelBit(state.enabled_channels, i)) {
      continue;
    }
    CameraShakeChannel& channel = state.channels[static_cast<std::size_t>(i)];
    const CurveSample shape =
        host.SampleSegmentedCurve(channel.shape_curve, channel.phase);
    // A refused shape curve ends the channel like the curve's end does.
    if (IsCurveRefusal(shape.status) || shape.value < 0) {
      StopCameraShakeChannels(state, ChannelMask(i));
      continue;
    }
    const std::int32_t wave = Math::WrappingMultiply(
        Math::WrappingMultiply(Math::Sine(channel.angle * kShakeAngleScale),
                               shape.value),
        channel.amplitude);
    StoreShake(state.camera, i, wave >> kShakeAmplitudeBits);
    const std::uint8_t step = CameraShakePhaseStep(channel, host);
    const std::uint8_t random_step = RandomCameraShakePhaseStep(channel, host);
    channel.angle =
        static_cast<std::uint8_t>(channel.angle + step + random_step);
    ++channel.phase;
  }
}

// decomp: BATTLE.PRG func_800D21C0 (src/BATTLE/BATTLE.PRG/5BF94.c:6681)
void InterpolateEffectCameraLinear(EffectCameraState& state) noexcept {
  InterpolatePose(state, Math::InterpolateTimedVector,
                  Math::InterpolateTimedScalar);
}

// decomp: BATTLE.PRG func_800D236C (src/BATTLE/BATTLE.PRG/5BF94.c:6708)
void InterpolateEffectCameraCosine(EffectCameraState& state) noexcept {
  InterpolatePose(state, Math::InterpolateCosineVector,
                  Math::InterpolateCosineScalar);
}

// decomp: BATTLE.PRG func_800D2518 (src/BATTLE/BATTLE.PRG/5BF94.c:6735)
void HoldEffectCameraStartPose(EffectCameraState& state) noexcept {
  state.camera = state.motion_ends[0];
}

// decomp: BATTLE.PRG func_800D2560 (src/BATTLE/BATTLE.PRG/5BF94.c:6737)
void UpdateEffectCameraMotion(EffectCameraState& state,
                              EffectCameraHost& host) {
  const bool had_transition = state.transition_mode != 0;
  if (state.motion_duration != 0) {
    switch (static_cast<CameraTransition>(state.transition_mode)) {
      case CameraTransition::kLinear:
        InterpolateEffectCameraLinear(state);
        break;
      case CameraTransition::kCosine:
        InterpolateEffectCameraCosine(state);
        break;
      case CameraTransition::kHold:
        HoldEffectCameraStartPose(state);
        break;
      case CameraTransition::kNone:
      default:
        break;
    }
    if (state.motion_frame <= state.motion_duration) {
      state.motion_frame = static_cast<std::int16_t>(state.motion_frame + 1);
      if (state.motion_frame > state.motion_duration) {
        state.motion_frame = 0;
        state.motion_duration = 0;
        SetEffectCameraTransitionMode(
            state, static_cast<std::int32_t>(CameraTransition::kNone));
      }
    }
  }
  if (state.enabled_channels != 0) {
    UpdateCameraShakeChannels(state, host);
  } else {
    state.camera.shake_roll = 0;
    state.camera.shake_depth = 0;
    state.camera.shake_vertical = 0;
    state.camera.shake_lateral = 0;
  }
  if (state.enabled_channels != 0 || had_transition) {
    ApplyEffectCamera(state.camera, host);
  }
}

// decomp: BATTLE.PRG func_800D66FC (src/BATTLE/BATTLE.PRG/5BF94.c:8308)
void FocusCameraOnEffectTransform(EffectCameraState& state,
                                  EffectCameraHost& host,
                                  Math::Vec3 transform_translation,
                                  std::uint8_t duration_frames) {
  CaptureEffectCamera(host, state.motion_ends[0]);
  CaptureEffectCamera(host, state.motion_ends[1]);
  state.motion_ends[1].look_at = FixedVector(transform_translation);
  BeginEasedTransition(state, duration_frames);
}

// decomp: BATTLE.PRG func_800D67C4 (src/BATTLE/BATTLE.PRG/5BF94.c:8323)
void RestoreActionEffectCamera(EffectCameraState& state, EffectCameraHost& host,
                               std::uint8_t duration_frames) {
  CaptureEffectCamera(host, state.motion_ends[0]);
  state.motion_ends[1] = state.saved_action_camera;
  BeginEasedTransition(state, duration_frames);
}
}  // namespace VS::Effects
