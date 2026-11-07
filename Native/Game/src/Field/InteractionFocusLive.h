#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifndef VS_OWNED_FIELD_INTERACTIONFOCUSLIVE_H
#define VS_OWNED_FIELD_INTERACTIONFOCUSLIVE_H
#include "Actors/Location.h"
#include "Legacy/Psx.h"

// decomp: D_800F1910_t (src/BATTLE/BATTLE.PRG/146C.c:159)
typedef struct Field_InteractionFocus {
  Actors_ActorLocationQuery actor_query;
} Field_InteractionFocus;

#endif  // VS_OWNED_FIELD_INTERACTIONFOCUSLIVE_H
