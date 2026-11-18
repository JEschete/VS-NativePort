#pragma once
// Hand-maintained (tools/port/owned.csv).
#include "Actors/Location.h"
#include "Legacy/Psx.h"

// decomp: D_800F1910_t (src/BATTLE/BATTLE.PRG/146C.c:159)
struct Field_InteractionFocus {
  Actors_ActorLocationQuery actor_query;
};
