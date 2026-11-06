#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy play time record. GameClockLive.cpp defines it and advances it
// with the typed rules (State/GameClock.hpp); the menus and save files still
// read it directly.

#include "State/GameTime.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: vs_main_gametime (SLUS_010.40 main.h:173 vs_Gametime_t)
extern State_PlayTime State_playTime;

#ifdef __cplusplus
}
#endif
