// Hand-maintained (tools/port/owned.csv).
#include "Battle/Ai/Relations.h"

#include <stdlib.h>

#include "Battle/Actor.h"
#include "Battle/Ai/ActorCleanup.h"
#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Battle/Ai/AiViews.h"
#include "Battle/Ai/DecisionCallbacks.h"
#include "Battle/Ai/Engagement.h"
#include "Battle/Ai/Perception.h"
#include "Battle/Damage.h"
#include "Core/BiosRand.h"
#include "Core/Diagnostics.h"
#include "Legacy/Psx.h"

static void RequireAshleyAllianceState(Battle_Ai_ReactionView* state) {
  if (state == NULL ||
      state->actorId >=
          sizeof Battle_Ai_actorAiStates / sizeof Battle_Ai_actorAiStates[0]) {
    Core_Errorf("ai", "Ashley alliance requires a valid actor state");
    abort();
  }
}

// decomp: BATTLE.PRG func_800E7370; categories at 0x80069C0C (7E05C.s:857).
void Battle_Ai_InitializeActorAiRelations(Battle_Ai_AiCreationView* view) {
  enum {
    kActorSlots =
        sizeof Battle_Ai_actorAiStates / sizeof Battle_Ai_actorAiStates[0],
    kNoncombatCategory = 3
  };
  Battle_Ai_ActorAiState* initial = (void*)view;
  if (initial == NULL || initial->actorId >= kActorSlots) abort();
  static const u_char categories[][2] = {{3, 2}, {1, 1}, {1, 2}, {2, 3}};
  unsigned pending = 1u << initial->actorId;
  unsigned visited = 0;
  while (pending != 0) {
    unsigned source_id = 0;
    while ((pending & (1u << source_id)) == 0) ++source_id;
    pending &= ~(1u << source_id);
    visited |= 1u << source_id;
    Battle_Ai_ActorAiState* source = source_id == initial->actorId
                                         ? initial
                                         : Battle_Ai_actorAiStates[source_id];
    if (source == NULL ||
        source->settingsBits21 >= sizeof categories / sizeof categories[0]) {
      abort();
    }
    unsigned count = 0;
    for (Battle_Actor* actor = Battle_actors[0]; actor != NULL;
         actor = actor->next) {
      if (++count > kActorSlots || (unsigned)actor->id >= kActorSlots) abort();
      Battle_Ai_ActorAiState* other = Battle_Ai_actorAiStates[actor->id];
      if (other == NULL) abort();
      const unsigned target_class =
          actor->id != 0 && other->settingsBits21 < kNoncombatCategory;
      source->relations[actor->id].classification =
          source_id == (unsigned)actor->id
              ? 2
              : categories[source->settingsBits21][target_class];
      if (other->relations[source_id].classification == 0 &&
          !(visited & (1u << actor->id))) {
        pending |= 1u << actor->id;
      }
    }
  }
}

// decomp: BATTLE.PRG func_800DC19C (src/BATTLE/BATTLE.PRG/7399C.s:11).
void Battle_Ai_EndAshleyAlliance(Battle_Ai_ReactionView* state) {
  enum {
    kClearAllianceMarker = 0x40,
    kRecoveryMarker = 6,
    kRecoveryFrames = 0x28,
    kRecoveryCommand = 2,
    kRecoveryRange = 0x80
  };
  RequireAshleyAllianceState(state);
  Battle_Ai_ActorAiState* actor_state = (Battle_Ai_ActorAiState*)state;
  Battle_Ai_SettingsView* settings = (Battle_Ai_SettingsView*)state;

  state->allianceReactionCount = 0;
  if (state->temporaryAshleyAlliance == 0) return;

  state->temporaryAshleyAlliance = 0;
  Battle_Ai_SetActorAwarenessIndicator(state->actorId, kClearAllianceMarker);
  Battle_Ai_InitializeActorAiRelations((Battle_Ai_AiCreationView*)state);

  int (*saved_callback)(Battle_Ai_ActorAiState*) =
      settings->savedDecisionCallback;
  settings->savedDecisionCallback = NULL;
  if (saved_callback != NULL) {
    settings->decisionCallback = saved_callback;
  }

  if (state->settingsBit25 != 0) return;
  Battle_Ai_SetTemporaryAwarenessIndicator(actor_state, kRecoveryMarker,
                                           kRecoveryFrames);
  Battle_Ai_SetActorAiReactionWithDelay((Battle_Ai_PerceptionUpdateView*)state,
                                        kRecoveryCommand, kRecoveryRange);
}

// decomp: BATTLE.PRG func_800DC210 (src/BATTLE/BATTLE.PRG/7399C.s:43).
void Battle_Ai_TryBeginAshleyAlliance(Battle_Ai_ReactionView* state,
                                      int forced) {
  enum {
    kRollMask = 0x7F,
    kFavorableThreshold = 0x4C,
    kRequiredReactions = 2,
    kAllianceMarker = 0x80
  };
  RequireAshleyAllianceState(state);
  if (forced == 0) {
    if ((Core_Random_Rand() & kRollMask) >= kFavorableThreshold) return;
    // 7399C.s stores the byte but tests the untruncated register value.
    const unsigned reactions = (unsigned)state->allianceReactionCount + 1;
    state->allianceReactionCount = (u_char)reactions;
    if (reactions < kRequiredReactions) return;
  }

  Battle_Ai_SettingsView* settings = (Battle_Ai_SettingsView*)state;
  state->temporaryAshleyAlliance = 1;
  settings->savedDecisionCallback = settings->decisionCallback;
  settings->decisionCallback = Battle_Ai_UpdateEngagementApproachBehavior;
  Battle_Ai_SetActorAwarenessIndicator(state->actorId, kAllianceMarker);
}

// decomp: BATTLE.PRG func_800E72D0 (src/BATTLE/BATTLE.PRG/7E05C.s:810).
void Battle_Ai_ApplySavedActorAiRelations(void) {
  enum {
    kActorSlots =
        sizeof Battle_Ai_actorAiStates / sizeof Battle_Ai_actorAiStates[0],
    kPreservedRelationFlag = 8,
    kClassificationMask = 3,
    kPreviousClassShift = 3,
    kTimerShift = 6,
    kTimerMask = 0x7F
  };
  unsigned count = 0;
  for (Battle_Actor* actor = Battle_actors[0]; actor != NULL;
       actor = actor->next) {
    if (++count > kActorSlots || (unsigned)actor->id >= kActorSlots) abort();
    Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[actor->id];
    if (state == NULL) abort();
    if (state->savedRelationOptions < 0) continue;
    const unsigned options = (u_short)state->savedRelationOptions;
    Battle_Ai_RelationWord* relation = &state->relations[0];
    relation->active = 0;
    relation->reserved0Bit1 &= kPreservedRelationFlag;
    relation->previousClassification =
        (options >> kPreviousClassShift) & kClassificationMask;
    relation->classification = (options >> 1) & kClassificationMask;
    relation->classificationTimer = (options >> kTimerShift) & kTimerMask;
    if (options & 1) Battle_Ai_TryBeginAshleyAlliance((void*)state, 1);
  }
}
