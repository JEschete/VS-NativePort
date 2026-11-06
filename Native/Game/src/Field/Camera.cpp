// Hand-maintained (tools/port/owned.csv).
#include "Field/Camera.hpp"  // NOLINT(build/include): typed API

#include <cstdint>
#include <optional>
#include <string_view>

#include "Core/Log.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Field {
namespace {
namespace Math = Core::Math;

constexpr std::string_view kChannel = "field.camera";

// ONE: a world unit in Q12 and a full turn in angle units.
constexpr std::int32_t kFixedOne = 4096;
constexpr std::int32_t kWholeUnitMask = ~(kFixedOne - 1);
constexpr std::int32_t kAngleMask = kFixedOne - 1;

// The look-at hovers 90 units above Ashley's feet; the first-person eye is
// 180 units up.
constexpr std::int32_t kLookAtHeight = 90;
constexpr std::int32_t kEyeHeight = 180;

// _clampPositionToZoneBounds: the zone bounds are in 128-unit cells, kept the
// orbit's radius (its distance / 2304 in Q12, times 360) inside them; the
// look-at height only follows a rise beyond 96 units.
constexpr std::int32_t kOrbitRadiusDivisor = 2304;
constexpr std::int32_t kZoneCellScale = kFixedOne * 128;
constexpr std::int32_t kZoneMarginPerRadius = 360;
constexpr std::int32_t kHeightHoldRange = kFixedOne * 96;

// _computeStepAndDisplacement: the screen rectangle the target may sit in
// without the camera chasing it, and the tracking speed's ramp.
constexpr std::int32_t kScreenRowStride = 0x10000;
constexpr std::int32_t kCentredLeft = 61;
constexpr std::uint32_t kCentredWidth = 199;
constexpr std::int32_t kCentredTop = 60;
constexpr std::int32_t kCentredBottom = 180;
constexpr std::int32_t kChaseDistance = 40;
constexpr std::int32_t kTrackingAcceleration = 8;
constexpr std::int32_t kMinChaseSpeed = 8;

// _snapLookAtToPosition: how far the look-at may lag Ashley per frame.
constexpr std::int32_t kDeadZoneStep = 0x12000;
constexpr std::int32_t kDeadZoneHeight = 0x60000;

// func_8006E158: quarter turns snap to eighths of a turn and ease in their
// last 0x78 angle units; the zoom eases over its last 0xA0 units.
constexpr std::int32_t kOrbitStepAngle = 0x200;
constexpr std::int32_t kOrbitStepMask = ~(kOrbitStepAngle - 1);
constexpr std::int32_t kOrbitHalfStep = kOrbitStepAngle / 2;
constexpr std::int32_t kOrbitSnapMask = 0xE00;
constexpr std::int32_t kTurnEaseWindow = 0x78;
constexpr std::int32_t kTurnAcceleration = 8;
constexpr std::int32_t kMaxTurnSpeed = 40;
constexpr std::int32_t kZoomEaseWindow = 0xA0;
constexpr std::int32_t kZoomAcceleration = 16;
constexpr std::int32_t kMaxZoomSpeed = 64;

// func_8006E640: the action camera's top tracking speeds.
constexpr std::int32_t kActionTrackingSpeed = 0x28;
constexpr std::int32_t kSphereTrackingSpeed = 8;
constexpr int kSphereDragMode = 1;

// func_8006EC7C: the first-person look speed, stick dead zone and pitch limit.
constexpr std::int32_t kLookStep = 32;
constexpr std::int32_t kStickCentre = 128;
constexpr std::int32_t kStickDeadZone = 64;
constexpr std::uint32_t kStickLiveRange = 128;
constexpr std::int32_t kStickDivisor = 3;
constexpr std::int32_t kMaxFirstPersonPitch = 896;

// The first-person transition: its clip planes, look distance and speed.
constexpr std::int32_t kFirstPersonNearClip = 4;
constexpr std::int32_t kFirstPersonProjectionDistance = 224;
constexpr std::int32_t kFirstPersonFarClip = 0x2000;
constexpr std::int32_t kFirstPersonLookDistance = 0x900;
constexpr std::int32_t kTransitionSpeed = 0x80;
constexpr int kIdlePoseActors = 16;
constexpr int kTransitionModeFirstPerson = 1;
constexpr int kTransitionModeNormal = 0;
// Ashley's model hides once the projection distance passes 272.
constexpr std::int32_t kHideAshleyProjection = 272;
constexpr std::uint32_t kActorModelShownFlag = 2;

// func_80074B14: a platform's layer in the actor's state flags, and the cell
// flag that marks a view trigger.
constexpr std::uint32_t kPlatformLayerMask = 0x70000;
constexpr int kPlatformLayerShift = 16;
constexpr std::uint8_t kCellOccludesView = 1;

// func_80069D78: the facing word keeps its low 15 bits.
constexpr std::uint32_t kFacingKeepMask = 0x7FFF;
constexpr int kFirstPersonFacingShift = 16;

// vs_battle_syncCameraAnglesFromPosition: the distance with no horizontal
// offset.
constexpr std::int32_t kVerticalCameraDistance = kFixedOne / 4;

// vs_battle_initialiseCameraFromSpherical.
constexpr std::int32_t kKeepFacing = -1;
constexpr std::int32_t kKeepZoom = -1;
constexpr std::int32_t kFacingOctants = 8;
constexpr std::int32_t kNearZoomPreset = 1;
constexpr std::int32_t kFarZoomPreset = 2;
constexpr std::int32_t kResetFarClip = 0x1000;

// func_8007CE74: the pitches a script may pick; anything else is the default.
constexpr std::int32_t kLowPitchPreset = -0x300;
constexpr std::int32_t kHighPitchPreset = -0x80;
constexpr std::int32_t kDefaultPitch = -0x180;

[[nodiscard]] std::int32_t ToFixed(std::int32_t units) {
  return Math::WrappingMultiply(units, kFixedOne);
}

[[nodiscard]] CameraVector Add(const CameraVector& left,
                               const CameraVector& right) {
  return {Math::WrappingAdd(left.x, right.x),
          Math::WrappingAdd(left.y, right.y),
          Math::WrappingAdd(left.z, right.z)};
}

[[nodiscard]] CameraVector Scale(const CameraVector& vector,
                                 std::int32_t factor) {
  return {Math::WrappingMultiply(vector.x, factor),
          Math::WrappingMultiply(vector.y, factor),
          Math::WrappingMultiply(vector.z, factor)};
}

// (to - from) / ONE per axis, truncated.
[[nodiscard]] CameraVector UnitsBetween(const CameraVector& from,
                                        const CameraVector& to) {
  return {Math::WrappingSubtract(to.x, from.x) / kFixedOne,
          Math::WrappingSubtract(to.y, from.y) / kFixedOne,
          Math::WrappingSubtract(to.z, from.z) / kFixedOne};
}

// tan(angle) in Q12; the R3000 quotient (logged) where the cosine is zero.
[[nodiscard]] std::int32_t FixedTangent(std::int32_t sine, std::int32_t angle) {
  const std::int32_t cosine = Math::Cosine(angle);
  if (cosine == 0) {
    VS_LOG_WARN(kChannel,
                "camera pitch {} has a zero cosine; its slope takes the R3000 "
                "quotient",
                angle);
  }
  return Math::R3000Quotient(Math::WrappingMultiply(sine, kFixedOne), cosine);
}

// The orbit direction (or its reverse) for a yaw and pitch, scaled by the
// distance.
[[nodiscard]] CameraVector OrbitOffset(const OrbitAngles& angles,
                                       bool toward_look_at) {
  const std::int32_t sign = toward_look_at ? -1 : 1;
  const CameraVector slope{
      sign * Math::Sine(angles.yaw),
      FixedTangent(sign * Math::Sine(angles.pitch), angles.pitch),
      sign * Math::Cosine(angles.yaw)};
  return Scale(Math::NormalizeVector(slope).direction, angles.distance);
}

[[nodiscard]] Actors::ActorPlacementView PlacementOrOrigin(CameraHost& host,
                                                           int actor_id) {
  const std::optional<Actors::ActorPlacementView> placement =
      host.ActorPlacement(actor_id);
  if (!placement) {
    VS_LOG_WARN(kChannel,
                "actor {} has no placement; the camera uses the origin",
                actor_id);
    return {};
  }
  return *placement;
}

// Ashley's look-at point: 90 units above her feet, in Q12.
[[nodiscard]] CameraVector LookAtAbove(const Actors::MotionVector& position) {
  return {ToFixed(position.x),
          Math::WrappingSubtract(ToFixed(position.y), ToFixed(kLookAtHeight)),
          ToFixed(position.z)};
}

[[nodiscard]] bool IsCameraInputLocked(CameraHost& host) {
  const std::uint32_t control_mode = host.ControlMode();
  if (control_mode == kControlModeLocked) {
    return true;
  }
  if (control_mode != kControlModeAction) {
    return false;
  }
  const std::uint32_t flow = host.ActionFlowMode();
  return flow == kFlowActionLocked || flow == kFlowFollowActor;
}

[[nodiscard]] bool MayTurnCamera(CameraHost& host) {
  return !IsCameraInputLocked(host) && !host.TargetPagesUseShoulders();
}

[[nodiscard]] std::uint32_t HeldShoulders(CameraHost& host) {
  return host.HeldButtons() & (kPadL1 | kPadR1);
}

[[nodiscard]] std::int32_t ClampZoneAxis(std::int32_t value,
                                         std::int16_t minimum,
                                         std::int16_t maximum,
                                         std::int32_t radius) {
  const std::int32_t margin =
      Math::WrappingMultiply(radius, kZoneMarginPerRadius);
  const std::int32_t low = Math::WrappingAdd(
      Math::WrappingMultiply(minimum, kZoneCellScale), margin);
  const std::int32_t high = Math::WrappingSubtract(
      Math::WrappingMultiply(maximum, kZoneCellScale), margin);
  if (high < low) {
    return Math::WrappingAdd(low, high) >> 1;
  }
  if (value < low) {
    return low;
  }
  return high < value ? high : value;
}

[[nodiscard]] std::int32_t DeadZoneAxis(std::int32_t look_at,
                                        std::int32_t target) {
  const std::int32_t offset = Math::WrappingSubtract(target, look_at);
  if (offset > kDeadZoneStep) {
    return Math::WrappingAdd(look_at, kDeadZoneStep);
  }
  if (offset < -kDeadZoneStep) {
    return Math::WrappingSubtract(look_at, kDeadZoneStep);
  }
  return target;
}

void TurnRight(CameraHost& host, OrbitMotion& motion, OrbitAngles& values) {
  values.yaw = Math::WrappingSubtract(values.yaw, motion.turn_speed);
  if (values.yaw <= Math::WrappingAdd(motion.target_yaw, kTurnEaseWindow)) {
    if (HeldShoulders(host) == kPadR1 && MayTurnCamera(host)) {
      motion.target_yaw =
          Math::WrappingSubtract(motion.target_yaw, kOrbitStepAngle) &
          kOrbitStepMask;
    }
    if (motion.turn_speed > kTurnAcceleration) {
      motion.turn_speed -= kTurnAcceleration;
    }
  } else if (motion.turn_speed < kMaxTurnSpeed) {
    motion.turn_speed += kTurnAcceleration;
    if (motion.turn_speed > kMaxTurnSpeed) {
      motion.turn_speed = kMaxTurnSpeed;
    }
  }
  if (values.yaw <= motion.target_yaw) {
    values.yaw &= kOrbitSnapMask;
    motion.turn = kNoTurn;
  }
}

void TurnLeft(CameraHost& host, OrbitMotion& motion, OrbitAngles& values) {
  values.yaw = Math::WrappingAdd(values.yaw, motion.turn_speed);
  if (values.yaw >=
      Math::WrappingSubtract(motion.target_yaw, kTurnEaseWindow)) {
    if (HeldShoulders(host) == kPadL1 && MayTurnCamera(host)) {
      motion.target_yaw =
          Math::WrappingAdd(motion.target_yaw, kOrbitStepAngle) &
          kOrbitStepMask;
    }
    if (motion.turn_speed > kTurnAcceleration) {
      motion.turn_speed -= kTurnAcceleration;
    }
  } else if (motion.turn_speed < kMaxTurnSpeed) {
    motion.turn_speed += kTurnAcceleration;
    if (motion.turn_speed > kMaxTurnSpeed) {
      motion.turn_speed = kMaxTurnSpeed;
    }
  }
  if (values.yaw >= motion.target_yaw) {
    values.yaw &= kOrbitSnapMask;
    motion.turn = kNoTurn;
  }
}

void ZoomIn(SphericalCamera& orbit) {
  if (orbit.values.distance <= kNearCameraDistance) {
    orbit.values.distance = kNearCameraDistance;
    orbit.motion.zoom_mode = kNearZoom;
    return;
  }
  if (orbit.values.distance <=
      Math::WrappingAdd(orbit.preferred_distance, kZoomEaseWindow)) {
    if (orbit.zoom_velocity < -kZoomAcceleration) {
      orbit.zoom_velocity += kZoomAcceleration;
    } else {
      orbit.zoom_velocity = -kZoomAcceleration;
    }
  } else if (orbit.zoom_velocity > -kMaxZoomSpeed) {
    orbit.zoom_velocity -= kZoomAcceleration;
    if (orbit.zoom_velocity < -kMaxZoomSpeed) {
      orbit.zoom_velocity = -kMaxZoomSpeed;
    }
  }
  if (orbit.values.distance > orbit.preferred_distance) {
    orbit.values.distance =
        Math::WrappingAdd(orbit.values.distance, orbit.zoom_velocity);
  }
}

void ZoomOut(SphericalCamera& orbit) {
  if (orbit.values.distance >= kFarCameraDistance) {
    orbit.values.distance = kFarCameraDistance;
    orbit.motion.zoom_mode = kFarZoom;
    return;
  }
  if (orbit.values.distance >=
      Math::WrappingSubtract(orbit.preferred_distance, kZoomEaseWindow)) {
    if (orbit.zoom_velocity > kZoomAcceleration) {
      orbit.zoom_velocity -= kZoomAcceleration;
    } else {
      orbit.zoom_velocity = kZoomAcceleration;
    }
  } else if (orbit.zoom_velocity < kMaxZoomSpeed) {
    orbit.zoom_velocity += kZoomAcceleration;
    if (orbit.zoom_velocity > kMaxZoomSpeed) {
      orbit.zoom_velocity = kMaxZoomSpeed;
    }
  }
  if (orbit.values.distance < orbit.preferred_distance) {
    orbit.values.distance =
        Math::WrappingAdd(orbit.values.distance, orbit.zoom_velocity);
  }
}

[[nodiscard]] bool IsStickDeflected(std::uint8_t position) {
  return static_cast<std::uint32_t>(position) -
             static_cast<std::uint32_t>(kStickDeadZone) >
         kStickLiveRange;
}

// The deflection past the dead zone, -64..-1 or 1..63.
[[nodiscard]] std::int32_t StickOffset(std::uint8_t position) {
  return position - kStickCentre <= 0
             ? position - kStickDeadZone
             : position - (kStickCentre + kStickDeadZone);
}

// The remainder of a turn, keeping the sign as C's % does.
[[nodiscard]] std::int32_t ReduceAngle(std::int32_t angle) {
  return angle % kFixedOne;
}

void RefuseMissingTransition(std::string_view routine) {
  VS_LOG_WARN(kChannel,
              "{} has no first-person camera transition; the camera is left "
              "as it is",
              routine);
}
}  // namespace

// decomp: BATTLE.PRG func_80069D78 (src/BATTLE/BATTLE.PRG/146C.c:776)
void UpdateCameraFacingState(FieldCameraState& state) {
  SyncCameraAnglesFromPosition(state, nullptr);
  state.view_facing_word =
      (state.view_facing_word & kFacingKeepMask) |
      (static_cast<std::uint32_t>(state.first_person_view_enabled)
       << kFirstPersonFacingShift);
}

// decomp: BATTLE.PRG _addMinDelta (src/BATTLE/BATTLE.PRG/146C.c:2389)
std::int32_t AddCameraStepWithFallback(std::int32_t base, std::int32_t step,
                                       std::int32_t remaining) {
  const bool take_step = remaining < 0 ? step >= remaining && step != 0
                                       : remaining >= step && step != 0;
  return Math::WrappingAdd(base, take_step ? step : remaining);
}

// decomp: BATTLE.PRG _add_min (src/BATTLE/BATTLE.PRG/146C.c:2409)
std::int32_t AddClampedCameraStep(std::int32_t base, std::int32_t step,
                                  std::int32_t remaining) {
  const bool overshoots = remaining < 0 ? step < remaining : remaining < step;
  return Math::WrappingAdd(base, overshoots ? remaining : step);
}

// decomp: BATTLE.PRG _clampPositionToZoneBounds (146C.c:2426)
CameraVector ClampCameraLookAtToZone(CameraHost& host,
                                     const FieldCameraState& state,
                                     const CameraVector& position) {
  const CameraBounds bounds = host.ZoneBounds();
  const std::int32_t radius =
      Math::WrappingMultiply(state.spherical.values.distance, kFixedOne) /
      kOrbitRadiusDivisor;
  CameraVector clamped{};
  clamped.x = ClampZoneAxis(position.x, bounds.min_x, bounds.max_x, radius) &
              kWholeUnitMask;
  clamped.z = ClampZoneAxis(position.z, bounds.min_z, bounds.max_z, radius) &
              kWholeUnitMask;
  const std::int32_t reference = state.camera.look_at.y;
  const std::int32_t rise = Math::WrappingSubtract(position.y, reference);
  clamped.y = rise > kHeightHoldRange || rise < -kHeightHoldRange ? position.y
                                                                  : reference;
  if (clamped.y > 0) {
    clamped.y = 0;
  }
  return clamped;
}

// decomp: BATTLE.PRG _computeStepAndDisplacement (146C.c:2481)
CameraTrackingStep ComputeCameraTrackingStep(CameraHost& host,
                                             FieldCameraState& state,
                                             const Actors::MotionVector& target,
                                             bool clamp_to_zone,
                                             std::int32_t max_speed) {
  CameraVector fixed{ToFixed(target.x), ToFixed(target.y), ToFixed(target.z)};
  if (clamp_to_zone) {
    fixed = ClampCameraLookAtToZone(host, state, fixed);
  }
  const Actors::MotionVector scaled{
      .x = Math::WrapSigned16(static_cast<std::uint32_t>(fixed.x / kFixedOne)),
      .y = Math::WrapSigned16(static_cast<std::uint32_t>(fixed.y / kFixedOne)),
      .z = Math::WrapSigned16(static_cast<std::uint32_t>(fixed.z / kFixedOne)),
      .w = 0};

  const std::int32_t screen = host.ProjectToScreen(scaled);
  CameraTrackingStep result;
  result.not_centered = true;
  if (!clamp_to_zone) {
    const std::int32_t screen_y = screen / kScreenRowStride;
    const std::int32_t screen_x = Math::WrappingSubtract(
        screen, Math::WrappingMultiply(screen_y, kScreenRowStride));
    result.not_centered =
        static_cast<std::uint32_t>(screen_x - kCentredLeft) >= kCentredWidth ||
        screen_y <= kCentredTop || screen_y >= kCentredBottom;
  }

  const CameraVector& look_at = state.camera.look_at;
  result.displacement = {Math::WrappingSubtract(ToFixed(scaled.x), look_at.x),
                         Math::WrappingSubtract(ToFixed(scaled.y), look_at.y),
                         Math::WrappingSubtract(ToFixed(scaled.z), look_at.z)};
  const CameraVector direction =
      Math::NormalizeVector({result.displacement.x / kFixedOne,
                             result.displacement.y / kFixedOne,
                             result.displacement.z / kFixedOne})
          .direction;

  std::int32_t& speed = state.spherical.tracking_speed;
  if (result.not_centered) {
    std::int32_t distance = 1;
    if (direction.x != 0) {
      distance = Math::R3000Quotient(result.displacement.x, direction.x);
    } else if (direction.z != 0) {
      distance = Math::R3000Quotient(result.displacement.z, direction.z);
    } else if (direction.y != 0) {
      distance = Math::R3000Quotient(result.displacement.y, direction.y);
    }
    if (distance > kChaseDistance) {
      speed = Math::WrappingAdd(speed, kTrackingAcceleration);
      if (max_speed < speed) {
        speed = max_speed;
      }
    } else {
      speed = Math::WrappingSubtract(speed, kTrackingAcceleration);
      if (speed < kMinChaseSpeed) {
        speed = kMinChaseSpeed;
      }
    }
  } else {
    speed = Math::WrappingSubtract(speed, kTrackingAcceleration);
    if (speed < 0) {
      speed = 0;
    }
  }
  result.step = Scale(direction, speed);
  return result;
}

// decomp: BATTLE.PRG _stepTowardTarget (src/BATTLE/BATTLE.PRG/146C.c:2569)
std::int32_t StepCameraLookAtTowardTarget(CameraHost& host,
                                          FieldCameraState& state,
                                          const Actors::MotionVector& target,
                                          bool clamp_to_zone,
                                          std::int32_t max_speed) {
  const CameraTrackingStep tracking =
      ComputeCameraTrackingStep(host, state, target, clamp_to_zone, max_speed);
  CameraVector& look_at = state.camera.look_at;
  const auto add =
      tracking.not_centered ? AddCameraStepWithFallback : AddClampedCameraStep;
  look_at.x = add(look_at.x, tracking.step.x, tracking.displacement.x);
  look_at.z = add(look_at.z, tracking.step.z, tracking.displacement.z);
  look_at.y = add(look_at.y, tracking.step.y, tracking.displacement.y);
  if (look_at.y > 0) {
    look_at.y = 0;
  }
  return state.spherical.tracking_speed;
}

// decomp: BATTLE.PRG _snapLookAtToPosition (146C.c:2597)
void TrackCameraLookAtWithinDeadZone(CameraHost& host, FieldCameraState& state,
                                     const CameraVector& target) {
  const CameraVector clamped = ClampCameraLookAtToZone(host, state, target);
  CameraVector& look_at = state.camera.look_at;
  const std::int32_t rise = Math::WrappingSubtract(clamped.y, look_at.y);
  look_at.x = DeadZoneAxis(look_at.x, clamped.x);
  look_at.z = DeadZoneAxis(look_at.z, clamped.z);
  if (rise < -kDeadZoneHeight) {
    look_at.y =
        Math::WrappingAdd(look_at.y, Math::WrappingAdd(kDeadZoneHeight, rise));
  } else if (rise > kDeadZoneHeight) {
    look_at.y = Math::WrappingSubtract(
        look_at.y, Math::WrappingSubtract(kDeadZoneHeight, rise));
  }
  if (look_at.y > 0) {
    look_at.y = 0;
  }
}

// decomp: BATTLE.PRG func_8006E158 (src/BATTLE/BATTLE.PRG/146C.c:2641)
void UpdateCameraOrbitInput(CameraHost& host, FieldCameraState& state) {
  SphericalCamera& orbit = state.spherical;
  if (orbit.motion.turn == kNoTurn) {
    if (MayTurnCamera(host)) {
      const std::uint32_t shoulders = HeldShoulders(host);
      if (shoulders == kPadR1) {
        orbit.motion.turn = kTurningRight;
        orbit.motion.target_yaw =
            Math::WrappingSubtract(orbit.values.yaw, kOrbitStepAngle) &
            kOrbitStepMask;
      } else if (shoulders == kPadL1) {
        orbit.motion.turn = kTurningLeft;
        orbit.motion.target_yaw =
            Math::WrappingAdd(orbit.values.yaw, kOrbitStepAngle) &
            kOrbitStepMask;
      }
    }
  } else if (orbit.motion.turn == kTurningRight) {
    TurnRight(host, orbit.motion, orbit.values);
  } else if (orbit.motion.turn == kTurningLeft) {
    TurnLeft(host, orbit.motion, orbit.values);
  }

  if ((host.PressedButtons() & kPadSelect) != 0) {
    const bool zoomed_near = orbit.motion.zoom_mode == kZoomingIn ||
                             orbit.motion.zoom_mode == kNearZoom;
    const bool zoomed_far = orbit.motion.zoom_mode == kZoomingOut ||
                            orbit.motion.zoom_mode == kFarZoom;
    if (!IsCameraInputLocked(host)) {
      if (zoomed_near) {
        orbit.motion.zoom_mode = kZoomingOut;
      } else if (zoomed_far) {
        orbit.motion.zoom_mode = kZoomingIn;
      }
    }
    if (orbit.motion.zoom_mode == kZoomingIn ||
        orbit.motion.zoom_mode == kNearZoom) {
      orbit.preferred_distance = kNearCameraDistance;
    } else if (orbit.motion.zoom_mode == kZoomingOut ||
               orbit.motion.zoom_mode == kFarZoom) {
      orbit.preferred_distance = kFarCameraDistance;
    }
  }

  if (orbit.motion.zoom_mode == kZoomingIn) {
    ZoomIn(orbit);
  } else if (orbit.motion.zoom_mode == kZoomingOut) {
    ZoomOut(orbit);
  }
}

// decomp: BATTLE.PRG func_8006E640 (src/BATTLE/BATTLE.PRG/146C.c:2794)
void TrackActionCameraTarget(CameraHost& host, FieldCameraState& state,
                             int actor_id) {
  const Actors::ActorPlacementView placement =
      PlacementOrOrigin(host, actor_id);
  Actors::MotionVector target{
      .x = placement.position.x,
      .y = Math::WrapSigned16(
          static_cast<std::uint32_t>(placement.position.y - kLookAtHeight)),
      .z = placement.position.z,
      .w = 0};
  UpdateCameraTileConstraint(host, state, actor_id, placement.tile);
  const std::uint32_t flow = host.ActionFlowMode();
  if (flow == kFlowChooseDirectTarget || flow == kFlowChooseAreaTarget) {
    UpdateCameraOrbitInput(host, state);
    const std::optional<ActionCameraTargets> targets = host.ActionTargets();
    if (!targets) {
      VS_LOG_WARN(kChannel,
                  "no battle action for the camera to frame; it holds still");
      return;
    }
    if (targets->preview) {
      target = *targets->preview;
    }
    StepCameraLookAtTowardTarget(host, state, target, false,
                                 kActionTrackingSpeed);
  } else if (flow == kFlowPlaceArea) {
    UpdateCameraOrbitInput(host, state);
    const std::optional<ActionCameraTargets> targets = host.ActionTargets();
    if (!targets) {
      VS_LOG_WARN(kChannel,
                  "no battle action for the camera to frame; it holds still");
      return;
    }
    target = targets->area_center;
    StepCameraLookAtTowardTarget(host, state, target, false,
                                 host.SphereInteractionMode() == kSphereDragMode
                                     ? kSphereTrackingSpeed
                                     : kActionTrackingSpeed);
  } else if (flow == kFlowFollowActor) {
    UpdateCameraOrbitInput(host, state);
    StepCameraLookAtTowardTarget(host, state, target, true,
                                 kActionTrackingSpeed);
  } else if (flow == kFlowActionLocked) {
    UpdateCameraOrbitInput(host, state);
  }
}

// decomp: BATTLE.PRG func_8006EBF8 (src/BATTLE/BATTLE.PRG/146C.c:2965)
void UpdateAshleyFollowCamera(CameraHost& host, FieldCameraState& state) {
  if (!host.IsInteractionPaused()) {
    host.UpdateRoomActorMovement();
  }
  const Actors::ActorPlacementView ashley = PlacementOrOrigin(host, 0);
  TrackCameraLookAtWithinDeadZone(host, state, LookAtAbove(ashley.position));
  SetCameraPositionFromAngles(state, state.spherical.values);
}

// decomp: BATTLE.PRG func_8006EC7C (src/BATTLE/BATTLE.PRG/146C.c:2984)
void UpdateFirstPersonCamera(CameraHost& host, FieldCameraState& state) {
  OrbitAngles& values = state.spherical.values;
  const std::uint32_t held = host.HeldButtons();
  if ((held & kPadUp) != 0) {
    values.pitch = ReduceAngle(Math::WrappingSubtract(values.pitch, kLookStep));
  }
  if ((held & kPadDown) != 0) {
    values.pitch = ReduceAngle(Math::WrappingAdd(values.pitch, kLookStep));
  }
  if ((held & kPadLeft) != 0) {
    values.yaw = ReduceAngle(Math::WrappingSubtract(values.yaw, kLookStep));
  }
  if ((held & kPadRight) != 0) {
    values.yaw = ReduceAngle(Math::WrappingAdd(values.yaw, kLookStep));
  }
  if (const std::optional<StickPosition> stick = host.AnalogRightStick()) {
    if (IsStickDeflected(stick->x)) {
      values.yaw = ReduceAngle(
          Math::WrappingAdd(values.yaw, StickOffset(stick->x) / kStickDivisor));
    }
    if (IsStickDeflected(stick->y)) {
      values.pitch = ReduceAngle(Math::WrappingAdd(
          values.pitch, StickOffset(stick->y) / kStickDivisor));
    }
  }
  if (values.pitch > kMaxFirstPersonPitch) {
    values.pitch = kMaxFirstPersonPitch;
  } else if (values.pitch < -kMaxFirstPersonPitch) {
    values.pitch = -kMaxFirstPersonPitch;
  }

  host.TurnActorTo(0, values.yaw & kAngleMask);
  const Actors::ActorPlacementView ashley = PlacementOrOrigin(host, 0);
  state.camera.position = {
      ToFixed(ashley.position.x),
      ToFixed(Math::WrappingSubtract(ashley.position.y, kEyeHeight)),
      ToFixed(ashley.position.z)};
  SetCameraLookAtFromAngles(state, state.spherical.values);
}

// decomp: BATTLE.PRG func_8006EF10 (src/BATTLE/BATTLE.PRG/146C.c:3044)
void StorePreferredActionCameraDistance(CameraHost& host,
                                        const FieldCameraState& state) {
  const std::int32_t mode = state.spherical.motion.zoom_mode;
  if (mode == kZoomingIn || mode == kNearZoom) {
    host.SetPreferredActionCameraDistance(kNearCameraDistance);
  } else if (mode == kZoomingOut || mode == kFarZoom) {
    host.SetPreferredActionCameraDistance(kFarCameraDistance);
  }
}

// decomp: BATTLE.PRG _computeCameraTransition (146C.c:3058)
void ComputeFirstPersonCameraTransition(CameraHost& host,
                                        FieldCameraState& state,
                                        const CameraVector& target) {
  if (!state.transition) {
    RefuseMissingTransition("ComputeFirstPersonCameraTransition");
    return;
  }
  const CameraVector destination = target;
  const ViewCamera& camera = state.camera;
  FirstPersonTransition& transition = *state.transition;
  transition.direction =
      Math::NormalizeVector(UnitsBetween(camera.position, destination))
          .direction;
  transition.speed = kTransitionSpeed;
  transition.target_position = destination;
  const std::int32_t descent_per_frame =
      Math::WrappingMultiply(transition.direction.y, transition.speed);
  if (descent_per_frame == 0) {
    VS_LOG_WARN(kChannel,
                "first-person camera transition has no vertical travel; its "
                "frame count takes the R3000 quotient");
  }
  transition.frames = Math::R3000Quotient(
      Math::WrappingSubtract(destination.y, camera.position.y),
      descent_per_frame);

  transition.target_look_at =
      Add(OrbitOffset({.yaw = transition.to_spherical.values.yaw,
                       .pitch = transition.to_spherical.values.pitch,
                       .distance = kFirstPersonLookDistance,
                       .mode = 0},
                      true),
          transition.target_position);

  const std::int32_t frames = transition.frames;
  if (frames == 0) {
    VS_LOG_WARN(kChannel,
                "first-person camera transition takes no frames; its steps "
                "take the R3000 quotient");
  }
  transition.look_at_step = {
      Math::R3000Quotient(
          Math::WrappingSubtract(transition.target_look_at.x, camera.look_at.x),
          frames),
      Math::R3000Quotient(
          Math::WrappingSubtract(transition.target_look_at.y, camera.look_at.y),
          frames),
      Math::R3000Quotient(
          Math::WrappingSubtract(transition.target_look_at.z, camera.look_at.z),
          frames)};
  transition.far_clip_step = Math::R3000Quotient(
      Math::WrappingSubtract(transition.to_far_clip, camera.far_clip), frames);
  transition.near_clip_step = Math::R3000Quotient(
      Math::WrappingSubtract(transition.to_near_clip, host.NearClip()), frames);
  transition.projection_distance_step = Math::R3000Quotient(
      Math::WrappingSubtract(transition.to_projection_distance,
                             host.ProjectionDistance()),
      frames);
}

// decomp: BATTLE.PRG _stepCameraTransition (146C.c:3121)
bool StepFirstPersonCameraTransition(CameraHost& host,
                                     FieldCameraState& state) {
  if (!state.transition) {
    RefuseMissingTransition("StepFirstPersonCameraTransition");
    return false;
  }
  const FirstPersonTransition transition = *state.transition;
  ViewCamera& camera = state.camera;
  const CameraVector travel = Scale(transition.direction, transition.speed);
  const std::int32_t current_y = camera.position.y & kWholeUnitMask;
  const std::int32_t target_y = transition.target_position.y & kWholeUnitMask;
  const std::int32_t next_y =
      Math::WrappingAdd(camera.position.y, travel.y) & kWholeUnitMask;
  if ((current_y >= target_y && target_y >= next_y) ||
      (target_y >= current_y && next_y >= target_y)) {
    camera.position = transition.target_position;
    camera.look_at = transition.target_look_at;
    state.spherical = transition.to_spherical;
    camera.far_clip = transition.to_far_clip;
    host.SetNearClip(transition.to_near_clip);
    host.SetProjectionDistance(transition.to_projection_distance);
    return true;
  }
  camera.position = Add(camera.position, travel);
  camera.look_at = Add(camera.look_at, transition.look_at_step);
  camera.far_clip =
      Math::WrappingAdd(camera.far_clip, transition.far_clip_step);
  host.SetNearClip(
      Math::WrappingAdd(host.NearClip(), transition.near_clip_step));
  host.SetProjectionDistance(Math::WrappingAdd(
      host.ProjectionDistance(), transition.projection_distance_step));
  return false;
}

// decomp: BATTLE.PRG _initBattleCameraTransition (146C.c:4544)
void BeginFirstPersonView(CameraHost& host, FieldCameraState& state) {
  if (!state.transition) {
    state.transition = host.AllocateTransition();
    if (!state.transition) {
      VS_LOG_WARN(kChannel,
                  "no heap for the first-person camera transition; the view "
                  "is not entered");
      return;
    }
  }
  FirstPersonTransition& transition = *state.transition;
  if (host.ControlMode() != kControlModeFirstPerson) {
    transition.from_camera = state.camera;
    transition.from_spherical = state.spherical;
    transition.from_near_clip = host.NearClip();
    transition.from_projection_distance = host.ProjectionDistance();
    transition.from_far_clip = state.camera.far_clip;
  }
  transition.to_near_clip = kFirstPersonNearClip;
  transition.to_projection_distance = kFirstPersonProjectionDistance;
  transition.to_far_clip = kFirstPersonFarClip;
  const Actors::ActorPlacementView ashley = PlacementOrOrigin(host, 0);
  transition.to_spherical.values.yaw = ashley.position.w;
  transition.to_spherical.values.pitch = 0;
  transition.to_spherical.values.distance = state.spherical.values.distance;
  ComputeFirstPersonCameraTransition(
      host, state,
      {ToFixed(ashley.position.x),
       ToFixed(Math::WrappingSubtract(ashley.position.y, kEyeHeight)),
       ToFixed(ashley.position.z)});

  host.SetControlMode(kControlModeFirstPerson);
  host.SetActionFlowMode(kFirstPersonEntering);
  host.SetHudHidden(true);
  for (int actor = 0; actor < kIdlePoseActors; ++actor) {
    host.RestoreActorIdlePose(actor);
  }
  host.SetMinimapVisible(false);
  host.SetMovingPlatformsPaused(true);
  host.SetActorMarkersHidden(true);
  host.SetActorTransitionMode(kTransitionModeFirstPerson);
}

// decomp: BATTLE.PRG _endBattleCameraTransition (146C.c:4591)
void EndFirstPersonView(CameraHost& host, FieldCameraState& state) {
  if (state.transition) {
    const FirstPersonTransition transition = *state.transition;
    state.camera = transition.from_camera;
    state.spherical = transition.from_spherical;
    state.spherical.motion.target_yaw = state.spherical.values.yaw;
    host.SetNearClip(transition.from_near_clip);
    host.SetProjectionDistance(transition.from_projection_distance);
    host.FreeTransition();
    state.transition.reset();
  } else {
    RefuseMissingTransition("EndFirstPersonView");
  }
  host.SetHudHidden(false);
  host.SetMinimapVisible(true);
  host.SetMovingPlatformsPaused(false);
  host.SetActorMarkersHidden(false);
  host.SetActorTransitionMode(kTransitionModeNormal);
  host.EnterFieldExploration();
}

// decomp: BATTLE.PRG func_80074580 (src/BATTLE/BATTLE.PRG/146C.c:5165)
void UpdateFirstPersonViewEntry(CameraHost& host, FieldCameraState& state) {
  if (host.ProjectionDistance() < kHideAshleyProjection) {
    host.SetActorDisplayFlags(0,
                              host.ActorStateFlags(0) & ~kActorModelShownFlag);
  }
  if (StepFirstPersonCameraTransition(host, state)) {
    host.SetActionFlowMode(kFirstPersonLooking);
    state.first_person_view_enabled = 1;
    state.geometry_draw_mode = 1;
    host.ClearGeometryOcclusion();
  }
}

// decomp: BATTLE.PRG _checkFirstPersonViewExit (146C.c:5179)
void CheckFirstPersonViewExit(CameraHost& host, FieldCameraState& state) {
  if ((host.PressedButtons() &
       (kPadCross | kPadRightStickButton | kPadStart)) == 0) {
    return;
  }
  if (!state.transition) {
    RefuseMissingTransition("CheckFirstPersonViewExit");
    return;
  }
  host.SetActionFlowMode(kFirstPersonLeaving);
  state.first_person_view_enabled = 0;
  state.geometry_draw_mode = 0;
  const Actors::ActorPlacementView ashley = PlacementOrOrigin(host, 0);
  UpdateCameraTileConstraint(host, state, 0, ashley.tile);
  if (!state.transition) {
    RefuseMissingTransition("CheckFirstPersonViewExit");
    return;
  }

  FirstPersonTransition transition = *state.transition;
  SyncCameraAnglesFromPosition(state, &transition.to_spherical.values);
  const std::int32_t yaw = transition.to_spherical.values.yaw;
  transition.to_spherical.values.yaw =
      yaw % kOrbitStepAngle > kOrbitHalfStep
          ? Math::WrappingAdd(yaw & kOrbitStepMask, kOrbitStepAngle)
          : yaw & kOrbitStepMask;
  transition.from_spherical.motion.turn = kNoTurn;
  transition.from_spherical.values.yaw = transition.to_spherical.values.yaw;
  transition.to_spherical = transition.from_spherical;
  transition.to_near_clip = transition.from_near_clip;
  transition.to_projection_distance = transition.from_projection_distance;
  transition.to_far_clip = transition.from_far_clip;
  transition.from_camera.position = ComputeSphericalCameraPosition(
      transition.from_camera.look_at, transition.to_spherical.values);
  state.transition = transition;
  ComputeFirstPersonCameraTransition(host, state,
                                     transition.from_camera.position);
}

// decomp: BATTLE.PRG func_80074744 (src/BATTLE/BATTLE.PRG/146C.c:5222)
void UpdateFirstPersonViewExit(CameraHost& host, FieldCameraState& state) {
  if (host.ProjectionDistance() > kHideAshleyProjection) {
    host.SetActorDisplayFlags(0,
                              host.ActorStateFlags(0) | kActorModelShownFlag);
  }
  if (StepFirstPersonCameraTransition(host, state)) {
    EndFirstPersonView(host, state);
  }
}

// decomp: BATTLE.PRG func_80074B14 (src/BATTLE/BATTLE.PRG/146C.c:5339)
void UpdateCameraTileConstraint(CameraHost& host, const FieldCameraState& state,
                                int actor_id, const Actors::PackedTile& tile) {
  const std::uint32_t platform_layer =
      host.ActorStateFlags(actor_id) & kPlatformLayerMask;
  int cell_x = 0;
  int cell_z = 0;
  int layer = 0;
  if (platform_layer != 0) {
    layer = static_cast<int>(platform_layer >> kPlatformLayerShift);
  } else {
    cell_x = tile.x;
    cell_z = tile.z;
    layer = tile.layer;
  }
  const std::optional<std::uint8_t> flags =
      host.CollisionCellFlags(cell_x, cell_z, layer);
  if (!flags) {
    VS_LOG_WARN(kChannel,
                "camera tile has no authored collision cell: x={} z={} "
                "layer={}",
                cell_x, cell_z, layer);
  }
  if (flags && (*flags & kCellOccludesView) != 0) {
    host.ApplyGeometryOcclusion(
        tile.x, tile.z, tile.layer,
        state.spherical.preferred_distance == kFarCameraDistance);
    return;
  }
  host.ClearGeometryOcclusion();
}

// decomp: BATTLE.PRG _isLookAtAtDestination (146C.c:6788)
bool IsCameraLookAtOnAshley(CameraHost& host, const FieldCameraState& state) {
  const Actors::ActorPlacementView ashley = PlacementOrOrigin(host, 0);
  CameraVector destination = LookAtAbove(ashley.position);
  if (destination.y > 0) {
    destination.y = 0;
  }
  destination = ClampCameraLookAtToZone(host, state, destination);
  const CameraVector& look_at = state.camera.look_at;
  return destination.x == (look_at.x & kWholeUnitMask) &&
         destination.z == (look_at.z & kWholeUnitMask) &&
         destination.y == (look_at.y & kWholeUnitMask);
}

// decomp: BATTLE.PRG func_80077EC4 (src/BATTLE/BATTLE.PRG/146C.c:6811)
void ResetCameraToAshley(CameraHost& host, FieldCameraState& state) {
  const CameraPlacement placement =
      InitializeSphericalCamera(host, state, kKeepFacing, kKeepZoom, true);
  state.camera.position = placement.position;
  state.camera.look_at = placement.look_at;
  state.camera.far_clip = kResetFarClip;
  state.camera.angles = {};
}

// decomp: BATTLE.PRG vs_battle_syncCameraAnglesFromPosition (146C.c:8057)
std::int32_t SyncCameraAnglesFromPosition(FieldCameraState& state,
                                          OrbitAngles* orbit) {
  const CameraVector to_camera =
      UnitsBetween(state.camera.look_at, state.camera.position);
  const CameraVector unit = Math::NormalizeVector(to_camera).direction;
  const std::int32_t yaw =
      Math::WrappingAdd(Math::Arctangent2(unit.x, unit.z), kFixedOne) %
      kFixedOne;
  state.camera.angles.y = yaw;
  const std::int32_t pitch = Math::Arctangent2(
      unit.y, Math::SquareRootFixed(Math::WrappingAdd(
                  Math::WrappingMultiply(unit.x, unit.x) / kFixedOne,
                  Math::WrappingMultiply(unit.z, unit.z) / kFixedOne)));
  state.camera.angles.x = pitch;
  if (orbit != nullptr) {
    orbit->yaw = yaw;
    orbit->pitch = pitch;
    std::int32_t offset = 0;
    std::int32_t component = 0;
    if (to_camera.x != 0) {
      offset = to_camera.x;
      component = unit.x;
    } else if (to_camera.z != 0) {
      offset = to_camera.z;
      component = unit.z;
    }
    if (offset == 0) {
      orbit->distance = kVerticalCameraDistance;
    } else {
      if (component == 0) {
        VS_LOG_WARN(kChannel,
                    "camera offset {} normalizes to a zero axis; its distance "
                    "takes the R3000 quotient",
                    offset);
      }
      orbit->distance = Math::R3000Quotient(ToFixed(offset), component);
    }
  }
  return yaw;
}

// decomp: BATTLE.PRG _computeSphericalOffset (146C.c:8092)
CameraVector ComputeSphericalCameraPosition(const CameraVector& base,
                                            const OrbitAngles& angles) {
  return Add(OrbitOffset(angles, false), base);
}

// decomp: BATTLE.PRG _setCameraPositionFromAngles (146C.c:8104)
void SetCameraPositionFromAngles(FieldCameraState& state,
                                 const OrbitAngles& angles) {
  state.camera.position =
      ComputeSphericalCameraPosition(state.camera.look_at, angles);
}

// decomp: BATTLE.PRG _setCameraLookAtFromAngles (146C.c:8110)
void SetCameraLookAtFromAngles(FieldCameraState& state,
                               const OrbitAngles& angles) {
  state.camera.look_at = Add(OrbitOffset(angles, true), state.camera.position);
}

// decomp: BATTLE.PRG vs_battle_getCameraPosition (146C.c:8125)
CameraVector CameraPosition(const FieldCameraState& state) {
  return state.camera.position;
}

// decomp: BATTLE.PRG vs_battle_setCameraPosition (146C.c:8132)
void SetCameraPosition(FieldCameraState& state, const CameraVector& position) {
  state.camera.position = position;
}

// decomp: BATTLE.PRG vs_battle_getCameraLookAt (146C.c:8139)
CameraVector CameraLookAt(const FieldCameraState& state) {
  return state.camera.look_at;
}

// decomp: BATTLE.PRG vs_battle_setCameraLookAt (146C.c:8146)
void SetCameraLookAt(FieldCameraState& state, const CameraVector& look_at) {
  state.camera.look_at = look_at;
}

// decomp: BATTLE.PRG vs_battle_setCameraRoll (146C.c:8153)
void SetCameraRoll(FieldCameraState& state, std::int32_t roll) {
  state.camera.angles.z = roll;
}

// decomp: BATTLE.PRG vs_battle_getCameraRoll (146C.c:8155)
std::int32_t CameraRoll(const FieldCameraState& state) {
  return state.camera.angles.z & kAngleMask;
}

// decomp: BATTLE.PRG func_8007BDD8 (src/BATTLE/BATTLE.PRG/146C.c:8573)
std::int32_t SetFirstPersonViewDisabled(CameraHost& host,
                                        FieldCameraState& state,
                                        std::int32_t disabled) {
  host.ClearGeometryOcclusion();
  state.first_person_view_enabled = ~disabled & 1;
  return state.first_person_view_enabled;
}

// decomp: BATTLE.PRG vs_battle_initialiseCameraFromSpherical (146C.c:9071)
CameraPlacement InitializeSphericalCamera(CameraHost& host,
                                          FieldCameraState& state,
                                          std::int32_t facing,
                                          std::int32_t mode,
                                          bool look_at_is_view_camera) {
  SphericalCamera& orbit = state.spherical;
  if (facing != kKeepFacing) {
    const std::int32_t yaw =
        Math::WrappingMultiply(facing % kFacingOctants, kOrbitStepAngle);
    orbit.motion.target_yaw = yaw;
    orbit.values.yaw = yaw;
  }
  if (mode == kNearZoomPreset) {
    orbit.preferred_distance = kNearCameraDistance;
    orbit.values.distance = kNearCameraDistance;
    orbit.motion.zoom_mode = kZoomingIn;
  } else if (mode == kFarZoomPreset) {
    orbit.preferred_distance = kFarCameraDistance;
    orbit.values.distance = kFarCameraDistance;
    orbit.motion.zoom_mode = kZoomingOut;
  }
  const Actors::ActorPlacementView ashley = PlacementOrOrigin(host, 0);
  CameraVector look_at = LookAtAbove(ashley.position);
  if (look_at_is_view_camera) {
    state.camera.look_at = look_at;
  }
  look_at = ClampCameraLookAtToZone(host, state, look_at);
  if (look_at_is_view_camera) {
    state.camera.look_at = look_at;
  }
  return {.position = ComputeSphericalCameraPosition(look_at, orbit.values),
          .look_at = look_at};
}

// decomp: BATTLE.PRG func_8007CE74 (src/BATTLE/BATTLE.PRG/146C.c:9106)
void SetCameraPitchPreset(FieldCameraState& state, std::int32_t pitch) {
  state.spherical.values.pitch =
      pitch == kLowPitchPreset || pitch == kHighPitchPreset ? pitch
                                                            : kDefaultPitch;
}
}  // namespace VS::Field
