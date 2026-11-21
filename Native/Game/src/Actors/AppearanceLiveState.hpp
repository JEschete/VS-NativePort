#pragma once
// Hand-maintained (tools/port/owned.csv).
// The room's model scale as the copied code reads it. AppearanceLive.cpp
// defines it; SetLiveRoomActorScale is its only writer.

#include "Legacy/Psx.hpp"

namespace VS::Actors {

// decomp: D_800F4438 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/30DB0.c:107): Q12
// x, y and z scales. New actors take x, truncated to a halfword, as their model
// scale.
extern VS::Core::Gte::VECTOR roomActorScale;

}  // namespace VS::Actors
