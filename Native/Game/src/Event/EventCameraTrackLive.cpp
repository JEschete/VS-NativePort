// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraTrackLive.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>

#include "Actors/ActorPlacement.hpp"
#include "Actors/ActorStateLive.hpp"
#include "Actors/SkeletonLive.hpp"
#include "Core/Log.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/MatrixLive.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/InterpolationCurveLive.h"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CollisionMapLive.hpp"

extern "C" {
#include "Event/Camera.h"
#include "Event/ScriptCamera.h"
#include "Event/ScriptControlOpcodes.h"
#include "State/Timers.h"
}

namespace VS::Event {
namespace {
using Core::Math::Vec3;

// The bone query's endpoint selector the motion asks for.
constexpr int kBoneEndpoint = 2;
// What the original's unassigned operation leaves the dispatcher.
constexpr std::intptr_t kUnassignedResult = -1;

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS_LOG_ERROR("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

Event_CameraState& RequiredCamera(const char* operation) {
  if (Event_camera == nullptr) Refuse(operation, "missing event camera");
  return *Event_camera;
}

Event_CameraTrack& RequiredTrack(Event_CameraTrack* track,
                                 const char* operation) {
  if (track == nullptr) Refuse(operation, "missing camera track");
  return *track;
}

const char* TrackName(const Event_CameraTrack& track) {
  return Event_camera != nullptr && &track == &Event_camera->eye ? "eye"
                                                                 : "look_at";
}

Vec3 LoadVector(const VECTOR& vector) {
  return {static_cast<std::int32_t>(vector.vx),
          static_cast<std::int32_t>(vector.vy),
          static_cast<std::int32_t>(vector.vz)};
}

// The original writes three words and keeps the pad.
void StoreVector(VECTOR& vector, Vec3 value) {
  vector.vx = value.x;
  vector.vy = value.y;
  vector.vz = value.z;
}

EventCameraTrack LoadTrack(const Event_CameraTrack& record) {
  return {.motion_mode = record.motionMode,
          .primary_easing = record.easingPrimary,
          .secondary_easing = record.easingSecondary,
          .tertiary_easing = record.easingTertiary,
          .elapsed_frames = record.elapsedFrames,
          .duration_frames = record.durationFrames,
          .follow_request = record.followRequest,
          .following = record.isFollowing,
          .motion_scalar = record.motionScalar,
          .motion_scalar_delta = record.motionScalarDelta,
          .orbit_angle = record.orbitAngle,
          .orbit_angle_delta = record.orbitAngleDelta,
          .start_position = LoadVector(record.startPosition),
          .destination = LoadVector(record.destination),
          .displacement = LoadVector(record.displacement),
          .height_offset = LoadVector(record.heightOffset),
          .follow_delta = LoadVector(record.followDelta),
          .follow_position = LoadVector(record.followPosition),
          .previous_position = LoadVector(record.previousCameraPosition),
          .computed_position = LoadVector(record.computedPosition),
          .current_position = LoadVector(record.currentPosition),
          .partner_position = LoadVector(record.partnerPosition)};
}

void StoreTrack(Event_CameraTrack& record, const EventCameraTrack& track) {
  record.motionMode = track.motion_mode;
  record.easingPrimary = track.primary_easing;
  record.easingSecondary = track.secondary_easing;
  record.easingTertiary = track.tertiary_easing;
  record.elapsedFrames = track.elapsed_frames;
  record.durationFrames = track.duration_frames;
  record.followRequest = track.follow_request;
  record.isFollowing = track.following;
  record.motionScalar = track.motion_scalar;
  record.motionScalarDelta = track.motion_scalar_delta;
  record.orbitAngle = track.orbit_angle;
  record.orbitAngleDelta = track.orbit_angle_delta;
  StoreVector(record.startPosition, track.start_position);
  StoreVector(record.destination, track.destination);
  StoreVector(record.displacement, track.displacement);
  StoreVector(record.heightOffset, track.height_offset);
  StoreVector(record.followDelta, track.follow_delta);
  StoreVector(record.followPosition, track.follow_position);
  StoreVector(record.previousCameraPosition, track.previous_position);
  StoreVector(record.computedPosition, track.computed_position);
  StoreVector(record.currentPosition, track.current_position);
  StoreVector(record.partnerPosition, track.partner_position);
}

}  // namespace

EventCameraTracks LoadLiveEventCameraTracks(const Event_CameraState& camera) {
  return {.look_at = LoadTrack(camera.lookAt),
          .eye = LoadTrack(camera.eye),
          .origin_actor = camera.originActorId,
          .origin_offset = LoadVector(camera.originOffset),
          .last_terrain_position = LoadVector(camera.lastTerrainPosition)};
}

void StoreLiveEventCameraTracks(Event_CameraState& camera,
                                const EventCameraTracks& tracks) {
  StoreTrack(camera.lookAt, tracks.look_at);
  StoreTrack(camera.eye, tracks.eye);
  StoreVector(camera.lastTerrainPosition, tracks.last_terrain_position);
}

std::int16_t LiveEventCameraTrackHost::FloorHeight(std::int32_t world_x,
                                                   std::int32_t world_z) {
  return Field::LiveFloorHeightAt(world_x, world_z);
}

std::int32_t LiveEventCameraTrackHost::Ease(std::uint8_t curve,
                                            std::uint8_t frame,
                                            std::uint8_t duration) {
  return Event_EvaluateInterpolationCurveEntry(curve, frame, duration);
}

int LiveEventCameraTrackHost::ResolveActorReference(std::uint16_t reference) {
  return ResolveLiveScriptActorReference(reference);
}

std::optional<std::int16_t> LiveEventCameraTrackHost::ActorFacing(int actor) {
  const std::optional<Actors::ActorPlacementView> placement =
      Actors::LiveActorPlacementView(actor);
  if (!placement.has_value()) return std::nullopt;
  return placement->position.w;
}

std::optional<Core::Math::Position3>
LiveEventCameraTrackHost::TaggedBonePosition(int actor, int tag) {
  SVECTOR position{};
  if (Actors::LiveActorTaggedBonePosition(actor, tag, &position,
                                          kBoneEndpoint) < 0) {
    return std::nullopt;
  }
  return Core::Math::Position3{
      .x = position.vx, .y = position.vy, .z = position.vz};
}

namespace {
// The track update needs the workspace only for the terrain glide's last
// position, where the original read it.
void UpdateTrack(Event_CameraTrack& record, std::int32_t tick_speed,
                 const char* operation) {
  LiveEventCameraTrackHost host;
  EventCameraTrack track = LoadTrack(record);
  // Zero-duration interpolation provenance, logged before the update as the
  // generated path did.
  if (track.motion_mode != kEventCameraTrackIdle &&
      track.motion_mode != kCameraMotionTerrainGlide &&
      track.duration_frames == 0) {
    VS_LOG_WARN("event.camera",
                "zero duration track={} mode={} easing={} elapsed={} "
                "tick_speed={}",
                TrackName(record), unsigned{track.motion_mode},
                unsigned{track.primary_easing}, unsigned{track.elapsed_frames},
                tick_speed);
  }
  Event_CameraState* camera = nullptr;
  Vec3 last_terrain_position;
  if (track.motion_mode == kCameraMotionTerrainGlide) {
    camera = &RequiredCamera(operation);
    last_terrain_position = LoadVector(camera->lastTerrainPosition);
  }
  UpdateEventCameraTrack(track, last_terrain_position, tick_speed, host);
  StoreTrack(record, track);
  if (camera != nullptr) {
    StoreVector(camera->lastTerrainPosition, last_terrain_position);
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800BC2E8 (F01221, src/BATTLE/BATTLE.PRG/4A0A8.c:3200,
// 0x800BC2E8-0x800BC9E0)
std::intptr_t CommandCameraMotionEntry(std::uint8_t* instruction,
                                       std::int16_t /*thread_index*/) {
  constexpr const char* kOperation = "CommandCameraMotion";
  if (instruction == nullptr) Refuse(kOperation, "missing instruction");
  Event_CameraState& camera = RequiredCamera(kOperation);
  LiveEventCameraTrackHost host;
  EventCameraTracks tracks = LoadLiveEventCameraTracks(camera);
  const ScriptCommand command{instruction,
                              CameraMotionCommandLength(instruction[0])};
  const CameraMotionOutcome outcome =
      CommandCameraMotion(command, tracks, host);
  StoreLiveEventCameraTracks(camera, tracks);
  if (outcome == CameraMotionOutcome::kApplied) {
    const Event_CameraTrack& started =
        (instruction[0] & kCameraMotionEyeTrackBit) != 0 ? camera.eye
                                                         : camera.lookAt;
    VS_LOG_DEBUG("event.camera", "start track={} mode={} easing={} duration={}",
                 TrackName(started), unsigned{started.motionMode},
                 unsigned{started.easingPrimary},
                 unsigned{started.durationFrames});
  }
  if (outcome != CameraMotionOutcome::kUnassigned) return 0;
  VS_LOG_WARN("event.camera", "{}: opcode {:#04x} has no operation", kOperation,
              instruction[0]);
  return kUnassignedResult;
}

// decomp: BATTLE.PRG func_800BCA8C (F01223, src/BATTLE/BATTLE.PRG/4A0A8.c:3389,
// 0x800BCA8C-0x800BCFB4)
void UpdateEventCameraTrackEntry(Event_CameraTrack* track,
                                 Event_CameraTrack* /*other*/) {
  constexpr const char* kOperation = "UpdateEventCameraTrack";
  UpdateTrack(RequiredTrack(track, kOperation), State_tickSpeed, kOperation);
}

// decomp: BATTLE.PRG func_800BCFB4 (F01224, src/BATTLE/BATTLE.PRG/4A0A8.c:3502,
// 0x800BCFB4-0x800BD2B8)
void AdvanceTerrainCameraTrackEntry(Event_CameraTrack* track) {
  constexpr const char* kOperation = "AdvanceTerrainCameraTrack";
  Event_CameraTrack& record = RequiredTrack(track, kOperation);
  Event_CameraState& camera = RequiredCamera(kOperation);
  LiveEventCameraTrackHost host;
  EventCameraTrack state = LoadTrack(record);
  Vec3 last_terrain_position = LoadVector(camera.lastTerrainPosition);
  AdvanceTerrainCameraTrack(state, last_terrain_position, host);
  StoreTrack(record, state);
  StoreVector(camera.lastTerrainPosition, last_terrain_position);
}

// decomp: BATTLE.PRG func_800BD2B8 (F01225, src/BATTLE/BATTLE.PRG/4A0A8.c:3556,
// 0x800BD2B8-0x800BD444)
int EventCameraGroundClearanceEntry(MATRIX* probe) {
  if (probe == nullptr) {
    Refuse("EventCameraGroundClearance", "missing probe frame");
  }
  LiveEventCameraTrackHost host;
  return EventCameraGroundClearance(Core::Math::TransformOf(*probe), host);
}

// decomp: BATTLE.PRG func_800BE53C (F01239, src/BATTLE/BATTLE.PRG/4A0A8.c:4074,
// 0x800BE53C-0x800BE5A4)
void ScaleEventCameraTrackDurationEntry(Event_CameraTrack* track) {
  Event_CameraTrack& record =
      RequiredTrack(track, "ScaleEventCameraTrackDuration");
  EventCameraTrack state = LoadTrack(record);
  ScaleEventCameraTrackDuration(state);
  record.durationFrames = state.duration_frames;
}

// decomp: BATTLE.PRG func_800BE5A4 (F01240, src/BATTLE/BATTLE.PRG/4A0A8.c:4082,
// 0x800BE5A4-0x800BE628)
void ScaleEventCameraDurationEntry(std::int16_t selector) {
  Event_CameraState& camera = RequiredCamera("ScaleEventCameraDuration");
  EventCameraTrack look_at = LoadTrack(camera.lookAt);
  EventCameraTrack eye = LoadTrack(camera.eye);
  ScaleEventCameraDuration(look_at, eye, selector);
  camera.lookAt.durationFrames = look_at.duration_frames;
  camera.eye.durationFrames = eye.duration_frames;
}

// decomp: BATTLE.PRG _setVectorImm (F01246, src/BATTLE/BATTLE.PRG/4A0A8.c:4148,
// 0x800BE7B8-0x800BE824)
VECTOR* ReadScriptFixedVectorEntry(VECTOR* destination,
                                   std::uint8_t* operands) {
  constexpr const char* kOperation = "ReadScriptFixedVector";
  if (destination == nullptr) Refuse(kOperation, "missing destination");
  if (operands == nullptr) Refuse(kOperation, "missing operands");
  StoreVector(*destination,
              ReadScriptFixedVector(
                  std::span<const std::uint8_t, kScriptFixedVectorBytes>{
                      operands, kScriptFixedVectorBytes}));
  return destination;
}
}  // namespace VS::Event

// decomp: BATTLE.PRG func_800BC2E8 (F01221,
// src/BATTLE/BATTLE.PRG/4A0A8.c:3200), opcodes 0xC0-0xDF in the normal and
// skip tables and the field camera and orbit return commands ScriptCamera.cpp
// issues.
std::intptr_t Event_CommandCameraMotionOpcode(std::uint8_t* instruction,
                                              std::int16_t thread_index) {
  return VS::Event::CommandCameraMotionEntry(instruction, thread_index);
}
