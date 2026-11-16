// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraFrame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
namespace {
using Core::Math::Position3;
using Core::Math::Vec3;
using Core::Math::WrappingAdd;
using Core::Math::WrappingSubtract;

// _sVectorToFixedPointVector: whole units to 20.12 fixed point.
constexpr std::int32_t kFixedOne = 0x1000;

Vec3 Add(Vec3 left, Vec3 right) {
  return {.x = WrappingAdd(left.x, right.x),
          .y = WrappingAdd(left.y, right.y),
          .z = WrappingAdd(left.z, right.z)};
}

Vec3 Widen(Position3 offset) {
  return {.x = offset.x * kFixedOne,
          .y = offset.y * kFixedOne,
          .z = offset.z * kFixedOne};
}

// Clears a changed setting's flag after `apply` takes its value.
template <typename Apply>
void ApplyChanged(EventCameraSetting& setting, Apply apply) {
  if (setting.changed == 0) return;
  apply(setting.value);
  setting.changed = 0;
}

constexpr std::array kStepOrder{
    EventCameraStep::kLookAtTrack,
    EventCameraStep::kEyeTrack,
    EventCameraStep::kCameraRoll,
    EventCameraStep::kScreenRoll,
    EventCameraStep::kProjectionDistance,
    EventCameraStep::kNearClip,
    EventCameraStep::kFarClip,
    EventCameraStep::kShake,
};
}  // namespace

// decomp: BATTLE.PRG func_800BD610 (F01228, src/BATTLE/BATTLE.PRG/4A0A8.c:3663)
std::uint8_t EventCameraCompletionMask(
    const EventCameraProgress& progress) noexcept {
  std::uint8_t mask = 0;
  if (progress.look_at_mode == kEventCameraTrackIdle) mask |= kCameraLookAtIdle;
  if (progress.eye_mode == kEventCameraTrackIdle) mask |= kCameraEyeIdle;
  if (progress.camera_roll_duration == 0) mask |= kCameraRollIdle;
  if (progress.projection_duration == 0) mask |= kCameraProjectionIdle;
  if (progress.near_clip_duration == 0) mask |= kCameraNearClipIdle;
  if (progress.far_clip_duration == 0) mask |= kCameraFarClipIdle;
  if (progress.screen_effect_duration == 0) mask |= kCameraScreenEffectIdle;
  // The original added the two duration bytes (lbu) before testing.
  const unsigned shake_frames = unsigned{progress.shake_durations[0]} +
                                unsigned{progress.shake_durations[1]};
  if (shake_frames == 0) mask |= kCameraShakeIdle;
  return mask;
}

// decomp: BATTLE.PRG func_800BB41C (F01197, src/BATTLE/BATTLE.PRG/4A0A8.c:2788)
std::optional<std::intptr_t> CommandWaitCameraTracks(ScriptCommand command,
                                                     std::uint8_t completion) {
  if (command.size() < kWaitCameraTracksLength) return std::nullopt;
  const std::uint8_t awaited = OperandByte(command, 1);
  return (awaited & completion) != awaited ? 1 : 0;
}

// decomp: BATTLE.PRG vs_battle_applyCameraState (F01219,
// src/BATTLE/BATTLE.PRG/4A0A8.c:3117)
void ApplyEventCameraState(EventCameraFrame& frame, bool use_field_camera,
                           std::uint8_t preserve_texture_lod,
                           EventCameraOutputPort& output) {
  EventCameraPair view;
  if (use_field_camera) {
    view = output.InitializeFieldCamera();
    frame.look_at = view.look_at;
    frame.eye = view.eye;
  } else {
    view = {.look_at = frame.look_at, .eye = frame.eye};
    if (frame.origin_actor != kEventCameraWorldOrigin) {
      view.look_at = Add(view.look_at, frame.origin_offset);
      view.eye = Add(view.eye, frame.origin_offset);
    }
  }
  if (frame.shake_active_count != 0) {
    const Vec3 look_at_shake = Widen(frame.look_at_shake);
    const Vec3 eye_shake = Widen(frame.eye_shake);
    // The original stored the yaw as a halfword (0x1F8000DA).
    const auto yaw = static_cast<std::int16_t>(
        output.FixedAtan2(WrappingSubtract(view.look_at.x, view.eye.x),
                          WrappingSubtract(view.look_at.z, view.eye.z)));
    const Core::Math::Mat3 rotation =
        output.BuildRotation({.x = 0, .y = yaw, .z = 0});
    const Vec3 look_at_turned =
        output.ApplyMatrix(rotation, look_at_shake, EventShakeTarget::kLookAt);
    const Vec3 eye_turned =
        output.ApplyMatrix(rotation, eye_shake, EventShakeTarget::kEye);
    view.look_at = Add(view.look_at, look_at_turned);
    view.eye = Add(view.eye, eye_turned);
  }
  output.SetLookAt(view.look_at);
  output.SetPosition(view.eye);
  ApplyChanged(frame.camera_roll,
               [&output](std::int16_t value) { output.SetRoll(value); });
  ApplyChanged(frame.screen_roll, [&output](std::int16_t value) {
    output.SetScreenRotation(value);
  });
  ApplyChanged(frame.projection_distance, [&output](std::int16_t value) {
    output.SetProjectionDistance(value);
  });
  ApplyChanged(frame.near_clip,
               [&output](std::int16_t value) { output.SetNearClip(value); });
  ApplyChanged(frame.far_clip,
               [&output, preserve_texture_lod](std::int16_t value) {
                 output.SetFarClip(value, preserve_texture_lod);
               });
}

// decomp: BATTLE.PRG func_800BC9E0 (F01222, src/BATTLE/BATTLE.PRG/4A0A8.c:3371)
void UpdateEventCameraTracks(EventCameraStepPort& steps) {
  const EventCameraPair current = steps.CurrentPositions();
  steps.SetPartnerPositions({.look_at = current.eye, .eye = current.look_at});
  for (const EventCameraStep step : kStepOrder) steps.Advance(step);
}
}  // namespace VS::Event
