#pragma once
// Hand-maintained (tools/port/owned.csv).
// Ashley's move input as the copied code reads it. ActorCommandsLive.cpp
// defines it; UpdateLiveAshleyMovementInput writes words 1 and 3 and the room
// loader word 0.

#include "Legacy/Psx.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_800F1A50 (BATTLE/BATTLE.PRG 146C.c): word 1 is the lateral and
// word 3 the forward stick or d-pad input of the last frame.
enum { kActorsAshleyMoveInputWords = 12 };
extern int Actors_ashleyMoveInput[kActorsAshleyMoveInputWords];

#ifdef __cplusplus
}
#endif
