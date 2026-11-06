#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "State/SceneTimer.hpp"

// Bindings from the live scene timer record (State_sceneTimer, State/Timers.h)
// and tick speed to the typed rules (State/SceneTimer.hpp), in the original
// argument order for the generated callers (tools/port/copy_fixups.py
// OWNED_CALL_BINDINGS). The time mirrors into the live script flags.
namespace VS::State {
[[nodiscard]] SceneTimer LiveSceneTimer() noexcept;
void StoreLiveSceneTimer(const SceneTimer& timer) noexcept;

void StartLiveSceneTimer(int mode, int reset_scope, int packed_time);
void ResetLiveSceneTimer();
void SetLiveSceneTimerPaused(int paused);
// Returns the tick speed after the request.
int SetLiveTickSpeed(int requested_tick_speed);
// Returns 1 when an expiry trigger fired, else 0.
int UpdateLiveSceneTimer();
}  // namespace VS::State
