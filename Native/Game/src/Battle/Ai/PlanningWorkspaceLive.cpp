#include "Battle/Ai/PlanningWorkspaceLive.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Boot/SessionLive.hpp"

extern "C" {
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActionSelection.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/MovementBehavior.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
}

namespace VS::Battle::Ai {
namespace {

// The completed-plan reconcile and relationship update read the request's
// header as func_800DEEA4_t; its target list starts where that header ends.
static_assert(sizeof(Battle_HitResult) == sizeof(Battle_Ai_EngagementActor));
static_assert(offsetof(Battle_ActionRequest, targets) ==
              sizeof(Battle_Ai_ReactionRecord));

// actor->command.fields.unk8 bit 5 (74088.c:1590, lbu 0x8).
constexpr unsigned kSuppressAwarenessIndicator = 0x20;

class BoundPlanningWorkspace {
 public:
  explicit BoundPlanningWorkspace(PlanningWorkspace& workspace) {
    Battle_Ai_planningWorkspace =
        reinterpret_cast<Battle_Ai_ReactionRecord*>(&workspace.result);
    Battle_Ai_planningPassCandidates = workspace.candidates;
    Battle_Ai_candidateExclusions = workspace.exclusions;
    Battle_Ai_fallbackCandidates = workspace.fallback;
  }
  BoundPlanningWorkspace(const BoundPlanningWorkspace&) = delete;
  BoundPlanningWorkspace& operator=(const BoundPlanningWorkspace&) = delete;
  BoundPlanningWorkspace(BoundPlanningWorkspace&&) = delete;
  BoundPlanningWorkspace& operator=(BoundPlanningWorkspace&&) = delete;
  ~BoundPlanningWorkspace() {
    Battle_Ai_planningWorkspace = nullptr;
    Battle_Ai_planningPassCandidates = nullptr;
    Battle_Ai_candidateExclusions = nullptr;
    Battle_Ai_fallbackCandidates = nullptr;
  }

 private:
  // A session transition would skip this destructor and leave the globals
  // pointing at the caller's workspace (#266).
  VS::Boot::LiveSessionTransitionBarrier barrier_;
};

Battle_Ai_ActorAiState& AiStateOf(int slot) {
  if (slot < 0 ||
      slot >= static_cast<int>(std::size(Battle_Ai_actorAiStates)) ||
      Battle_Ai_actorAiStates[slot] == nullptr) {
    throw std::logic_error("battle actor has no AI state");
  }
  return *Battle_Ai_actorAiStates[slot];
}

Battle_Actor& ActorOf(const PlanningActor& actor) {
  for (Battle_Actor* live = Battle_actors[0]; live != nullptr;
       live = live->next) {
    if (live->id == actor.slot) {
      return *live;
    }
  }
  throw std::logic_error("planned actor left the battle actor list");
}

Battle_ActorStats& StatsOf(const Battle_Actor& actor) {
  if (actor.stats == nullptr) {
    throw std::logic_error("battle actor has no stats");
  }
  return *actor.stats;
}

class LiveActionPlanningRoom final : public ActionPlanningRoom {
 public:
  explicit LiveActionPlanningRoom(Battle_ActionRequest* request)
      : record_(reinterpret_cast<Battle_Ai_ReactionRecord*>(request)) {}

  void ResetReadiness() override { Battle_Ai_roomAiState->readinessLevel = 0; }

  void ReconcileCompletedPlan() override {
    Battle_Ai_ReconcileCompletedActionPlan(record_);
  }

  void ApplyRelationshipChanges() override {
    Battle_Ai_ApplyActionRelationshipChanges(record_);
  }

  [[nodiscard]] std::vector<PlanningActor> Actors() const override {
    std::vector<PlanningActor> actors;
    for (const Battle_Actor* live = Battle_actors[0]; live != nullptr;
         live = live->next) {
      actors.push_back(
          {live->id, AiStateOf(live->id).actorId, StatsOf(*live).currentHp});
    }
    return actors;
  }

  void PrepareActorPlan(const PlanningActor& actor) override {
    auto& plan = AiStateOf(actor.slot);
    plan.opaqueE8 = 0;
    plan.opaqueE7 = 0;
    plan.opaque19A = 0;
    // The original loads both speeds with lbu.
    const auto& stats = StatsOf(ActorOf(actor));
    VS::Battle::Ai::SetLiveActorVerticalMovementSteps(
        actor.actor_id, static_cast<unsigned char>(stats.walkSpeed),
        static_cast<unsigned char>(stats.runSpeed));
    Battle_Ai_SelectFallbackAction(&plan);
  }

  void ClearReengagementGrace(const PlanningActor& actor) override {
    AiStateOf(actor.slot).reengagementGraceFrames = 0;
  }

  void SetDefeated(const PlanningActor& actor, bool defeated) override {
    AiStateOf(actor.slot).isDead = defeated ? 1 : 0;
  }

  [[nodiscard]] bool BuildActorPlan(const PlanningActor& actor) override {
    auto& plan = AiStateOf(actor.slot);
    Battle_Ai_BuildActorActionPlan(
        reinterpret_cast<Battle_Ai_ActionPlanView*>(&plan));
    return plan.usesRoomActionDelay != 0;
  }

  [[nodiscard]] bool SuppressesAwarenessIndicator(
      const PlanningActor& actor) const override {
    return (ActorOf(actor).commandState & kSuppressAwarenessIndicator) != 0;
  }

  void ResetAwarenessIndicator(const PlanningActor& actor) override {
    Battle_Ai_ResetActorAwarenessIndicator(
        reinterpret_cast<Battle_Ai_PlanEvaluationView*>(
            &AiStateOf(actor.slot)));
  }

  void SetAwarenessIndicator(const PlanningActor& actor, int frames) override {
    Battle_Ai_SetActorAwarenessIndicator(actor.actor_id, frames);
  }

 private:
  Battle_Ai_ReactionRecord* record_;
};

}  // namespace

PlanningRequest PlanningRequestOf(const Battle_ActionRequest& request) {
  return {request.primaryTarget.target.targetActor,
          request.primaryTarget.targetsPosition};
}

int RebuildLiveActorActionPlans(Battle_ActionRequest* request) {
  PlanningWorkspace workspace;
  const BoundPlanningWorkspace binding(workspace);
  LiveActionPlanningRoom room(request);
  std::optional<PlanningRequest> planning;
  if (request != nullptr) {
    planning = PlanningRequestOf(*request);
  }
  RebuildActorActionPlans(room, planning);
  return 0;
}

}  // namespace VS::Battle::Ai
