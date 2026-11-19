#pragma once
// Hand-maintained (tools/port/owned.csv).
// The play time. GameClockLive.cpp defines it and advances it with the typed
// rules (State/GameClock.hpp); the menus and save files read it directly.

#include "State/GameClock.hpp"

namespace VS::State {

// decomp: vs_main_gametime (SLUS_010.40 main.h:173 vs_Gametime_t)
extern PlayTime playTime;

}  // namespace VS::State
