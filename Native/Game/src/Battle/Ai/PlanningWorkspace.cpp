#include "Battle/Ai/PlanningWorkspace.hpp"

namespace VS::Battle::Ai {

// func_800DEC88, BATTLE/BATTLE.PRG/74088.c:1547. The original walks the
// battle actor list three times between the completed-plan reconcile and the
// relationship update; the planning storage it allocates is bound by the
// caller.
void RebuildActorActionPlans(ActionPlanningRoom& room,
                             const std::optional<PlanningRequest>& request) {
  room.ResetReadiness();
  room.ReconcileCompletedPlan();
  for (const PlanningActor& actor : room.Actors()) {
    room.PrepareActorPlan(actor);
    if (request && request->targets_position == 0 &&
        request->primary_target == actor.actor_id) {
      room.ClearReengagementGrace(actor);
    }
  }
  room.ApplyRelationshipChanges();
  for (const PlanningActor& actor : room.Actors()) {
    room.SetDefeated(actor, actor.hp == 0);
  }
  for (const PlanningActor& actor : room.Actors()) {
    if (!room.BuildActorPlan(actor)) {
      continue;
    }
    if (room.SuppressesAwarenessIndicator(actor)) {
      room.ResetAwarenessIndicator(actor);
    } else {
      room.SetAwarenessIndicator(actor, kAwarenessIndicatorFrames);
    }
  }
  room.ResetReadiness();
}

}  // namespace VS::Battle::Ai
