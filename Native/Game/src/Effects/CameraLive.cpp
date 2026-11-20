// Hand-maintained (tools/port/owned.csv).
#include "Effects/CameraLive.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <type_traits>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/RandomLive.hpp"
#include "Effects/Camera.hpp"
#include "Effects/CameraLiveState.hpp"
#include "Effects/Curves.hpp"
#include "Effects/CurvesLive.hpp"
#include "Effects/Runtime.hpp"
#include "Effects/TransformsLiveState.hpp"
#include "Field/CameraLive.hpp"
#include "Legacy/Psx.hpp"
#include "Render/Camera.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/Render.hpp"
#include "Render/UiPrimitives.hpp"

using VS::Core::Gte::MATRIX;
using VS::Core::Gte::VECTOR;

namespace VS::Effects {
// decomp: D_800F54B8 (D_800F54B8@BATTLE/BATTLE.PRG)
LiveCameraShakeChannel cameraShakeChannels[kCameraShakeChannelCount];
// decomp: D_800F54D0 (D_800F54D0@BATTLE/BATTLE.PRG)
std::int16_t cameraMotionFrame;
// decomp: D_800F54D8 (D_800F54D8@BATTLE/BATTLE.PRG)
LiveEffectCamera effectCamera;
// decomp: D_800F5518 (D_800F5518@BATTLE/BATTLE.PRG)
std::uint8_t enabledShakeChannels;
// decomp: D_800F5520 (D_800F5520@BATTLE/BATTLE.PRG)
LiveEffectCamera cameraMotionEnds[kCameraMotionEndCount];
// decomp: D_800F55A0 (D_800F55A0@BATTLE/BATTLE.PRG)
std::int16_t cameraMotionDuration;
// decomp: D_800F55A8 (D_800F55A8@BATTLE/BATTLE.PRG)
LiveEffectCamera savedActionCamera;
// decomp: D_800F55E8 (D_800F55E8@BATTLE/BATTLE.PRG)
std::uint32_t cameraTransitionMode;

static_assert(std::size(cameraShakeChannels) == kCameraShakeChannels);

namespace {
namespace Math = VS::Core::Math;

constexpr std::uint8_t kShapeCurveMask = 0x1F;
constexpr int kTransformSlots = 16;
static_assert(std::tuple_size_v<decltype(LiveEffectInstance::transforms)> ==
              kTransformSlots);

std::uint64_t refusedCalls = 0;

// True on the 1st, 2nd, 4th, 8th... occurrence, to keep repeated
// diagnostics readable.
[[nodiscard]] bool ShouldReport(std::uint64_t& count) {
  return std::has_single_bit(++count);
}

}  // namespace

EffectCameraPose LoadLiveEffectCameraPose(const LiveEffectCamera& camera) {
  return {.position = Math::VectorOf(camera.position),
          .look_at = Math::VectorOf(camera.look_at),
          .shake_lateral = camera.shake_lateral,
          .shake_vertical = camera.shake_vertical,
          .shake_depth = camera.shake_depth,
          .shake_roll = camera.shake_roll,
          .roll = camera.roll,
          .near_clip = camera.near_clip,
          .projection_distance = camera.projection_distance,
          .far_clip = camera.far_clip};
}

void StoreLiveEffectCameraPose(LiveEffectCamera& camera,
                               const EffectCameraPose& pose) {
  Math::StoreVector(camera.position, pose.position);
  Math::StoreVector(camera.look_at, pose.look_at);
  camera.shake_lateral = pose.shake_lateral;
  camera.shake_vertical = pose.shake_vertical;
  camera.shake_depth = pose.shake_depth;
  camera.shake_roll = pose.shake_roll;
  camera.roll = pose.roll;
  camera.near_clip = pose.near_clip;
  camera.projection_distance = pose.projection_distance;
  camera.far_clip = pose.far_clip;
}

namespace {
[[nodiscard]] CameraShakeChannel ChannelOf(
    const LiveCameraShakeChannel& channel) {
  return {.phase = channel.phase,
          .shape_curve = channel.shape_curve,
          .step_uses_curve = channel.step_uses_curve != 0,
          .random_step_uses_curve = channel.random_step_uses_curve != 0,
          .step_curve = channel.step_curve,
          .random_step_curve = channel.random_step_curve,
          .amplitude = channel.amplitude,
          .angle = channel.angle};
}

void StoreChannel(LiveCameraShakeChannel& channel,
                  const CameraShakeChannel& value) {
  channel.phase = value.phase;
  channel.shape_curve = value.shape_curve & kShapeCurveMask;
  channel.step_uses_curve = value.step_uses_curve ? 1U : 0U;
  channel.random_step_uses_curve = value.random_step_uses_curve ? 1U : 0U;
  channel.step_curve = value.step_curve;
  channel.random_step_curve = value.random_step_curve;
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
            .near_clip = VS::Render::nearClip,
            .projection_distance = VS::Render::projectionDistance,
            .far_clip = VS::Render::FrameScratchpad().camera.far_clip};
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

  std::int32_t Random() override { return VS::Core::NextBiosRand(); }
};

void ReportRefusal(const char* entry, const char* reason) {
  if (ShouldReport(refusedCalls)) {
    VS::Core::LogWarn("effects", "effect camera: {} refused: {} ({} refusals)",
                      entry, reason, refusedCalls);
  }
}
}  // namespace

EffectCameraState LoadLiveEffectCameraState() {
  EffectCameraState state;
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    state.channels.at(i) = ChannelOf(cameraShakeChannels[i]);
  }
  state.motion_frame = cameraMotionFrame;
  state.camera = LoadLiveEffectCameraPose(effectCamera);
  state.enabled_channels = enabledShakeChannels;
  for (std::size_t i = 0; i < state.motion_ends.size(); ++i) {
    state.motion_ends.at(i) = LoadLiveEffectCameraPose(cameraMotionEnds[i]);
  }
  state.motion_duration = cameraMotionDuration;
  state.saved_action_camera = LoadLiveEffectCameraPose(savedActionCamera);
  state.transition_mode = cameraTransitionMode;
  return state;
}

void StoreLiveEffectCameraState(const EffectCameraState& state) {
  for (std::size_t i = 0; i < state.channels.size(); ++i) {
    StoreChannel(cameraShakeChannels[i], state.channels.at(i));
  }
  cameraMotionFrame = state.motion_frame;
  StoreLiveEffectCameraPose(effectCamera, state.camera);
  enabledShakeChannels = state.enabled_channels;
  for (std::size_t i = 0; i < state.motion_ends.size(); ++i) {
    StoreLiveEffectCameraPose(cameraMotionEnds[i], state.motion_ends.at(i));
  }
  cameraMotionDuration = state.motion_duration;
  StoreLiveEffectCameraPose(savedActionCamera, state.saved_action_camera);
  cameraTransitionMode = state.transition_mode;
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

void LiveCaptureEffectCamera(LiveEffectCamera* camera) {
  if (camera == nullptr) {
    ReportRefusal("capture", "no camera");
    return;
  }
  EffectCameraPose pose = LoadLiveEffectCameraPose(*camera);
  CaptureEffectCamera(LiveEffectCameraHost(), pose);
  StoreLiveEffectCameraPose(*camera, pose);
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

void LiveFocusCameraOnEffectTransform(LiveEffectInstance* instance,
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

void LiveRestoreActionEffectCamera(LiveEffectInstance* /*instance*/,
                                   std::uint8_t duration_frames) {
  EffectCameraState state = LoadLiveEffectCameraState();
  RestoreActionEffectCamera(state, LiveEffectCameraHost(), duration_frames);
  StoreLiveEffectCameraState(state);
}
}  // namespace VS::Effects
