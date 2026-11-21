// Hand-maintained (tools/port/owned.csv).
#include "Event/EventCameraTrack.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "Core/Math/Distance.hpp"
#include "Core/Math/Matrix.hpp"
#include "Core/Math/ScalarMath.hpp"
#include "Core/Math/Vector.hpp"
#include "Event/EventCameraOwnership.hpp"
#include "Event/ScriptCommand.hpp"

namespace VS::Event {
namespace {
using Core::Math::AddVectors;
using Core::Math::Angles3;
using Core::Math::FixedVectorMagnitude;
using Core::Math::MultiplyFixed;
using Core::Math::NormalizeFixedVector;
using Core::Math::Position3;
using Core::Math::ScaleFixedVector;
using Core::Math::ShortVectorToFixed;
using Core::Math::SubtractVectors;
using Core::Math::Transform3;
using Core::Math::Vec3;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingSubtract;

constexpr unsigned kFractionBits = 12;
constexpr std::int32_t kTurn = 4096;
constexpr std::int32_t kHalfTurn = kTurn / 2;
constexpr std::uint32_t kAngleMask = kTurn - 1;
constexpr std::uint8_t kOperationMask = 0xF;
constexpr unsigned kNibbleBits = 4;

// The motion opcodes' operations; 6 and 12-15 have no case.
enum class Operation : std::uint8_t {
  kSetDestination = 0,
  kJump = 1,
  kLinear = 2,
  kPartnerRadius = 3,
  kOrbit = 4,
  kPartnerOrbit = 5,
  kBoneDestination = 7,
  kFollowBone = 8,
  kPartnerOffset = 9,
  kFacingBoneDestination = 10,
  kTerrainGlide = 11,
};

// The operation without a case, between the partner orbit and the bone.
constexpr std::uint8_t kUnassignedOperation = 6;

// The script's opcode sizes, by operation.
constexpr std::array<std::uint8_t, 16> kCommandLengths{
    7, 1, 3, 3, 4, 9, 1, 10, 11, 2, 10, 3, 1, 1, 1, 1,
};

// Operand offsets.
constexpr std::size_t kVectorOperand = 1;
constexpr std::size_t kEasingOperand = 1;
constexpr std::size_t kDurationOperand = 2;
constexpr std::size_t kOrbitEasingOperand = 2;
constexpr std::size_t kOrbitDurationOperand = 3;
constexpr std::size_t kPartnerOrbitTurnOperand = 1;
constexpr std::size_t kPartnerOrbitHeightOperand = 3;
constexpr std::size_t kPartnerOrbitRadiusOperand = 5;
constexpr std::size_t kPartnerOrbitEasingOperand = 6;
constexpr std::size_t kPartnerOrbitHeightEasingOperand = 7;
constexpr std::size_t kPartnerOrbitDurationOperand = 8;
constexpr std::size_t kActorOperand = 1;
constexpr std::size_t kBoneTagOperand = 3;
constexpr std::size_t kBoneOffsetOperand = 4;
constexpr std::size_t kFollowBoneDurationOperand = 10;
constexpr std::size_t kPartnerOffsetDurationOperand = 1;
// The partner orbit's radius scale is 2.6 fixed point.
constexpr unsigned kRadiusScaleBits = 6;

// The ground clearance probes, in the probe frame, and the clearance kept.
constexpr std::array<Vec3, 4> kGroundProbes{{
    {.x = 80, .y = 80, .z = 150},
    {.x = -80, .y = 80, .z = 150},
    {.x = 80, .y = -80, .z = 150},
    {.x = -80, .y = -80, .z = 150},
}};
constexpr std::int32_t kGroundClearance = 200;
constexpr std::uint32_t kFloorHeightMask = 0x7FFF;
constexpr std::int32_t kFloorHeightSign = 0x4000;

// The terrain glide looks at most this many steps ahead; fewer than two
// steps left ends it.
constexpr std::int32_t kMaxTerrainSteps = 10;
constexpr std::int32_t kMinTerrainSteps = 2;

// The follow blend's easing curve.
constexpr std::uint8_t kFollowEasing = 1;
constexpr std::int32_t kDurationDistanceDivisor = 1000;

[[nodiscard]] std::uint8_t LowNibble(std::uint8_t value) {
  return static_cast<std::uint8_t>(value & kOperationMask);
}

[[nodiscard]] std::uint8_t HighNibble(std::uint8_t value) {
  return static_cast<std::uint8_t>(value >> kNibbleBits);
}

[[nodiscard]] std::uint16_t ActorReference(ScriptCommand command) {
  return static_cast<std::uint16_t>(OperandHalf(command, kActorOperand));
}

[[nodiscard]] Vec3 FixedVectorAt(ScriptCommand command, std::size_t offset) {
  return ReadScriptFixedVector(
      command.subspan(offset).first<kScriptFixedVectorBytes>());
}

// The whole units of a Q12 position: a transform's translation.
[[nodiscard]] Vec3 WholeUnits(Vec3 position) {
  return {.x = position.x >> kFractionBits,
          .y = position.y >> kFractionBits,
          .z = position.z >> kFractionBits};
}

// The frame the ground clearance probes: facing `partner` from `position`.
[[nodiscard]] Transform3 ProbeFrame(Vec3 position, Vec3 partner) {
  return {.rotation = Core::Math::DirectionRotationMatrix(
              SubtractVectors(partner, position)),
          .translation = WholeUnits(position)};
}

// The horizontal offset from `center` to `point`.
[[nodiscard]] Vec3 HorizontalOffset(Vec3 point, Vec3 center) {
  return {.x = WrappingSubtract(point.x, center.x),
          .y = 0,
          .z = WrappingSubtract(point.z, center.z)};
}

// A Q12 yaw rotation applied to a full-word vector.
[[nodiscard]] Vec3 RotateYaw(std::int32_t yaw, Vec3 vector) {
  return Core::Math::ApplyMatrixLong(
      Core::Math::BuildRotationXyz(Angles3{.x = 0, .y = yaw, .z = 0}), vector);
}

[[nodiscard]] Vec3 SubtractOrigin(const EventCameraTracks& tracks,
                                  Vec3 position) {
  if (tracks.origin_actor == kEventCameraWorldOrigin) return position;
  return SubtractVectors(position, tracks.origin_offset);
}

// The low 15 bits of a floor height, sign-extended, less the clearance.
[[nodiscard]] std::int32_t FloorClearance(std::int16_t floor_height) {
  const auto low = static_cast<std::int32_t>(
      static_cast<std::uint16_t>(floor_height) & kFloorHeightMask);
  return (low ^ kFloorHeightSign) - kFloorHeightSign - kGroundClearance;
}

// The orbit turn from the current angle to the destination's, the shorter
// way unless the command names a direction; a zero turn becomes a full turn
// in that direction.
[[nodiscard]] std::int16_t OrbitTurn(std::int32_t angle_to_destination,
                                     std::int16_t current_angle,
                                     std::uint8_t direction) {
  std::int32_t turn = angle_to_destination - current_angle;
  if (turn < -kHalfTurn) turn += kTurn;
  if (turn > kHalfTurn) turn -= kTurn;
  if (direction == kCameraTurnNegative) {
    if (turn == 0) {
      turn = -kTurn;
    } else if (turn > 0) {
      turn -= kTurn;
    }
  } else if (direction == kCameraTurnPositive) {
    if (turn == 0) {
      turn = kTurn;
    } else if (turn < 0) {
      turn += kTurn;
    }
  }
  return static_cast<std::int16_t>(turn);
}

[[nodiscard]] std::int16_t MaskedAngle(std::int32_t angle) {
  return static_cast<std::int16_t>(static_cast<std::uint32_t>(angle) &
                                   kAngleMask);
}

void StartLinearMotion(EventCameraTrack& track, std::uint8_t mode,
                       ScriptCommand command) {
  track.motion_mode = mode;
  track.elapsed_frames = 0;
  track.primary_easing = OperandByte(command, kEasingOperand);
  track.duration_frames = OperandByte(command, kDurationOperand);
  track.start_position = track.current_position;
  track.displacement =
      SubtractVectors(track.destination, track.current_position);
}

void StartOrbit(EventCameraTrack& track, ScriptCommand command) {
  const std::uint8_t direction = OperandByte(command, kEasingOperand);
  const std::uint8_t easing = OperandByte(command, kOrbitEasingOperand);
  track.motion_mode = kCameraMotionOrbit;
  track.elapsed_frames = 0;
  track.duration_frames = OperandByte(command, kOrbitDurationOperand);
  track.primary_easing = HighNibble(direction);
  track.secondary_easing = LowNibble(easing);
  track.tertiary_easing = HighNibble(easing);
  track.start_position = track.partner_position;
  const Vec3 from_partner =
      HorizontalOffset(track.current_position, track.partner_position);
  track.motion_scalar = FixedVectorMagnitude(from_partner);
  track.orbit_angle =
      MaskedAngle(Core::Math::FixedAtan2(from_partner.x, from_partner.z));
  const Vec3 to_destination =
      HorizontalOffset(track.destination, track.partner_position);
  track.motion_scalar_delta = WrappingSubtract(
      FixedVectorMagnitude(to_destination), track.motion_scalar);
  const std::int32_t destination_angle =
      MaskedAngle(Core::Math::FixedAtan2(to_destination.x, to_destination.z));
  track.orbit_angle_delta =
      OrbitTurn(destination_angle, track.orbit_angle, LowNibble(direction));
  track.height_offset.y =
      WrappingSubtract(track.current_position.y, track.partner_position.y);
  track.displacement.y = WrappingSubtract(
      WrappingSubtract(track.destination.y, track.partner_position.y),
      track.height_offset.y);
}

void StartPartnerOrbit(EventCameraTrack& track, ScriptCommand command) {
  const std::uint8_t easing = OperandByte(command, kPartnerOrbitEasingOperand);
  track.motion_mode = kCameraMotionPartnerOrbit;
  track.elapsed_frames = 0;
  track.duration_frames = OperandByte(command, kPartnerOrbitDurationOperand);
  track.primary_easing = LowNibble(easing);
  track.secondary_easing = HighNibble(easing);
  track.tertiary_easing =
      LowNibble(OperandByte(command, kPartnerOrbitHeightEasingOperand));
  track.destination = track.partner_position;
  const Vec3 from_partner =
      HorizontalOffset(track.current_position, track.partner_position);
  track.motion_scalar = FixedVectorMagnitude(from_partner);
  const std::int32_t scaled_radius =
      WrappingMultiply(track.motion_scalar,
                       OperandByte(command, kPartnerOrbitRadiusOperand)) >>
      kRadiusScaleBits;
  track.motion_scalar_delta =
      WrappingSubtract(scaled_radius, track.motion_scalar);
  track.orbit_angle = static_cast<std::int16_t>(
      Core::Math::FixedAtan2(from_partner.x, from_partner.z));
  track.orbit_angle_delta = OperandHalf(command, kPartnerOrbitTurnOperand);
  const std::int32_t height = WrappingMultiply(
      OperandHalf(command, kPartnerOrbitHeightOperand), 1 << kFractionBits);
  track.start_position.y = track.current_position.y;
  track.displacement.y = WrappingSubtract(height, track.start_position.y);
}

void SetBoneDestination(EventCameraTracks& tracks, EventCameraTrack& track,
                        ScriptCommand command, EventCameraTrackHost& host) {
  const int actor = host.ResolveActorReference(ActorReference(command));
  const std::optional<Position3> bone =
      host.TaggedBonePosition(actor, OperandByte(command, kBoneTagOperand));
  const Vec3 bone_position = ShortVectorToFixed(bone.value_or(Position3{}));
  track.destination = SubtractOrigin(
      tracks,
      AddVectors(bone_position, FixedVectorAt(command, kBoneOffsetOperand)));
}

void StartFollowBone(EventCameraTrack& track, ScriptCommand command,
                     EventCameraTrackHost& host) {
  track.motion_mode = kCameraMotionBone;
  track.motion_scalar = host.ResolveActorReference(ActorReference(command));
  track.motion_scalar_delta = OperandByte(command, kBoneTagOperand);
  track.elapsed_frames = 0;
  track.duration_frames = OperandByte(command, kFollowBoneDurationOperand);
  track.displacement = FixedVectorAt(command, kBoneOffsetOperand);
}

void SetFacingBoneDestination(EventCameraTracks& tracks,
                              EventCameraTrack& track, ScriptCommand command,
                              EventCameraTrackHost& host) {
  const int actor = host.ResolveActorReference(ActorReference(command));
  const std::int16_t facing = host.ActorFacing(actor).value_or(0);
  const Vec3 offset =
      RotateYaw(facing, FixedVectorAt(command, kBoneOffsetOperand));
  // The original's bone output was the rotation's angles: a failed query
  // left (0, facing, 0) there.
  const std::optional<Position3> bone =
      host.TaggedBonePosition(actor, OperandByte(command, kBoneTagOperand));
  const Position3 bone_point =
      bone.value_or(Position3{.x = 0, .y = facing, .z = 0});
  track.destination = SubtractOrigin(
      tracks, AddVectors(ShortVectorToFixed(bone_point), offset));
}

void StartTerrainGlide(EventCameraTracks& tracks, EventCameraTrack& track,
                       ScriptCommand command, EventCameraFloorHost& host) {
  const std::uint8_t duration = OperandByte(command, kDurationOperand);
  track.motion_mode = kCameraMotionTerrainGlide;
  track.elapsed_frames = 0;
  track.primary_easing = 0;
  track.duration_frames = duration;
  track.current_position.y = EventCameraGroundClearance(
      ProbeFrame(track.current_position, track.partner_position), host);
  track.computed_position = track.current_position;
  track.motion_scalar_delta = FixedVectorMagnitude(
      SubtractVectors(track.destination, track.computed_position));
  track.motion_scalar =
      Core::Math::R3000Quotient(track.motion_scalar_delta, duration);
  tracks.last_terrain_position = {.x = kTerrainPositionUnset,
                                  .y = kTerrainPositionUnset,
                                  .z = kTerrainPositionUnset};
}

// The yaw of an orbit `frame` into its motion, as the halfword the rotation
// reads.
[[nodiscard]] std::int16_t OrbitYaw(const EventCameraTrack& track,
                                    EventCameraTrackHost& host) {
  const std::int32_t progress = host.Ease(
      track.primary_easing, track.elapsed_frames, track.duration_frames);
  return Core::Math::WrapSigned16(static_cast<std::uint32_t>(WrappingAdd(
      track.orbit_angle,
      WrappingMultiply(track.orbit_angle_delta, progress) >> kFractionBits)));
}

// The orbit radius vector, rotated by the yaw.
[[nodiscard]] Vec3 OrbitOffset(const EventCameraTrack& track,
                               EventCameraTrackHost& host) {
  const std::int16_t yaw = OrbitYaw(track, host);
  const std::int32_t radius = WrappingAdd(
      track.motion_scalar,
      MultiplyFixed(track.motion_scalar_delta,
                    host.Ease(track.secondary_easing, track.elapsed_frames,
                              track.duration_frames)));
  return RotateYaw(yaw, Vec3{.x = 0, .y = 0, .z = radius});
}

[[nodiscard]] std::int32_t OrbitHeight(const EventCameraTrack& track,
                                       EventCameraTrackHost& host) {
  return MultiplyFixed(track.displacement.y,
                       host.Ease(track.tertiary_easing, track.elapsed_frames,
                                 track.duration_frames));
}

[[nodiscard]] Vec3 PartnerRadiusPosition(const EventCameraTrack& track,
                                         EventCameraTrackHost& host) {
  const std::int32_t progress = host.Ease(
      track.primary_easing, track.elapsed_frames, track.duration_frames);
  const Vec3 along = AddVectors(track.start_position,
                                ScaleFixedVector(track.displacement, progress));
  const Vec3 direction =
      NormalizeFixedVector(SubtractVectors(along, track.partner_position));
  const std::int32_t final_radius = FixedVectorMagnitude(
      SubtractVectors(track.partner_position, track.destination));
  const std::int32_t radius = WrappingAdd(
      track.motion_scalar,
      MultiplyFixed(WrappingSubtract(final_radius, track.motion_scalar),
                    host.Ease(track.primary_easing, track.elapsed_frames,
                              track.duration_frames)));
  return AddVectors(track.partner_position,
                    ScaleFixedVector(direction, radius));
}

void ComputeMotion(EventCameraTrack& track, Vec3& last_terrain_position,
                   EventCameraTrackHost& host) {
  switch (track.motion_mode) {
    case kCameraMotionLinear: {
      const std::int32_t progress = host.Ease(
          track.primary_easing, track.elapsed_frames, track.duration_frames);
      track.computed_position = AddVectors(
          track.start_position, ScaleFixedVector(track.displacement, progress));
      break;
    }
    case kCameraMotionPartnerRadius:
      track.computed_position = PartnerRadiusPosition(track, host);
      break;
    case kCameraMotionOrbit: {
      Vec3 offset = OrbitOffset(track, host);
      offset.y = WrappingAdd(track.height_offset.y, OrbitHeight(track, host));
      track.computed_position = AddVectors(track.start_position, offset);
      break;
    }
    case kCameraMotionPartnerOrbit: {
      const Vec3 offset = OrbitOffset(track, host);
      track.computed_position.x = WrappingAdd(track.destination.x, offset.x);
      track.computed_position.z = WrappingAdd(track.destination.z, offset.z);
      track.computed_position.y =
          WrappingAdd(track.start_position.y, OrbitHeight(track, host));
      break;
    }
    case kCameraMotionBone: {
      // The original read a stale scratch vector after a failed query.
      const std::optional<Position3> bone = host.TaggedBonePosition(
          track.motion_scalar, track.motion_scalar_delta);
      track.computed_position = AddVectors(
          ShortVectorToFixed(bone.value_or(Position3{})), track.displacement);
      break;
    }
    case kCameraMotionPartnerOffset:
      track.computed_position =
          AddVectors(track.displacement, track.partner_position);
      break;
    case kCameraMotionTerrainGlide:
      AdvanceTerrainCameraTrack(track, last_terrain_position, host);
      break;
    default:
      break;
  }
}

// The follow blend and the end of the motion.
void FinishFrame(EventCameraTrack& track, EventCameraTrackHost& host) {
  track.previous_position = track.current_position;
  if ((track.follow_request & track.following) != 0) {
    track.follow_position =
        AddVectors(track.follow_position, track.follow_delta);
    const std::int32_t blend =
        host.Ease(kFollowEasing, track.elapsed_frames, track.duration_frames);
    track.current_position = AddVectors(
        track.follow_position,
        ScaleFixedVector(
            SubtractVectors(track.computed_position, track.follow_position),
            blend));
  } else {
    track.current_position = track.computed_position;
  }
  if (track.elapsed_frames != track.duration_frames) return;
  track.motion_mode = kEventCameraTrackIdle;
  if (track.follow_request == 0) return;
  track.following = 1;
  track.follow_position = track.current_position;
  track.follow_delta =
      SubtractVectors(track.current_position, track.previous_position);
}
}  // namespace

std::size_t CameraMotionCommandLength(std::uint8_t opcode) {
  return kCommandLengths.at(LowNibble(opcode));
}

// decomp: BATTLE.PRG setVectorImm (F01246, src/BATTLE/BATTLE.PRG/4DEA8.c:4797;
// was _setVectorImm, 0x800BE7B8-0x800BE824)
Core::Math::Vec3 ReadScriptFixedVector(
    std::span<const std::uint8_t, kScriptFixedVectorBytes> operands) {
  constexpr std::int32_t kUnit = 1 << kFractionBits;
  constexpr std::size_t kHalfBytes = sizeof(std::int16_t);
  const ScriptCommand bytes{operands};
  return {.x = OperandHalf(bytes, 0) * kUnit,
          .y = OperandHalf(bytes, kHalfBytes) * kUnit,
          .z = OperandHalf(bytes, 2 * kHalfBytes) * kUnit};
}

// decomp: BATTLE.PRG func_800BD2B8 (F01225, src/BATTLE/BATTLE.PRG/4DEA8.c:4206,
// 0x800BD2B8-0x800BD444)
std::int32_t EventCameraGroundClearance(const Core::Math::Transform3& probe,
                                        EventCameraFloorHost& host) {
  std::int32_t lowest = 0;
  bool first = true;
  for (const Vec3& offset : kGroundProbes) {
    const Vec3 point = Core::Math::RotateTranslate(probe, offset);
    const std::int32_t clearance =
        FloorClearance(host.FloorHeight(point.x, point.z));
    lowest = first ? clearance : std::min(lowest, clearance);
    first = false;
  }
  return lowest * (1 << kFractionBits);
}

// decomp: BATTLE.PRG func_800BCFB4 (F01224, src/BATTLE/BATTLE.PRG/4DEA8.c:4152,
// 0x800BCFB4-0x800BD2B8)
void AdvanceTerrainCameraTrack(EventCameraTrack& track,
                               Core::Math::Vec3& last_terrain_position,
                               EventCameraFloorHost& host) {
  const Vec3 origin = track.computed_position;
  const std::int32_t destination_floor = EventCameraGroundClearance(
      ProbeFrame(track.destination, track.partner_position), host);
  const Vec3 remaining{.x = WrappingSubtract(track.destination.x, origin.x),
                       .y = WrappingSubtract(destination_floor, origin.y),
                       .z = WrappingSubtract(track.destination.z, origin.z)};
  std::int32_t steps = Core::Math::R3000Quotient(
      FixedVectorMagnitude(remaining), track.motion_scalar);
  if (steps < kMinTerrainSteps) {
    track.elapsed_frames = track.duration_frames;
  } else if (steps > kMaxTerrainSteps) {
    steps = kMaxTerrainSteps;
  }
  Vec3 step =
      ScaleFixedVector(NormalizeFixedVector(remaining), track.motion_scalar);
  Vec3 candidate = origin;
  for (std::int32_t index = 1; index <= steps; ++index) {
    candidate = AddVectors(candidate, step);
    const std::int32_t floor = EventCameraGroundClearance(
        ProbeFrame(candidate, track.partner_position), host);
    if (floor < candidate.y) {
      const Vec3 climb{.x = WrappingSubtract(candidate.x, origin.x),
                       .y = WrappingSubtract(floor, origin.y),
                       .z = WrappingSubtract(candidate.z, origin.z)};
      step = ScaleFixedVector(NormalizeFixedVector(climb), track.motion_scalar);
      break;
    }
  }
  const Vec3 next = AddVectors(step, origin);
  if (next == last_terrain_position) {
    track.elapsed_frames = track.duration_frames;
    return;
  }
  last_terrain_position = origin;
  track.computed_position = next;
}

// decomp: BATTLE.PRG func_800BCA8C (F01223, src/BATTLE/BATTLE.PRG/4DEA8.c:4039,
// 0x800BCA8C-0x800BCFB4)
void UpdateEventCameraTrack(EventCameraTrack& track,
                            Core::Math::Vec3& last_terrain_position,
                            std::int32_t tick_speed,
                            EventCameraTrackHost& host) {
  if (track.motion_mode == kEventCameraTrackIdle) return;
  if (track.motion_mode != kCameraMotionTerrainGlide) {
    track.elapsed_frames = static_cast<std::uint8_t>(
        WrappingAdd(track.elapsed_frames, tick_speed >> 1));
    track.elapsed_frames =
        std::min(track.elapsed_frames, track.duration_frames);
  }
  ComputeMotion(track, last_terrain_position, host);
  FinishFrame(track, host);
}

// decomp: BATTLE.PRG vs_battle_script_Cx_Dx_camera (F01221,
// src/BATTLE/BATTLE.PRG/4DEA8.c:3850; was func_800BC2E8, 0x800BC2E8-0x800BC9E0)
CameraMotionOutcome CommandCameraMotion(ScriptCommand command,
                                        EventCameraTracks& tracks,
                                        EventCameraTrackHost& host) {
  if (command.empty() ||
      command.size() < CameraMotionCommandLength(command.front())) {
    return CameraMotionOutcome::kRefused;
  }
  const std::uint8_t opcode = command.front();
  const bool moves_eye = (opcode & kCameraMotionEyeTrackBit) != 0;
  EventCameraTrack& track = moves_eye ? tracks.eye : tracks.look_at;
  EventCameraTrack& paired_track = moves_eye ? tracks.look_at : tracks.eye;
  switch (static_cast<Operation>(LowNibble(opcode))) {
    case Operation::kSetDestination:
      track.destination = FixedVectorAt(command, kVectorOperand);
      break;
    case Operation::kJump:
      track.motion_mode = kEventCameraTrackIdle;
      track.current_position = track.destination;
      paired_track.partner_position = track.destination;
      break;
    case Operation::kLinear:
      StartLinearMotion(track, kCameraMotionLinear, command);
      break;
    case Operation::kPartnerRadius:
      StartLinearMotion(track, kCameraMotionPartnerRadius, command);
      track.motion_scalar = FixedVectorMagnitude(
          SubtractVectors(track.partner_position, track.current_position));
      break;
    case Operation::kOrbit:
      StartOrbit(track, command);
      break;
    case Operation::kPartnerOrbit:
      StartPartnerOrbit(track, command);
      break;
    case Operation::kBoneDestination:
      SetBoneDestination(tracks, track, command, host);
      break;
    case Operation::kFollowBone:
      StartFollowBone(track, command, host);
      break;
    case Operation::kPartnerOffset:
      track.motion_mode = kCameraMotionPartnerOffset;
      track.elapsed_frames = 0;
      track.duration_frames =
          OperandByte(command, kPartnerOffsetDurationOperand);
      track.displacement =
          SubtractVectors(track.current_position, track.partner_position);
      break;
    case Operation::kFacingBoneDestination:
      SetFacingBoneDestination(tracks, track, command, host);
      break;
    case Operation::kTerrainGlide:
      StartTerrainGlide(tracks, track, command, host);
      break;
    default:
      return LowNibble(opcode) == kUnassignedOperation
                 ? CameraMotionOutcome::kUnassigned
                 : CameraMotionOutcome::kIgnored;
  }
  return CameraMotionOutcome::kApplied;
}

// decomp: BATTLE.PRG func_800BE53C (F01239, src/BATTLE/BATTLE.PRG/4DEA8.c:4723,
// 0x800BE53C-0x800BE5A4)
void ScaleEventCameraTrackDuration(EventCameraTrack& track) {
  const std::int32_t distance =
      FixedVectorMagnitude(
          SubtractVectors(track.current_position, track.destination)) >>
      kFractionBits;
  track.duration_frames = static_cast<std::uint8_t>(Core::Math::R3000Quotient(
      WrappingMultiply(distance, track.duration_frames),
      kDurationDistanceDivisor));
}

// decomp: BATTLE.PRG func_800BE5A4 (F01240, src/BATTLE/BATTLE.PRG/4DEA8.c:4731,
// 0x800BE5A4-0x800BE628)
void ScaleEventCameraDuration(EventCameraTrack& look_at, EventCameraTrack& eye,
                              std::int16_t selector) {
  if (selector == kScaleBothDurations) {
    ScaleEventCameraTrackDuration(look_at);
    ScaleEventCameraTrackDuration(eye);
    const auto mean = static_cast<std::uint8_t>(
        (look_at.duration_frames + eye.duration_frames) >> 1);
    eye.duration_frames = mean;
    look_at.duration_frames = mean;
    return;
  }
  ScaleEventCameraTrackDuration(selector == kScaleLookAtDuration ? look_at
                                                                 : eye);
}
}  // namespace VS::Event
