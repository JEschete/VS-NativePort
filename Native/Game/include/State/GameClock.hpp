#pragma once
// Hand-maintained (tools/port/owned.csv).

#include "State/Clock.hpp"

// The play time clock the save files and menus show (decomp:
// vs_main_gametime, SLUS_010.40 main.h:173 vs_Gametime_t). It stops at 100
// hours.
namespace VS::State {
inline constexpr int kMaxPlayTimeHours = 100;

using PlayTime = ClockTime;

// decomp: SLUS_010.40 vs_main_gametimeUpdate (main.c:8188), its clock part:
// advances by `tick_speed` frames, carrying whole seconds, minutes and
// hours, unless the clock has reached kMaxPlayTimeHours.
void AdvancePlayTime(PlayTime& time, int tick_speed) noexcept;
}  // namespace VS::State
