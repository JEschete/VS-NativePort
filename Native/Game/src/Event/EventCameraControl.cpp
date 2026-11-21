// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraControl.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/EventCameraTrack.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
namespace {
using Core::Math::FixedVectorMagnitude;
using Core::Math::SubtractVectors;
using Core::Math::WrappingAdd;

// One whole unit in 20.12 fixed point.
constexpr std::int32_t kFixedOne = 1 << 12;
constexpr std::uint8_t kFacingMask = 7;

// Operand offsets.
constexpr std::size_t kFollowOperand = 1;
constexpr std::size_t kFacingOperand = 1;
constexpr std::size_t kDistanceOperand = 2;

// The follow operand's bits.
constexpr std::uint8_t kLookAtFollowBit = 1;
constexpr std::uint8_t kEyeFollowBit = 2;

// The restore opcode's facing operand that keeps the yaw, and the half turn
// it adds to the others.
constexpr std::uint8_t kKeepFacingOperand = 0xFF;
constexpr std::uint8_t kHalfTurnEighths = 4;

void RunMotion(ScriptCommand command, EventCameraTracks& tracks,
               EventCameraTrackHost& motion) {
  static_cast<void>(CommandCameraMotion(command, tracks, motion));
}

// The whole-unit height offset as a 20.12 fixed lift, or nullopt past the
// table.
std::optional<std::int32_t> OrbitReturnLift(std::uint8_t distance_index) {
  if (distance_index >= kOrbitReturnHeightOffsets.size()) return std::nullopt;
  return Core::Math::WrappingMultiply(
      kOrbitReturnHeightOffsets.at(distance_index), kFixedOne);
}
}  // namespace

// decomp: BATTLE.PRG func_800BE3A0 (F01237, src/BATTLE/BATTLE.PRG/4DEA8.c:4676,
// 0x800BE3A0-0x800BE3D0)
void StopEventCameraMotion(EventCameraTracks& tracks) {
  tracks.eye.motion_mode = kEventCameraTrackIdle;
  tracks.look_at.motion_mode = kEventCameraTrackIdle;
  tracks.eye.following = 0;
  tracks.eye.follow_request = 0;
  tracks.look_at.following = 0;
  tracks.look_at.follow_request = 0;
}

// decomp: BATTLE.PRG vs_battle_script_DE (F01193,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3410; was func_800BB33C, 0x800BB33C-0x800BB35C)
void CommandStopCameraMotion(EventCameraTracks& tracks) {
  StopEventCameraMotion(tracks);
}

// decomp: BATTLE.PRG func_800BE36C (F01236, src/BATTLE/BATTLE.PRG/4DEA8.c:4668,
// 0x800BE36C-0x800BE3A0)
void SetEventCameraFollowing(EventCameraTracks& tracks,
                             std::uint8_t look_at_follow,
                             std::uint8_t eye_follow) {
  tracks.look_at.follow_request = look_at_follow;
  tracks.look_at.following = 0;
  tracks.eye.follow_request = eye_follow;
  tracks.eye.following = 0;
}

// decomp: BATTLE.PRG vs_battle_script_DF (F01194,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3416; was func_800BB35C, 0x800BB35C-0x800BB38C)
bool CommandSetCameraFollowing(ScriptCommand command,
                               EventCameraTracks& tracks) {
  if (command.size() < kSetCameraFollowingLength) return false;
  const std::uint8_t follow = OperandByte(command, kFollowOperand);
  SetEventCameraFollowing(
      tracks, static_cast<std::uint8_t>(follow & kLookAtFollowBit),
      static_cast<std::uint8_t>((follow & kEyeFollowBit) != 0 ? 1 : 0));
  return true;
}

// decomp: BATTLE.PRG vs_battle_script_E9 (F01196,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3429; was func_800BB3BC, 0x800BB3BC-0x800BB41C)
std::optional<FieldCameraReturnRequest> CommandRestoreFieldCamera(
    ScriptCommand command) {
  if (command.size() < kRestoreFieldCameraLength) return std::nullopt;
  const std::uint8_t facing = OperandByte(command, kFacingOperand);
  const std::uint8_t distance = OperandByte(command, kDistanceOperand);
  FieldCameraReturnRequest request;
  if (facing != kKeepFacingOperand) {
    request.facing =
        static_cast<std::int16_t>((facing + kHalfTurnEighths) & kFacingMask);
  }
  if (distance != 0) request.distance_mode = distance;
  return request;
}

// decomp: BATTLE.PRG func_800BC1CC (F01220, src/BATTLE/BATTLE.PRG/4DEA8.c:3823,
// 0x800BC1CC-0x800BC2E8)
bool ConsumeFieldCameraOverride(std::uint8_t& skip_next_field_camera_restore) {
  if (skip_next_field_camera_restore == 0) return false;
  skip_next_field_camera_restore = 0;
  return true;
}

// decomp: BATTLE.PRG func_800BC1CC (F01220, src/BATTLE/BATTLE.PRG/4DEA8.c:3823,
// 0x800BC1CC-0x800BC2E8)
FieldCameraReturnGlide RestoreEventFieldCamera(
    EventCameraTracks& tracks, FieldCameraReturnRequest request,
    EventFieldCameraHost& field_camera, EventCameraTrackHost& motion) {
  const FieldCameraPlacement field =
      field_camera.PlaceFieldCamera(request.facing, request.distance_mode);
  tracks.eye.destination = field.eye;
  tracks.look_at.destination = field.look_at;
  const std::int32_t eye_distance = FixedVectorMagnitude(
      SubtractVectors(tracks.eye.current_position, tracks.eye.destination));
  const std::int32_t look_at_distance = FixedVectorMagnitude(SubtractVectors(
      tracks.look_at.current_position, tracks.look_at.destination));
  FieldCameraReturnGlide glide{.travel_frames = Core::Math::R3000Quotient(
                                   WrappingAdd(eye_distance, look_at_distance),
                                   kFieldReturnTravelPerFrame),
                               .started = false};
  glide.started = static_cast<std::int16_t>(glide.travel_frames) != 0;
  if (!glide.started) return glide;
  const FieldCameraReturnCommands commands = FieldCameraReturnCommandsFor(
      static_cast<std::uint8_t>(glide.travel_frames));
  RunMotion(commands.look_at, tracks, motion);
  RunMotion(commands.eye, tracks, motion);
  return glide;
}

// decomp: BATTLE.PRG func_800BE3D0 (F01238, src/BATTLE/BATTLE.PRG/4DEA8.c:4689,
// 0x800BE3D0-0x800BE53C)
bool StartEventCameraOrbitReturn(EventCameraTracks& tracks,
                                 SavedFieldCameraView view,
                                 std::uint8_t orbit_side,
                                 EventFieldCameraHost& field_camera,
                                 EventCameraTrackHost& motion) {
  const std::int32_t distance_mode = view.distance_index + 1;
  const std::int32_t start_facing =
      (view.facing - 1 + orbit_side * 2) & kFacingMask;
  FieldCameraPlacement start =
      field_camera.PlaceFieldCamera(start_facing, distance_mode);
  const std::optional<std::int32_t> lift = OrbitReturnLift(view.distance_index);
  const std::int32_t height = lift.value_or(0);
  start.eye.y = WrappingAdd(start.eye.y, height);
  start.look_at.y = WrappingAdd(start.look_at.y, height);
  tracks.look_at.current_position = start.look_at;
  tracks.look_at.partner_position = start.eye;
  tracks.eye.partner_position = start.look_at;
  tracks.eye.current_position = start.eye;
  tracks.look_at.motion_mode = kEventCameraTrackIdle;
  const FieldCameraPlacement destination =
      field_camera.PlaceFieldCamera(view.facing, distance_mode);
  tracks.eye.destination = destination.eye;
  tracks.look_at.destination = destination.look_at;
  RunMotion(kOrbitReturnEyeCommand, tracks, motion);
  RunMotion(kOrbitReturnLookAtCommand, tracks, motion);
  return lift.has_value();
}

}  // namespace VS::Event
