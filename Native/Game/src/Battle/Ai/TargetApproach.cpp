// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/TargetApproach.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "Battle/Ai/ActorCommands.hpp"
#include "Battle/Ai/DecisionPrimitives.hpp"
#include "Core/Diagnostics.h"
#include "Core/Math/Distance.hpp"
#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr std::uint32_t kTileByteMask = 0xFF;
constexpr unsigned kTileZShift = 16;
constexpr std::uint32_t kTileXZMask = 0xFF00FF;
constexpr int kTileShift = 7;
constexpr int kTileCentre = 64;
// Model positions in 64-unit steps, the target's half tiles.
constexpr int kHalfTileShift = 6;
constexpr std::uint32_t kAngleMask = 0xFFF;

constexpr int kCloseApproachOption = 1;
// A target unchanged for more than this many calls restarts the approach.
constexpr std::uint16_t kRetargetCalls = 180;

// The range mode's hysteresis: far until within range - 96, close within
// range - 128 and back to in range beyond range - 32.
constexpr std::uint8_t kFar = 0;
constexpr std::uint8_t kInRange = 1;
constexpr std::uint8_t kClose = 2;
constexpr std::int32_t kFarMargin = 96;
constexpr std::int32_t kCloseMargin = 128;
constexpr std::int32_t kLeaveCloseMargin = 32;

// The approach's projected destination needs the "hold distance" flag.
constexpr std::uint16_t kHoldDistanceFlag = 8;
constexpr int kWalkLocomotion = 0;
constexpr int kStepLocomotion = 1;
constexpr std::uint8_t kChaseCalls = 255;
constexpr auto kFirstWalkingOpcode = std::to_underlying(CommandOpcode::kWalk);

// The awareness marker a lost target shows, and for how long.
constexpr int kLostTargetMarker = 7;
constexpr int kLostTargetFrames = 40;
constexpr int kHesitationChance = 128;
constexpr int kLookAroundKind = 1;
constexpr int kWaitKind = 3;
constexpr int kApproachSlot = 1;
constexpr int kFacingSlot = 6;

std::nullopt_t Refuse(const char* message) {
  Core_Warnf("ai", "%s", message);
  return std::nullopt;
}

bool IsActorSlot(int actor_id) {
  return actor_id >= 0 && actor_id < kTargetApproachActorSlots;
}

int TileX(std::uint32_t tile) { return static_cast<int>(tile & kTileByteMask); }

int TileZ(std::uint32_t tile) {
  return static_cast<int>((tile >> kTileZShift) & kTileByteMask);
}

bool SameTile(std::uint32_t first, std::uint32_t second) {
  return (first & kTileXZMask) == (second & kTileXZMask);
}

// The original's subu: the 32-bit difference, wrapping.
std::int32_t Difference(std::int32_t first, std::int32_t second) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(first) -
                                   static_cast<std::uint32_t>(second));
}

// (value - margin) squared, the low 32 bits of the original's mult.
std::uint32_t SquaredLess(std::int32_t value, std::int32_t margin) {
  const auto offset =
      static_cast<std::uint32_t>(value) - static_cast<std::uint32_t>(margin);
  return offset * offset;
}

std::uint32_t RelationOf(const TargetApproacher& approacher, int target) {
  return approacher.relations.at(static_cast<std::size_t>(target));
}

std::uint32_t DistanceOf(const TargetApproacher& approacher, int target) {
  return approacher.distances_squared.at(static_cast<std::size_t>(target));
}

// Whether the target left the tile the approach last saw it on, or moved its
// destination.
bool TargetMoved(const TargetApproacher& approacher,
                 const ApproachTargetModel& model) {
  if (model.moving) {
    return TileX(approacher.destination) !=
               (approacher.target_half_tile_x >> 1) ||
           TileZ(approacher.destination) !=
               (approacher.target_half_tile_z >> 1);
  }
  return (model.x >> kHalfTileShift) != approacher.target_half_tile_x ||
         (model.z >> kHalfTileShift) != approacher.target_half_tile_z;
}

// The range mode for the target's squared distance; the reachable bit must be
// set.
std::uint8_t NextRangeMode(const TargetApproacher& approacher,
                           std::uint32_t distance) {
  const std::int32_t range = approacher.range;
  switch (approacher.range_mode) {
    case kFar:
      return SquaredLess(range, kFarMargin) < distance ? kFar : kInRange;
    case kInRange:
      if (SquaredLess(range, 0) < distance) return kFar;
      return SquaredLess(range, kCloseMargin) >= distance ? kClose : kInRange;
    case kClose:
      return SquaredLess(range, kLeaveCloseMargin) < distance ? kInRange
                                                              : kClose;
    default:
      return approacher.range_mode;
  }
}

void Recover(TargetApproachWorld& world, TargetApproacher& approacher,
             int target) {
  approacher.movement_target = static_cast<std::uint8_t>(target);
  world.RecoverMovement(approacher);
}

// A far target: walk or run toward it. An actor holding its distance projects
// its destination back toward itself by the range less 96, and walks straight
// to the original destination when that lands on its own tile.
void ApproachFarTarget(TargetApproachWorld& world, TargetApproacher& approacher,
                       int option, int target) {
  if (option == kCloseApproachOption) approacher.close_approach = 1;
  approacher.facing_locked = 1;
  approacher.gait =
      IsAiTargetBeyondNearRadius(DistanceOf(approacher, target)) ? 1 : 0;
  if ((approacher.movement_flags & kHoldDistanceFlag) != 0) {
    const std::uint32_t original = approacher.destination;
    const int delta_x = approacher.x -
                        (TileX(approacher.destination) << kTileShift) -
                        kTileCentre;
    const int delta_z = approacher.z -
                        (TileZ(approacher.destination) << kTileShift) -
                        kTileCentre;
    const int angle = static_cast<int>(
        static_cast<std::uint32_t>(Core::Math::Arctangent2(delta_x, delta_z)) &
        kAngleMask);
    const std::uint32_t squared =
        Core::Math::SquaredVectorLength(delta_x, 0, delta_z);
    approacher.saved_destination = original;
    const auto distance = static_cast<std::uint32_t>(
        Core::Math::SquareRootInteger(static_cast<std::int32_t>(squared)));
    const std::uint32_t margin = static_cast<std::uint32_t>(approacher.range) -
                                 static_cast<std::uint32_t>(kFarMargin);
    const std::uint32_t magnitude =
        static_cast<std::int32_t>(margin) >= 0 ? margin : 0U - margin;
    world.ProjectClearRadialDestination(approacher, approacher.tile, angle,
                                        distance - magnitude);
    if (SameTile(approacher.destination, approacher.tile)) {
      approacher.approach_directly = 1;
      approacher.destination = original;
    }
  }
  approacher.movement_target = approacher.target_id;
  world.DispatchLocomotion(approacher, kWalkLocomotion, 0);
}

// A close target: step onto its position, or its point. Returns false without
// a battle actor.
bool CloseInOnTarget(TargetApproachWorld& world, TargetApproacher& approacher,
                     const ApproachTargetModel& model, int target) {
  approacher.height_filter = 1;
  world.ClearWalkTests();
  approacher.near_target_detour = 1;
  approacher.move_x = model.x;
  approacher.move_z = model.z;
  if (approacher.approaches_target_point != 0) {
    approacher.gait = 1;
    approacher.toward_point = 1;
    approacher.target_x = static_cast<std::int16_t>(approacher.move_x);
    approacher.target_z = static_cast<std::int16_t>(approacher.move_z);
  } else {
    approacher.movement_target = static_cast<std::uint8_t>(target);
  }
  const int angle =
      Core::Math::Arctangent2(Difference(approacher.move_x, approacher.x),
                              Difference(approacher.move_z, approacher.z));
  world.DispatchLocomotion(approacher, kStepLocomotion, angle);
  const auto opcode = world.CommandOpcode();
  if (!opcode) return false;
  if (*opcode >= kFirstWalkingOpcode) approacher.walked_close = 1;
  return true;
}

// Looks around for a lost target: true while the actor keeps hesitating.
bool LookForLostTarget(TargetApproachWorld& world,
                       TargetApproacher& approacher) {
  world.SetTemporaryAwareness(approacher, kLostTargetMarker, kLostTargetFrames);
  return world.HesitationTurn(approacher, kHesitationChance, kLookAroundKind,
                              kApproachSlot) != 0;
}

// The approach toward an active target. Returns false without a battle
// actor.
bool ApproachActiveTarget(TargetApproachWorld& world,
                          TargetApproacher& approacher,
                          const ApproachTargetModel& model, int option,
                          int target) {
  approacher.chasing_lost_target = 0;
  approacher.range_mode =
      (RelationOf(approacher, target) & kApproachRelationReachable) != 0
          ? NextRangeMode(approacher, DistanceOf(approacher, target))
          : kFar;
  if ((approacher.range_mode != kClose && approacher.walked_close != 0) ||
      (approacher.range_mode != kFar && approacher.recover_in_range != 0)) {
    Recover(world, approacher, target);
    return true;
  }
  if (option == 0 && approacher.range_mode == kClose) {
    approacher.range_mode = kInRange;
  }
  switch (approacher.range_mode) {
    case kFar:
      ApproachFarTarget(world, approacher, option, target);
      return true;
    case kInRange:
      Recover(world, approacher, target);
      return true;
    case kClose:
      return CloseInOnTarget(world, approacher, model, target);
    default:
      return true;
  }
}

// The approach toward a target no longer active: start or follow a chase to
// its last tile, or look for it. Returns whether the actor is still
// approaching.
bool ApproachLostTarget(TargetApproachWorld& world,
                        TargetApproacher& approacher, int target) {
  if (approacher.chasing_lost_target == 0 &&
      (RelationOf(approacher, target) & kApproachRelationWasActive) != 0) {
    if (approacher.chases_lost_target == 0) {
      if ((RelationOf(approacher, target) & kApproachRelationVisible) != 0) {
        return false;
      }
      return LookForLostTarget(world, approacher);
    }
    approacher.chasing_lost_target = 1;
    approacher.chase_timeout = kChaseCalls;
    approacher.chase_destination =
        (approacher.chase_destination & ~kTileXZMask) |
        (static_cast<std::uint32_t>(approacher.target_half_tile_z >> 1)
         << kTileZShift) |
        static_cast<std::uint32_t>(approacher.target_half_tile_x >> 1);
  }
  if (approacher.walked_close != 0) {
    Recover(world, approacher, target);
    return true;
  }
  if (approacher.chasing_lost_target == 0) {
    return world.HesitationTurn(approacher, 0, kWaitKind, kApproachSlot) != 0;
  }
  approacher.destination = approacher.chase_destination;
  if (approacher.chase_timeout != 0) --approacher.chase_timeout;
  if (SameTile(approacher.destination, approacher.tile)) {
    if ((RelationOf(approacher, target) & kApproachRelationVisible) != 0) {
      return false;
    }
    if (LookForLostTarget(world, approacher)) return true;
  } else if (approacher.chase_timeout != 0) {
    approacher.gait = 1;
    approacher.facing_locked = 1;
    world.DispatchLocomotion(approacher, kWalkLocomotion, 0);
    return true;
  }
  approacher.chasing_lost_target = 0;
  return false;
}
}  // namespace

std::optional<bool> UpdateTargetApproachMovement(TargetApproachWorld& world,
                                                 TargetApproacher& approacher,
                                                 int option) {
  const int target = approacher.target_id;
  if (target == approacher.actor_id) {
    world.IssueCommand(approacher, CommandIntent::kStand);
    world.RefreshTargetTile(approacher);
    return true;
  }
  if (!IsActorSlot(target)) {
    return Refuse("AI target approach requires a target in the actor slots");
  }
  const auto model = world.TargetModel(target);
  if (!model) return Refuse("AI target approach requires the target's model");
  if (approacher.flies) option = kCloseApproachOption;

  bool restart = TargetMoved(approacher, *model) ||
                 approacher.range != approacher.previous_range ||
                 target != approacher.previous_target_id ||
                 option != approacher.previous_option;
  if (!restart) {
    // The original's post-increment, evaluated only when nothing else
    // restarted the approach.
    const std::uint16_t calls = approacher.retarget_timer;
    approacher.retarget_timer = static_cast<std::uint16_t>(calls + 1);
    restart = calls > kRetargetCalls;
  }
  if (restart) {
    approacher.retarget_timer = 0;
    approacher.walked_close = 0;
    approacher.recover_in_range = 0;
  }
  approacher.previous_option = static_cast<std::uint8_t>(option);
  approacher.previous_target_id = static_cast<std::uint8_t>(target);
  approacher.previous_range = approacher.range;

  bool approaching = true;
  if ((RelationOf(approacher, target) & kApproachRelationActive) != 0) {
    if (!ApproachActiveTarget(world, approacher, *model, option, target)) {
      return Refuse("AI target approach requires a battle actor");
    }
  } else {
    approaching = ApproachLostTarget(world, approacher, target);
  }
  world.RefreshTargetTile(approacher);
  return approaching;
}

std::optional<bool> UpdateTargetFacing(TargetApproachWorld& world,
                                       TargetApproacher& approacher,
                                       int target) {
  if (target == approacher.actor_id) {
    world.IssueCommand(approacher, CommandIntent::kStand);
    return true;
  }
  if (approacher.facing_delay == 0) return false;
  if (approacher.reaction_pending != 0) {
    if (world.HesitationTurn(approacher, kHesitationChance, kWaitKind,
                             kFacingSlot) != 0) {
      return true;
    }
    approacher.facing_pending = 0;
    return false;
  }
  if (!IsActorSlot(target)) {
    return Refuse("AI target facing requires a target in the actor slots");
  }
  const int angle =
      Core::Math::Arctangent2(Difference(approacher.x, approacher.move_x),
                              Difference(approacher.z, approacher.move_z));
  const bool visible =
      (RelationOf(approacher, target) & kApproachRelationVisible) != 0;
  if (!SameTile(approacher.destination, approacher.previous_facing_tile) &&
      visible && approacher.facing_pending == 0) {
    approacher.facing_pending = 1;
    approacher.angle = static_cast<std::int16_t>(angle);
    approacher.previous_facing_tile = approacher.destination;
  }
  if (approacher.facing_pending == 0) return false;
  approacher.turn_timer = approacher.facing_delay;
  const auto facing = static_cast<std::uint32_t>(approacher.model_facing +
                                                 approacher.model_look_yaw) &
                      kAngleMask;
  if (facing == (static_cast<std::uint32_t>(approacher.angle) & kAngleMask)) {
    if (!visible) {
      if (world.HesitationTurn(approacher, kHesitationChance, kLookAroundKind,
                               kFacingSlot) != 0) {
        return true;
      }
      approacher.facing_pending = 0;
      return false;
    }
    approacher.facing_pending = 1;
    approacher.angle = static_cast<std::int16_t>(
        static_cast<std::uint32_t>(angle) & kAngleMask);
    approacher.previous_facing_tile = approacher.destination;
  }
  world.IssueCommand(approacher, CommandIntent::kTurn);
  return true;
}
}  // namespace VS::Battle::Ai
