#pragma once
// Hand-maintained (tools/port/owned.csv).

// Binding from the live play time (VS::State::playTime, State/PlayTime.hpp)
// to the typed clock (State/GameClock.hpp).
namespace VS::State {
// decomp: SLUS_010.40 vs_main_gametimeUpdate (main.c:8188). Waits `ticks`
// logical ticks (below one waits one; the original's VSync(0) and VSync(n)),
// services the disc queue, then advances the play time by the tick speed. No
// caller asked the original for VSync(1)'s scanline count, and VSync's result
// was never read.
void UpdateLivePlayTimeAndLoading(int ticks = 1);
}  // namespace VS::State
