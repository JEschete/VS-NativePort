// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/Decisions.hpp"

#include <algorithm>

#include "Core/Math/ScalarMath.hpp"

namespace VS::Battle::Ai {
namespace {
constexpr unsigned kActionStatusBits = 31;
// Statuses 0 and 1 approach the best candidate; status 0 also caps the
// approach distance at the candidate's current distance.
constexpr unsigned kApproachStatusMask = 3;
constexpr unsigned kCappedApproachBit = 1;
constexpr int kKeepWalkingCommand = 1;
constexpr int kStopCommand = 2;
constexpr int kWalkingBehaviorMode = 1;
constexpr int kAshleyActor = 0;

int FleeOrWander(DecisionActor& actor) {
  if (actor.FleeVisibleEnemies() == 0) {
    actor.WanderAroundSavedCenter();
  }
  return 1;
}
}  // namespace

// decomp: func_800DAA6C (src/BATTLE/BATTLE.PRG/71618.c:944).
int UpdatePatrolCombatBehavior(DecisionActor& actor) {
  if (actor.Fleeing()) {
    return FleeOrWander(actor);
  }
  if (actor.UpdateCombatPursuit() == 0) {
    actor.PatrolWaypoints();
  }
  return 1;
}

// decomp: BATTLE.PRG entry 0x800DBA4C (src/BATTLE/BATTLE.PRG/72B70.s, after
// .L800DBA40).
int UpdateFleeOrWanderBehavior(DecisionActor& actor) {
  if (!actor.Fleeing()) {
    return actor.UpdateCombatPursuit();
  }
  return FleeOrWander(actor);
}

// decomp: BATTLE.PRG entry 0x800DBA88 (src/BATTLE/BATTLE.PRG/72B70.s, inside
// func_800DB93C).
int UpdateAdaptivePursuitBehavior(DecisionActor& actor) {
  if (actor.Fleeing()) {
    return actor.FleeVisibleEnemies();
  }
  const unsigned mode =
      (1U << (actor.ActionStatus() & kActionStatusBits)) & kApproachStatusMask;
  if (mode == 0) {
    return actor.UpdateCombatPursuit();
  }
  const std::optional<PursuitCandidate> candidate = actor.BestCandidate();
  if (!candidate) {
    return 0;
  }
  int distance = candidate->extended_reach ? kExtendedPursuitDistance
                                           : kDefaultPursuitDistance;
  if ((mode & kCappedApproachBit) != 0) {
    distance = std::min(
        distance, VS::Core::Math::SquareRootInteger(
                      static_cast<std::int32_t>(candidate->distance_squared)));
  }
  const int result = actor.ApproachActor(candidate->target, distance);
  return result != 0 ? result : actor.TrackActorPosition(candidate->target);
}

// decomp: BATTLE.PRG entry 0x800DBB98 (src/BATTLE/BATTLE.PRG/72B70.s, after
// .L800DBB80).
int UpdateSupportFollowBehavior(DecisionActor& actor) {
  const int result = actor.UpdateCombatPursuit();
  if (result != 0) {
    return result;
  }
  const std::optional<SupportTarget> ally = actor.FindSupportTarget();
  if (!ally) {
    return 0;
  }
  if (ally->distance_squared > kSupportDistanceSquared) {
    actor.MoveToTile(ally->tile);
  } else {
    actor.WanderAroundTile(ally->tile, kSupportWanderRadius);
  }
  return 1;
}

// decomp: BATTLE.PRG D_800DBC0C (src/BATTLE/BATTLE.PRG/72B70.s:681).
int UpdateEngagementApproachBehavior(DecisionActor& actor) {
  if (actor.EngagementReady()) {
    const int pursuit = actor.UpdateCombatPursuit();
    if (pursuit != 0) {
      return pursuit;
    }
  }
  const int result = actor.ApproachActor(kAshleyActor, kDefaultPursuitDistance);
  return result != 0 ? result : actor.TrackActorPosition(kAshleyActor);
}

// decomp: BATTLE.PRG func_800DB7B4 (src/BATTLE/BATTLE.PRG/72B70.s:357).
void UpdateActorLocomotionMode(DecisionActor& actor) {
  const int mode = actor.BehaviorMode();
  if (mode == 0) {
    return;
  }
  if (mode == kWalkingBehaviorMode && actor.CommandIsLocomotion()) {
    actor.IssueCommand(kKeepWalkingCommand);
    return;
  }
  actor.ReleaseReservations();
  actor.ClearBehaviorMode();
  actor.IssueCommand(kStopCommand);
}

// decomp: BATTLE.PRG func_800DB820 (src/BATTLE/BATTLE.PRG/72B70.s:390).
void DispatchMovementBehavior(DecisionActor& actor) {
  if (actor.TryExecutePlannedActions() || actor.TryRecoverBlockedMovement() ||
      actor.RunDecisionBehavior()) {
    return;
  }
  actor.WanderAroundSavedCenter();
}
}  // namespace VS::Battle::Ai
