#pragma once
// Hand-maintained (tools/port/owned.csv).
// The scene timer and tick speed. SceneTimerLive.cpp defines them and runs
// them with the typed rules (State/SceneTimer.hpp); the renderer, input, actor
// and save code read them directly.

#include <cstddef>

#include "State/SceneTimer.hpp"

namespace VS::State {

// The original's D_80050468_t, which the guest memory binds.
inline constexpr std::size_t kSceneTimerBytes = 8;
static_assert(sizeof(SceneTimer) == kSceneTimerBytes);

// decomp: D_80050468 (SLUS_010.40 main.h:422)
extern SceneTimer sceneTimer;
// decomp: vs_gametime_tickspeed: the frames a tick advances, 2 or 4.
extern int tickSpeed;

}  // namespace VS::State
