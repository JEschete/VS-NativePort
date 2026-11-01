#ifndef NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_
#define NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_

#include <optional>
#include <vector>

namespace VS::Battle::Ai {

// The queued action the plans react to (func_800DEEA4_t at
// BATTLE/BATTLE.PRG/74088.c:388): its primary target's actor and that target's
// targets-position word.
struct PlanningRequest {
  int primary_target = 0;
  int targets_position = 0;
};

// One battle actor as the planner sees it on a walk of the live actor list.
// `slot` indexes the actor AI state table; `actor_id` is the AI state's own id.
struct PlanningActor {
  int slot = 0;
  int actor_id = 0;
  int hp = 0;
};

inline constexpr int kAwarenessIndicatorFrames = 128;

class ActionPlanningRoom {
 public:
  ActionPlanningRoom() = default;
  ActionPlanningRoom(const ActionPlanningRoom&) = delete;
  ActionPlanningRoom& operator=(const ActionPlanningRoom&) = delete;
  ActionPlanningRoom(ActionPlanningRoom&&) = delete;
  ActionPlanningRoom& operator=(ActionPlanningRoom&&) = delete;
  virtual ~ActionPlanningRoom() = default;

  virtual void ResetReadiness() = 0;
  virtual void ReconcileCompletedPlan() = 0;
  virtual void ApplyRelationshipChanges() = 0;
  // Walks the live actor list again; each planning pass reads a fresh walk.
  [[nodiscard]] virtual std::vector<PlanningActor> Actors() const = 0;
  // Clears the per-plan scratch, refreshes vertical movement and selects the
  // fallback action.
  virtual void PrepareActorPlan(const PlanningActor& actor) = 0;
  virtual void ClearReengagementGrace(const PlanningActor& actor) = 0;
  virtual void SetDefeated(const PlanningActor& actor, bool defeated) = 0;
  // Returns whether the rebuilt plan uses the room action delay.
  [[nodiscard]] virtual bool BuildActorPlan(const PlanningActor& actor) = 0;
  [[nodiscard]] virtual bool SuppressesAwarenessIndicator(
      const PlanningActor& actor) const = 0;
  virtual void ResetAwarenessIndicator(const PlanningActor& actor) = 0;
  virtual void SetAwarenessIndicator(const PlanningActor& actor,
                                     int frames) = 0;
};

// func_800DEC88, BATTLE/BATTLE.PRG/74088.c:1547.
void RebuildActorActionPlans(ActionPlanningRoom& room,
                             const std::optional<PlanningRequest>& request);

}  // namespace VS::Battle::Ai

#endif  // NATIVE_GAME_INCLUDE_BATTLE_AI_PLANNINGWORKSPACE_HPP_
