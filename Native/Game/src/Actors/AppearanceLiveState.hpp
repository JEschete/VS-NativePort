#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room's model scale as the copied code reads it. AppearanceLive.cpp
// defines it; SetLiveRoomActorScale is its only writer.

#include "Legacy/Psx.h"

namespace VS::Actors {

// decomp: D_800F4438 (BATTLE/BATTLE.PRG 30DB0.c): Q12 x, y and z scales.
// New actors take x, truncated to a halfword, as their model scale.
extern VECTOR roomActorScale;

}  // namespace VS::Actors
