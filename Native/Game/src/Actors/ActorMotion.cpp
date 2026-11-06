// Hand-maintained (tools/port/owned.csv).
// The typed actor motion tweens and movement-state rules (BATTLE.PRG 146C.c,
// 30DB0.c, 3A1A0.c and 40564.c).
#include "Actors/ActorMotion.hpp"

#include <bit>
#include <cstdlib>
#include <limits>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
using Core::Math::R3000Quotient;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;

constexpr int kRateFromActor = -1;
constexpr int kStepsFromDistance = -1;
constexpr int kTileCentre = 64;
constexpr int kQuarterTurns3 = 0xC00;
constexpr int kFacingUnits = 16;
constexpr std::uint8_t kTileKeepsFacing = 0xFF;
constexpr std::uint8_t kUnlimitedRunSpeed = 0xFF;
constexpr int kFallReach = 0x3F;
constexpr int kAshleySlot = 0;
constexpr std::uint8_t kAshleyRequestMode = 0;
constexpr std::uint8_t kActorRequestMode = 1;
constexpr std::uint8_t kLockedCommandState = 0x20;
constexpr int kLockedTravelAnimation = 0x1F;
constexpr int kIdleAnimation = 1;
constexpr int kScriptedPathBypass = 2;

// ActorJumpAnimationProfile's results ahead of the actor's own profile.
constexpr int kActionAnimationProfile = 3;
constexpr int kJumpingProfile = 4;
constexpr int kMotionLockedProfile = 5;
constexpr int kActionPoseProfile = 6;
constexpr std::uint8_t kJumpProfileMask = 3;

[[nodiscard]] std::int16_t Narrow16(int value) {
  return static_cast<std::int16_t>(value);
}

[[nodiscard]] std::uint16_t Unsigned16(int value) {
  return static_cast<std::uint16_t>(value);
}

// A halfword yaw difference brought into [-0x800, 0x800] once each way.
[[nodiscard]] std::int16_t WrapShortYaw(std::int16_t delta) {
  if (delta > kHalfTurn) delta = Narrow16(delta - kAngleOne);
  if (delta < -kHalfTurn) delta = Narrow16(delta + kAngleOne);
  return delta;
}

[[nodiscard]] int TileCentre(int tile) {
  return (tile * kTileUnits) + kTileCentre;
}

// The heading along a world offset (0 along +z).
[[nodiscard]] int HeadingTo(int delta_x, int delta_z) {
  return kQuarterTurns3 - Core::Math::Arctangent2(delta_z, delta_x);
}

// The frame count a tween takes for `numerator` over `kUnit`, rounded up away
// from zero; the halfword keeps the low bits. The remainder mask needs a
// positive power of two, which also rules out a zero divisor.
template <int kUnit>
[[nodiscard]] std::uint16_t FramesFor(int numerator) {
  static_assert(kUnit > 0 && std::has_single_bit(static_cast<unsigned>(kUnit)));
  const int whole = numerator / kUnit;
  return Unsigned16((numerator & (kUnit - 1)) != 0 ? whole + 1 : whole);
}

// SquareRootInteger of the summed squares; the sum wraps as the original's
// addu does.
[[nodiscard]] int TweenLength(const MotionVector& motion) {
  const int x = motion.x * motion.x;
  const int z = motion.z * motion.z;
  const int y = motion.y * motion.y;
  return Core::Math::SquareRootInteger(WrappingAdd(WrappingAdd(x, z), y));
}

void AddRootMotion(ActorMotion& actor) {
  actor.position.x = Narrow16(actor.position.x + actor.root_motion.x);
  actor.position.y = Narrow16(actor.position.y + actor.root_motion.y);
  actor.position.z = Narrow16(actor.position.z + actor.root_motion.z);
}

void RecenterYaw(ActorMotion& actor) {
  const auto sum =
      Narrow16(static_cast<std::uint16_t>(actor.facing) + actor.facing_delta);
  actor.facing = Narrow16(sum % kAngleOne);
}

void SetCurrentTile(ActorMotion& actor, PackedTile tile) {
  actor.current_tile.x = tile.x;
  actor.current_tile.layer = tile.layer;
  actor.current_tile.z = tile.z;
}

[[nodiscard]] MotionVector OffsetTo(const ActorMotion& actor,
                                    const MotionVector& target) {
  MotionVector offset;
  offset.x = Narrow16(target.x - actor.position.x);
  offset.y = Narrow16(target.y - actor.position.y);
  offset.z = Narrow16(target.z - actor.position.z);
  return offset;
}

[[nodiscard]] std::uint16_t RateOr(int rate, std::uint16_t actor_rate) {
  return rate == kRateFromActor ? actor_rate : Unsigned16(rate);
}

// Runs `tween` with the actor's move rate set to `rate` (kept for -1).
template <typename Tween>
void WithMoveRate(ActorMotion& actor, int rate, Tween tween) {
  if (rate == kRateFromActor) {
    tween();
    return;
  }
  const std::uint16_t saved = actor.move_rate;
  actor.move_rate = Unsigned16(rate);
  tween();
  actor.move_rate = saved;
}

// Runs `tween` with the actor's turn rate set to `rate` (kept for -1).
template <typename Tween>
void WithTurnRate(ActorMotion& actor, int rate, Tween tween) {
  if (rate == kRateFromActor) {
    tween();
    return;
  }
  const std::uint16_t saved = actor.turn_rate;
  actor.turn_rate = Unsigned16(rate);
  tween();
  actor.turn_rate = saved;
}

// The R3000 remainder: a zero divisor leaves the dividend in HI.
[[nodiscard]] int R3000Remainder(int dividend, int divisor) {
  if (divisor == 0) return dividend;
  if (divisor == -1) return 0;
  return dividend % divisor;
}

// The magnitude of a register; INT_MIN stays INT_MIN as negu leaves it.
[[nodiscard]] int RegisterMagnitude(int value) {
  if (value == std::numeric_limits<int>::min()) return value;
  return value < 0 ? -value : value;
}

// One component's share of the transition still to play out.
void StepTransitionComponent(std::int16_t& root, std::int16_t& position,
                             int frames) {
  const int step = R3000Quotient(root, frames);
  root = Narrow16(root - step);
  position = Narrow16(position + step);
}
}  // namespace

// decomp: BATTLE.PRG func_8009DC38 (src/BATTLE/BATTLE.PRG/30DB0.c:2304)
void StopActorMotion(ActorMotion& actor) {
  ClearActorMovementModes(actor);
  actor.move_frames = 0;
  actor.turn_frames = 0;
  actor.root_motion.x = 0;
  actor.root_motion.y = 0;
}

// decomp: BATTLE.PRG func_8009DC8C (src/BATTLE/BATTLE.PRG/30DB0.c:2315)
void ClearActorMovementModes(ActorMotion& actor) {
  actor.jump_countdown = 0;
  actor.movement_mode = 0;
  actor.weapon_pose_active = false;
  actor.pose_state = 0;
  actor.locomotion_state = 0;
  actor.movement_pending = false;
}

// decomp: BATTLE.PRG func_8009E180 (src/BATTLE/BATTLE.PRG/30DB0.c:2491)
int ActorFacingDeltaToPoint(const ActorMotion& actor,
                            const MotionVector* target) {
  if (target == nullptr) return 0;
  const int delta_x = target->x - actor.position.x;
  const int delta_z = target->z - actor.position.z;
  int heading = actor.facing;
  if (delta_x != 0 || delta_z != 0) {
    heading = HeadingTo(delta_x, delta_z) & kAngleMask;
  }
  heading -= actor.facing;
  if (heading > kHalfTurn) return heading - kAngleOne;
  if (heading < -kHalfTurn) return heading + kAngleOne;
  return heading;
}

// decomp: BATTLE.PRG func_800A0ABC (src/BATTLE/BATTLE.PRG/30DB0.c:4015)
void ResetActorRunMomentum(ActorMotion& actor) {
  actor.momentum_distance = 0;
  actor.run_frames = 0;
  actor.is_running = false;
}

// decomp: BATTLE.PRG func_800A0AFC (src/BATTLE/BATTLE.PRG/30DB0.c:4025)
void SetActorWalkRunSpeeds(ActorMotion& actor, int walk_speed_fixed,
                           int run_speed_fixed) {
  actor.walk_speed = static_cast<std::uint8_t>(walk_speed_fixed / kAngleOne);
  actor.run_speed = static_cast<std::uint8_t>(run_speed_fixed / kAngleOne);
  if (walk_speed_fixed >= run_speed_fixed) actor.run_speed = kUnlimitedRunSpeed;
}

// decomp: BATTLE.PRG func_800A0B5C (src/BATTLE/BATTLE.PRG/30DB0.c:4037)
void SetActorTravelStepCounts(ActorMotion& actor, int move_steps_per_tile,
                              int turn_steps_per_revolution) {
  actor.move_rate =
      Unsigned16(move_steps_per_tile < 0 ? 1 : move_steps_per_tile);
  actor.turn_rate =
      Unsigned16(turn_steps_per_revolution < 0 ? 1 : turn_steps_per_revolution);
}

// decomp: BATTLE.PRG func_800A0BA0 (src/BATTLE/BATTLE.PRG/30DB0.c:4056)
TravelStepCounts ActorTravelStepCounts(const ActorMotion& actor) {
  return {.move_steps_per_tile = actor.move_rate,
          .turn_steps_per_revolution = actor.turn_rate};
}

// decomp: BATTLE.PRG func_800A2FBC (src/BATTLE/BATTLE.PRG/3A1A0.c:239)
void StepActorTransitionDisplacement(ActorMotion& actor) {
  const int frames = actor.transition_frames;
  StepTransitionComponent(actor.root_motion.x, actor.position.x, frames);
  StepTransitionComponent(actor.root_motion.y, actor.position.y, frames);
  StepTransitionComponent(actor.root_motion.z, actor.position.z, frames);
  actor.transition_frames = Unsigned16(frames - 1);
}

// decomp: BATTLE.PRG func_800A3054 (src/BATTLE/BATTLE.PRG/3A1A0.c:256)
void SetActorFallDestination(ActorMotion& actor, int direction_x,
                             int direction_z) {
  actor.destination_mode = 1;
  actor.destination_x =
      Narrow16(actor.position.x + (direction_x < 0 ? -kFallReach : kFallReach));
  actor.destination_z =
      Narrow16(actor.position.z + (direction_z < 0 ? -kFallReach : kFallReach));
}

// decomp: BATTLE.PRG func_800A525C (src/BATTLE/BATTLE.PRG/3A1A0.c:1336)
void ClearActorMovementRequest(ActorMotion& actor) {
  actor.request_mode =
      actor.slot == kAshleySlot ? kAshleyRequestMode : kActorRequestMode;
  actor.request_direction = 0;
  actor.request_distance = 0;
  actor.request_facing = 0;
}

// decomp: BATTLE.PRG func_800AC690_getProfile
// (src/BATTLE/BATTLE.PRG/40564.c:2278)
int ActorJumpAnimationProfile(const ActorMotion& actor) {
  if (actor.action_animation_state != 0) return kActionAnimationProfile;
  if (actor.is_jumping) return kJumpingProfile;
  if (actor.motion_locked) return kMotionLockedProfile;
  if (actor.action_pose_active) return kActionPoseProfile;
  return actor.jump_profile & kJumpProfileMask;
}

// decomp: BATTLE.PRG func_800A97EC (src/BATTLE/BATTLE.PRG/40564.c:522)
void MoveActorToTileAtRate(MotionTerrainHost& terrain, ActorMotion& actor,
                           PackedTile tile, int move_steps_per_tile,
                           int turn_steps_per_revolution) {
  if (move_steps_per_tile == 0 && turn_steps_per_revolution == 0) {
    SetActorTileTranslation(terrain, actor, tile, kStepsFromDistance, 0);
    const int angle = ((tile.spare * kFacingUnits) % kAngleOne) - actor.facing;
    SetActorYawTurn(actor, Narrow16(angle), 0);
    actor.route_phase = kRouteIdle;
    SetCurrentTile(actor, tile);
    return;
  }
  actor.route_turn_rate = RateOr(turn_steps_per_revolution, actor.turn_rate);
  actor.route_move_rate = RateOr(move_steps_per_tile, actor.move_rate);
  SetActorTileTurn(actor, tile, kStepsFromDistance, actor.route_turn_rate);
  actor.route_target.x = Narrow16(TileCentre(tile.x));
  actor.route_target.z = Narrow16(TileCentre(tile.z));
  actor.route_target.y = Narrow16(
      terrain.SupportHeight(actor.route_target, SupportProbe::kWithPlatforms));
  actor.route_target.w = tile.spare == kTileKeepsFacing
                             ? Narrow16(kKeepFacing)
                             : Narrow16(tile.spare * kFacingUnits);
  actor.route_phase = kRouteTurning;
  actor.route_uses_duration = false;
}

// decomp: BATTLE.PRG func_800A9988 (src/BATTLE/BATTLE.PRG/40564.c:574)
void MoveActorToPositionAtRate(MotionTerrainHost& terrain, ActorMotion& actor,
                               const MotionVector& target,
                               int move_steps_per_tile,
                               int turn_steps_per_revolution) {
  constexpr int kFloorHeightShift = 17;
  const auto facing = static_cast<std::uint16_t>(target.w);
  const int delta_x = target.x - actor.position.x;
  const int delta_z = target.z - actor.position.z;
  const bool on_target = delta_x == 0 && delta_z == 0;
  if (move_steps_per_tile == 0 && turn_steps_per_revolution == 0) {
    if (facing != kKeepFacing || on_target) {
      actor.facing = Narrow16(facing);
    } else {
      actor.facing = Narrow16(HeadingTo(delta_x, delta_z) & kAngleMask);
    }
    actor.turn_frames = 0;
    TranslateActorToAtRate(actor, target, 0);
    return;
  }
  int angle = 0;
  if (on_target) {
    if (facing == kKeepFacing) return;
    angle = Narrow16(facing);
  } else {
    angle = HeadingTo(delta_x, delta_z) & kAngleMask;
  }
  TurnActorToAtRate(actor, Narrow16(angle), turn_steps_per_revolution);
  actor.route_move_rate = RateOr(move_steps_per_tile, actor.move_rate);
  actor.route_turn_rate = RateOr(turn_steps_per_revolution, actor.turn_rate);
  actor.route_target.x = target.x;
  actor.route_target.z = target.z;
  int height = static_cast<std::uint16_t>(target.y);
  if (height == kHeightFromFloor) {
    const auto floor = static_cast<std::uint32_t>(
        terrain.InterpolatedFloorHeight(target.x, target.z));
    height = static_cast<int>(floor << kFloorHeightShift) >> kFloorHeightShift;
    const int gimmick = terrain.GimmickSupportHeight(target.x / kTileUnits,
                                                     target.x / kTileUnits);
    if (gimmick != 0) height = gimmick;
  }
  actor.route_target.y = Narrow16(height);
  actor.route_target.w = target.w;
  actor.route_phase = kRouteTurning;
  actor.route_uses_duration = false;
}

// decomp: BATTLE.PRG func_800A9C54 (src/BATTLE/BATTLE.PRG/40564.c:661)
void TranslateActorToTileAtRate(MotionTerrainHost& terrain, ActorMotion& actor,
                                PackedTile tile, int move_steps_per_tile) {
  const int rate = move_steps_per_tile == kRateFromActor ? actor.move_rate
                                                         : move_steps_per_tile;
  SetActorTileTranslation(terrain, actor, tile, kStepsFromDistance, rate);
  SetCurrentTile(actor, tile);
}

// decomp: BATTLE.PRG func_800A9CDC (src/BATTLE/BATTLE.PRG/40564.c:676)
void TurnActorTowardTileAtRate(ActorMotion& actor, PackedTile tile,
                               int turn_steps_per_revolution) {
  const int rate = turn_steps_per_revolution == kRateFromActor
                       ? actor.turn_rate
                       : turn_steps_per_revolution;
  SetActorTileTurn(actor, tile, kStepsFromDistance, rate);
}

// decomp: BATTLE.PRG func_800A9D24 (src/BATTLE/BATTLE.PRG/40564.c:687)
void TranslateActorByAtRate(ActorMotion& actor, const MotionVector& translation,
                            int move_steps_per_tile) {
  WithMoveRate(actor, move_steps_per_tile, [&] {
    SetActorTranslation(actor, translation, kStepsFromDistance);
  });
}

// decomp: BATTLE.PRG func_800A9D90 (src/BATTLE/BATTLE.PRG/40564.c:705)
void TranslateActorToAtRate(ActorMotion& actor, const MotionVector& target,
                            int move_steps_per_tile) {
  const MotionVector translation = OffsetTo(actor, target);
  WithMoveRate(actor, move_steps_per_tile, [&] {
    SetActorTranslation(actor, translation, kStepsFromDistance);
  });
}

// decomp: BATTLE.PRG func_800A9E38 (src/BATTLE/BATTLE.PRG/40564.c:728)
void TurnActorByAtRate(ActorMotion& actor, std::int16_t yaw_delta,
                       int turn_steps_per_revolution) {
  WithTurnRate(actor, turn_steps_per_revolution,
               [&] { SetActorYawTurn(actor, yaw_delta, kStepsFromDistance); });
}

// decomp: BATTLE.PRG func_800A9EB4 (src/BATTLE/BATTLE.PRG/40564.c:746)
void TurnActorToAtRate(ActorMotion& actor, std::int16_t target_yaw,
                       int turn_steps_per_revolution) {
  const std::int16_t delta = WrapShortYaw(Narrow16(target_yaw - actor.facing));
  WithTurnRate(actor, turn_steps_per_revolution,
               [&] { SetActorYawTurn(actor, delta, kStepsFromDistance); });
}

// decomp: BATTLE.PRG func_800A9F64 (src/BATTLE/BATTLE.PRG/40564.c:774)
void RotateActorByAtRate(ActorMotion& actor, const MotionVector& rotation_delta,
                         int turn_steps_per_revolution) {
  WithTurnRate(actor, turn_steps_per_revolution, [&] {
    SetActorRotation(actor, rotation_delta, kStepsFromDistance);
  });
}

// decomp: BATTLE.PRG func_800A9FD0 (src/BATTLE/BATTLE.PRG/40564.c:793)
void RotateActorToAtRate(ActorMotion& actor,
                         const MotionVector& target_rotation,
                         int turn_steps_per_revolution) {
  MotionVector delta;
  delta.x = WrapShortYaw(Narrow16(target_rotation.x - actor.rotation_x));
  delta.y = WrapShortYaw(Narrow16(target_rotation.y - actor.facing));
  delta.z = WrapShortYaw(Narrow16(target_rotation.z - actor.rotation_z));
  WithTurnRate(actor, turn_steps_per_revolution,
               [&] { SetActorRotation(actor, delta, kStepsFromDistance); });
}

// decomp: BATTLE.PRG func_800AA108 (src/BATTLE/BATTLE.PRG/40564.c:842)
void MoveActorToTileOverTime(MotionTerrainHost& terrain, ActorMotion& actor,
                             PackedTile tile, int move_steps, int turn_steps) {
  actor.route_move_rate = Unsigned16(move_steps);
  if (move_steps == 0 && turn_steps == 0) {
    SetActorTileTranslation(terrain, actor, tile, kStepsFromDistance, 0);
    const int yaw_delta =
        ((tile.spare * kFacingUnits) % kAngleOne) - actor.facing;
    SetActorYawTurn(actor, Narrow16(yaw_delta), 0);
    actor.route_phase = kRouteIdle;
  } else {
    SetActorTileTurn(actor, tile, turn_steps, actor.turn_rate);
    actor.route_phase = kRouteTurning;
  }
  SetCurrentTile(actor, tile);
  actor.route_uses_duration = true;
}

// decomp: BATTLE.PRG func_800AA218 (src/BATTLE/BATTLE.PRG/40564.c:874)
void TranslateActorToTileOverTime(MotionTerrainHost& terrain,
                                  ActorMotion& actor, PackedTile tile,
                                  int move_steps) {
  SetActorTileTranslation(terrain, actor, tile, move_steps, actor.move_rate);
  SetCurrentTile(actor, tile);
}

// decomp: BATTLE.PRG func_800AA290 (src/BATTLE/BATTLE.PRG/40564.c:886)
// The original samples the support height with an unset target height,
// which the floor-only probe never reads.
void SetActorTileTranslation(MotionTerrainHost& terrain, ActorMotion& actor,
                             PackedTile tile, int move_steps,
                             int move_steps_per_tile) {
  MotionVector target;
  target.x = Narrow16(TileCentre(tile.x));
  target.z = Narrow16(TileCentre(tile.z));
  actor.root_motion.x = Narrow16(target.x - actor.position.x);
  actor.root_motion.z = Narrow16(target.z - actor.position.z);
  actor.root_motion.y =
      Narrow16(terrain.SupportHeight(target, SupportProbe::kFloorOnly) -
               actor.position.y);
  if (actor.root_motion.x == 0 && actor.root_motion.z == 0 &&
      actor.root_motion.y == 0) {
    return;
  }
  if (move_steps == kStepsFromDistance) {
    const int length =
        WrappingMultiply(TweenLength(actor.root_motion), move_steps_per_tile);
    actor.move_frames = FramesFor<kTileUnits>(length);
    if (actor.move_frames == 0) AddRootMotion(actor);
    return;
  }
  if (move_steps == 0) {
    actor.move_frames = 0;
    AddRootMotion(actor);
    return;
  }
  actor.move_frames = Unsigned16(move_steps);
}

// decomp: BATTLE.PRG func_800AA454 (src/BATTLE/BATTLE.PRG/40564.c:948)
void TurnActorTowardTileOverTime(ActorMotion& actor, PackedTile tile,
                                 int turn_steps) {
  SetActorTileTurn(actor, tile, turn_steps, actor.turn_rate);
}

// decomp: BATTLE.PRG func_800AA490 (src/BATTLE/BATTLE.PRG/40564.c:953)
void SetActorTileTurn(ActorMotion& actor, PackedTile tile, int turn_steps,
                      int turn_steps_per_revolution) {
  const int delta_x = tile.x - actor.current_tile.x;
  const int delta_z = tile.z - actor.current_tile.z;
  int angle = tile.spare * kFacingUnits;
  if (delta_x != 0 || delta_z != 0) {
    angle = HeadingTo(delta_x, delta_z) % kAngleOne;
  }
  angle -= actor.facing;
  if (angle >= kHalfTurn) angle -= kAngleOne;
  if (angle < -kHalfTurn) angle += kAngleOne;
  actor.facing_delta = Narrow16(angle);
  actor.rotation_step_x = 0;
  actor.rotation_step_z = 0;
  if (angle == 0) return;
  if (turn_steps == kStepsFromDistance) {
    const int scaled =
        WrappingMultiply(angle < 0 ? -angle : angle, turn_steps_per_revolution);
    actor.turn_frames = FramesFor<kAngleOne>(scaled);
    if (actor.turn_frames != 0) return;
  } else if (turn_steps != 0) {
    actor.turn_frames = Unsigned16(turn_steps);
    return;
  } else {
    actor.turn_frames = 0;
  }
  actor.facing = Narrow16((actor.facing + actor.facing_delta) & kAngleMask);
}

// decomp: BATTLE.PRG func_800AA600 (src/BATTLE/BATTLE.PRG/40564.c:1016)
void SetActorMotion(ActorMotion& actor, const MotionVector& translation,
                    int move_steps) {
  SetActorTranslation(actor, translation, move_steps);
}

// decomp: BATTLE.PRG func_800AA620 (src/BATTLE/BATTLE.PRG/40564.c:1018)
void TranslateActorToOverTime(ActorMotion& actor, const MotionVector& target,
                              int move_steps) {
  SetActorTranslation(actor, OffsetTo(actor, target), move_steps);
}

// decomp: BATTLE.PRG func_800AA698 (src/BATTLE/BATTLE.PRG/40564.c:1032)
void SetActorTranslation(ActorMotion& actor, const MotionVector& translation,
                         int move_steps) {
  actor.root_motion.x = translation.x;
  actor.root_motion.y = translation.y;
  actor.root_motion.z = translation.z;
  if (move_steps == kStepsFromDistance) {
    const int length =
        WrappingMultiply(TweenLength(actor.root_motion), actor.move_rate);
    actor.move_frames = FramesFor<kTileUnits>(length);
    if (actor.move_frames != 0) return;
  } else if (move_steps != 0) {
    actor.move_frames = Unsigned16(move_steps);
    return;
  } else {
    actor.move_frames = 0;
  }
  AddRootMotion(actor);
  actor.current_tile.x =
      static_cast<std::uint8_t>(actor.position.x / kTileUnits);
  actor.current_tile.z =
      static_cast<std::uint8_t>(actor.position.z / kTileUnits);
}

// decomp: BATTLE.PRG func_800AA82C (src/BATTLE/BATTLE.PRG/40564.c:1089)
void SetActorYawMotion(ActorMotion& actor, std::int16_t yaw_delta,
                       int turn_steps) {
  SetActorYawTurn(actor, yaw_delta, turn_steps);
}

// decomp: BATTLE.PRG func_800AA850 (src/BATTLE/BATTLE.PRG/40564.c:1091)
void TurnActorToOverTime(ActorMotion& actor, std::uint16_t target_yaw,
                         int turn_steps) {
  SetActorYawTurn(actor, WrapShortYaw(Narrow16(target_yaw - actor.facing)),
                  turn_steps);
}

// decomp: BATTLE.PRG func_800AA8D0 (src/BATTLE/BATTLE.PRG/40564.c:1115)
void TurnActorToAtAngularSpeed(ActorMotion& actor, std::int16_t target_yaw,
                               int angular_speed) {
  const std::int16_t delta = WrapShortYaw(Narrow16(target_yaw - actor.facing));
  const int magnitude = delta < 0 ? -delta : delta;
  const int speed = RegisterMagnitude(angular_speed);
  int turn_steps = R3000Quotient(magnitude, speed);
  if (R3000Remainder(magnitude, speed) != 0) ++turn_steps;
  SetActorYawTurn(actor, delta, turn_steps);
}

// decomp: BATTLE.PRG func_800AA984 (src/BATTLE/BATTLE.PRG/40564.c:1149)
void SetActorYawTurn(ActorMotion& actor, std::int16_t yaw_delta,
                     int turn_steps) {
  actor.facing_delta = yaw_delta;
  actor.rotation_step_x = 0;
  actor.rotation_step_z = 0;
  if (turn_steps == kStepsFromDistance) {
    const int magnitude = yaw_delta < 0 ? -yaw_delta : yaw_delta;
    actor.turn_frames = FramesFor<kAngleOne>(magnitude * actor.turn_rate);
    if (actor.turn_frames != 0) return;
  } else if (turn_steps != 0) {
    actor.turn_frames = Unsigned16(turn_steps);
    return;
  } else {
    actor.turn_frames = 0;
  }
  RecenterYaw(actor);
}

// decomp: BATTLE.PRG func_800AAA68 (src/BATTLE/BATTLE.PRG/40564.c:1195)
void SetActorRotationMotion(ActorMotion& actor,
                            const MotionVector& rotation_delta,
                            int turn_steps) {
  SetActorRotation(actor, rotation_delta, turn_steps);
}

// decomp: BATTLE.PRG func_800AAA88 (src/BATTLE/BATTLE.PRG/40564.c:1197)
void SetActorRotation(ActorMotion& actor, const MotionVector& rotation_delta,
                      int turn_steps) {
  actor.rotation_step_x = rotation_delta.x;
  actor.facing_delta = rotation_delta.y;
  actor.rotation_step_z = rotation_delta.z;
  if (turn_steps == kStepsFromDistance) {
    int largest = std::abs(actor.rotation_step_x);
    if (largest < std::abs(actor.facing_delta)) {
      largest = std::abs(actor.facing_delta);
    }
    if (largest < std::abs(actor.rotation_step_z)) {
      largest = std::abs(actor.rotation_step_z);
    }
    actor.turn_frames = FramesFor<kAngleOne>(largest * actor.turn_rate);
    if (actor.turn_frames != 0) return;
  } else if (turn_steps != 0) {
    actor.turn_frames = Unsigned16(turn_steps);
    return;
  } else {
    actor.turn_frames = 0;
  }
  actor.rotation_x =
      Narrow16((actor.rotation_x + actor.rotation_step_x) & kAngleMask);
  actor.facing = Narrow16((actor.facing + actor.facing_delta) & kAngleMask);
  actor.rotation_z =
      Narrow16((actor.rotation_z + actor.rotation_step_z) & kAngleMask);
}

// decomp: BATTLE.PRG func_800AABD0 (src/BATTLE/BATTLE.PRG/40564.c:1244)
void ScaleActorToOverTime(ActorMotion& actor, const MotionVector& target_scale,
                          int scale_steps) {
  if (scale_steps == 0) {
    actor.scale_frames = 0;
    actor.scale.x = target_scale.x;
    actor.scale.y = target_scale.y;
    actor.scale.z = target_scale.z;
    return;
  }
  actor.scale_delta.x = Narrow16(target_scale.x - actor.scale.x);
  actor.scale_delta.y = Narrow16(target_scale.y - actor.scale.y);
  actor.scale_delta.z = Narrow16(target_scale.z - actor.scale.z);
  if (actor.scale_delta.x == 0 && actor.scale_delta.y == 0 &&
      actor.scale_delta.z == 0) {
    actor.scale_frames = 0;
    return;
  }
  actor.scale_frames = Unsigned16(scale_steps);
}

// decomp: BATTLE.PRG func_800AC540 (src/BATTLE/BATTLE.PRG/40564.c:2233)
void UpdateActorRoute(ActorRouteHost& host) {
  switch (host.Route().phase) {
    case kRouteTurning: {
      if (host.TurnFrames() != 0) return;
      host.PlayAnimation(host.MotionLocked() ? kLockedTravelAnimation
                                             : host.TravelAnimation());
      const ActorRoute route = host.Route();
      host.TranslateTo(route.target, route.move_rate, route.uses_duration);
      host.SetRoutePhase(kRouteWalking);
      return;
    }
    case kRouteWalking: {
      if (host.MoveFrames() != 0) return;
      host.PlayAnimation(kIdleAnimation);
      const ActorRoute route = host.Route();
      const auto facing = static_cast<std::uint16_t>(route.target.w);
      if (facing != kKeepFacing) {
        host.TurnTo(facing & kAngleMask, route.turn_rate, route.uses_duration);
      }
      host.SetRoutePhase(kRouteIdle);
      return;
    }
    default:
      return;
  }
}

// decomp: BATTLE.PRG func_8007CFF8 (src/BATTLE/BATTLE.PRG/146C.c:9163)
void BeginScriptedActorPath(ScriptedPathHost& host,
                            const MotionVector& target) {
  host.SetCollisionBypass(kScriptedPathBypass);
  host.SetNavigationOverride(target.x, target.z);
}

// decomp: BATTLE.PRG func_8007D03C (src/BATTLE/BATTLE.PRG/146C.c:9169)
bool IsScriptedActorPathFinished(ScriptedPathHost& host) {
  const int status = host.NavigationStatus();
  if (status == static_cast<int>(ScriptedPathStatus::kArrived)) {
    host.SetCollisionBypass(host.CollisionBypass() & ~kScriptedPathBypass);
    return true;
  }
  return status == static_cast<int>(ScriptedPathStatus::kUntracked);
}

// decomp: BATTLE.PRG func_8007D260 (src/BATTLE/BATTLE.PRG/146C.c:9232)
void ResetActorMovement(std::uint8_t* battle_command_state,
                        ActorMotion* motion) {
  if (battle_command_state != nullptr &&
      *battle_command_state != kLockedCommandState) {
    *battle_command_state = 0;
  }
  if (motion != nullptr) StopActorMotion(*motion);
}
}  // namespace VS::Actors
