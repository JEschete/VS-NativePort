#pragma once
// Hand-maintained (tools/port/owned.csv).
#include "Actors/Location.hpp"
#include "Legacy/Psx.hpp"

namespace VS::Field {

// decomp: D_800F1910_t (src/BATTLE/BATTLE.PRG/146C.c:94)
struct InteractionFocus {
  VS::Actors::ActorLocationQuery actor_query;
};

}  // namespace VS::Field
