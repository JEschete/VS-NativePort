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
#include "Event/Camera.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CameraLive.hpp"
#include "Render/FramebufferEffects.hpp"
#include "Render/ScreenOverlayLive.hpp"

using VS::Core::Gte::CVECTOR;
using VS::Core::Gte::VECTOR;

namespace VS::Event {
namespace {
using Core::Math::Vec3;

// The original allocation size; the workspace keeps its PS1 layout.
constexpr std::size_t kEventCameraWorkspaceBytes = 0x28C;
static_assert(sizeof(CameraState) == kEventCameraWorkspaceBytes);

// The framebuffer effect settings the camera start requests: a neutral tint
// and a unit (ONE) record, which the original passes to both the scale and
// the offset setters (src/BATTLE/BATTLE.PRG/4DEA8.c:3699, 3704 and 3710).
// decomp: D_800E9C08 (BATTLE.PRG file offset 0x81408)
constexpr CVECTOR kDefaultScreenTint{.r = 0x80, .g = 0x80, .b = 0x80, .cd = 0};
// decomp: D_800E9C0C (BATTLE.PRG file offset 0x8140C)
constexpr Render::FramebufferEffectTranslation kDefaultScreenTransform{
    .x = 0x1000,
    .y = 0x1000,
    .reserved08 = 0x1000,
    .reserved0c = 0,
};

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

CameraState& RequiredCamera(const char* operation) {
  if (cameraWorkspace == nullptr) Refuse(operation, "missing event camera");
  return *cameraWorkspace;
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

std::uint8_t StoredFlag(bool flag) { return flag ? 1 : 0; }

void StoreTrackMotion(CameraTrack& track, const EventCameraTrackStart& start) {
  track.motion_mode = start.motion_mode;
  track.is_following = start.following ? 1 : 0;
  track.follow_request = start.follow_requested ? 1 : 0;
}

void StoreScreenValues(TripleTween& tween, const EventScreenTweenStart& start) {
  if (start.value2.has_value()) tween.current_value2 = *start.value2;
  tween.current_value1 = start.value1;
  tween.current_value0 = start.value0;
}

void StoreLens(LensTween& lens, const EventLensStart& start) {
  lens.duration_frames = start.duration_frames;
  lens.value = start.value;
  lens.has_changed = StoredFlag(start.changed);
}

// The first stages: the tracks from the field camera and the camera roll.
void StoreTracksAndRoll(CameraState& camera, const EventCameraStart& start) {
  StoreVector(camera.eye.current_position, start.eye.position);
  StoreVector(camera.look_at.partner_position, start.look_at.partner);
  StoreVector(camera.look_at.current_position, start.look_at.position);
  StoreVector(camera.eye.partner_position, start.eye.partner);
  StoreTrackMotion(camera.look_at, start.look_at);
  StoreTrackMotion(camera.eye, start.eye);
  camera.camera_roll.duration_frames = start.camera_roll.duration_frames;
  camera.camera_roll.angle = start.camera_roll.angle;
  camera.camera_roll.has_changed = StoredFlag(start.camera_roll.changed);
}

// Each framebuffer effect request follows the stores before it in the
// original; the tween durations land after their request.
void StoreScreenTweens(CameraState& camera, const EventCameraStart& start) {
  Render::SetLiveFramebufferEffectEnabled(start.screen_capture_enabled ? 1 : 0);
  camera.captured_screen_roll.duration_frames =
      start.screen_roll.duration_frames;
  camera.captured_screen_roll.angle = start.screen_roll.angle;
  Render::SetLiveFramebufferEffectRotation(start.screen_roll.angle);
  camera.captured_screen_roll.has_changed =
      StoredFlag(start.screen_roll.changed);
  StoreScreenValues(camera.screen_scale, start.screen_scale);
  Render::SetLiveFramebufferEffectScale(&kDefaultScreenTransform);
  camera.screen_scale.duration_frames = start.screen_scale.duration_frames;
  StoreScreenValues(camera.screen_tint, start.screen_tint);
  Render::SetLiveFramebufferEffectTint(&kDefaultScreenTint);
  camera.screen_tint.duration_frames = start.screen_tint.duration_frames;
  StoreScreenValues(camera.screen_translation, start.screen_translation);
  Render::SetLiveFramebufferEffectOffset(&kDefaultScreenTransform);
  camera.screen_translation.duration_frames =
      start.screen_translation.duration_frames;
  Render::SetLiveFramebufferEffectBlendMode(start.screen_blend_mode);
}

void StoreLensShakeAndOrigin(CameraState& camera,
                             const EventCameraStart& start) {
  StoreScreenValues(camera.screen_effect_tween, start.screen_warp);
  camera.screen_effect_tween.duration_frames =
      start.screen_warp.duration_frames;
  StoreLens(camera.projection_distance, start.projection_distance);
  StoreLens(camera.near_clip, start.near_clip);
  StoreLens(camera.far_clip, start.far_clip);
  camera.shake_oscillators.at(1).duration_frames = start.shake_durations[1];
  camera.shake_oscillators.at(0).duration_frames = start.shake_durations[0];
  camera.shake_active_count = start.shake_count;
  camera.origin_actor_id = start.origin_actor;
}

EventCameraOrigin LoadOrigin(const CameraState& camera) {
  return {.actor = camera.origin_actor_id,
          .offset = LoadVector(camera.origin_offset),
          .eye_position = LoadVector(camera.eye.current_position),
          .look_at_position = LoadVector(camera.look_at.current_position)};
}

void StoreOrigin(CameraState& camera, const EventCameraOrigin& origin) {
  StoreVector(camera.eye.current_position, origin.eye_position);
  StoreVector(camera.look_at.current_position, origin.look_at_position);
  camera.origin_actor_id = origin.actor;
  StoreVector(camera.origin_offset, origin.offset);
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
  cameraWorkspace = static_cast<CameraState*>(
      Core::Memory::AllocateLiveFromHighEnd(sizeof(CameraState)));
  skipNextFieldCameraRestore = 0;
  CameraState& camera = RequiredCamera("InitializeEventCamera");
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
  skipNextFieldCameraRestore = EventFieldCameraOverride(field_camera_override);
}

// decomp: BATTLE.PRG func_800BBE10 (F01217, src/BATTLE/BATTLE.PRG/4A0A8.c:3086,
// 0x800BBE10-0x800BBE94)
void SetEventCameraOriginActorEntry(std::uint16_t origin_actor) {
  CameraState& camera = RequiredCamera("SetEventCameraOriginActor");
  LiveEventCameraOriginHost host;
  EventCameraOrigin origin = LoadOrigin(camera);
  SetEventCameraOriginActor(origin, origin_actor, host);
  StoreOrigin(camera, origin);
}

// decomp: BATTLE.PRG func_800BBE94 (F01218, src/BATTLE/BATTLE.PRG/4A0A8.c:3101,
// 0x800BBE94-0x800BBF14)
void UpdateEventCameraOriginEntry() {
  CameraState& camera = RequiredCamera("UpdateEventCameraOrigin");
  LiveEventCameraOriginHost host;
  EventCameraOrigin origin = LoadOrigin(camera);
  UpdateEventCameraOrigin(origin, host);
  StoreOrigin(camera, origin);
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BB38C (F01195, src/BATTLE/BATTLE.PRG/4A0A8.c:2774,
// 0x800BB38C-0x800BB3BC), opcode 0xE8 in the normal and skip tables.
std::intptr_t VS::Event::CommandSetCameraOriginActorOpcode(
    std::uint8_t* instruction, std::int16_t /*thread_index*/) {
  namespace event = VS::Event;
  constexpr const char* kOperation = "CommandSetCameraOriginActor";
  if (instruction == nullptr) event::Refuse(kOperation, "missing instruction");
  VS::Event::CameraState& camera = event::RequiredCamera(kOperation);
  event::LiveEventCameraOriginHost host;
  event::EventCameraOrigin origin = event::LoadOrigin(camera);
  static_cast<void>(event::CommandSetCameraOriginActor(
      event::ScriptCommand{instruction, event::kSetCameraOriginActorLength},
      origin, host));
  event::StoreOrigin(camera, origin);
  return 0;
}
