#pragma once
// Hand-maintained (tools/port/owned.csv).

// Binding from the live play time (VS::State::playTime, State/PlayTime.hpp)
// to the typed clock (State/GameClock.hpp).
namespace VS::State {
// decomp: SLUS_010.40 vs_main_gametimeUpdate (main.c:8188). Waits for the
// vertical sync (idling for any wait but 1), services the disc queue, then
// advances the play time by the tick speed. Returns VSync's result.
int UpdateLivePlayTimeAndLoading(int vsync);
}  // namespace VS::State
