#pragma once
// Hand-maintained (tools/port/owned.csv).
#ifdef __cplusplus
#include <chrono>

namespace VS::Core {
int FrameCounterTicks(std::chrono::nanoseconds elapsed);
}

extern "C" {
#endif
void Core_ResetFrameTimingBaseline(void);
int Core_ElapsedFrameCounterTicks(void);
// Replays: each read since the baseline advances by `step` units instead of
// reading the clock, because this value feeds the AI's random state. 0 stops.
void Core_StepFrameCounterTicks(int step);
#ifdef __cplusplus
}
#endif
