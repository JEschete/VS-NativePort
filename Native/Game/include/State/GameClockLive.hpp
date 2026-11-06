#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "State/GameClock.hpp"

// Binding from the live play time record (State_playTime, State/PlayTime.h)
// to the typed clock (State/GameClock.hpp), in the original argument order
// for the generated callers (tools/port/copy_fixups.py OWNED_CALL_BINDINGS).
namespace VS::State {
[[nodiscard]] PlayTime LivePlayTime() noexcept;
void StoreLivePlayTime(const PlayTime& time) noexcept;

// decomp: SLUS_010.40 vs_main_gametimeUpdate (main.c:8188). Waits for the
// vertical sync (idling for any wait but 1), services the disc queue, then
// advances the play time by the tick speed. Returns VSync's result.
int UpdateLivePlayTimeAndLoading(int vsync);
}  // namespace VS::State
