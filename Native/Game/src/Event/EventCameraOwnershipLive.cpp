// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraOwnershipLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Memory/HeapLive.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/ScreenOverlayLive.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
}

namespace VS::Event {
namespace {
using Core::Math::Vec3;

// The original allocation size; the workspace keeps its PS1 layout.
constexpr std::size_t kEventCameraWorkspaceBytes = 0x28C;
static_assert(sizeof(Event_CameraState) == kEventCameraWorkspaceBytes);

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS_LOG_ERROR("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

Event_CameraState& RequiredCamera(const char* operation) {
  if (Event_camera == nullptr) Refuse(operation, "missing event camera");
  return *Event_camera;
}

Vec3 LoadVector(const VECTOR& vector) {
  return {static_cast<std::int32_t>(vector.vx),
          static_cast<std::int32_t>(vector.vy),
          static_cast<std::int32_t>(vector.vz)};
}

// _copyVector and _vectorAdd write three words and keep the pad.
void StoreVector(VECTOR& vector, Vec3 value) {
  vector.vx = value.x;
  vector.vy = value.y;
  vector.vz = value.z;
}

char StoredFlag(bool flag) { return static_cast<char>(flag ? 1 : 0); }

void StoreTrackMotion(Event_CameraTrack& track,
                      const EventCameraTrackStart& start) {
  track.motionMode = start.motion_mode;
  track.isFollowing = static_cast<u_char>(start.following ? 1 : 0);
  track.followRequest = static_cast<u_char>(start.follow_requested ? 1 : 0);
}

void StoreScreenValues(Event_TripleTween& tween,
                       const EventScreenTweenStart& start) {
  if (start.value2.has_value()) tween.currentValue2 = *start.value2;
  tween.currentValue1 = start.value1;
  tween.currentValue0 = start.value0;
}

void StoreLens(Event_LensTween& lens, const EventLensStart& start) {
  lens.durationFrames = static_cast<char>(start.duration_frames);
  lens.value = start.value;
  lens.hasChanged = StoredFlag(start.changed);
}

// The first stages: the tracks from the field camera and the camera roll.
void StoreTracksAndRoll(Event_CameraState& camera,
                        const EventCameraStart& start) {
  StoreVector(camera.eye.currentPosition, start.eye.position);
  StoreVector(camera.lookAt.partnerPosition, start.look_at.partner);
  StoreVector(camera.lookAt.currentPosition, start.look_at.position);
  StoreVector(camera.eye.partnerPosition, start.eye.partner);
  StoreTrackMotion(camera.lookAt, start.look_at);
  StoreTrackMotion(camera.eye, start.eye);
  camera.cameraRoll.durationFrames = start.camera_roll.duration_frames;
  camera.cameraRoll.angle = start.camera_roll.angle;
  camera.cameraRoll.hasChanged = StoredFlag(start.camera_roll.changed);
}

// Each framebuffer effect request follows the stores before it in the
// original; the tween durations land after their request.
void StoreScreenTweens(Event_CameraState& camera,
                       const EventCameraStart& start) {
  Render::SetLiveFramebufferEffectEnabled(start.screen_capture_enabled ? 1 : 0);
  camera.capturedScreenRoll.durationFrames = start.screen_roll.duration_frames;
  camera.capturedScreenRoll.angle = start.screen_roll.angle;
  Render::SetLiveFramebufferEffectRotation(start.screen_roll.angle);
  camera.capturedScreenRoll.hasChanged = StoredFlag(start.screen_roll.changed);
  StoreScreenValues(camera.screenScale, start.screen_scale);
  Render::SetLiveFramebufferEffectScale(&Event_kDefaultScreenTransform);
  camera.screenScale.durationFrames = start.screen_scale.duration_frames;
  StoreScreenValues(camera.screenTint, start.screen_tint);
  Render::SetLiveFramebufferEffectTint(&Event_kDefaultScreenTint);
  camera.screenTint.durationFrames = start.screen_tint.duration_frames;
  StoreScreenValues(camera.screenTranslation, start.screen_translation);
  Render::SetLiveFramebufferEffectOffset(&Event_kDefaultScreenTransform);
  camera.screenTranslation.durationFrames =
      start.screen_translation.duration_frames;
  Render::SetLiveFramebufferEffectBlendMode(start.screen_blend_mode);
}

void StoreLensShakeAndOrigin(Event_CameraState& camera,
                             const EventCameraStart& start) {
  StoreScreenValues(camera.screenEffectTween, start.screen_warp);
  camera.screenEffectTween.durationFrames = start.screen_warp.duration_frames;
  StoreLens(camera.projectionDistance, start.projection_distance);
  StoreLens(camera.nearClip, start.near_clip);
  StoreLens(camera.farClip, start.far_clip);
  camera.shakeOscillatorB.reserved0B = static_cast<char>(start.shake_b_busy);
  camera.shakeOscillatorA.reserved0B = static_cast<char>(start.shake_a_busy);
  camera.shakeOscillatorA.envelopeShape.count = start.shake_count;
  camera.originActorId = start.origin_actor;
}

EventCameraOrigin LoadOrigin(const Event_CameraState& camera) {
  return {.actor = camera.originActorId,
          .offset = LoadVector(camera.originOffset),
          .eye_position = LoadVector(camera.eye.currentPosition),
          .look_at_position = LoadVector(camera.lookAt.currentPosition)};
}

void StoreOrigin(Event_CameraState& camera, const EventCameraOrigin& origin) {
  StoreVector(camera.eye.currentPosition, origin.eye_position);
  StoreVector(camera.lookAt.currentPosition, origin.look_at_position);
  camera.originActorId = origin.actor;
  StoreVector(camera.originOffset, origin.offset);
}

class LiveEventCameraOriginHost final : public EventCameraOriginHost {
 public:
  int ResolveActorReference(std::uint16_t reference) override {
    return ResolveLiveScriptActorReference(reference);
  }

  std::optional<Core::Math::Position3> ActorPosition(int actor) override {
    const std::optional<Actors::ActorPlacementView> placement =
        Actors::LiveActorPlacementView(actor);
    if (!placement.has_value()) return std::nullopt;
    return Core::Math::Position3{.x = placement->position.x,
                                 .y = placement->position.y,
                                 .z = placement->position.z};
  }
};
}  // namespace

// decomp: BATTLE.PRG func_800BBBE8 (F01214, src/BATTLE/BATTLE.PRG/4A0A8.c:3014,
// 0x800BBBE8-0x800BBDDC)
void InitializeEventCameraEntry() {
  Event_camera = static_cast<Event_CameraState*>(
      Core::Memory::AllocateLiveFromHighEnd(sizeof(Event_CameraState)));
  Event_skipNextFieldCameraRestore = 0;
  Event_CameraState& camera = RequiredCamera("InitializeEventCamera");
  VECTOR field_eye{};
  VECTOR field_look_at{};
  Field::LiveCameraPosition(&field_eye);
  Field::LiveCameraLookAt(&field_look_at);
  const EventCameraStart start =
      StartEventCamera(LoadVector(field_eye), LoadVector(field_look_at));
  StoreTracksAndRoll(camera, start);
  StoreScreenTweens(camera, start);
  StoreLensShakeAndOrigin(camera, start);
}

// decomp: BATTLE.PRG func_800BBE04 (F01216, src/BATTLE/BATTLE.PRG/4A0A8.c:3084,
// 0x800BBE04-0x800BBE10)
void SetEventFieldCameraOverrideEntry(int field_camera_override) {
  Event_skipNextFieldCameraRestore =
      static_cast<char>(EventFieldCameraOverride(field_camera_override));
}

// decomp: BATTLE.PRG func_800BBE10 (F01217, src/BATTLE/BATTLE.PRG/4A0A8.c:3086,
// 0x800BBE10-0x800BBE94)
void SetEventCameraOriginActorEntry(std::uint16_t origin_actor) {
  Event_CameraState& camera = RequiredCamera("SetEventCameraOriginActor");
  LiveEventCameraOriginHost host;
  EventCameraOrigin origin = LoadOrigin(camera);
  SetEventCameraOriginActor(origin, origin_actor, host);
  StoreOrigin(camera, origin);
}

// decomp: BATTLE.PRG func_800BBE94 (F01218, src/BATTLE/BATTLE.PRG/4A0A8.c:3101,
// 0x800BBE94-0x800BBF14)
void UpdateEventCameraOriginEntry() {
  Event_CameraState& camera = RequiredCamera("UpdateEventCameraOrigin");
  LiveEventCameraOriginHost host;
  EventCameraOrigin origin = LoadOrigin(camera);
  UpdateEventCameraOrigin(origin, host);
  StoreOrigin(camera, origin);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB38C (F01195, src/BATTLE/BATTLE.PRG/4A0A8.c:2774,
// 0x800BB38C-0x800BB3BC), opcode 0xE8 in the normal and skip tables.
std::intptr_t Event_CommandSetCameraOriginActorOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandSetCameraOriginActor";
  if (instruction == nullptr) event::Refuse(kOperation, "missing instruction");
  Event_CameraState& camera = event::RequiredCamera(kOperation);
  event::LiveEventCameraOriginHost host;
  event::EventCameraOrigin origin = event::LoadOrigin(camera);
  static_cast<void>(event::CommandSetCameraOriginActor(
      event::ScriptCommand{instruction, event::kSetCameraOriginActorLength},
      origin, host));
  event::StoreOrigin(camera, origin);
  return 0;
}
