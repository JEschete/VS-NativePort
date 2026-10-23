#include "Battle/Ai/PlanningWorkspace.hpp"

#include <cstddef>

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

namespace {
static_assert(sizeof(Battle_HitResult) == sizeof(Battle_Ai_EngagementActor));
static_assert(offsetof(Battle_ActionRequest, targets) ==
              sizeof(Battle_Ai_ReactionRecord));

Battle_Ai_EvaluationActorView* FirstActor() {
  return reinterpret_cast<Battle_Ai_EvaluationActorView*>(Battle_actors[0]);
}

Battle_Ai_PlanEvaluationView* Plan(Battle_Ai_EvaluationActorView* actor) {
  return reinterpret_cast<Battle_Ai_PlanEvaluationView*>(
      Battle_Ai_actorAiStates[actor->id]);
}

void PrepareActors(Battle_Ai_EvaluationContextView* context) {
  for (auto* actor = FirstActor(); actor != nullptr; actor = actor->next) {
    auto* state = Plan(actor);
    state->opaqueE8 = 0;
    state->opaqueE7 = 0;
    state->opaque19A = 0;
    Battle_Ai_SetActorVerticalMovementSteps(
        state->actorId, actor->stats->opaque31, actor->stats->opaque33);
    Battle_Ai_SelectFallbackAction(Battle_Ai_actorAiStates[actor->id]);
    if (context != nullptr && !context->opaque44 &&
        context->id == state->actorId) {
      state->reengagementGraceFrames = 0;
    }
  }
}

void BuildPlans() {
  constexpr int kAwarenessFlag = 32;
  constexpr int kAwarenessDuration = 128;
  for (auto* actor = FirstActor(); actor != nullptr; actor = actor->next) {
    Plan(actor)->isDead = actor->stats->hp == 0;
  }
  for (auto* actor = FirstActor(); actor != nullptr; actor = actor->next) {
    auto* state = Plan(actor);
    Battle_Ai_BuildActorActionPlan(
        reinterpret_cast<Battle_Ai_ActionPlanView*>(state));
    if (state->usesRoomActionDelay) {
      if (actor->opaque8 & kAwarenessFlag) {
        Battle_Ai_ResetActorAwarenessIndicator(state);
      } else {
        Battle_Ai_SetActorAwarenessIndicator(state->actorId,
                                             kAwarenessDuration);
      }
    }
  }
}
}  // namespace

// func_800DEC88, BATTLE/BATTLE.PRG/74088.c:1547. Typed call-local storage
// replaces the fixed 0x1ECC allocation and PS1 offsets into its result buffer.
int Battle_Ai_RebuildActorActionPlans(void* input) {
  VS::Game::ai::PlanningWorkspace workspace;
  auto* context = static_cast<Battle_Ai_EvaluationContextView*>(input);
  Battle_Ai_roomAiState->readinessLevel = 0;
  Battle_Ai_planningWorkspace =
      reinterpret_cast<Battle_Ai_ReactionRecord*>(&workspace.result);
  Battle_Ai_planningPassCandidates = workspace.candidates;
  Battle_Ai_candidateExclusions = workspace.exclusions;
  Battle_Ai_fallbackCandidates = workspace.fallback;

  Battle_Ai_ReconcileCompletedActionPlan(
      reinterpret_cast<Battle_Ai_ReactionRecord*>(context));
  PrepareActors(context);
  Battle_Ai_ApplyActionRelationshipChanges(
      reinterpret_cast<Battle_Ai_ReactionRecord*>(context));
  BuildPlans();
  Battle_Ai_roomAiState->readinessLevel = 0;

  Battle_Ai_planningWorkspace = nullptr;
  Battle_Ai_planningPassCandidates = nullptr;
  Battle_Ai_candidateExclusions = nullptr;
  Battle_Ai_fallbackCandidates = nullptr;
  return 0;
}
