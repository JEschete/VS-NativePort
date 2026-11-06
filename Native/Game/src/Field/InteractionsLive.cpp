#include "Field/InteractionsLive.hpp"

extern "C" {
#include "Field/Interactions.h"
}

namespace VS::Field {
bool CheckAshleyTileTriggersLive(int actor_id, Actors_ActorLocation& location) {
  return Field_CheckAshleyTileTriggers(
             actor_id, reinterpret_cast<u_char*>(&location)) != 0;
}
}  // namespace VS::Field
