#include "Battle/Ai/PlanningWorkspaceLive.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include "Actors/ActorSpawningLive.hpp"
#include "Battle/Actor.hpp"
#include "Battle/Ai/ActionPlanning.hpp"
#include "Battle/Ai/ActorState.hpp"
#include "Battle/Ai/AiViews.hpp"
#include "Battle/Ai/EngagementLive.hpp"
#include "Battle/Ai/FrameUpdateLive.hpp"
#include "Battle/Ai/Movement.hpp"
#include "Battle/Ai/MovementBehaviorLive.hpp"
#include "Battle/Ai/Perception.hpp"
#include "Battle/Ai/PlanningPrimitivesLive.hpp"
#include "Battle/Ai/PlanningWorkspace.hpp"
#include "Battle/Damage.hpp"

using VS::Battle::ActionRequestRecord;

namespace VS::Battle::Ai {
namespace {

// The completed-plan reconcile and relationship update read the request's
// header as func_800DEEA4_t; its target list starts where that header ends.
static_assert(sizeof(VS::Battle::HitResult) ==
              sizeof(VS::Battle::Ai::EngagementActor));
static_assert(offsetof(ActionRequestRecord, targets) ==
              sizeof(VS::Battle::Ai::ReactionRecord));

// actor->command.fields.unk8 bit 5 (74088.c:1590, lbu 0x8).
constexpr unsigned kSuppressAwarenessIndicator = 0x20;

// Binds the planning globals to one call's workspace; unbinds them on every
// exit, including a session transfer unwinding through the call.
class BoundPlanningWorkspace {
 public:
  explicit BoundPlanningWorkspace(PlanningWorkspace& workspace) {
    VS::Battle::Ai::planningWorkspace =
        reinterpret_cast<VS::Battle::Ai::ReactionRecord*>(&workspace.result);
    VS::Battle::Ai::planningPassCandidates = workspace.candidates;
    VS::Battle::Ai::candidateExclusions = workspace.exclusions;
    VS::Battle::Ai::fallbackCandidates = workspace.fallback;
  }
  BoundPlanningWorkspace(const BoundPlanningWorkspace&) = delete;
  BoundPlanningWorkspace& operator=(const BoundPlanningWorkspace&) = delete;
  BoundPlanningWorkspace(BoundPlanningWorkspace&&) = delete;
  BoundPlanningWorkspace& operator=(BoundPlanningWorkspace&&) = delete;
  ~BoundPlanningWorkspace() {
    VS::Battle::Ai::planningWorkspace = nullptr;
    VS::Battle::Ai::planningPassCandidates = nullptr;
    VS::Battle::Ai::candidateExclusions = nullptr;
    VS::Battle::Ai::fallbackCandidates = nullptr;
  }
};

VS::Battle::Ai::ActorAiState& AiStateOf(int slot) {
  if (slot < 0 ||
      slot >= static_cast<int>(std::size(VS::Battle::Ai::actorAiStates)) ||
      VS::Battle::Ai::actorAiStates[slot] == nullptr) {
    throw std::logic_error("battle actor has no AI state");
  }
  return *VS::Battle::Ai::actorAiStates[slot];
}

VS::Battle::Actor& ActorOf(const PlanningActor& actor) {
  for (VS::Battle::Actor* live = VS::Battle::actors[0]; live != nullptr;
       live = VS::Actors::ResolveLiveActorInstance(live->next)) {
    if (live->id == actor.slot) {
      return *live;
    }
  }
  throw std::logic_error("planned actor left the battle actor list");
}

VS::Battle::ActorStats& StatsOf(const VS::Battle::Actor& actor) {
  if (actor.stats == nullptr) {
    throw std::logic_error("battle actor has no stats");
  }
  return *actor.stats;
}

class LiveActionPlanningRoom final : public ActionPlanningRoom {
 public:
  explicit LiveActionPlanningRoom(ActionRequestRecord* request)
      : request_(request),
        record_(reinterpret_cast<VS::Battle::Ai::ReactionRecord*>(request)) {}

  void ResetReadiness() override {
    VS::Battle::Ai::roomAiState->readiness_level = 0;
  }

  void ReconcileCompletedPlan() override {
    ReconcileLiveCompletedActionPlan(request_);
  }

  void ApplyRelationshipChanges() override {
    ApplyLiveActionRelationshipChanges(record_);
  }

  [[nodiscard]] std::vector<PlanningActor> Actors() const override {
    std::vector<PlanningActor> actors;
    for (const VS::Battle::Actor* live = VS::Battle::actors[0]; live != nullptr;
         live = VS::Actors::ResolveLiveActorInstance(live->next)) {
      actors.push_back(
          {live->id, AiStateOf(live->id).actor_id, StatsOf(*live).current_hp});
    }
    return actors;
  }

  void PrepareActorPlan(const PlanningActor& actor) override {
    auto& plan = AiStateOf(actor.slot);
    plan.opaque_e8 = 0;
    plan.opaque_e7 = 0;
    plan.recovery_frames = 0;
    // The original loads both speeds with lbu.
    const auto& stats = StatsOf(ActorOf(actor));
    VS::Battle::Ai::SetLiveActorVerticalMovementSteps(
        actor.actor_id, static_cast<unsigned char>(stats.walk_speed),
        static_cast<unsigned char>(stats.run_speed));
    SelectLiveFallbackAction(&plan);
  }

  void ClearReengagementGrace(const PlanningActor& actor) override {
    AiStateOf(actor.slot).reengagement_grace_frames = 0;
  }

  void SetDefeated(const PlanningActor& actor, bool defeated) override {
    AiStateOf(actor.slot).is_dead = defeated ? 1 : 0;
  }

  [[nodiscard]] bool BuildActorPlan(const PlanningActor& actor) override {
    auto& plan = AiStateOf(actor.slot);
    BuildLiveActorActionPlan(&plan);
    return plan.temporary_ashley_alliance != 0;
  }

  [[nodiscard]] bool SuppressesAwarenessIndicator(
      const PlanningActor& actor) const override {
    return (ActorOf(actor).command_state & kSuppressAwarenessIndicator) != 0;
  }

  void ResetAwarenessIndicator(const PlanningActor& actor) override {
    VS::Battle::Ai::ResetLiveActorAwarenessIndicator(
        reinterpret_cast<VS::Battle::Ai::PlanEvaluationView*>(
            &AiStateOf(actor.slot)));
  }

  void SetAwarenessIndicator(const PlanningActor& actor, int frames) override {
    VS::Battle::Ai::SetLiveActorAwarenessIndicator(actor.actor_id, frames);
  }

 private:
  const ActionRequestRecord* request_;
  VS::Battle::Ai::ReactionRecord* record_;
};

}  // namespace

PlanningRequest PlanningRequestOf(const ActionRequestRecord& request) {
  return {request.primary_target.target.target_actor,
          request.primary_target.targets_position};
}

int RebuildLiveActorActionPlans(ActionRequestRecord* request) {
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
