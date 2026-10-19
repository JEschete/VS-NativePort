// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/DecisionCallbacks.h"

#include "Actors/ActorAllocation.h"
#include "Actors/ActorRecord.h"
#include "Actors/Animation.h"
#include "Actors/ModelInstance.h"
#include "Actors/Movement.h"
#include "Actors/Shadow.h"
#include "Actors/Skeleton.h"
#include "Actors/TextureAnimation.h"
#include "Battle/Actor.h"
#include "Battle/Ai/ActionPlanning.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/Behavior.h"
#include "Battle/Ai/FrameUpdate.h"
#include "Battle/Ai/Movement.h"
#include "Battle/Ai/Navigation.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/Math/FixedPoint.h"
#include "Core/Math/ScalarMath.h"
#include "Core/Math/Vector.h"
#include "Core/Timing.h"
#include "Field/Collision.h"
#include "Field/Gimmicks.h"
#include "Items/Inventory.h"
#include "Items/Loadout.h"
#include "Legacy/Psx.h"
#include "Render/Palette.h"

enum {
  kDefaultPursuitDistance = 256,
  kExtendedPursuitDistance = 320,
  kSupportDistanceSquared = 0x64000,
  kSupportWanderRadius = 5,
  kFirstNonLocomotionCommand = 9,
  kActionStatusBitMask = 31,
  kApproachModeMask = 3
};

// decomp: BATTLE.PRG entry 0x800DBA4C (src/BATTLE/BATTLE.PRG/72B70.s, after
// .L800DBA40).
int Battle_Ai_UpdateFleeOrWanderBehavior(Battle_Ai_ActorAiState* state) {
  if (state->opaqueE == 0) {
    return Battle_Ai_UpdateCombatPursuit(state);
  }
  if (!Battle_Ai_FleeVisibleEnemies((Battle_Ai_CandidateMovementView*)state,
                                    1)) {
    Battle_Ai_WanderAroundSavedCenter(state);
  }
  return 1;
}

// decomp: BATTLE.PRG entry 0x800DBA88 (src/BATTLE/BATTLE.PRG/72B70.s, inside
// func_800DB93C).
int Battle_Ai_UpdateAdaptivePursuitBehavior(Battle_Ai_ActorAiState* state) {
  if (state->opaqueE != 0) {
    return Battle_Ai_FleeVisibleEnemies((Battle_Ai_CandidateMovementView*)state,
                                        1);
  }
  const unsigned mode =
      (1u << (state->actionStatus & kActionStatusBitMask)) & kApproachModeMask;
  if (mode == 0) {
    return Battle_Ai_UpdateCombatPursuit(state);
  }
  const Battle_Ai_ActionCandidate* candidate = state->candidateOrder[0];
  if (candidate == NULL || state->model == NULL) {
    return 0;
  }
  int distance = state->model->opaqueABit5 ? kExtendedPursuitDistance
                                           : kDefaultPursuitDistance;
  if ((mode & 1) != 0) {
    const int candidate_distance =
        Core_Math_SquareRootInteger(candidate->distanceSquared);
    if (candidate_distance < distance) {
      distance = candidate_distance;
    }
  }
  const int result =
      Battle_Ai_ApproachActor(state, candidate->targetId, distance, 0);
  return result != 0 ? result
                     : Battle_Ai_TrackActorPosition(state, candidate->targetId);
}

// decomp: BATTLE.PRG entry 0x800DBB98 (src/BATTLE/BATTLE.PRG/72B70.s, after
// .L800DBB80).
int Battle_Ai_UpdateSupportFollowBehavior(Battle_Ai_ActorAiState* state) {
  const int result = Battle_Ai_UpdateCombatPursuit(state);
  if (result != 0) {
    return result;
  }
  unsigned tile = 0;
  const int target = Battle_Ai_FindActorByRelationship(state, &tile, 2);
  if ((unsigned)target >=
      sizeof state->distancesSquared / sizeof state->distancesSquared[0]) {
    return 0;
  }
  if (state->distancesSquared[target] > kSupportDistanceSquared) {
    Battle_Ai_MoveToTile((Battle_Ai_CandidateMovementView*)state, tile, 0);
  } else {
    Battle_Ai_WanderAroundTile((Battle_Ai_CandidateMovementView*)state, tile,
                               kSupportWanderRadius);
  }
  return 1;
}

// decomp: BATTLE.PRG D_800DBC0C (src/BATTLE/BATTLE.PRG/72B70.s:681).
int Battle_Ai_UpdateEngagementApproachBehavior(Battle_Ai_ActorAiState* state) {
  if (Battle_Ai_IsEngagementReady(state)) {
    const int pursuit = Battle_Ai_UpdateCombatPursuit(state);
    if (pursuit != 0) {
      return pursuit;
    }
  }
  const int result =
      Battle_Ai_ApproachActor(state, 0, kDefaultPursuitDistance, 0);
  return result != 0 ? result : Battle_Ai_TrackActorPosition(state, 0);
}

// decomp: BATTLE.PRG func_800DB7B4 (src/BATTLE/BATTLE.PRG/72B70.s:357).
void Battle_Ai_UpdateActorLocomotionMode(Battle_Ai_DecisionDispatchView* view) {
  Battle_Ai_ActorAiState* state = (void*)view;
  if (state->behaviorMode == 0) return;
  if (state->behaviorMode == 1 && ((Battle_Actor*)state->actor)->commandOpcode <
                                      kFirstNonLocomotionCommand) {
    Battle_Ai_IssueActorCommand(state, 1);
    return;
  }
  Battle_Ai_ReleaseActorAiReservations(state);
  state->behaviorMode = 0;
  Battle_Ai_IssueActorCommand(state, 2);
}

// decomp: BATTLE.PRG func_800DB820 (src/BATTLE/BATTLE.PRG/72B70.s:390).
void Battle_Ai_DispatchMovementBehavior(Battle_Ai_DecisionDispatchView* view) {
  Battle_Ai_ActorAiState* state = (void*)view;
  Battle_Ai_SettingsView* settings = (void*)state;
  if (Battle_Ai_TryExecutePlannedActions(state) ||
      Battle_Ai_TryRecoverBlockedMovement((void*)state)) {
    return;
  }
  if (settings->decisionCallback != NULL && settings->decisionCallback(state)) {
    return;
  }
  Battle_Ai_WanderAroundSavedCenter(state);
}
