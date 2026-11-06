#pragma once

#include <cstdint>

#include "Actors/MotionRequests.hpp"

// The actor motion tweens and movement-state rules (BATTLE.PRG 146C.c,
// 30DB0.c, 3A1A0.c and 40564.c): the movement-mode resets, the walk/run
// speeds and step rates, the translations, turns, rotations and scales an
// actor plays out over its frame counters, the route that strings a turn, a
// walk and a final facing together, and the scripted-path start and finish.
// A rate of -1 takes the actor's own rate; a step count of 0 applies the
// change at once and -1 derives the count from the distance and the rate.
// Angles are 4096 to a turn; a heading of 0 points along -z and 0x400 along
// -x.
namespace VS::Actors {
inline constexpr int kTileUnits = 128;
inline constexpr int kAngleOne = 0x1000;
inline constexpr int kAngleMask = 0xFFF;
inline constexpr int kHalfTurn = 0x800;
// A route target or move target whose facing (w) keeps the travel heading,
// and a move target whose height (y) takes the floor height.
inline constexpr std::uint16_t kKeepFacing = 0x8000;
inline constexpr std::uint16_t kHeightFromFloor = 0x8000;

// route_phase: idle, turning towards the target, then walking to it.
inline constexpr int kRouteIdle = 0;
inline constexpr int kRouteTurning = 1;
inline constexpr int kRouteWalking = 2;

// The actor-record fields the motion rules read and write. Bitfield widths
// are noted; every rule keeps the values it stores within them.
struct ActorMotion {
  // Model instance (Actors_ModelInstance, at the start of the record).
  std::uint8_t action_animation_state = 0;  // 4 bits
  bool movement_pending = false;
  std::uint8_t movement_mode = 0;     // 3 bits
  std::uint8_t locomotion_state = 0;  // 2 bits
  bool motion_locked = false;
  bool is_jumping = false;
  std::uint8_t pose_state = 0;  // 3 bits
  std::uint8_t slot = 0;
  std::uint8_t jump_countdown = 0;
  std::uint16_t turn_frames = 0;
  // The frames left of the current translation.
  std::uint16_t move_frames = 0;
  MotionVector position{};
  std::int16_t rotation_x = 0;
  std::int16_t facing = 0;
  std::int16_t rotation_z = 0;
  MotionVector scale{};
  // The translation still to play out.
  MotionVector root_motion{};
  std::int16_t rotation_step_x = 0;
  std::int16_t facing_delta = 0;
  std::int16_t rotation_step_z = 0;
  MotionVector scale_delta{};
  PackedTile current_tile{};  // x, layer and z

  // Actor record.
  std::uint8_t route_phase = 0;  // 2 bits
  bool route_uses_duration = false;
  bool action_pose_active = false;
  bool is_running = false;
  bool weapon_pose_active = false;
  std::uint8_t jump_profile = 0;  // 2 bits
  std::uint8_t run_speed = 0;
  std::uint8_t walk_speed = 0;
  std::uint16_t turn_rate = 0;
  std::uint16_t move_rate = 0;
  std::uint16_t route_move_rate = 0;
  std::uint16_t route_turn_rate = 0;
  std::uint16_t scale_frames = 0;
  std::uint16_t transition_frames = 0;
  MotionVector route_target{};
  std::int16_t destination_x = 0;
  std::int16_t destination_z = 0;
  std::int16_t destination_mode = 0;
  std::int16_t momentum_distance = 0;
  std::uint8_t run_frames = 0;
  std::uint8_t request_mode = 0;
  std::int16_t request_direction = 0;
  std::int16_t request_facing = 0;
  std::uint32_t request_distance = 0;

  friend bool operator==(const ActorMotion&, const ActorMotion&) = default;
};

// The terrain the tile and position moves sample for their target height.
enum class SupportProbe : std::uint8_t {
  kWithPlatforms = 1,  // floor, gimmicks and moving platforms
  kFloorOnly = 4,      // floor and gimmicks
};

class MotionTerrainHost {
 public:
  MotionTerrainHost() = default;
  MotionTerrainHost(const MotionTerrainHost&) = delete;
  MotionTerrainHost& operator=(const MotionTerrainHost&) = delete;
  MotionTerrainHost(MotionTerrainHost&&) = delete;
  MotionTerrainHost& operator=(MotionTerrainHost&&) = delete;
  virtual ~MotionTerrainHost() = default;

  // The support height under (origin.x, origin.z).
  [[nodiscard]] virtual int SupportHeight(const MotionVector& origin,
                                          SupportProbe probe) = 0;
  [[nodiscard]] virtual int InterpolatedFloorHeight(int x, int z) = 0;
  // A standing gimmick's top on the tile, or 0.
  [[nodiscard]] virtual int GimmickSupportHeight(int tile_x, int tile_z) = 0;
};

struct TravelStepCounts {
  int move_steps_per_tile = 0;
  int turn_steps_per_revolution = 0;
};

// func_8009DC38: clears the movement modes, both frame counters and the root
// motion's x and y.
void StopActorMotion(ActorMotion& actor);
// func_8009DC8C: clears the jump countdown, movement, weapon pose, pose and
// locomotion states.
void ClearActorMovementModes(ActorMotion& actor);
// func_8009E180: the signed yaw from the actor's facing towards `target`;
// 0 without a target.
[[nodiscard]] int ActorFacingDeltaToPoint(const ActorMotion& actor,
                                          const MotionVector* target);
// func_800A0ABC: stops the run momentum.
void ResetActorRunMomentum(ActorMotion& actor);
// func_800A0AFC: the walk and run speeds from Q12 speeds; no run speed limit
// when walking is as fast.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActorWalkRunSpeeds(ActorMotion& actor, int walk_speed_fixed,
                           int run_speed_fixed);
// func_800A0B5C: the move and turn rates; a negative rate becomes 1.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActorTravelStepCounts(ActorMotion& actor, int move_steps_per_tile,
                              int turn_steps_per_revolution);
// func_800A0BA0
[[nodiscard]] TravelStepCounts ActorTravelStepCounts(const ActorMotion& actor);

// func_800A2FBC: one frame of a travel transition; each root-motion
// component moves 1/transition_frames of itself into the position. A zero
// frame count divides as the R3000 does (quotient -1, or 1 below zero).
void StepActorTransitionDisplacement(ActorMotion& actor);
// func_800A3054: a fall destination 63 units past the position along the
// fall's x and z directions.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActorFallDestination(ActorMotion& actor, int direction_x,
                             int direction_z);
// func_800A525C: an empty movement request; Ashley's slot takes mode 0,
// every other actor mode 1.
void ClearActorMovementRequest(ActorMotion& actor);
// func_800AC690_getProfile: the jump animation profile; 3 during an action
// animation, 4 while jumping, 5 while motion-locked, 6 in an action pose,
// else the actor's own jump profile.
[[nodiscard]] int ActorJumpAnimationProfile(const ActorMotion& actor);

// func_800A97EC: a route to the centre of `tile` (turn, walk, then face the
// tile's facing); both rates 0 place the actor there at once.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveActorToTileAtRate(MotionTerrainHost& terrain, ActorMotion& actor,
                           PackedTile tile, int move_steps_per_tile,
                           int turn_steps_per_revolution);
// func_800A9988: a route to `target`; a height of kHeightFromFloor takes the
// floor (or gimmick) height and a facing of kKeepFacing keeps the travel
// heading. The gimmick height is read at tile (x, x) as in the original.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveActorToPositionAtRate(MotionTerrainHost& terrain, ActorMotion& actor,
                               const MotionVector& target,
                               int move_steps_per_tile,
                               int turn_steps_per_revolution);
// func_800A9C54
void TranslateActorToTileAtRate(MotionTerrainHost& terrain, ActorMotion& actor,
                                PackedTile tile, int move_steps_per_tile);
// func_800A9CDC
void TurnActorTowardTileAtRate(ActorMotion& actor, PackedTile tile,
                               int turn_steps_per_revolution);
// func_800A9D24
void TranslateActorByAtRate(ActorMotion& actor, const MotionVector& translation,
                            int move_steps_per_tile);
// func_800A9D90
void TranslateActorToAtRate(ActorMotion& actor, const MotionVector& target,
                            int move_steps_per_tile);
// func_800A9E38
void TurnActorByAtRate(ActorMotion& actor, std::int16_t yaw_delta,
                       int turn_steps_per_revolution);
// func_800A9EB4
void TurnActorToAtRate(ActorMotion& actor, std::int16_t target_yaw,
                       int turn_steps_per_revolution);
// func_800A9F64
void RotateActorByAtRate(ActorMotion& actor, const MotionVector& rotation_delta,
                         int turn_steps_per_revolution);
// func_800A9FD0
void RotateActorToAtRate(ActorMotion& actor,
                         const MotionVector& target_rotation,
                         int turn_steps_per_revolution);
// func_800AA108: a timed route to `tile`; both counts 0 place the actor there
// at once.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void MoveActorToTileOverTime(MotionTerrainHost& terrain, ActorMotion& actor,
                             PackedTile tile, int move_steps, int turn_steps);
// func_800AA218
void TranslateActorToTileOverTime(MotionTerrainHost& terrain,
                                  ActorMotion& actor, PackedTile tile,
                                  int move_steps);
// func_800AA290: the root motion to the centre of `tile` at its floor height.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActorTileTranslation(MotionTerrainHost& terrain, ActorMotion& actor,
                             PackedTile tile, int move_steps,
                             int move_steps_per_tile);
// func_800AA454
void TurnActorTowardTileOverTime(ActorMotion& actor, PackedTile tile,
                                 int turn_steps);
// func_800AA490: the yaw towards `tile` (its facing when the actor stands on
// it).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the original order.
void SetActorTileTurn(ActorMotion& actor, PackedTile tile, int turn_steps,
                      int turn_steps_per_revolution);
// func_800AA600
void SetActorMotion(ActorMotion& actor, const MotionVector& translation,
                    int move_steps);
// func_800AA620
void TranslateActorToOverTime(ActorMotion& actor, const MotionVector& target,
                              int move_steps);
// func_800AA698: the root motion; applied at once, the current tile follows
// the position.
void SetActorTranslation(ActorMotion& actor, const MotionVector& translation,
                         int move_steps);
// func_800AA82C
void SetActorYawMotion(ActorMotion& actor, std::int16_t yaw_delta,
                       int turn_steps);
// func_800AA850
void TurnActorToOverTime(ActorMotion& actor, std::uint16_t target_yaw,
                         int turn_steps);
// func_800AA8D0: a turn of |angular_speed| units a frame; a zero speed
// divides as the R3000 does and turns at once.
void TurnActorToAtAngularSpeed(ActorMotion& actor, std::int16_t target_yaw,
                               int angular_speed);
// func_800AA984
void SetActorYawTurn(ActorMotion& actor, std::int16_t yaw_delta,
                     int turn_steps);
// func_800AAA68
void SetActorRotationMotion(ActorMotion& actor,
                            const MotionVector& rotation_delta, int turn_steps);
// func_800AAA88
void SetActorRotation(ActorMotion& actor, const MotionVector& rotation_delta,
                      int turn_steps);
// func_800AABD0
void ScaleActorToOverTime(ActorMotion& actor, const MotionVector& target_scale,
                          int scale_steps);

// The route fields UpdateActorRoute reads.
struct ActorRoute {
  std::uint8_t phase = kRouteIdle;
  bool uses_duration = false;
  MotionVector target{};
  std::uint16_t move_rate = 0;
  std::uint16_t turn_rate = 0;
};

// The actor a route drives. The animations it plays can rewrite the record,
// so the route is read again after each one; the moves run the tweens above
// for the same actor.
class ActorRouteHost {
 public:
  ActorRouteHost() = default;
  ActorRouteHost(const ActorRouteHost&) = delete;
  ActorRouteHost& operator=(const ActorRouteHost&) = delete;
  ActorRouteHost(ActorRouteHost&&) = delete;
  ActorRouteHost& operator=(ActorRouteHost&&) = delete;
  virtual ~ActorRouteHost() = default;

  [[nodiscard]] virtual ActorRoute Route() = 0;
  virtual void SetRoutePhase(int phase) = 0;
  [[nodiscard]] virtual int TurnFrames() = 0;
  [[nodiscard]] virtual int MoveFrames() = 0;
  [[nodiscard]] virtual bool MotionLocked() = 0;
  [[nodiscard]] virtual int TravelAnimation() = 0;
  virtual void PlayAnimation(int animation) = 0;
  // TranslateActorToAtRate, or TranslateActorToOverTime with `over_time`.
  virtual void TranslateTo(const MotionVector& target, int rate,
                           bool over_time) = 0;
  // TurnActorToAtRate, or TurnActorToOverTime with `over_time`.
  virtual void TurnTo(int target_yaw, int rate, bool over_time) = 0;
};

// func_800AC540: once the turn ends, plays the travel animation (0x1F while
// motion-locked) and walks to the route target; once the walk ends, plays
// the idle animation and turns to the target's facing unless kKeepFacing.
void UpdateActorRoute(ActorRouteHost& host);

// A scripted path's status from the AI navigation override.
enum class ScriptedPathStatus : std::uint8_t {
  kArrived = 0,
  kMoving = 1,
  kUntracked = 2,
};

class ScriptedPathHost {
 public:
  ScriptedPathHost() = default;
  ScriptedPathHost(const ScriptedPathHost&) = delete;
  ScriptedPathHost& operator=(const ScriptedPathHost&) = delete;
  ScriptedPathHost(ScriptedPathHost&&) = delete;
  ScriptedPathHost& operator=(ScriptedPathHost&&) = delete;
  virtual ~ScriptedPathHost() = default;

  [[nodiscard]] virtual int CollisionBypass() = 0;
  virtual void SetCollisionBypass(int flags) = 0;
  virtual void SetNavigationOverride(int x, int z) = 0;
  [[nodiscard]] virtual int NavigationStatus() = 0;
};

// func_8007CFF8: lets the actor pass through other actors and hands it to the
// AI navigation override towards (target.x, target.z).
void BeginScriptedActorPath(ScriptedPathHost& host, const MotionVector& target);
// func_8007D03C: true once the actor arrives (which restores its collision)
// or the override tracks nothing.
[[nodiscard]] bool IsScriptedActorPathFinished(ScriptedPathHost& host);

// func_8007D260: drops the actor's battle command unless it is the locked
// command state 0x20, then stops its motion. Either may be absent.
void ResetActorMovement(std::uint8_t* battle_command_state,
                        ActorMotion* motion);
}  // namespace VS::Actors
