// Hand-maintained (tools/port/owned.csv).
#include <cstdlib>

#include "Battle/Ai/ReactionDelay.hpp"
#include "Core/Diagnostics.h"

namespace VS::Battle::Ai {
// decomp: BATTLE.PRG func_800E4C64 (src/BATTLE/BATTLE.PRG/7BDD4.s:571).
void ResetActorAiReactionDelay(ActorReactionDelays& actors, int actor_id) {
  if (actor_id < 0 || actor_id >= kAiActorSlotCount) {
    Core_Errorf("ai", "reaction delay actor ID is out of bounds: %d", actor_id);
    std::abort();
  }
  if (actor_id == 0) return;
  static_cast<void>(
      actors.SetMinimumActionDelay(actor_id, kReactionDelayFrames));
}
}  // namespace VS::Battle::Ai
