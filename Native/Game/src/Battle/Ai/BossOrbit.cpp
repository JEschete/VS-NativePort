// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/BossOrbit.hpp"

#include <cstdint>
#include <cstdlib>
#include <optional>

#include "Battle/Ai/ActorCommands.hpp"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
using Core::Math::Arctangent2;
using Core::Math::Cosine;
using Core::Math::R3000Quotient;
using Core::Math::Sine;
using Core::Math::SquaredVectorLength;
using Core::Math::SquareRootInteger;
using Core::Math::WrappingAdd;
using Core::Math::WrappingMultiply;
using Core::Math::WrappingNegate;
using Core::Math::WrappingSubtract;

constexpr int kAngleMask = 0xFFF;
constexpr int kFullTurn = 0x1000;
constexpr int kHalfTurn = 0x800;
constexpr int kThreeQuarterTurn = 0xC00;
// World units to the Q12 movement vector.
constexpr int kFixedOne = 0x1000;
// The angular velocity's whole angle units.
constexpr int kVelocityShift = 16;
constexpr int kVelocityOne = 1 << kVelocityShift;
constexpr int kHalfWordBits = 16;
constexpr std::uint32_t kHalfWordMask = 0xFFFF;

// The retreat flags the model for its first steps.
constexpr int kRetreatFlaggedSteps = 14;

constexpr int kRadiusStep = 32;
constexpr int kHeightSnap = 16;
constexpr int kHeightStep = 12;
// Above this angular speed an aligned boss eases by an eighth, not a step.
constexpr int kEasedAngularSpeed = 0x280000;
constexpr int kAlignedEasing = 8;
constexpr int kOpposedEasing = 4;
constexpr int kMaxAngularSpeed = 0x820000;
// The player within about 19.5 degrees of the heading counts as ahead.
constexpr int kAheadTolerance = 222;
// The HP ratio (16.16) below which each health phase requests a move.
constexpr int kFirstHealthMark = 0xC000;
constexpr int kSecondHealthMark = 0x4000;
// A slow, low boss requests a move about once in 512 calls.
constexpr int kSlowAngularSpeed = 0x40000;
constexpr int kLowHeightOffset = 30;
constexpr int kRandomMoveMask = 511;
// The height correction's scales: closing in while facing the player,
// pulling away, and a stray boss.
constexpr int kClosingScale = -80;
constexpr int kPullingScale = 300;
constexpr int kStrayScale = 10;
constexpr int kCorrectionLimit = 4;
constexpr int kMinHeightOffset = -100;
constexpr int kMaxHeightOffset = 500;
constexpr std::int32_t kMaxMoveY = 0xFF000;
// The retreat's closing attack: row 4, column 0 of the boss's attacks.
constexpr int kRetreatAttackRow = 4;
// The core limb lives while its HP is at least this.
constexpr int kLivingCoreHp = 2;

// The facing toward (dx, dz): the ratan2 angle reflected a quarter turn
// back.
int FacingAngle(int dx, int dz) {
  return (kThreeQuarterTurn - (Arctangent2(dx, dz) & kAngleMask)) & kAngleMask;
}

// The original's absolute value: INT32_MIN keeps its sign.
int Magnitude(int value) { return value < 0 ? WrappingNegate(value) : value; }

int SignOf(int value) { return value >= 0 ? 1 : -1; }

std::int32_t Packed(int low, int high) {
  return static_cast<std::int32_t>(
      (static_cast<std::uint32_t>(high) << kHalfWordBits) |
      (static_cast<std::uint32_t>(low) & kHalfWordMask));
}

// Eases the distance from the centre toward the orbit radius by one step and
// snaps within a step.
int EasedRadius(std::int32_t distance_squared) {
  int radius = SquareRootInteger(distance_squared);
  if (radius < kArenaOrbitRadius) radius += kRadiusStep;
  if (radius > kArenaOrbitRadius) radius -= kRadiusStep;
  if (std::abs(radius - kArenaOrbitRadius) < kRadiusStep) {
    radius = kArenaOrbitRadius;
  }
  return radius;
}

std::int16_t EasedHeight(std::int16_t height) {
  if (std::abs(height - kArenaOrbitHeight) < kHeightSnap) {
    return kArenaOrbitHeight;
  }
  return static_cast<std::int16_t>(
      height < kArenaOrbitHeight ? height + kHeightStep : height - kHeightStep);
}

// Whether turning by the velocity's sign brings the boss's angle around the
// centre toward `heading`.
bool IsAligned(int current_angle, int heading, int sign) {
  const int cross = (Cosine(kThreeQuarterTurn - current_angle) *
                     Sine(kThreeQuarterTurn - heading)) -
                    (Sine(kThreeQuarterTurn - current_angle) *
                     Cosine(kThreeQuarterTurn - heading));
  return cross * sign >= 0;
}

// The angular acceleration: a step along the velocity, eased by part of the
// velocity when fast or turning against it, and reversed when the boss is
// tracking or off its heading and not aligned.
int AngularDelta(int velocity, int current_angle, int heading, bool tracking,
                 bool aligned) {
  const int sign = SignOf(velocity);
  int delta = sign * kVelocityOne;
  if (!aligned) {
    delta += velocity / kOpposedEasing;
  } else if (Magnitude(velocity) > kEasedAngularSpeed) {
    delta += velocity / kAlignedEasing;
  }
  const bool ahead = Cosine(current_angle - heading) >= Cosine(kAheadTolerance);
  if ((tracking || !ahead) && !aligned) delta = -delta;
  return delta;
}

// The health marks request a move in turn; a slow, low tracking boss also
// requests one at random. Only while the move cooldown is spent.
void RequestMove(BossOrbitWorld& world, const BossStats& stats,
                 ArenaOrbit& orbit, bool tracking) {
  // The original divides without testing for zero HP.
  const int ratio = R3000Quotient(
      WrappingMultiply(stats.current_hp, kVelocityOne), stats.max_hp);
  if (orbit.cooldown != 0) return;
  if ((orbit.health_phase == 0 && ratio < kFirstHealthMark) ||
      (orbit.health_phase == 1 && ratio < kSecondHealthMark)) {
    ++orbit.health_phase;
    orbit.move_requested = 1;
  } else if (Magnitude(orbit.angular_velocity) < kSlowAngularSpeed &&
             tracking && orbit.height_offset < kLowHeightOffset &&
             (world.Random() & kRandomMoveMask) == 0) {
    orbit.move_requested = 1;
  }
}

// The height offset's change: proportional to the angular speed over the
// angle left to turn, limited to four times the speed.
int HeightCorrection(const ArenaOrbit& orbit, int current_angle,
                     int player_angle, bool tracking, bool aligned) {
  int remaining = 0;
  int scale = 0;
  if (tracking && aligned) {
    remaining = current_angle - player_angle;
    scale = WrappingSubtract(kClosingScale, orbit.height_offset);
  } else if (tracking || aligned) {
    remaining = player_angle - (current_angle - kHalfTurn);
    scale = WrappingSubtract(kPullingScale, orbit.height_offset);
  } else {
    remaining = Magnitude(orbit.angular_velocity) >> kVelocityShift;
    scale = kStrayScale;
  }
  remaining &= kAngleMask;
  if (remaining > kHalfTurn) remaining = kFullTurn - remaining;
  if (remaining == 0) {
    remaining = 1;
    scale = 0;
  }
  const int speed = Magnitude(orbit.angular_velocity);
  const int correction = WrappingMultiply(speed, scale) / remaining;
  if (WrappingMultiply(speed, kCorrectionLimit) < Magnitude(correction)) {
    // The original limits by the correction's sign word, so an over-limit
    // rise drops to zero and only a fall is clamped.
    return correction < 0
               ? WrappingMultiply(WrappingNegate(speed), kCorrectionLimit)
               : 0;
  }
  return correction;
}

void InitializeOrbit(const OrbitingBoss& boss, ArenaOrbit& orbit) {
  if (orbit.started == 0) {
    orbit.heading = static_cast<std::uint16_t>(
        FacingAngle(kArenaCentre - boss.x, kArenaCentre - boss.z));
    orbit.saved_height = boss.y;
    orbit.started = 1;
  }
  if (orbit.distance_cached == 0) {
    orbit.distance_squared = static_cast<std::int32_t>(
        SquaredVectorLength(kArenaCentre - boss.x, 0, kArenaCentre - boss.z));
    ++orbit.distance_cached;
  }
}

// Steps the heading by the velocity and aims the boss at the orbit's point
// at the eased radius, raised by the height offset.
void AimAlongOrbit(BossOrbitWorld& world, OrbitingBoss& boss, ArenaOrbit& orbit,
                   int radius, int centre_y) {
  constexpr int kCentre = kArenaCentre * kFixedOne;
  orbit.heading = static_cast<std::uint16_t>(
      orbit.heading + (orbit.angular_velocity >> kVelocityShift));
  boss.movement_target = 0;
  boss.move_x =
      WrappingAdd(kCentre, WrappingMultiply(radius, Cosine(orbit.heading)));
  boss.move_z =
      WrappingAdd(kCentre, WrappingMultiply(radius, Sine(orbit.heading)));
  boss.move_y = centre_y - (orbit.height_offset * kFixedOne);
  if (boss.move_y > kMaxMoveY - 1) boss.move_y = kMaxMoveY;
  world.IssueCommand(boss, CommandIntent::kMoveVector);
}
}  // namespace

std::optional<bool> UpdateBossRadialRetreat(BossOrbitWorld& world,
                                            OrbitingBoss& boss,
                                            ArenaOrbit& orbit) {
  if (!boss.model_flag) return std::nullopt;
  boss.candidate_count = 0;
  // The original also squared the distance to the centre here and discarded
  // it; only GTE registers changed.
  boss.model_flag = orbit.path_step < kRetreatFlaggedSteps;
  if (orbit.retreat_phase == 0) {
    orbit.is_orbiting = 1;
    orbit.retreat_angle = static_cast<std::int16_t>(
        FacingAngle(boss.x - kArenaCentre, boss.z - kArenaCentre));
    orbit.path_step = 0;
    ++orbit.retreat_phase;
    orbit.start_x = boss.x;
    orbit.start_z = boss.z;
  } else if (orbit.retreat_phase == 1) {
    const std::optional<RetreatStep> end =
        world.RetreatPathStep(orbit.path_step);
    if (!end) return std::nullopt;
    if (end->distance == 0 && end->height == 0) {
      const std::optional<BossPlayer> player = world.Player();
      if (!player) return std::nullopt;
      if (player->moving) {
        world.RecoverMovement(boss);
        return false;
      }
      orbit.retreat_phase = 0;
      orbit.heading = static_cast<std::uint16_t>(
          FacingAngle(kArenaCentre - boss.x, kArenaCentre - boss.z));
      boss.model_flag = false;
      return true;
    }
  }
  const std::optional<RetreatStep> step =
      world.RetreatPathStep(orbit.path_step);
  if (!step) return std::nullopt;
  boss.move_x =
      (boss.x * kFixedOne) + (step->distance * Cosine(orbit.retreat_angle));
  boss.move_z =
      (boss.z * kFixedOne) + (step->distance * Sine(orbit.retreat_angle));
  boss.move_y = (boss.y + step->height) * kFixedOne;
  world.IssueCommand(boss, CommandIntent::kMoveVector);
  orbit.distance_cached = 0;
  ++orbit.path_step;
  return false;
}

std::optional<bool> IsBossActionReady(BossOrbitWorld& world,
                                      OrbitingBoss& boss) {
  if (boss.candidate_count == 0) return false;
  const std::optional<bool> cooled = world.IsFirstCandidateCooldownReady(boss);
  if (!cooled) return std::nullopt;
  if (!*cooled) return false;
  const std::optional<std::uint16_t> threshold =
      world.FirstCandidateThreshold();
  const std::optional<std::uint16_t> readiness = world.ReadinessLevel();
  if (!threshold || !readiness) return std::nullopt;
  return *threshold < *readiness;
}

bool UpdateBossOrbitTracking(BossOrbitWorld& world, OrbitingBoss& boss,
                             ArenaOrbit& orbit) {
  if (!boss.model_flag || !boss.stats) return false;
  InitializeOrbit(boss, orbit);
  boss.model_flag = false;
  const int radius = EasedRadius(orbit.distance_squared);
  orbit.distance_squared = WrappingMultiply(radius, radius);
  orbit.saved_height = EasedHeight(orbit.saved_height);
  const int centre_y = orbit.saved_height * kFixedOne;
  const std::optional<BossPlayer> player = world.Player();
  if (!player) return false;
  const int player_angle =
      Arctangent2(kArenaCentre - player->x, kArenaCentre - player->z);
  const int current_angle =
      Arctangent2(kArenaCentre - boss.x, kArenaCentre - boss.z);

  const std::optional<bool> ready = IsBossActionReady(world, boss);
  if (!ready) return false;
  const bool tracking = *ready && world.IsEngagementReady(boss);
  // A tracking boss turns toward the player; otherwise it keeps opposite.
  const int heading = tracking ? player_angle : player_angle + kHalfTurn;
  const bool aligned =
      IsAligned(current_angle, heading, SignOf(orbit.angular_velocity));
  orbit.angular_velocity =
      WrappingAdd(orbit.angular_velocity,
                  AngularDelta(orbit.angular_velocity, current_angle, heading,
                               tracking, aligned));
  if (Magnitude(orbit.angular_velocity) > kMaxAngularSpeed) {
    orbit.angular_velocity = SignOf(orbit.angular_velocity) * kMaxAngularSpeed;
  }
  RequestMove(world, *boss.stats, orbit, tracking);

  const int correction =
      HeightCorrection(orbit, current_angle, player_angle, tracking, aligned);
  orbit.height_offset =
      WrappingAdd(orbit.height_offset, correction >> kVelocityShift);
  if (orbit.height_offset < kMinHeightOffset) {
    orbit.height_offset = kMinHeightOffset;
  }
  if (orbit.height_offset > kMaxHeightOffset) {
    orbit.height_offset = kMaxHeightOffset;
  }
  AimAlongOrbit(world, boss, orbit, radius, centre_y);
  return true;
}

bool UpdateOrbitingBossBehavior(BossOrbitWorld& world, OrbitingBoss& boss,
                                ArenaOrbit& orbit) {
  const std::optional<bool> ready = IsBossActionReady(world, boss);
  if (!ready) return false;
  if (*ready) world.TryEngage(boss);
  std::uint8_t progress = world.Progress();
  if (orbit.cooldown > 0) {
    orbit.cooldown = static_cast<std::int16_t>(orbit.cooldown - 1);
  }
  switch (static_cast<ArenaProgress>(progress)) {
    case ArenaProgress::kOrbiting:
      if (orbit.plan_saved != 0) {
        orbit.plan_saved = 0;
        boss.candidate_count = orbit.saved_plan_count;
      }
      orbit.is_orbiting = 0;
      if (!UpdateBossOrbitTracking(world, boss, orbit)) return false;
      if (orbit.move_requested != 0 && orbit.cooldown == 0 && boss.stats &&
          boss.stats->core_hp >= kLivingCoreHp) {
        orbit.cooldown = kArenaMoveCooldown;
        orbit.move_requested = 0;
        orbit.saved_plan_count =
            static_cast<std::uint8_t>(boss.candidate_count);
        progress = static_cast<std::uint8_t>(ArenaProgress::kMoveRequested);
        boss.candidate_count = 0;
        orbit.plan_saved = 1;
      }
      break;
    case ArenaProgress::kRetreating: {
      orbit.started = 0;
      world.Disengage(boss);
      const std::optional<bool> ended =
          UpdateBossRadialRetreat(world, boss, orbit);
      if (!ended) return false;
      if (!*ended) break;
      if (orbit.is_orbiting != 0) {
        const std::optional<BossBattleActor> actor = world.BattleActor();
        if (!actor) return false;
        if (actor->core_hp >= kLivingCoreHp) {
          const std::optional<BossPlayer> player = world.Player();
          if (!player) return false;
          // The closing attack at the player's position, raised to half the
          // player's collision height.
          ActorCommandPacket packet = actor->command;
          packet.arg0 = Packed(kRetreatAttackRow, 0);
          packet.arg1 =
              Packed(player->x, player->y - (player->collision_height >> 1));
          packet.arg2 = Packed(player->z, packet.arg2 >> kHalfWordBits);
          packet.opcode = CommandOpcode::kAttack;
          world.QueueBattleCommand(packet);
        }
        orbit.saved_height = boss.y;
        boss.candidate_count = orbit.saved_plan_count;
        orbit.plan_saved = 0;
      } else {
        progress = static_cast<std::uint8_t>(ArenaProgress::kRetreatEnded);
      }
      orbit.height_offset = 0;
      orbit.move_requested = 0;
      orbit.angular_velocity = 0;
      break;
    }
    default:
      break;
  }
  world.SetProgress(progress);
  return true;
}
}  // namespace VS::Battle::Ai
