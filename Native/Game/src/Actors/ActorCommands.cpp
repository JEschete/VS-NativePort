// Hand-maintained (tools/port/owned.csv).
// Typed C++ promotion of the BATTLE.PRG queued actor commands and Ashley's
// direct movement input (146C.c:2837-2936, 5577-5866).
#include "Actors/ActorCommands.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Actors {
namespace {
constexpr std::int32_t kQ12One = 4096;
constexpr int kQ12Shift = 12;
// A Q12 argument up to this size is taken whole; a larger one in chunks.
constexpr std::int32_t kWholeOffsetLimit = 0x7FFF;
constexpr std::int32_t kOffsetChunk = 0x8000;
constexpr std::int16_t kOffsetChunkStep = 8;

constexpr int kFacingHalfTurn = 0x800;
constexpr int kFacingMask = 0xFFF;
constexpr int kTileSize = 128;
constexpr int kTileCentre = 64;
// Door transition steps (Field_screenTransitionStep) that walk Ashley
// through the door.
constexpr auto kDoorWalkSteps = std::to_array<unsigned>({1U, 2U, 4U});

constexpr std::uint8_t kSoundMask = 0x03;
constexpr int kTintShift = 2;
constexpr std::uint8_t kTintMask = 0x03;
constexpr std::uint8_t kTintBits = 0x0C;
constexpr std::uint8_t kTintRed = 1;
constexpr std::uint8_t kTintRestore = 2;

// Head tracking targets and steps (func_8009FC60).
constexpr int kHeadTrackDefault = -2;
constexpr int kHeadTrackAim = -3;
constexpr int kHeadTrackFrame = -4;
constexpr int kHeadStepDefault = -1;
constexpr int kHeadStepFull = 0x1000;

// Gait classes that run each kind of move instead of walking.
constexpr std::uint8_t kGaitRunsTiles = 2;
constexpr std::uint8_t kGaitRunsTilesFast = 0x40;
constexpr std::uint8_t kGaitRunsFacing = 4;
constexpr std::uint8_t kGaitRunsFacingFast = 0x80;
constexpr std::uint8_t kGaitRunsFlight = 0x10;
constexpr std::uint8_t kGaitRunsVector = 0x20;

// The original's command opcodes.
enum Opcode : std::uint8_t {
  kOpFinish = 1,
  kOpRestorePose = 2,
  kOpTurnNavigation = 3,
  kOpTurnAtSpeed = 4,
  kOpFinishQuiet = 5,
  kOpHoldFrame = 6,
  kOpHoldFrameFull = 7,
  kOpStopGround = 8,
  kOpWalkToTile = 9,
  kOpWalkFacing = 10,
  kOpJump = 11,
  kOpJumpAlternate = 12,
  kOpGroundOffset = 13,
  kOpAirborneJump = 14,
  kOpNavigateToTile = 15,
  kOpFlight = 16,
  kOpFlightAlternate = 17,
  kOpNavigationJump = 18,
  kOpMoveToPosition = 19,
  kOpCombatPose = 20,
  kOpTileTravel = 21,
  kOpTimedTravel = 22,
  kOpTeleport = 23,
  kOpLimbAction = 24,
};

std::int16_t ConsumeAxis(std::int32_t& value) {
  if (value < 0) {
    if (value >= -kWholeOffsetLimit) {
      const auto step = static_cast<std::int16_t>(value / kQ12One);
      value = 0;
      return step;
    }
    value += kOffsetChunk;
    return static_cast<std::int16_t>(-kOffsetChunkStep);
  }
  if (value <= kWholeOffsetLimit) {
    const auto step = static_cast<std::int16_t>(value / kQ12One);
    value = 0;
    return step;
  }
  value -= kOffsetChunk;
  return kOffsetChunkStep;
}

// The facing that looks back along (dx, dz), as the original adds half a
// turn to the arctangent without wrapping it.
int FacingAlong(int dx, int dz) {
  return Core::Math::Arctangent2(dx, dz) + kFacingHalfTurn;
}

int FacingTowards(ActorCommandHost& host, std::uint8_t target,
                  const CommandPlacement& from) {
  const CommandPlacement to = host.Placement(target);
  return FacingAlong(to.position.x - from.position.x,
                     to.position.z - from.position.z);
}

int GaitSpeed(const CommandActorGait& gait, bool runs) {
  return (runs ? gait.run_speed : gait.walk_speed) << kQ12Shift;
}

std::uint8_t WhileMoving(ActorCommandHost& host, int actor_id) {
  return (host.StateFlags(actor_id) & kStateMovementActive) != 0
             ? kCommandWaiting
             : kCommandIdle;
}

// Opcode 13 in both states: one step of the queued offset, waiting while any
// x or z remains or the actor still moves (the y remainder is not tested).
std::uint8_t ContinueGroundOffset(ActorCommandHost& host, int actor_id,
                                  ActorCommand& command) {
  const MotionVector step = ConsumeActorMovementOffset(command);
  host.SetCommandArgs(actor_id, command.args);
  host.QueueGroundOffset(actor_id, step);
  const bool moving = (host.StateFlags(actor_id) & kStateMovementActive) != 0;
  return (command.args[0] != 0 || command.args[1] != 0 || moving)
             ? kCommandWaiting
             : kCommandIdle;
}

void AdvanceWaitingCommand(ActorCommandHost& host, int actor_id,
                           ActorCommand& command) {
  std::uint8_t state = command.state;
  switch (command.opcode) {
    case kOpWalkToTile:
    case kOpWalkFacing:
    case kOpJump:
    case kOpJumpAlternate:
    case kOpAirborneJump:
    case kOpNavigateToTile:
    case kOpNavigationJump:
      if ((host.StateFlags(actor_id) & kStateMovementActive) == 0) {
        state = kCommandIdle;
      }
      break;
    case kOpGroundOffset:
      state = ContinueGroundOffset(host, actor_id, command);
      break;
    case kOpTeleport:
      if ((host.StateFlags(actor_id) & kStateReacting) == 0) {
        state = kCommandIdle;
      }
      break;
    case kOpCombatPose:
    case kOpTileTravel:
    case kOpTimedTravel:
      if ((host.StateFlags(actor_id) & kStateTravelPose) == 0) {
        state = kCommandIdle;
      }
      break;
    default:
      host.ReportUnknownWaitingOpcode(actor_id, command.opcode);
      break;
  }
  host.SetCommandState(actor_id, state);
  if (host.ControlMode() == kFieldMovementControlMode) {
    host.UpdateMovement(actor_id);
  }
}

// Opcodes 15, 9 and 10: a navigation request at the command's speed, or the
// actor's gait speed, towards its halfword direction or target.
std::uint8_t NavigationCommand(ActorCommandHost& host, int actor_id,
                               const ActorCommand& command,
                               const CommandPlacement& position) {
  host.SetHeadTracking(actor_id, kHeadTrackDefault, std::nullopt,
                       kHeadStepDefault);
  const std::int16_t direction = command.Half(0);
  int speed = 0;
  bool faces_target = command.target < kCommandNoTarget;
  if (command.opcode == kOpNavigateToTile) {
    speed = command.Half(1) * kQ12One;
  } else {
    const CommandActorGait gait = host.Gait(actor_id);
    const bool runs = command.opcode == kOpWalkToTile
                          ? gait.gait_class == kGaitRunsTiles ||
                                gait.gait_class == kGaitRunsTilesFast
                          : gait.gait_class == kGaitRunsFacing ||
                                gait.gait_class == kGaitRunsFacingFast;
    speed = GaitSpeed(gait, runs);
    faces_target = faces_target && command.opcode == kOpWalkToTile;
  }
  const int facing =
      faces_target ? FacingTowards(host, command.target, position) : direction;
  host.QueueNavigationTarget(actor_id, direction, speed, facing);
  return WhileMoving(host, actor_id);
}

std::uint8_t BeginTileJumpCommand(ActorCommandHost& host, int actor_id,
                                  const ActorCommand& command,
                                  const CommandPlacement& position) {
  host.SetHeadTracking(actor_id, kHeadTrackDefault, std::nullopt,
                       kHeadStepDefault);
  // The tile's centre, held as halfwords as the original's target vector.
  const auto centre_x =
      static_cast<std::int16_t>(command.Byte(0) * kTileSize + kTileCentre);
  const auto centre_z =
      static_cast<std::int16_t>(command.Byte(2) * kTileSize + kTileCentre);
  const int facing = FacingAlong(centre_x - position.position.x,
                                 centre_z - position.position.z);
  CommandJump jump = CommandJump::kTile;
  if (command.opcode == kOpNavigationJump) {
    jump = CommandJump::kNavigation;
  } else if (command.opcode == kOpAirborneJump) {
    jump = CommandJump::kAirborne;
  }
  host.BeginJump(actor_id, jump, command.Tile(), command.args[1], facing);
  return kCommandWaiting;
}

std::uint8_t BeginTeleportCommand(ActorCommandHost& host, int actor_id,
                                  const ActorCommand& command,
                                  const CommandPlacement& position) {
  host.SetHeadTracking(actor_id, kHeadTrackDefault, std::nullopt,
                       kHeadStepDefault);
  int facing = position.position.w;
  if (command.target < kCommandNoTarget) {
    const CommandPlacement target = host.Placement(command.target);
    facing = FacingAlong((target.tile.x - command.Byte(0)) * kQ12One,
                         (target.tile.z - command.Byte(2)) * kQ12One);
  }
  host.BeginTeleport(actor_id, command.Tile(), facing);
  return kCommandWaiting;
}

std::uint8_t QueueFlightCommand(ActorCommandHost& host, int actor_id,
                                const ActorCommand& command,
                                const CommandPlacement& position,
                                std::uint8_t running_gait) {
  host.SetHeadTracking(actor_id, kHeadTrackDefault, std::nullopt,
                       kHeadStepDefault);
  const CommandActorGait gait = host.Gait(actor_id);
  const int speed = GaitSpeed(gait, gait.gait_class == running_gait);
  const Core::Math::Vec3 direction =
      Core::Math::NormalizeVector(
          {command.Half(0), command.Half(1), command.Half(2)})
          .direction;
  const MotionVector step{.x = static_cast<std::int16_t>(direction.x),
                          .y = static_cast<std::int16_t>(direction.y),
                          .z = static_cast<std::int16_t>(direction.z)};
  const int facing = command.target >= kCommandNoTarget
                         ? FacingAlong(step.x, step.z)
                         : FacingTowards(host, command.target, position);
  host.QueueFlightMovement(actor_id, step, speed, facing);
  return kCommandIdle;
}

std::uint8_t MoveToPositionCommand(ActorCommandHost& host, int actor_id,
                                   const ActorCommand& command,
                                   const CommandPlacement& position) {
  // Q12 world target less the position, kept to the halfword the step holds.
  const MotionVector step{.x = static_cast<std::int16_t>(
                              command.args[0] / kQ12One - position.position.x),
                          .y = static_cast<std::int16_t>(
                              command.args[2] / kQ12One - position.position.y),
                          .z = static_cast<std::int16_t>(
                              command.args[1] / kQ12One - position.position.z)};
  const int facing = command.target >= kCommandNoTarget
                         ? FacingAlong(step.x, step.z)
                         : FacingTowards(host, command.target, position);
  host.QueueMovementVector(actor_id, step, facing);
  return kCommandIdle;
}

std::uint8_t HoldFrameCommand(ActorCommandHost& host, int actor_id,
                              const ActorCommand& command,
                              const CommandPlacement& position, int step) {
  host.QueueNavigationTarget(actor_id, position.position.w, 0,
                             position.position.w);
  host.SetHeadTracking(actor_id, kHeadTrackFrame,
                       MotionVector{.y = command.Half(0)}, step);
  return kCommandIdle;
}

std::uint8_t TravelCommand(ActorCommandHost& host, int actor_id,
                           const ActorCommand& command,
                           const CommandPlacement& position, TravelPose pose) {
  int facing = 0;
  if (command.target < kCommandNoTarget) {
    facing = FacingTowards(host, command.target, position);
  } else if (pose == TravelPose::kTimed) {
    // The Q12 halfwords give whole units (-8 to 7) less the position, kept
    // to a halfword as the original's step.
    const auto step_x = static_cast<std::int16_t>(command.Half(0) / kQ12One -
                                                  position.position.x);
    const auto step_z = static_cast<std::int16_t>(command.Half(2) / kQ12One -
                                                  position.position.z);
    facing = FacingAlong(step_x, step_z);
  } else {
    // A byte over 4096 is zero: the step is the tile's negation.
    const auto step_x =
        static_cast<std::int16_t>(command.Byte(0) / kQ12One - position.tile.x);
    const auto step_z =
        static_cast<std::int16_t>(command.Byte(2) / kQ12One - position.tile.z);
    facing = FacingAlong(step_x, step_z);
  }
  host.BeginTravel(actor_id, pose, command.Vector(), facing);
  return kCommandWaiting;
}

std::uint8_t LimbActionCommand(ActorCommandHost& host, int actor_id,
                               const ActorCommand& command) {
  host.SetHeadTracking(actor_id, kHeadTrackDefault, std::nullopt,
                       kHeadStepDefault);
  const auto limb = static_cast<std::uint16_t>(command.Half(0));
  const auto slot = static_cast<std::uint16_t>(command.Half(1));
  std::uint16_t action = host.LimbActionId(actor_id, limb, slot).value_or(0);
  if (action == 0) {
    action = 1;
  }
  return host.BeginActionSelection(actor_id, action)
             ? kCommandActionPreviewMissing
             : kCommandIdle;
}

std::uint8_t StartCommand(ActorCommandHost& host, int actor_id,
                          ActorCommand& command,
                          const CommandPlacement& position) {
  const bool no_target = command.target >= kCommandNoTarget;
  switch (command.opcode) {
    case kOpFinish:
    case kOpFinishQuiet:
      return kCommandIdle;
    case kOpRestorePose:
      if (no_target) {
        host.RestoreNeutralPose(actor_id);
      } else {
        host.RestoreIdlePose(actor_id);
      }
      return kCommandIdle;
    case kOpTurnNavigation:
      host.SetHeadTracking(actor_id, kHeadTrackFrame, std::nullopt, 0);
      host.TurnNavigation(actor_id, command.Half(0));
      return kCommandIdle;
    case kOpTurnAtSpeed:
      host.SetHeadTracking(actor_id, kHeadTrackAim,
                           MotionVector{.y = command.Half(0)},
                           kHeadStepDefault);
      host.TurnAtSpeed(actor_id, command.Half(0), command.Half(1));
      return kCommandIdle;
    case kOpNavigateToTile:
    case kOpWalkToTile:
    case kOpWalkFacing:
      return NavigationCommand(host, actor_id, command, position);
    case kOpJump:
    case kOpJumpAlternate:
    case kOpAirborneJump:
    case kOpNavigationJump:
      return BeginTileJumpCommand(host, actor_id, command, position);
    case kOpGroundOffset:
      return ContinueGroundOffset(host, actor_id, command);
    case kOpTeleport:
      return BeginTeleportCommand(host, actor_id, command, position);
    case kOpFlight:
      return QueueFlightCommand(host, actor_id, command, position,
                                kGaitRunsFlight);
    case kOpFlightAlternate:
      return QueueFlightCommand(host, actor_id, command, position,
                                kGaitRunsVector);
    case kOpMoveToPosition:
      return MoveToPositionCommand(host, actor_id, command, position);
    case kOpHoldFrame:
      return HoldFrameCommand(host, actor_id, command, position,
                              command.Half(1));
    case kOpHoldFrameFull:
      return HoldFrameCommand(host, actor_id, command, position, kHeadStepFull);
    case kOpTimedTravel:
      return TravelCommand(host, actor_id, command, position,
                           TravelPose::kTimed);
    case kOpTileTravel:
      return TravelCommand(host, actor_id, command, position,
                           TravelPose::kTile);
    case kOpCombatPose:
      host.FinishCombatPose(actor_id, position.position.w);
      return kCommandWaiting;
    case kOpStopGround:
      if ((host.StateFlags(actor_id) & kStateMotionLocked) == 0) {
        host.QueueGroundOffset(actor_id, MotionVector{});
      }
      return kCommandIdle;
    case kOpLimbAction:
      return LimbActionCommand(host, actor_id, command);
    default:
      return kCommandIdle;
  }
}

int RunQueuedCommand(ActorCommandHost& host, int actor_id,
                     ActorCommand& command) {
  const CommandPlacement position = host.Placement(actor_id);
  if ((command.flags & kSoundMask) != 0) {
    host.PlayModelSound(actor_id, command.flags & kSoundMask);
  }
  switch ((command.flags >> kTintShift) & kTintMask) {
    case kTintRed:
      host.TintRed(actor_id);
      break;
    case kTintRestore:
      host.RestoreTint();
      break;
    default:
      break;
  }
  command.flags = static_cast<std::uint8_t>(command.flags & ~kTintBits);
  host.SetCommandFlags(actor_id, command.flags);

  host.SetCommandState(actor_id,
                       StartCommand(host, actor_id, command, position));
  if (host.ControlMode() == kFieldMovementControlMode) {
    host.UpdateMovement(actor_id);
    return 0;
  }
  return host.CheckTrapAndFallTriggers(actor_id, position);
}

int AxisInput(int value) {
  if (value > kAshleyStickDeadZone) {
    return value - kAshleyStickDeadZone;
  }
  if (value < -kAshleyStickDeadZone) {
    return value + kAshleyStickDeadZone;
  }
  return 0;
}
}  // namespace

std::int16_t ActorCommand::Half(unsigned index) const {
  constexpr unsigned kHalvesPerWord = 2;
  constexpr unsigned kHalfBits = 16;
  constexpr std::uint32_t kHalfMask = 0xFFFFU;
  const auto word = static_cast<std::uint32_t>(args.at(index / kHalvesPerWord));
  const unsigned shift = (index % kHalvesPerWord) * kHalfBits;
  return static_cast<std::int16_t>((word >> shift) & kHalfMask);
}

std::uint8_t ActorCommand::Byte(unsigned index) const {
  constexpr unsigned kBytesPerWord = 4;
  constexpr unsigned kByteBits = 8;
  constexpr std::uint32_t kByteMask = 0xFFU;
  const auto word = static_cast<std::uint32_t>(args.at(index / kBytesPerWord));
  const unsigned shift = (index % kBytesPerWord) * kByteBits;
  return static_cast<std::uint8_t>((word >> shift) & kByteMask);
}

PackedTile ActorCommand::Tile() const {
  enum TileByte : unsigned { kTileX, kTileLayer, kTileZ, kTileSpare };
  return {.x = Byte(kTileX),
          .layer = Byte(kTileLayer),
          .z = Byte(kTileZ),
          .spare = Byte(kTileSpare)};
}

MotionVector ActorCommand::Vector() const {
  enum VectorHalf : unsigned { kHalfX, kHalfY, kHalfZ, kHalfW };
  return {.x = Half(kHalfX),
          .y = Half(kHalfY),
          .z = Half(kHalfZ),
          .w = Half(kHalfW)};
}

MotionVector ConsumeActorMovementOffset(ActorCommand& command) {
  MotionVector step;
  step.x = ConsumeAxis(command.args[0]);
  step.z = ConsumeAxis(command.args[1]);
  step.y = ConsumeAxis(command.args[2]);
  return step;
}

int ExecuteQueuedActorCommands(ActorCommandHost& host) {
  for (int actor_id = 0; actor_id < kCommandActorSlots; ++actor_id) {
    std::optional<ActorCommand> command = host.Command(actor_id);
    if (!command || (host.StateFlags(actor_id) & kStateActionAnimating) != 0) {
      continue;
    }
    if (std::ranges::find(kBattleCommandActorKinds, host.Kind(actor_id)) ==
            kBattleCommandActorKinds.end() &&
        host.ControlMode() != kFieldMovementControlMode) {
      continue;
    }
    if (command->state == kCommandWaiting) {
      AdvanceWaitingCommand(host, actor_id, *command);
      continue;
    }
    if (command->state != kCommandQueued) {
      continue;
    }
    const int result = RunQueuedCommand(host, actor_id, *command);
    if (result != 0) {
      return result;
    }
  }
  return 0;
}

std::optional<AshleyMoveInput> AshleyStickMoveInput(AshleyStick stick) {
  const int x = stick.x - kAshleyStickCentre;
  const int y = stick.y - kAshleyStickCentre;
  const auto outside = [](int value) {
    return value < -kAshleyStickDeadZone || value > kAshleyStickDeadZone;
  };
  if (!outside(x) && !outside(y)) {
    return std::nullopt;
  }
  return AshleyMoveInput{.lateral = -AxisInput(x), .forward = AxisInput(y)};
}

AshleyMoveInput AshleyDirectionPadMoveInput(AshleyDirectionPad pad) {
  AshleyMoveInput input;
  if (pad.down) {
    input.forward += kAshleyDirectionPadInput;
  }
  if (pad.up) {
    input.forward -= kAshleyDirectionPadInput;
  }
  if (pad.left) {
    input.lateral += kAshleyDirectionPadInput;
  }
  if (pad.right) {
    input.lateral -= kAshleyDirectionPadInput;
  }
  return input;
}

int ClampAshleyCameraDistance(int distance) {
  return std::clamp(distance, kMinimumCameraDistance, kMaximumCameraDistance);
}

int AshleyMoveDirection(AshleyMoveInput input, int camera_yaw) {
  return (Core::Math::Arctangent2(input.lateral, input.forward) + camera_yaw +
          kFacingHalfTurn) &
         kFacingMask;
}

void UpdateAshleyMovementInput(AshleyMovementHost& host) {
  host.ConstrainCameraToPlayer();
  const CommandActorGait gait = host.PlayerGait();
  const std::uint32_t state_flags = host.PlayerStateFlags();
  const unsigned step = host.ScreenTransitionStep();
  if (std::ranges::find(kDoorWalkSteps, step) != kDoorWalkSteps.end()) {
    const std::optional<int> elevation = host.DoorEntryElevation();
    if (elevation) {
      constexpr int kElevationShift = 10;
      host.SetDirectMovement(*elevation << kElevationShift,
                             gait.walk_speed * kQ12One / 2);
    } else {
      host.SetDirectMovement(0, 0);
    }
    return;
  }

  host.UpdateCameraOrbit();
  const AshleyControls controls = host.Controls();
  AshleyMoveInput input;
  if (controls.stick) {
    input = AshleyStickMoveInput(*controls.stick).value_or(AshleyMoveInput{});
  }
  if (input == AshleyMoveInput{}) {
    input = AshleyDirectionPadMoveInput(controls.held);
  }
  host.SetMoveInput(input);
  host.SetCameraDistance(ClampAshleyCameraDistance(host.CameraDistance()));

  const bool walk_only = (state_flags & kStateActionPose) != 0;
  if (input != AshleyMoveInput{}) {
    const int direction = AshleyMoveDirection(input, host.CameraYaw());
    const int walk = gait.walk_speed * kQ12One;
    const int run = gait.run_speed * kQ12One;
    if (walk_only) {
      host.SetDirectMovement(direction, walk);
    } else if (controls.walk_held) {
      if (controls.jump_pressed) {
        host.BeginJumpMovement(direction, walk);
      } else {
        host.SetDirectMovement(direction, walk);
      }
    } else if (controls.jump_pressed) {
      host.BeginJumpMovement(direction, run);
    } else {
      host.SetDirectMovement(direction, run);
    }
  } else if (!walk_only && controls.jump_pressed) {
    host.BeginJumpMovement(AshleyMoveDirection({}, host.CameraYaw()), 0);
  } else {
    host.SetDirectMovement(Core::Math::Arctangent2(0, 0) & kFacingMask, 0);
  }
}
}  // namespace VS::Actors
