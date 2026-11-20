// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraFrameLive.hpp"

#include <atomic>
#include <cstdint>
#include <optional>

#include "Core/Gte.hpp"
#include "Core/Log.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/Camera.hpp"
#include "Event/EventCameraFrame.hpp"
#include "Event/EventCameraLensLive.hpp"
#include "Event/EventCameraTrackLive.hpp"
#include "Event/EventScreenTweensLive.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptVmLiveState.hpp"
#include "Field/CameraLive.hpp"
#include "Render/CameraViewLive.hpp"
#include "Render/ScreenOverlayLive.hpp"

using VS::Core::Gte::ApplyMatrixLV;
using VS::Core::Gte::MATRIX;
using VS::Core::Gte::SVECTOR;
using VS::Core::Gte::VECTOR;

namespace VS::Event {
namespace {
using Core::Math::Mat3;
using Core::Math::Vec3;

std::atomic<std::uint64_t> refusedCount{0};

void Skip(const char* operation, const char* reason) {
  refusedCount.fetch_add(1, std::memory_order_relaxed);
  VS::Core::LogError("event.camera", "{} skipped: {}", operation, reason);
}

CameraState* LiveCamera(const char* operation) {
  if (cameraWorkspace == nullptr) Skip(operation, "missing event camera");
  return cameraWorkspace;
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

EventCameraSetting LoadAngle(const AngleTween& tween) {
  return {.changed = tween.has_changed, .value = tween.angle};
}

EventCameraSetting LoadLens(const LensTween& tween) {
  return {.changed = tween.has_changed, .value = tween.value};
}

EventCameraFrame LoadFrame(const CameraState& camera) {
  return {.look_at = VectorOf(camera.look_at.current_position),
          .eye = VectorOf(camera.eye.current_position),
          .origin_actor = camera.origin_actor_id,
          .origin_offset = VectorOf(camera.origin_offset),
          .shake_active_count = camera.shake_active_count,
          .look_at_shake = camera.shake_look_at_offset,
          .eye_shake = camera.shake_eye_offset,
          .camera_roll = LoadAngle(camera.camera_roll),
          .screen_roll = LoadAngle(camera.captured_screen_roll),
          .projection_distance = LoadLens(camera.projection_distance),
          .near_clip = LoadLens(camera.near_clip),
          .far_clip = LoadLens(camera.far_clip)};
}

// Writes what ApplyEventCameraState may change: the tracks' positions (taken
// from the field camera) and the settings' changed flags.
void StoreFrame(CameraState& camera, const EventCameraFrame& frame) {
  StoreVector(camera.look_at.current_position, frame.look_at);
  StoreVector(camera.eye.current_position, frame.eye);
  camera.camera_roll.has_changed = frame.camera_roll.changed;
  camera.captured_screen_roll.has_changed = frame.screen_roll.changed;
  camera.projection_distance.has_changed = frame.projection_distance.changed;
  camera.near_clip.has_changed = frame.near_clip.changed;
  camera.far_clip.has_changed = frame.far_clip.changed;
}

EventCameraProgress LoadProgress(const CameraState& camera) {
  return {.look_at_mode = camera.look_at.motion_mode,
          .eye_mode = camera.eye.motion_mode,
          .camera_roll_duration = camera.camera_roll.duration_frames,
          .projection_duration = camera.projection_distance.duration_frames,
          .near_clip_duration = camera.near_clip.duration_frames,
          .far_clip_duration = camera.far_clip.duration_frames,
          .screen_effect_duration = camera.screen_effect_tween.duration_frames,
          .shake_durations = {camera.shake_oscillators.at(0).duration_frames,
                              camera.shake_oscillators.at(1).duration_frames}};
}

// The field camera, the renderer and the GTE. vs_battle_applyCameraState kept
// the look-at and eye shake vectors (0x1F800098, 0x1F8000A8), the yaw-only
// rotation angles (0x1F8000D8) and its matrix (0x1F8000F8) in the scratchpad;
// each is written before it is read within one call, so they are locals.
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
    SVECTOR angles{};
    MATRIX matrix{};
    angles.vx = static_cast<std::int16_t>(rotation.x);
    angles.vy = static_cast<std::int16_t>(rotation.y);
    angles.vz = static_cast<std::int16_t>(rotation.z);
    Core::Math::BuildLiveRotationXyz(&angles, &matrix);
    return Core::Math::MatrixOf(matrix);
  }

  Vec3 ApplyMatrix(const Mat3& rotation, Vec3 vector,
                   EventShakeTarget /*target*/) override {
    MATRIX matrix{};
    VECTOR record{};
    Core::Math::StoreMatrix(matrix, rotation);
    StoreVector(record, vector);
    ApplyMatrixLV(&matrix, &record, &record);
    return VectorOf(record);
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
};

// The live workspace's track positions and per-part updates.
class LiveCameraSteps final : public EventCameraStepPort {
 public:
  explicit LiveCameraSteps(CameraState& camera) : camera_(&camera) {}

  EventCameraPair CurrentPositions() override {
    return {.look_at = VectorOf(camera_->look_at.current_position),
            .eye = VectorOf(camera_->eye.current_position)};
  }

  void SetPartnerPositions(const EventCameraPair& partners) override {
    StoreVector(camera_->look_at.partner_position, partners.look_at);
    StoreVector(camera_->eye.partner_position, partners.eye);
  }

  void Advance(EventCameraStep step) override {
    CameraState& camera = *camera_;
    switch (step) {
      case EventCameraStep::kLookAtTrack:
        UpdateEventCameraTrackEntry(&camera.look_at, &camera.eye);
        break;
      case EventCameraStep::kEyeTrack:
        UpdateEventCameraTrackEntry(&camera.eye, &camera.look_at);
        break;
      case EventCameraStep::kCameraRoll:
        UpdateEventAngleTweenEntry(&camera.camera_roll);
        break;
      case EventCameraStep::kScreenRoll:
        UpdateEventAngleTweenEntry(&camera.captured_screen_roll);
        break;
      case EventCameraStep::kProjectionDistance:
        UpdateEventLensTweenEntry(&camera.projection_distance);
        break;
      case EventCameraStep::kNearClip:
        UpdateEventLensTweenEntry(&camera.near_clip);
        break;
      case EventCameraStep::kFarClip:
        UpdateEventLensTweenEntry(&camera.far_clip);
        break;
      case EventCameraStep::kShake:
        UpdateEventCameraShakeEntry();
        break;
    }
  }

 private:
  CameraState* camera_;
};
}  // namespace

// decomp: BATTLE.PRG vs_battle_applyCameraState (F01219,
// src/BATTLE/BATTLE.PRG/4A0A8.c:3117, 0x800BBF14-0x800BC1CC)
void ApplyEventCameraStateEntry() {
  CameraState* camera = LiveCamera("ApplyEventCameraState");
  if (camera == nullptr) return;
  LiveCameraOutput output;
  EventCameraFrame frame = LoadFrame(*camera);
  ApplyEventCameraState(frame, skipNextFieldCameraRestore != 0,
                        isScriptBackdropEnabled, output);
  StoreFrame(*camera, frame);
}

// decomp: BATTLE.PRG func_800BC9E0 (F01222, src/BATTLE/BATTLE.PRG/4A0A8.c:3371,
// 0x800BC9E0-0x800BCA8C)
void UpdateEventCameraTracksEntry() {
  CameraState* camera = LiveCamera("UpdateEventCameraTracks");
  if (camera == nullptr) return;
  LiveCameraSteps steps(*camera);
  UpdateEventCameraTracks(steps);
}

// decomp: BATTLE.PRG func_800BD610 (F01228, src/BATTLE/BATTLE.PRG/4A0A8.c:3663,
// 0x800BD610-0x800BD6C4)
int EventCameraCompletionMaskEntry() {
  CameraState* camera = LiveCamera("EventCameraCompletionMask");
  if (camera == nullptr) return 0;
  return EventCameraCompletionMask(LoadProgress(*camera));
}

std::uint64_t RefusedEventCameraFrameCount() noexcept {
  return refusedCount.load(std::memory_order_relaxed);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB41C (F01197, src/BATTLE/BATTLE.PRG/4A0A8.c:2788,
// 0x800BB41C-0x800BB450), opcode 0xE0.
std::intptr_t VS::Event::CommandWaitCameraTracksOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandWaitCameraTracks";
  if (instruction == nullptr) {
    event::Skip(kOperation, "missing instruction");
    return 0;
  }
  VS::Event::CameraState* camera = event::LiveCamera(kOperation);
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
