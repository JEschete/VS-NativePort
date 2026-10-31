// Hand-maintained (tools/port/owned.csv).
extern "C" {
#include "Battle/Ai/ActorStateHelpers.h"

#include <stdlib.h>

#include "Battle/Ai/ActorState.h"
#include "Battle/Ai/AiState.h"
#include "Legacy/Psx.h"

enum { kReactionDelayFrames = 30 };

// decomp: BATTLE.PRG func_800E4C64 (src/BATTLE/BATTLE.PRG/7BDD4.s:571).
void Battle_Ai_ResetActorAiReactionDelay(int actor_id) {
  if ((unsigned)actor_id >=
      sizeof Battle_Ai_actorAiStates / sizeof Battle_Ai_actorAiStates[0]) {
    Core_Debugf("ai", "reaction delay actor ID is out of bounds: %d", actor_id);
    abort();
  }
  if (actor_id == 0) return;
  Battle_Ai_ActorAiState* state = Battle_Ai_actorAiStates[actor_id];
  if (state != nullptr) state->minActionDelay = kReactionDelayFrames;
}
}  // extern "C"
