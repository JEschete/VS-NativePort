// Hand-maintained (tools/port/owned.csv).
// C counterparts: rood-boi pc/src/battle_motion.c:64-85,112-123,
// revision 3be5ba38b008cad919a2b1ca196432c8d4817335 (CC0-1.0).
#include "Actors/MotionRequests.hpp"

#include <bit>

namespace VS::Actors {
namespace {
void Jump(MotionActor& actor, JumpEntry entry, PackedTile target, int speed,
          int facing) {
  actor.JumpToTile(entry, target, speed, NarrowFacing(facing));
}

void Travel(MotionActor& actor, MotionVector route, int facing,
            TravelPose pose) {
  route.w = NarrowFacing(facing);
  actor.StartTravel(route, pose);
}
}  // namespace

std::int16_t NarrowFacing(int facing) {
  return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(facing));
}

// decomp: BATTLE.PRG func_800ACF54 (src/BATTLE/BATTLE.PRG/40564.c:2548)
GroundedHistory SaveActorGroundedTransform(const GroundedPose& pose) {
  GroundedHistory history{.previous_tile = pose.tile,
                          .position = pose.position,
                          .previous_facing = pose.facing,
                          .saved_placement = std::nullopt};
  if (pose.support_layer == 0) {
    history.saved_placement = pose.tile;
  }
  return history;
}

// decomp: BATTLE.PRG func_800B63AC (src/BATTLE/BATTLE.PRG/4A0A8.c:340)
void BeginTileJump(MotionActor& actor, PackedTile target, int speed,
                   int facing) {
  if (actor.Jumping()) {
    BeginAirborneTileJump(actor, target, speed, facing);
  } else if (actor.MotionLocked()) {
    BeginNavigationJump(actor, target, speed, facing);
  } else {
    actor.SetJumpProfile(kPlainJumpProfile);
    Jump(actor, JumpEntry::kPlain, target, speed, facing);
  }
}

// decomp: BATTLE.PRG func_800B6404 (src/BATTLE/BATTLE.PRG/4A0A8.c:368)
void BeginAirborneTileJump(MotionActor& actor, PackedTile target, int speed,
                           int facing) {
  actor.SetJumpFlags(false, true);
  Jump(actor, JumpEntry::kAirborne, target, speed, facing);
}

// decomp: BATTLE.PRG func_800B6438 (src/BATTLE/BATTLE.PRG/4A0A8.c:383)
void BeginNavigationJump(MotionActor& actor, PackedTile target, int speed,
                         int facing) {
  actor.SetJumpFlags(true, true);
  actor.PlayAnimation(kNavigationJumpAnimation, kNavigationJumpBlendFrames);
  Jump(actor, JumpEntry::kNavigation, target, speed, facing);
}

// decomp: BATTLE.PRG func_800B6580 (src/BATTLE/BATTLE.PRG/4A0A8.c:490)
void BeginTeleport(MotionActor& actor, PackedTile target, int facing) {
  // The route keeps the tile word, clears Z and carries the facing.
  const MotionVector route{
      .x = std::bit_cast<std::int16_t>(std::array{target.x, target.layer}),
      .y = std::bit_cast<std::int16_t>(std::array{target.z, target.spare}),
      .z = 0,
      .w = NarrowFacing(facing)};
  for (MotionActor* current = &actor;
       current != nullptr && !current->Reacting();
       current = current->TeleportCompanion()) {
    current->StartTeleport(route);
  }
}

// decomp: BATTLE.PRG func_800B65E8 (src/BATTLE/BATTLE.PRG/4A0A8.c:522)
void BeginTileTravel(MotionActor& actor, const MotionVector& target,
                     int facing) {
  Travel(actor, target, facing, TravelPose::kTile);
}

// decomp: BATTLE.PRG func_800B65F4 (src/BATTLE/BATTLE.PRG/4A0A8.c:531)
void BeginTimedTravel(MotionActor& actor, const MotionVector& target,
                      int facing) {
  Travel(actor, target, facing, TravelPose::kTimed);
}

// decomp: BATTLE.PRG func_800B6500 (src/BATTLE/BATTLE.PRG/4A0A8.c:445)
void QueueGroundOffset(MotionActor& actor, const MotionVector& step) {
  actor.QueueGroundOffset(step);
}

// decomp: BATTLE.PRG func_800B6530 (src/BATTLE/BATTLE.PRG/4A0A8.c:459)
void QueueMovementVector(MotionActor& actor, const MotionVector& step,
                         int facing) {
  actor.QueueMovement({.mode = MotionMode::kVector,
                       .step = step,
                       .distance = static_cast<std::uint32_t>(facing),
                       .facing = NarrowFacing(facing)});
}

// decomp: BATTLE.PRG func_800B653C (src/BATTLE/BATTLE.PRG/4A0A8.c:467)
void QueueFlightMovement(MotionActor& actor, const MotionVector& step,
                         int speed, int facing) {
  actor.QueueMovement({.mode = MotionMode::kFlight,
                       .step = step,
                       .distance = static_cast<std::uint32_t>(speed),
                       .facing = NarrowFacing(facing)});
}

// decomp: BATTLE.PRG func_800B6634 (src/BATTLE/BATTLE.PRG/4A0A8.c:551)
void TurnNavigationActor(MotionActor& actor, int facing) {
  actor.TurnAtRate(NarrowFacing(facing),
                   actor.CollisionRadius() < kLargeActorRadius
                       ? kSmallTurnRate
                       : kLargeTurnRate);
  actor.PlayTurnAnimation(facing);
}

// decomp: BATTLE.PRG func_800B668C (src/BATTLE/BATTLE.PRG/4A0A8.c:578)
bool TurnActorAtSpeed(MotionActor& actor, std::int16_t facing, int speed) {
  if (speed == 0) return false;
  actor.TurnAtAngularSpeed(facing, speed);
  actor.PlayTurnAnimation(facing);
  return true;
}
}  // namespace VS::Actors
