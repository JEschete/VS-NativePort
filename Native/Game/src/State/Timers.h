#pragma once
// Hand-maintained (tools/port/owned.csv).
// The legacy scene timer record and tick speed. SceneTimerLive.cpp defines
// them and binds them to the typed rules (State/SceneTimer.hpp); the
// renderer, input, actor and save code still read the tick speed and the
// record directly.

#include "State/SceneTimer.h"

#ifdef __cplusplus
extern "C" {
#endif

// decomp: D_80050468 (SLUS_010.40 main.h:422)
extern State_SceneTimer State_sceneTimer;
// decomp: vs_gametime_tickspeed: the frames a tick advances, 2 or 4.
extern int State_tickSpeed;

#ifdef __cplusplus
}
#endif
