#pragma once
// Hand-maintained (tools/port/owned.csv).
// Ashley's move input as the copied code reads it. ActorCommandsLive.cpp
// defines it; UpdateLiveAshleyMovementInput writes words 1 and 3 and the room
// loader word 0.

#include "Legacy/Psx.hpp"

namespace VS::Actors {

// decomp: D_800F1A50 (BATTLE/BATTLE.PRG src/BATTLE/BATTLE.PRG/146C.c:349): word
// 1 is the lateral and word 3 the forward stick or d-pad input of the last
// frame.
inline constexpr int kAshleyMoveInputWords = 12;
extern int ashleyMoveInput[kAshleyMoveInputWords];

}  // namespace VS::Actors
