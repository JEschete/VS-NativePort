// Hand-maintained (tools/port/owned.csv).
#include "Effects/CameraLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Effects/Camera.hpp"
#include "Effects/Curves.hpp"
#include "Effects/CurvesLive.hpp"
#include "Field/CameraLive.hpp"
#include "Legacy/Psx.h"
#include "Render/CameraViewLive.hpp"

extern "C" {
#include "Core/BiosRand.h"
#include "Effects/Camera.h"
#include "Effects/CameraLiveState.h"
#include "Effects/Runtime.h"
#include "Effects/Transforms.h"
#include "Field/Camera.h"
#include "Render/Camera.h"
#include "Render/Render.h"
#include "Render/UiPrimitives.h"

// decomp: D_800F54B8 (D_800F54B8@BATTLE/BATTLE.PRG)
Effects_CameraShakeChannel
    Effects_cameraShakeChannels[kEffectsCameraShakeChannelCount];
// decomp: D_800F54D0 (D_800F54D0@BATTLE/BATTLE.PRG)
std::int16_t Effects_cameraMotionFrame;
// decomp: D_800F54D8 (D_800F54D8@BATTLE/BATTLE.PRG)
Effects_EffectCamera Effects_effectCamera;
// decomp: D_800F5518 (D_800F5518@BATTLE/BATTLE.PRG)
char Effects_enabledShakeChannels;
// decomp: D_800F5520 (D_800F5520@BATTLE/BATTLE.PRG)
Effects_EffectCamera Effects_cameraMotionEnds[kEffectsCameraMotionEndCount];
// decomp: D_800F55A0 (D_800F55A0@BATTLE/BATTLE.PRG)
std::int16_t Effects_cameraMotionDuration;
// decomp: D_800F55A8 (D_800F55A8@BATTLE/BATTLE.PRG)
Effects_EffectCamera Effects_savedActionCamera;
// decomp: D_800F55E8 (D_800F55E8@BATTLE/BATTLE.PRG)
u_int Effects_cameraTransitionMode;
}

namespace {
// The original's layouts (5BF94.c:6474 shake channels, 6681 effect camera).
constexpr std::size_t kShakeChannelBytes = 6;
constexpr std::size_t kShakeStepCurveAt = 2;
constexpr std::size_t kShakeAngleAt = 5;
constexpr std::size_t kEffectCameraBytes = 0x40;
constexpr std::size_t kEffectCameraShakeAt = 0x20;
constexpr std::size_t kEffectCameraRollAt = 0x30;
constexpr std::size_t kEffectCameraFarClipAt = 0x3C;
}  // namespace

static_assert(sizeof(Effects_CameraShakeChannel) == kShakeChannelBytes);
static_assert(offsetof(Effects_CameraShakeChannel, stepCurve) ==
              kShakeStepCurveAt);
static_assert(offsetof(Effects_CameraShakeChannel, angle) == kShakeAngleAt);
static_assert(sizeof(Effects_EffectCamera) == kEffectCameraBytes);
static_assert(offsetof(Effects_EffectCamera, shakeLateral) ==
              kEffectCameraShakeAt);
static_assert(offsetof(Effects_EffectCamera, roll) == kEffectCameraRollAt);
static_assert(offsetof(Effects_EffectCamera, farClip) ==
              kEffectCameraFarClipAt);
static_assert(std::size(Effects_cameraShakeChannels) ==
              VS::Effects::kCameraShakeChannels);

namespace VS::Effects {
namespace {
namespace Math = VS::Core::Math;

constexpr std::uint8_t kShapeCurveMask = 0x1F;
constexpr int kTransformSlots = 16;
static_assert(std::extent_v<decltype(Effects_EffectInstance::transforms)> ==
              kTransformSlots);

std::uint64_t refused_calls = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

[[nodiscard]] EffectCameraPose PoseOf(const Effects_EffectCamera& camera) {
  return {.position = Math::VectorOf(camera.position),
          .look_at = Math::VectorOf(camera.lookAt),
          .shake_lateral = camera.shakeLateral,
          .shake_vertical = camera.shakeVertical,
          .shake_depth = camera.shakeDepth,
          .shake_roll = camera.shakeRoll,
          .roll = camera.roll,
          .near_clip = camera.nearClip,
          .projection_distance = camera.projectionDistance,
          .far_clip = camera.farClip};
}

void StorePose(Effects_EffectCamera& camera, const EffectCameraPose& pose) {
  Math::StoreVector(camera.position, pose.position);
  Math::StoreVector(camera.lookAt, pose.look_at);
  camera.shakeLateral = pose.shake_lateral;
  camera.shakeVertical = pose.shake_vertical;
  camera.shakeDepth = pose.shake_depth;
  camera.shakeRoll = pose.shake_roll;
  camera.roll = pose.roll;
  camera.nearClip = pose.near_clip;
  camera.projectionDistance = pose.projection_distance;
  camera.farClip = pose.far_clip;
}

[[nodiscard]] CameraShakeChannel ChannelOf(
    const Effects_CameraShakeChannel& channel) {
  return {.phase = channel.phase,
          .shape_curve = channel.shapeCurve,
          .curve_step = channel.curveStep != 0,
          .random_step = channel.randomStep != 0,
          .step_curve = channel.stepCurve,
          .random_step_curve = channel.randomStepCurve,
          .amplitude = channel.amplitude,
          .angle = channel.angle};
}

void StoreChannel(Effects_CameraShakeChannel& channel,
                  const CameraShakeChannel& value) {
  channel.phase = value.phase;
  channel.shapeCurve = value.shape_curve & kShapeCurveMask;
  channel.curveStep = value.curve_step ? 1U : 0U;
  channel.randomStep = value.random_step ? 1U : 0U;
  channel.stepCurve = value.step_curve;
  channel.randomStepCurve = value.random_step_curve;
  channel.amplitude = value.amplitude;
  channel.angle = value.angle;
}

class LiveCameraHost final : public EffectCameraHost {
 public:
  FieldCameraSnapshot CurrentCamera() override {
    VECTOR position{};
    VECTOR look_at{};
    VS::Field::LiveCameraPosition(&position);
    VS::Field::LiveCameraLookAt(&look_at);
    return {.position = Math::VectorOf(position),
            .look_at = Math::VectorOf(look_at),
            .roll = VS::Field::LiveCameraRoll(),
            .near_clip = Render_nearClip,
            .projection_distance = Render_projectionDistance,
            .far_clip = Render_frameScratch.camera.farClip};
  }

  void ApplyCamera(const AppliedEffectCamera& camera) override {
    VECTOR position{};
    Math::StoreVector(position, camera.position);
    VS::Field::LiveSetCameraPosition(&position);
    Math::StoreVector(position, camera.look_at);
    VS::Field::LiveSetCameraLookAt(&position);
    VS::Field::LiveSetCameraRoll(camera.roll);
    Render::SetLiveCameraNearClip(camera.near_clip);
    Render::SetLiveCameraProjectionDistance(camera.projection_distance);
    // #279: the original leaves the final depth offset in a1, so it is the
    // preserve_texture_lod argument (BATTLE func_800D1B80, 0x800D1CF8).
    Render::SetLiveCameraFarClip(camera.far_clip, camera.texture_lod_hint);
    Render::BuildLiveCameraViewMatrix();
  }

  CurveSample SampleCurve(std::int32_t curve_id,
                          std::int32_t distance) override {
    return SampleLiveEffectCurve(curve_id, distance);
  }

  CurveSample SampleSegmentedCurve(std::int32_t curve_index,
                                   std::int32_t distance) override {
    return SampleLiveSegmentedEffectCurve(curve_index, distance);
  }

  std::int32_t Random() override { return Core_Random_Rand(); }
};

void ReportRefusal(const char* entry, const char* reason) {
  if (ShouldReport(refused_calls)) {
    VS_LOG_WARN("effects", "effect camera: {} refused: {} ({} refusals)", entry,
                reason, refused_calls);
  }
}
}  // namespace

EffectCameraState LoadLiveEffectCameraState() {
  EffectCameraState state;
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    state.channels.at(i) = ChannelOf(Effects_cameraShakeChannels[i]);
  }
  state.motion_frame = Effects_cameraMotionFrame;
  state.camera = PoseOf(Effects_effectCamera);
  state.enabled_channels =
      static_cast<std::uint8_t>(Effects_enabledShakeChannels);
  for (std::size_t i = 0; i < state.motion_ends.size(); ++i) {
    state.motion_ends.at(i) = PoseOf(Effects_cameraMotionEnds[i]);
  }
  state.motion_duration = Effects_cameraMotionDuration;
  state.saved_action_camera = PoseOf(Effects_savedActionCamera);
  state.transition_mode = Effects_cameraTransitionMode;
  return state;
}

void StoreLiveEffectCameraState(const EffectCameraState& state) {
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    StoreChannel(Effects_cameraShakeChannels[i], state.channels.at(i));
  }
  Effects_cameraMotionFrame = state.motion_frame;
  StorePose(Effects_effectCamera, state.camera);
  Effects_enabledShakeChannels = static_cast<char>(state.enabled_channels);
  for (std::size_t i = 0; i < state.motion_ends.size(); ++i) {
    StorePose(Effects_cameraMotionEnds[i], state.motion_ends.at(i));
  }
  Effects_cameraMotionDuration = state.motion_duration;
  StorePose(Effects_savedActionCamera, state.saved_action_camera);
  Effects_cameraTransitionMode = state.transition_mode;
}

EffectCameraHost& LiveEffectCameraHost() {
  static LiveCameraHost host;
  return host;
}

void LiveResetEffectCameraMotion() {
  EffectCameraState state = LoadLiveEffectCameraState();
  ResetEffectCameraMotion(state);
  StoreLiveEffectCameraState(state);
}

void LiveCaptureEffectCamera(Effects_EffectCamera* camera) {
  if (camera == nullptr) {
    ReportRefusal("capture", "no camera");
    return;
  }
  EffectCameraPose pose = PoseOf(*camera);
  CaptureEffectCamera(LiveEffectCameraHost(), pose);
  StorePose(*camera, pose);
}

void LiveSetEffectCameraTransitionMode(int transition_mode) {
  EffectCameraState state = LoadLiveEffectCameraState();
  SetEffectCameraTransitionMode(state, transition_mode);
  StoreLiveEffectCameraState(state);
}

void LiveStartCameraShakeChannels(int channel_mask, int shape_curve,
                                  int amplitude, int step_uses_curve,
                                  int step_curve_or_value,
                                  int random_step_uses_curve,
                                  int random_step_curve_or_value) {
  EffectCameraState state = LoadLiveEffectCameraState();
  StartCameraShakeChannels(
      state, LiveEffectCameraHost(), channel_mask,
      {.shape_curve = shape_curve,
       .amplitude = amplitude,
       .step_uses_curve = step_uses_curve,
       .step_curve_or_value = step_curve_or_value,
       .random_step_uses_curve = random_step_uses_curve,
       .random_step_curve_or_value = random_step_curve_or_value});
  StoreLiveEffectCameraState(state);
}

void LiveUpdateEffectCameraMotion() {
  EffectCameraState state = LoadLiveEffectCameraState();
  UpdateEffectCameraMotion(state, LiveEffectCameraHost());
  StoreLiveEffectCameraState(state);
}

void LiveFocusCameraOnEffectTransform(Effects_EffectInstance* instance,
                                      std::uint8_t transform_index,
                                      std::uint8_t duration_frames) {
  if (instance == nullptr) {
    ReportRefusal("focus on transform", "no effect instance");
    return;
  }
  if (transform_index >= kTransformSlots) {
    ReportRefusal("focus on transform", "transform index past sixteen");
    return;
  }
  const MATRIX& world = instance->transforms[transform_index].world;
  EffectCameraState state = LoadLiveEffectCameraState();
  FocusCameraOnEffectTransform(state, LiveEffectCameraHost(),
                               {.x = static_cast<std::int32_t>(world.t[0]),
                                .y = static_cast<std::int32_t>(world.t[1]),
                                .z = static_cast<std::int32_t>(world.t[2])},
                               duration_frames);
  StoreLiveEffectCameraState(state);
}

void LiveRestoreActionEffectCamera(Effects_EffectInstance* /*instance*/,
                                   std::uint8_t duration_frames) {
  EffectCameraState state = LoadLiveEffectCameraState();
  RestoreActionEffectCamera(state, LiveEffectCameraHost(), duration_frames);
  StoreLiveEffectCameraState(state);
}
}  // namespace VS::Effects
