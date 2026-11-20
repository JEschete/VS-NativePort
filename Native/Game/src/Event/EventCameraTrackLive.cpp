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
#include "Event/Camera.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/InterpolationCurveLive.hpp"
#include "Event/ScriptCamera.hpp"
#include "Event/ScriptCommand.hpp"
#include "Event/ScriptControlOpcodes.hpp"
#include "Event/ScriptVmLive.hpp"
#include "Field/CollisionMapLive.hpp"
#include "State/Timers.hpp"

namespace VS::Event {
namespace {
using Core::Math::Vec3;

// The bone query's endpoint selector the motion asks for.
constexpr int kBoneEndpoint = 2;
// What the original's unassigned operation leaves the dispatcher.
constexpr std::intptr_t kUnassignedResult = -1;

[[noreturn]] void Refuse(const char* operation, const char* reason) {
  VS::Core::LogError("event.camera", "{} refused: {}", operation, reason);
  std::abort();
}

CameraState& RequiredCamera(const char* operation) {
  if (cameraWorkspace == nullptr) Refuse(operation, "missing event camera");
  return *cameraWorkspace;
}

CameraTrack& RequiredTrack(CameraTrack* track, const char* operation) {
  if (track == nullptr) Refuse(operation, "missing camera track");
  return *track;
}

const char* TrackName(const CameraTrack& track) {
  return cameraWorkspace != nullptr && &track == &cameraWorkspace->eye
             ? "eye"
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

EventCameraTrack LoadTrack(const CameraTrack& record) {
  return {.motion_mode = record.motion_mode,
          .primary_easing = record.easing_primary,
          .secondary_easing = record.easing_secondary,
          .tertiary_easing = record.easing_tertiary,
          .elapsed_frames = record.elapsed_frames,
          .duration_frames = record.duration_frames,
          .follow_request = record.follow_request,
          .following = record.is_following,
          .motion_scalar = record.motion_scalar,
          .motion_scalar_delta = record.motion_scalar_delta,
          .orbit_angle = record.orbit_angle,
          .orbit_angle_delta = record.orbit_angle_delta,
          .start_position = LoadVector(record.start_position),
          .destination = LoadVector(record.destination),
          .displacement = LoadVector(record.displacement),
          .height_offset = LoadVector(record.height_offset),
          .follow_delta = LoadVector(record.follow_delta),
          .follow_position = LoadVector(record.follow_position),
          .previous_position = LoadVector(record.previous_camera_position),
          .computed_position = LoadVector(record.computed_position),
          .current_position = LoadVector(record.current_position),
          .partner_position = LoadVector(record.partner_position)};
}

void StoreTrack(CameraTrack& record, const EventCameraTrack& track) {
  record.motion_mode = track.motion_mode;
  record.easing_primary = track.primary_easing;
  record.easing_secondary = track.secondary_easing;
  record.easing_tertiary = track.tertiary_easing;
  record.elapsed_frames = track.elapsed_frames;
  record.duration_frames = track.duration_frames;
  record.follow_request = track.follow_request;
  record.is_following = track.following;
  record.motion_scalar = track.motion_scalar;
  record.motion_scalar_delta = track.motion_scalar_delta;
  record.orbit_angle = track.orbit_angle;
  record.orbit_angle_delta = track.orbit_angle_delta;
  StoreVector(record.start_position, track.start_position);
  StoreVector(record.destination, track.destination);
  StoreVector(record.displacement, track.displacement);
  StoreVector(record.height_offset, track.height_offset);
  StoreVector(record.follow_delta, track.follow_delta);
  StoreVector(record.follow_position, track.follow_position);
  StoreVector(record.previous_camera_position, track.previous_position);
  StoreVector(record.computed_position, track.computed_position);
  StoreVector(record.current_position, track.current_position);
  StoreVector(record.partner_position, track.partner_position);
}

}  // namespace

EventCameraTracks LoadLiveEventCameraTracks(const CameraState& camera) {
  return {.look_at = LoadTrack(camera.look_at),
          .eye = LoadTrack(camera.eye),
          .origin_actor = camera.origin_actor_id,
          .origin_offset = LoadVector(camera.origin_offset),
          .last_terrain_position = LoadVector(camera.last_terrain_position)};
}

void StoreLiveEventCameraTracks(CameraState& camera,
                                const EventCameraTracks& tracks) {
  StoreTrack(camera.look_at, tracks.look_at);
  StoreTrack(camera.eye, tracks.eye);
  StoreVector(camera.last_terrain_position, tracks.last_terrain_position);
}

std::int16_t LiveEventCameraTrackHost::FloorHeight(std::int32_t world_x,
                                                   std::int32_t world_z) {
  return Field::LiveFloorHeightAt(world_x, world_z);
}

std::int32_t LiveEventCameraTrackHost::Ease(std::uint8_t curve,
                                            std::uint8_t frame,
                                            std::uint8_t duration) {
  return EvaluateInterpolationCurveEntry(curve, frame, duration);
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
void UpdateTrack(CameraTrack& record, std::int32_t tick_speed,
                 const char* operation) {
  LiveEventCameraTrackHost host;
  EventCameraTrack track = LoadTrack(record);
  // Zero-duration interpolation provenance, logged before the update as the
  // generated path did.
  if (track.motion_mode != kEventCameraTrackIdle &&
      track.motion_mode != kCameraMotionTerrainGlide &&
      track.duration_frames == 0) {
    VS::Core::LogWarn("event.camera",
                      "zero duration track={} mode={} easing={} elapsed={} "
                      "tick_speed={}",
                      TrackName(record), unsigned{track.motion_mode},
                      unsigned{track.primary_easing},
                      unsigned{track.elapsed_frames}, tick_speed);
  }
  CameraState* camera = nullptr;
  Vec3 last_terrain_position;
  if (track.motion_mode == kCameraMotionTerrainGlide) {
    camera = &RequiredCamera(operation);
    last_terrain_position = LoadVector(camera->last_terrain_position);
  }
  UpdateEventCameraTrack(track, last_terrain_position, tick_speed, host);
  StoreTrack(record, track);
  if (camera != nullptr) {
    StoreVector(camera->last_terrain_position, last_terrain_position);
  }
}
}  // namespace

// decomp: BATTLE.PRG func_800BC2E8 (F01221, src/BATTLE/BATTLE.PRG/4A0A8.c:3200,
// 0x800BC2E8-0x800BC9E0)
std::intptr_t CommandCameraMotionEntry(std::uint8_t* instruction,
                                       std::int16_t /*thread_index*/) {
  constexpr const char* kOperation = "CommandCameraMotion";
  if (instruction == nullptr) Refuse(kOperation, "missing instruction");
  CameraState& camera = RequiredCamera(kOperation);
  LiveEventCameraTrackHost host;
  EventCameraTracks tracks = LoadLiveEventCameraTracks(camera);
  const ScriptCommand command{instruction,
                              CameraMotionCommandLength(instruction[0])};
  const CameraMotionOutcome outcome =
      CommandCameraMotion(command, tracks, host);
  StoreLiveEventCameraTracks(camera, tracks);
  if (outcome == CameraMotionOutcome::kApplied) {
    const CameraTrack& started =
        (instruction[0] & kCameraMotionEyeTrackBit) != 0 ? camera.eye
                                                         : camera.look_at;
    VS::Core::LogDebug(
        "event.camera", "start track={} mode={} easing={} duration={}",
        TrackName(started), unsigned{started.motion_mode},
        unsigned{started.easing_primary}, unsigned{started.duration_frames});
  }
  if (outcome != CameraMotionOutcome::kUnassigned) return 0;
  VS::Core::LogWarn("event.camera", "{}: opcode {:#04x} has no operation",
                    kOperation, instruction[0]);
  return kUnassignedResult;
}

// decomp: BATTLE.PRG func_800BCA8C (F01223, src/BATTLE/BATTLE.PRG/4A0A8.c:3389,
// 0x800BCA8C-0x800BCFB4)
void UpdateEventCameraTrackEntry(CameraTrack* track, CameraTrack* /*other*/) {
  constexpr const char* kOperation = "UpdateEventCameraTrack";
  UpdateTrack(RequiredTrack(track, kOperation), VS::State::tickSpeed,
              kOperation);
}

// decomp: BATTLE.PRG func_800BCFB4 (F01224, src/BATTLE/BATTLE.PRG/4A0A8.c:3502,
// 0x800BCFB4-0x800BD2B8)
void AdvanceTerrainCameraTrackEntry(CameraTrack* track) {
  constexpr const char* kOperation = "AdvanceTerrainCameraTrack";
  CameraTrack& record = RequiredTrack(track, kOperation);
  CameraState& camera = RequiredCamera(kOperation);
  LiveEventCameraTrackHost host;
  EventCameraTrack state = LoadTrack(record);
  Vec3 last_terrain_position = LoadVector(camera.last_terrain_position);
  AdvanceTerrainCameraTrack(state, last_terrain_position, host);
  StoreTrack(record, state);
  StoreVector(camera.last_terrain_position, last_terrain_position);
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
void ScaleEventCameraTrackDurationEntry(CameraTrack* track) {
  CameraTrack& record = RequiredTrack(track, "ScaleEventCameraTrackDuration");
  EventCameraTrack state = LoadTrack(record);
  ScaleEventCameraTrackDuration(state);
  record.duration_frames = state.duration_frames;
}

// decomp: BATTLE.PRG func_800BE5A4 (F01240, src/BATTLE/BATTLE.PRG/4A0A8.c:4082,
// 0x800BE5A4-0x800BE628)
void ScaleEventCameraDurationEntry(std::int16_t selector) {
  CameraState& camera = RequiredCamera("ScaleEventCameraDuration");
  EventCameraTrack look_at = LoadTrack(camera.look_at);
  EventCameraTrack eye = LoadTrack(camera.eye);
  ScaleEventCameraDuration(look_at, eye, selector);
  camera.look_at.duration_frames = look_at.duration_frames;
  camera.eye.duration_frames = eye.duration_frames;
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
std::intptr_t VS::Event::CommandCameraMotionOpcode(std::uint8_t* instruction,
                                                   std::int16_t thread_index) {
  return VS::Event::CommandCameraMotionEntry(instruction, thread_index);
}
