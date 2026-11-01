#pragma once
// Hand-maintained (tools/port/owned.csv).
#include <chrono>

namespace VS::Core {
// Converts a frame-work duration to original root-counter units (15,734 per
// second), wrapped to 16 bits.
[[nodiscard]] int FrameCounterTicks(std::chrono::nanoseconds elapsed);

// Units elapsed since the last baseline, or the stepped replay value.
[[nodiscard]] int ElapsedFrameCounterTicks();

// Starts a new frame-work measurement and restarts the stepped sequence.
void ResetFrameTimingBaseline();

// Replays: each read since the baseline advances by `step` units instead of
// reading the clock, because this value feeds the AI's random state. 0 stops.
void StepFrameCounterTicks(int step);
}  // namespace VS::Core
