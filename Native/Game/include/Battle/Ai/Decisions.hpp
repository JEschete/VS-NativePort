#pragma once
// Hand-maintained (tools/port/owned.csv).

#include <cstdint>
#include <optional>

// The battle AI decision behaviours an actor runs once per movement step, the
// dispatcher that picks between planned actions, recovery and the behaviour,
// and the locomotion mode update that follows it. The battle runtime owns the
// actor AI state; these policies see an actor only through the host below, so
// they reach no AI state record, PS1 address or callback table.
namespace VS::Battle::Ai {
// Entries in the BATTLE.PRG decision behaviour table (D_800F1790).
inline constexpr unsigned kDecisionBehaviorCount = 9;
inline constexpr int kDefaultPursuitDistance = 256;
inline constexpr int kExtendedPursuitDistance = 320;
// Beyond this squared distance a supporting actor walks to its ally's tile
// rather than wandering around it.
inline constexpr std::uint32_t kSupportDistanceSquared = 0x64000;
inline constexpr int kSupportWanderRadius = 5;

// The best-ranked action candidate an adaptive pursuit approaches.
struct PursuitCandidate {
  int target = 0;
  std::uint32_t distance_squared = 0;
  // The actor's model reaches further (model flag bit 5).
  bool extended_reach = false;
};

// The ally a supporting actor follows.
struct SupportTarget {
  unsigned tile = 0;
  std::uint32_t distance_squared = 0;
};

// One actor's decision state and the movement primitives a behaviour issues.
// Integer results follow the original: nonzero means the step was handled.
class DecisionActor {
 public:
  virtual ~DecisionActor() = default;
  // The actor is fleeing (AI state byte 0x0E).
  [[nodiscard]] virtual bool Fleeing() const = 0;
  // The action planner's status code (AI state byte 0x131).
  [[nodiscard]] virtual unsigned ActionStatus() const = 0;
  virtual int FleeVisibleEnemies() = 0;
  virtual void WanderAroundSavedCenter() = 0;
  virtual int UpdateCombatPursuit() = 0;
  virtual void PatrolWaypoints() = 0;
  // Nothing when the actor has no ranked candidate or no model.
  [[nodiscard]] virtual std::optional<PursuitCandidate> BestCandidate()
      const = 0;
  virtual int ApproachActor(int target, int distance) = 0;
  virtual int TrackActorPosition(int target) = 0;
  // Nothing when no ally is in range of the actor relation table.
  virtual std::optional<SupportTarget> FindSupportTarget() = 0;
  virtual void MoveToTile(unsigned tile) = 0;
  virtual void WanderAroundTile(unsigned tile, int radius) = 0;
  [[nodiscard]] virtual bool EngagementReady() = 0;

  [[nodiscard]] virtual int BehaviorMode() const = 0;
  virtual void ClearBehaviorMode() = 0;
  // The actor's current command keeps it walking (opcode below 9).
  [[nodiscard]] virtual bool CommandIsLocomotion() const = 0;
  virtual void ReleaseReservations() = 0;
  virtual void IssueCommand(int command) = 0;

  virtual bool TryExecutePlannedActions() = 0;
  virtual bool TryRecoverBlockedMovement() = 0;
  // Runs the actor's decision behaviour; false when it has none or the
  // behaviour declined the step.
  virtual bool RunDecisionBehavior() = 0;
};

// Flees visible enemies (wandering when none are seen) or pursues and
// otherwise patrols waypoints (func_800DAA6C).
int UpdatePatrolCombatBehavior(DecisionActor& actor);
// Flees visible enemies (wandering when none are seen) or pursues (0x800DBA4C).
int UpdateFleeOrWanderBehavior(DecisionActor& actor);
// Flees, or for action statuses 0 and 1 approaches the best candidate within
// the model's reach (status 0 at most its current distance) and otherwise
// pursues (0x800DBA88).
int UpdateAdaptivePursuitBehavior(DecisionActor& actor);
// Pursues, or follows the nearest ally: walks to it from afar and wanders
// around it when close (0x800DBB98).
int UpdateSupportFollowBehavior(DecisionActor& actor);
// Pursues once engagement is ready, otherwise approaches Ashley
// (0x800DBC0C).
int UpdateEngagementApproachBehavior(DecisionActor& actor);
// Keeps a walking actor moving (command 1) or releases its reservations and
// stops it (command 2) when its behaviour mode ends (func_800DB7B4).
void UpdateActorLocomotionMode(DecisionActor& actor);
// Runs planned actions, blocked-movement recovery or the decision behaviour,
// in that order, and wanders when none handles the step (func_800DB820).
void DispatchMovementBehavior(DecisionActor& actor);

// The five-bit decision behaviour index in an actor's spawn settings word
// (bits 4-8).
[[nodiscard]] constexpr unsigned DecisionBehaviorIndex(
    std::uint32_t spawn_settings) {
  constexpr unsigned kIndexShift = 4;
  constexpr std::uint32_t kIndexMask = 31;
  return static_cast<unsigned>((spawn_settings >> kIndexShift) & kIndexMask);
}

// The decision behaviour table slot the spawn settings select, or nothing
// when the index is past the nine-entry table (#272).
[[nodiscard]] constexpr std::optional<unsigned> DecisionBehaviorSlot(
    std::uint32_t spawn_settings) {
  const unsigned index = DecisionBehaviorIndex(spawn_settings);
  if (index >= kDecisionBehaviorCount) {
    return std::nullopt;
  }
  return index;
}
}  // namespace VS::Battle::Ai
