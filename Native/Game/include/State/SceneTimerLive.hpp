#pragma once
// Hand-maintained (tools/port/owned.csv).

// Bindings from the live scene timer (VS::State::sceneTimer, State/Timers.hpp)
// and tick speed to the typed rules (State/SceneTimer.hpp), in the original
// argument order. The time mirrors into the live script flags.
namespace VS::State {
void StartLiveSceneTimer(int mode, int reset_scope, int packed_time);
void ResetLiveSceneTimer();
void SetLiveSceneTimerPaused(int paused);
// Returns the tick speed after the request.
int SetLiveTickSpeed(int requested_tick_speed);
// Returns 1 when an expiry trigger fired, else 0.
int UpdateLiveSceneTimer();
}  // namespace VS::State
