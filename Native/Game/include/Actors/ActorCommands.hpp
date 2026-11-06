#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <array>
#include <cstdint>
#include <optional>

#include "Actors/MotionRequests.hpp"

// The queued commands a movement script hands each battle actor and Ashley's
// direct movement input. The battle actor table owns the command records and
// the camera and controller their own state; these rules see them only
// through the hosts below, so they reach no actor record, PS1 address or
// byte layout.
namespace VS::Actors {
// Actor slots the command executor visits.
inline constexpr int kCommandActorSlots = 16;
// A command target at or above this names no actor.
inline constexpr std::uint8_t kCommandNoTarget = 16;

// Command states (battle actor byte 0x08).
inline constexpr std::uint8_t kCommandIdle = 0;
inline constexpr std::uint8_t kCommandActionPreviewMissing = 0x08;
inline constexpr std::uint8_t kCommandWaiting = 0x40;
inline constexpr std::uint8_t kCommandQueued = 0x80;

// Actor state flags (func_800A0BE0) the executor tests.
inline constexpr std::uint32_t kStateMovementActive = 0x00004000;
inline constexpr std::uint32_t kStateMotionLocked = 0x00200000;
inline constexpr std::uint32_t kStateActionAnimating = 0x01000000;
inline constexpr std::uint32_t kStateTravelPose = 0x04000000;
// Any nonzero reaction state (0x02000000 below 3, 0x10000000 from 3).
inline constexpr std::uint32_t kStateReacting = 0x12000000;
// An action pose or action animation: Ashley walks and cannot jump.
inline constexpr std::uint32_t kStateActionPose = 0x00080000;
// Battle actor kinds (actor 0x1C) whose commands also run outside the field
// control mode.
inline constexpr std::array<unsigned, 2> kBattleCommandActorKinds{2, 4};
// Field_controlMode while the field runs actor movement itself.
inline constexpr int kFieldMovementControlMode = 3;

// The argument words of a queued command.
inline constexpr std::size_t kCommandArgCount = 3;
using CommandArgs = std::array<std::int32_t, kCommandArgCount>;

// One actor's queued command: the state, opcode, target and flag bytes and
// the three argument words. The original reads the arguments through byte,
// halfword and vector views of its little-endian memory; the views below
// give the same values without reinterpreting storage.
struct ActorCommand {
  std::uint8_t state = 0;
  std::uint8_t opcode = 0;
  std::uint8_t target = 0;
  std::uint8_t flags = 0;
  CommandArgs args{};

  // Halfword `index` (0-3) of the first two arguments: the low then the
  // high half of each word.
  [[nodiscard]] std::int16_t Half(unsigned index) const;
  // Byte `index` (0-7) of the first two arguments, lowest first.
  [[nodiscard]] std::uint8_t Byte(unsigned index) const;
  // The first argument as a packed tile.
  [[nodiscard]] PackedTile Tile() const;
  // The first two arguments as four halfwords.
  [[nodiscard]] MotionVector Vector() const;

  bool operator==(const ActorCommand&) const = default;
};

// An actor's placement (func_800A1108): its packed tile and position, whose
// fourth halfword is the facing.
struct CommandPlacement {
  PackedTile tile{};
  MotionVector position{};
  bool operator==(const CommandPlacement&) const = default;
};

// The speeds and gait class (Battle_ActorStats 0x31-0x33) a command moves at.
struct CommandActorGait {
  std::uint8_t walk_speed = 0;
  std::uint8_t gait_class = 0;
  std::uint8_t run_speed = 0;
};

// Which motion request a jump command starts.
enum class CommandJump : std::uint8_t {
  kTile,       // func_800B63AC: picks the entry from the actor's state
  kAirborne,   // func_800B6404
  kNavigation  // func_800B6438
};

// The battle actor table, motion requests and battle state the executor
// drives.
class ActorCommandHost {
 public:
  virtual ~ActorCommandHost() = default;
  // The actor's command, or nothing for an empty slot. The executor reads it
  // once per actor and writes back only the parts the original stores, so a
  // field another system sets meanwhile is kept.
  [[nodiscard]] virtual std::optional<ActorCommand> Command(
      int actor_id) const = 0;
  virtual void SetCommandState(int actor_id, std::uint8_t state) = 0;
  virtual void SetCommandFlags(int actor_id, std::uint8_t flags) = 0;
  virtual void SetCommandArgs(int actor_id, const CommandArgs& args) = 0;
  [[nodiscard]] virtual unsigned Kind(int actor_id) const = 0;
  [[nodiscard]] virtual CommandActorGait Gait(int actor_id) const = 0;
  [[nodiscard]] virtual std::uint32_t StateFlags(int actor_id) const = 0;
  [[nodiscard]] virtual int ControlMode() const = 0;
  [[nodiscard]] virtual CommandPlacement Placement(int actor_id) = 0;

  // vs_main_nop9(0xC1, 0): a waiting command with an unknown opcode.
  virtual void ReportUnknownWaitingOpcode(int actor_id, int opcode) = 0;
  // func_800A4E68: the actor's movement for this frame.
  virtual void UpdateMovement(int actor_id) = 0;
  // func_800751B8: nonzero ends the frame's command pass with that result.
  virtual int CheckTrapAndFallTriggers(int actor_id,
                                       const CommandPlacement& placement) = 0;

  virtual void PlayModelSound(int actor_id, int sound) = 0;
  virtual void TintRed(int actor_id) = 0;
  virtual void RestoreTint() = 0;
  virtual void RestoreNeutralPose(int actor_id) = 0;
  virtual void RestoreIdlePose(int actor_id) = 0;
  // func_8009FC60; no aim passes a null aim vector.
  virtual void SetHeadTracking(int actor_id, int target,
                               const std::optional<MotionVector>& aim,
                               int step) = 0;

  virtual void TurnNavigation(int actor_id, int facing) = 0;
  virtual void TurnAtSpeed(int actor_id, std::int16_t facing, int speed) = 0;
  virtual void QueueNavigationTarget(int actor_id, int direction, int distance,
                                     int facing) = 0;
  virtual void BeginJump(int actor_id, CommandJump jump, PackedTile target,
                         int speed, int facing) = 0;
  virtual void BeginTeleport(int actor_id, PackedTile target, int facing) = 0;
  virtual void BeginTravel(int actor_id, TravelPose pose,
                           const MotionVector& target, int facing) = 0;
  virtual void QueueGroundOffset(int actor_id, const MotionVector& step) = 0;
  virtual void QueueMovementVector(int actor_id, const MotionVector& step,
                                   int facing) = 0;
  virtual void QueueFlightMovement(int actor_id, const MotionVector& step,
                                   int speed, int facing) = 0;
  virtual void FinishCombatPose(int actor_id, std::int16_t facing) = 0;
  // The action id in the actor's limb action table; nothing for an index
  // outside the table.
  [[nodiscard]] virtual std::optional<std::uint16_t> LimbActionId(
      int actor_id, unsigned limb, unsigned slot) const = 0;
  // Starts the action and reports whether no preview was selected.
  [[nodiscard]] virtual bool BeginActionSelection(int actor_id,
                                                  std::uint16_t action) = 0;
};

// decomp: BATTLE.PRG func_800753F8 (src/BATTLE/BATTLE.PRG/146C.c:5577)
// Takes each of the command's x, z and y arguments (Q12) whole when it is
// within 0x7FFF, or else 8 units (0x8000) of it, as one step of whole units
// and leaves the rest queued. The step's fourth halfword is zero (the
// original left it uninitialized).
[[nodiscard]] MotionVector ConsumeActorMovementOffset(ActorCommand& command);

// decomp: BATTLE.PRG func_80075554 (src/BATTLE/BATTLE.PRG/146C.c:5629)
// Advances each waiting command and starts each queued one, then runs the
// actor's movement (field control) or its trap and fall checks; a nonzero
// trap result ends the pass and is returned.
int ExecuteQueuedActorCommands(ActorCommandHost& host);

// Ashley's analog stick (the physical left stick, 0-255 per axis).
struct AshleyStick {
  std::uint8_t x = 0;
  std::uint8_t y = 0;
};

// The direction buttons Ashley's digital movement reads.
struct AshleyDirectionPad {
  bool up = false;
  bool down = false;
  bool left = false;
  bool right = false;
};

// Ashley's move input (Actors_ashleyMoveInput words 1 and 3): the lateral
// and forward components the movement direction is taken from.
struct AshleyMoveInput {
  int lateral = 0;
  int forward = 0;
  bool operator==(const AshleyMoveInput&) const = default;
};

// The controls Ashley's movement reads this frame.
struct AshleyControls {
  // An analog stick only counts on an analog controller.
  std::optional<AshleyStick> stick;
  AshleyDirectionPad held{};
  bool walk_held = false;     // R2
  bool jump_pressed = false;  // Square
};

inline constexpr int kAshleyStickCentre = 128;
inline constexpr int kAshleyStickDeadZone = 64;
inline constexpr int kAshleyDirectionPadInput = 16;
inline constexpr int kMinimumCameraDistance = 1536;
inline constexpr int kMaximumCameraDistance = 2304;

// The stick's move input outside the dead zone, which is taken off each
// axis; nothing while both axes rest inside it.
[[nodiscard]] std::optional<AshleyMoveInput> AshleyStickMoveInput(
    AshleyStick stick);
// The direction pad's move input; opposite buttons cancel.
[[nodiscard]] AshleyMoveInput AshleyDirectionPadMoveInput(
    AshleyDirectionPad pad);
// The orbit camera's distance kept within the range Ashley's movement uses.
[[nodiscard]] int ClampAshleyCameraDistance(int distance);
// The world facing (0-4095) of a move input seen from the camera's yaw.
[[nodiscard]] int AshleyMoveDirection(AshleyMoveInput input, int camera_yaw);

// The player actor, camera, screen transition and controller Ashley's
// movement reads, and the movement requests it issues.
class AshleyMovementHost {
 public:
  virtual ~AshleyMovementHost() = default;
  // func_800A1108 and func_80074B14: keeps the camera on Ashley's tile.
  virtual void ConstrainCameraToPlayer() = 0;
  [[nodiscard]] virtual CommandActorGait PlayerGait() const = 0;
  [[nodiscard]] virtual std::uint32_t PlayerStateFlags() const = 0;
  [[nodiscard]] virtual unsigned ScreenTransitionStep() const = 0;
  // The destination door's entry elevation while it sets the entry walk.
  [[nodiscard]] virtual std::optional<int> DoorEntryElevation() const = 0;
  virtual void UpdateCameraOrbit() = 0;
  [[nodiscard]] virtual AshleyControls Controls() const = 0;
  [[nodiscard]] virtual int CameraDistance() const = 0;
  virtual void SetCameraDistance(int distance) = 0;
  [[nodiscard]] virtual int CameraYaw() const = 0;
  virtual void SetMoveInput(AshleyMoveInput input) = 0;
  virtual void SetDirectMovement(int direction, int speed) = 0;
  virtual void BeginJumpMovement(int direction, int distance) = 0;
};

// decomp: BATTLE.PRG func_8006E7F0 (src/BATTLE/BATTLE.PRG/146C.c:2837)
// During a door transition Ashley walks in at the door's entry elevation (or
// stands); otherwise the stick or direction pad moves her relative to the
// camera, R2 or the walk-only state slows her and Square jumps.
void UpdateAshleyMovementInput(AshleyMovementHost& host);
}  // namespace VS::Actors
