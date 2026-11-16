// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraFrameLive.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Log.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraFrame.hpp"
#include "Event/EventCameraLensLive.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/EventScreenTweensLive.hpp"
#include "Event/ScriptCommand.hpp"
#include "Field/CameraLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ScreenOverlayLive.hpp"

extern "C" {
#include "Core/Gte.h"
#include "Core/Scratch.h"
#include "Event/Camera.h"
#include "Event/Screen.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "Event/ScriptVm.h"
}

namespace VS::Event {
namespace {
using Core::Math::Mat3;
using Core::Math::Vec3;

// The scratchpad words vs_battle_applyCameraState used: the look-at and eye
// shake vectors (0x1F800098, 0x1F8000A8), the yaw-only rotation angles
// (0x1F8000D8) and its matrix (0x1F8000F8).
constexpr std::size_t kLookAtShakeScratchOffset = 0x98;
constexpr std::size_t kEyeShakeScratchOffset = 0xA8;
constexpr std::size_t kRotationScratchOffset = 0xD8;
constexpr std::size_t kMatrixScratchOffset = 0xF8;

std::atomic<std::uint64_t> refused_count{0};

void Skip(const char* operation, const char* reason) {
  refused_count.fetch_add(1, std::memory_order_relaxed);
  VS_LOG_ERROR("event.camera", "{} skipped: {}", operation, reason);
}

Event_CameraState* LiveCamera(const char* operation) {
  if (Event_camera == nullptr) Skip(operation, "missing event camera");
  return Event_camera;
}

Vec3 VectorOf(const VECTOR& vector) {
  return {.x = static_cast<std::int32_t>(vector.vx),
          .y = static_cast<std::int32_t>(vector.vy),
          .z = static_cast<std::int32_t>(vector.vz)};
}

// Keeps the record's pad word.
void StoreVector(VECTOR& record, Vec3 vector) {
  record.vx = vector.x;
  record.vy = vector.y;
  record.vz = vector.z;
}

VECTOR RecordOf(Vec3 vector) {
  VECTOR record{};
  StoreVector(record, vector);
  return record;
}

EventCameraSetting LoadAngle(const Event_AngleTween& tween) {
  return {.changed = static_cast<std::uint8_t>(tween.hasChanged),
          .value = tween.angle};
}

EventCameraSetting LoadLens(const Event_LensTween& tween) {
  return {.changed = static_cast<std::uint8_t>(tween.hasChanged),
          .value = tween.value};
}

EventCameraFrame LoadFrame(const Event_CameraState& camera) {
  return {.look_at = VectorOf(camera.lookAt.currentPosition),
          .eye = VectorOf(camera.eye.currentPosition),
          .origin_actor = camera.originActorId,
          .origin_offset = VectorOf(camera.originOffset),
          .shake_active_count = camera.shakeOscillatorA.envelopeShape.count,
          .look_at_shake = {.x = camera.shakeEyeOffsetX,
                            .y = camera.shakeEyeOffsetY,
                            .z = camera.shakeEyeOffsetZ},
          .eye_shake = {.x = camera.shakeLookAtOffsetX,
                        .y = camera.shakeLookAtOffsetY,
                        .z = camera.shakeLookAtOffsetZ},
          .camera_roll = LoadAngle(camera.cameraRoll),
          .screen_roll = LoadAngle(camera.capturedScreenRoll),
          .projection_distance = LoadLens(camera.projectionDistance),
          .near_clip = LoadLens(camera.nearClip),
          .far_clip = LoadLens(camera.farClip)};
}

// Writes what ApplyEventCameraState may change: the tracks' positions (taken
// from the field camera) and the settings' changed flags.
void StoreFrame(Event_CameraState& camera, const EventCameraFrame& frame) {
  StoreVector(camera.lookAt.currentPosition, frame.look_at);
  StoreVector(camera.eye.currentPosition, frame.eye);
  camera.cameraRoll.hasChanged = static_cast<char>(frame.camera_roll.changed);
  camera.capturedScreenRoll.hasChanged =
      static_cast<char>(frame.screen_roll.changed);
  camera.projectionDistance.hasChanged =
      static_cast<char>(frame.projection_distance.changed);
  camera.nearClip.hasChanged = static_cast<char>(frame.near_clip.changed);
  camera.farClip.hasChanged = static_cast<char>(frame.far_clip.changed);
}

EventCameraProgress LoadProgress(const Event_CameraState& camera) {
  // The original read each oscillator's byte at +0xB (Camera.h reserved0B)
  // unsigned.
  return {.look_at_mode = camera.lookAt.motionMode,
          .eye_mode = camera.eye.motionMode,
          .camera_roll_duration = camera.cameraRoll.durationFrames,
          .projection_duration = static_cast<std::uint8_t>(
              camera.projectionDistance.durationFrames),
          .near_clip_duration =
              static_cast<std::uint8_t>(camera.nearClip.durationFrames),
          .far_clip_duration =
              static_cast<std::uint8_t>(camera.farClip.durationFrames),
          .screen_effect_duration = camera.screenEffectTween.durationFrames,
          .shake_durations = {
              static_cast<std::uint8_t>(camera.shakeOscillatorA.reserved0B),
              static_cast<std::uint8_t>(camera.shakeOscillatorB.reserved0B)}};
}

// The field camera, the renderer and the GTE, through the scratchpad words
// the original used. Without a scratchpad the rotation stays local.
class LiveCameraOutput final : public EventCameraOutputPort {
 public:
  EventCameraPair InitializeFieldCamera() override {
    VECTOR position{};
    VECTOR look_at{};
    Field::LiveInitializeSphericalCamera(&position, &look_at, -1, -1);
    return {.look_at = VectorOf(look_at), .eye = VectorOf(position)};
  }

  std::int32_t FixedAtan2(std::int32_t y, std::int32_t x) override {
    return Core::Math::FixedAtan2(y, x);
  }

  Mat3 BuildRotation(Core::Math::Angles3 rotation) override {
    SVECTOR local_angles{};
    MATRIX local_matrix{};
    auto* angles = Scratch<SVECTOR>(kRotationScratchOffset, &local_angles);
    auto* matrix = Scratch<MATRIX>(kMatrixScratchOffset, &local_matrix);
    // The original stored only vx, vy and vz; the pad is kept.
    angles->vx = static_cast<std::int16_t>(rotation.x);
    angles->vy = static_cast<std::int16_t>(rotation.y);
    angles->vz = static_cast<std::int16_t>(rotation.z);
    Core::Math::BuildLiveRotationXyz(angles, matrix);
    return Core::Math::MatrixOf(*matrix);
  }

  Vec3 ApplyMatrix(const Mat3& rotation, Vec3 vector,
                   EventShakeTarget target) override {
    MATRIX local_matrix{};
    VECTOR local_vector{};
    auto* matrix = Scratch<MATRIX>(kMatrixScratchOffset, &local_matrix);
    auto* record = Scratch<VECTOR>(target == EventShakeTarget::kLookAt
                                       ? kLookAtShakeScratchOffset
                                       : kEyeShakeScratchOffset,
                                   &local_vector);
    Core::Math::StoreMatrix(*matrix, rotation);
    StoreVector(*record, vector);
    Core_Gte_ApplyMatrixLV(matrix, record, record);
    return VectorOf(*record);
  }

  void SetLookAt(Vec3 look_at) override {
    const VECTOR record = RecordOf(look_at);
    Field::LiveSetCameraLookAt(&record);
  }
  void SetPosition(Vec3 position) override {
    const VECTOR record = RecordOf(position);
    Field::LiveSetCameraPosition(&record);
  }
  void SetRoll(std::int16_t angle) override { Field::LiveSetCameraRoll(angle); }
  void SetScreenRotation(std::int16_t angle) override {
    Render::SetLiveFramebufferEffectRotation(angle);
  }
  void SetProjectionDistance(std::int16_t distance) override {
    Render::SetLiveCameraProjectionDistance(distance);
  }
  void SetNearClip(std::int16_t near_clip) override {
    Render::SetLiveCameraNearClip(near_clip);
  }
  void SetFarClip(std::int16_t far_clip,
                  std::uint8_t preserve_texture_lod) override {
    Render::SetLiveCameraFarClip(far_clip, preserve_texture_lod);
  }

 private:
  template <typename Record>
  static Record* Scratch(std::size_t offset, Record* fallback) {
    auto* scratch = static_cast<std::byte*>(Core_ScratchBase());
    if (scratch == nullptr) return fallback;
    return reinterpret_cast<Record*>(scratch + offset);
  }
};

// The live workspace's track positions and per-part updates.
class LiveCameraSteps final : public EventCameraStepPort {
 public:
  explicit LiveCameraSteps(Event_CameraState& camera) : camera_(&camera) {}

  EventCameraPair CurrentPositions() override {
    return {.look_at = VectorOf(camera_->lookAt.currentPosition),
            .eye = VectorOf(camera_->eye.currentPosition)};
  }

  void SetPartnerPositions(const EventCameraPair& partners) override {
    StoreVector(camera_->lookAt.partnerPosition, partners.look_at);
    StoreVector(camera_->eye.partnerPosition, partners.eye);
  }

  void Advance(EventCameraStep step) override {
    Event_CameraState& camera = *camera_;
    switch (step) {
      case EventCameraStep::kLookAtTrack:
        UpdateEventCameraTrackEntry(&camera.lookAt, &camera.eye);
        break;
      case EventCameraStep::kEyeTrack:
        UpdateEventCameraTrackEntry(&camera.eye, &camera.lookAt);
        break;
      case EventCameraStep::kCameraRoll:
        UpdateEventAngleTweenEntry(&camera.cameraRoll);
        break;
      case EventCameraStep::kScreenRoll:
        UpdateEventAngleTweenEntry(&camera.capturedScreenRoll);
        break;
      case EventCameraStep::kProjectionDistance:
        UpdateEventLensTweenEntry(&camera.projectionDistance);
        break;
      case EventCameraStep::kNearClip:
        UpdateEventLensTweenEntry(&camera.nearClip);
        break;
      case EventCameraStep::kFarClip:
        UpdateEventLensTweenEntry(&camera.farClip);
        break;
      case EventCameraStep::kShake:
        UpdateEventCameraShakeEntry();
        break;
    }
  }

 private:
  Event_CameraState* camera_;
};
}  // namespace

// decomp: BATTLE.PRG vs_battle_applyCameraState (F01219,
// src/BATTLE/BATTLE.PRG/4A0A8.c:3117, 0x800BBF14-0x800BC1CC)
void ApplyEventCameraStateEntry() {
  Event_CameraState* camera = LiveCamera("ApplyEventCameraState");
  if (camera == nullptr) return;
  LiveCameraOutput output;
  EventCameraFrame frame = LoadFrame(*camera);
  ApplyEventCameraState(
      frame, Event_skipNextFieldCameraRestore != 0,
      static_cast<std::uint8_t>(Event_isScriptBackdropEnabled), output);
  StoreFrame(*camera, frame);
}

// decomp: BATTLE.PRG func_800BC9E0 (F01222, src/BATTLE/BATTLE.PRG/4A0A8.c:3371,
// 0x800BC9E0-0x800BCA8C)
void UpdateEventCameraTracksEntry() {
  Event_CameraState* camera = LiveCamera("UpdateEventCameraTracks");
  if (camera == nullptr) return;
  LiveCameraSteps steps(*camera);
  UpdateEventCameraTracks(steps);
}

// decomp: BATTLE.PRG func_800BD610 (F01228, src/BATTLE/BATTLE.PRG/4A0A8.c:3663,
// 0x800BD610-0x800BD6C4)
int EventCameraCompletionMaskEntry() {
  Event_CameraState* camera = LiveCamera("EventCameraCompletionMask");
  if (camera == nullptr) return 0;
  return EventCameraCompletionMask(LoadProgress(*camera));
}

std::uint64_t RefusedEventCameraFrameCount() noexcept {
  return refused_count.load(std::memory_order_relaxed);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB41C (F01197, src/BATTLE/BATTLE.PRG/4A0A8.c:2788,
// 0x800BB41C-0x800BB450), opcode 0xE0.
std::intptr_t Event_CommandWaitCameraTracksOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandWaitCameraTracks";
  if (instruction == nullptr) {
    event::Skip(kOperation, "missing instruction");
    return 0;
  }
  Event_CameraState* camera = event::LiveCamera(kOperation);
  if (camera == nullptr) return 0;
  const auto completion = static_cast<std::uint8_t>(
      event::EventCameraCompletionMask(event::LoadProgress(*camera)));
  const std::optional<std::intptr_t> result = event::CommandWaitCameraTracks(
      event::ScriptCommand{instruction, event::kWaitCameraTracksLength},
      completion);
  if (!result.has_value()) {
    event::Skip(kOperation, "short instruction");
    return 0;
  }
  return *result;
}
